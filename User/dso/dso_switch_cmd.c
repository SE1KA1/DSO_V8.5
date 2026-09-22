/**
 * @file dso_switch_cmd.c
 * @brief 开关面板 JSON 控制命令处理（对齐 dsosim protocol v0.9）。
 *
 * 协议（CRLF 定界，一行一 JSON）：
 *   请求  {"cmd":"set_sw","params":{"sw":5,"on":1},"seq":7}
 *   请求  {"cmd":"get_switch","params":{},"seq":8}
 *   应答  {"ver":"0.9","type":"ack","seq":7,"cmd":"set_sw","result":"ok","error":""}
 *   应答  {"ver":"0.9","type":"ack","seq":8,"cmd":"get_switch","result":"ok","error":"","data":{"sw":16383,"led":5,"temp":245,"temp_ok":1}}
 *
 * sw = 14-bit bitmap（bit0=SW1 … bit13=SW14），led = 3-bit（bit0=心跳 bit1=温度报警 bit2=电流报警）。
 * temp = SHT40 温度（十分之一 ℃，245 = 24.5℃）；temp_ok=1 表示传感器有效。
 * bit1 = SHT40 温度 > 阈值（LED2 黄）；bit2 = INA226 过流（LED3 红，芯片 Alert 同步拉低）。
 * 解析为最小 JSON 子集：容忍 json.dumps 默认分隔符（冒号前后空格）。
 *
 * LED0（心跳）不设独立任务：FreeRTOS 堆已被任务栈占满（148KB 临界），
 * 心跳在 get_switch 应答时按系统 tick 实时计算（1s 周期翻转），零堆开销。
 */
#include "dso_switch_cmd.h"
#include "dso_switch_panel.h"
#include "dso_sht40.h"
#include "dso_ina226.h"
#include "dso_ina226_u44.h"
#include "dso_watchdog.h"     /* 远程复位入口 DSO_Watchdog_RequestReboot */
#include "bsp.h"              /* stm32h7xx_hal + bsp_uart_fifo（提供 USART_TypeDef/__IO 前置） */
#include "lwip/netif.h"
#include "debug_uart_lock.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

/* 蜂鸣器静音状态：set_buzzer 命令置位，节奏引擎/串口任务读取。
 * 定义在文件顶部，因 DSO_Switch_Cmd_HandleLine()（下方）与 getter 均需引用。 */
static volatile uint8_t s_buzzer_mute = 0;         /* 1=静音（蜂鸣器完全停，LED 照常） */

/* ---- 最小 JSON 子集辅助（供本模块与 dso_fmc_telemetry 共用） ---- */

/* 返回 "key" 出现的位置（含引号），找不到返回 NULL */
static const char *json_find_key(const char *line, const char *key)
{
    char pat[32];
    if (line == NULL || key == NULL) {
        return NULL;
    }
    /* 必须真的构造出 "\"key\"" 再搜索：pat 若保持未初始化，strstr 用的是栈垃圾，
     * 解析结果随机（曾导致 get_switch/set_sw 时好时坏 → GUI "no reply"）。 */
    if (strlen(key) + 3u > sizeof(pat)) {
        return NULL;
    }
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    return strstr(line, pat);
}

/* 跳过空白 */
static const char *json_skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

/* 读取字符串值：{..."cmd": "set_sw"...} → buf；成功返回 1 */
static int json_get_str(const char *line, const char *key, char *buf, size_t buf_sz)
{
    const char *p = json_find_key(line, key);
    if (p == NULL || buf == NULL || buf_sz == 0) return 0;

    p = json_skip_ws(p + strlen(key) + 2);   /* 跳过 "key" 与空白 */
    if (*p != ':') return 0;
    p = json_skip_ws(p + 1);
    if (*p != '"') return 0;
    p++;
    {
        size_t n = 0;
        while (*p != '\0' && *p != '"' && n + 1 < buf_sz) {
            buf[n++] = *p++;
        }
        buf[n] = '\0';
    }
    return 1;
}

/* 读取整数值：{..."sw": 5} 或 {"sw":5}；成功返回 1 */
static int json_get_int(const char *line, const char *key, int *out)
{
    const char *p = json_find_key(line, key);
    if (p == NULL || out == NULL) return 0;

    p = json_skip_ws(p + strlen(key) + 2);
    if (*p != ':') return 0;
    p = json_skip_ws(p + 1);
    {
        long v = 0;
        int neg = 0;
        if (*p == '-') { neg = 1; p++; }
        if (*p < '0' || *p > '9') return 0;
        while (*p >= '0' && *p <= '9') {
            v = v * 10L + (long)(*p - '0');
            p++;
        }
        *out = neg ? (int)-v : (int)v;
    }
    return 1;
}

/* ---- 命令分发 ---- */

int DSO_Switch_Cmd_HandleLine(const char *line, char *reply, size_t reply_sz)
{
    char cmd[32];
    int  seq = 0;
    int  sw = 0;
    int  on = 0;

    if (line == NULL || reply == NULL || reply_sz == 0) return 0;
    if (!json_get_str(line, "cmd", cmd, sizeof(cmd))) return 0;

    (void)json_get_int(line, "seq", &seq);   /* 可选，默认 0 */

    if (strcmp(cmd, "set_sw") == 0) {
        const char *result = "ok";
        const char *error  = "";
        if (!json_get_int(line, "sw", &sw) || !json_get_int(line, "on", &on) ||
            sw < 0 || sw >= SW_TOTAL || (on != 0 && on != 1)) {
            result = "error";
            error  = "bad_param";
        } else {
            DSO_Switch_Set(sw, on);
        }
        snprintf(reply, reply_sz,
                 "{\"ver\":\"0.9\",\"type\":\"ack\",\"seq\":%d,"
                 "\"cmd\":\"set_sw\",\"result\":\"%s\",\"error\":\"%s\"}",
                 seq, result, error);
        return 1;
    }

    if (strcmp(cmd, "set_buzzer") == 0) {
        const char *result = "ok";
        const char *error  = "";
        if (!json_get_int(line, "on", &on) || (on != 0 && on != 1)) {
            result = "error";
            error  = "bad_param";
        } else {
            s_buzzer_mute = (on == 1) ? 1u : 0u;
        }
        snprintf(reply, reply_sz,
                 "{\"ver\":\"0.9\",\"type\":\"ack\",\"seq\":%d,"
                 "\"cmd\":\"set_buzzer\",\"result\":\"%s\",\"error\":\"%s\"}",
                 seq, result, error);
        return 1;
    }

    if (strcmp(cmd, "get_switch") == 0) {
        uint8_t led = DSO_Switch_GetLedBitmap();
        uint8_t eth_l = (netif_default != NULL &&
                         netif_is_up(netif_default)) ? 1u : 0u;
        uint8_t usb_a = DSO_Switch_UsbActive();
        /* LED0 心跳按系统 tick 实时计算（1s 翻转），避免独立任务占用堆 */
        led = (uint8_t)((led & ~0x01u) |
                        ((uint8_t)((xTaskGetTickCount() / pdMS_TO_TICKS(1000)) & 1u)));
        /* LED2(bit1)=温度告警：SHT40 分级≥预警(level≥1) 亮（黄）；
         * LED3(bit2)=电流告警：U10 或 U44 分级≥预警 亮（红） */
        led = (uint8_t)((led & ~0x06u) |
                        (DSO_Sht40_GetLevel() >= 1u ? 0x02u : 0u) |
                        ((DSO_Ina226_GetLevel() >= 1u ||
                          DSO_Ina226U44_GetLevel() >= 1u) ? 0x04u : 0u));
        snprintf(reply, reply_sz,
                 "{\"ver\":\"0.9\",\"type\":\"ack\",\"seq\":%d,"
                 "\"cmd\":\"get_switch\",\"result\":\"ok\",\"error\":\"\","
                 "\"data\":{\"sw\":%u,\"led\":%u,\"temp\":%ld,\"temp_ok\":%u,"
                 "\"eth_link\":%u,\"usb_active\":%u,\"buzzer_mute\":%u}}",
                 seq, (unsigned)DSO_Switch_GetBitmap(),
                 (unsigned)led,
                 (long)DSO_Sht40_GetTempDezi(),
                 (unsigned)DSO_Sht40_IsValid(),
                 (unsigned)eth_l, (unsigned)usb_a,
                 (unsigned)DSO_Switch_BuzzerMute());
        return 1;
    }

    if (strcmp(cmd, "reboot") == 0) {
        /* 远程复位（GUI 复位键）：本函数只置"待复位"标志，真正的软复位由喂狗任务
         * 延迟 ~300ms 执行——因为 TCP 回发发生在本函数返回之后，若在此直接复位，
         * ack 将永远发不出去。
         * params.mode 当前仅支持 "soft"；"hard"（停喂狗 → SP706T 拉低 NRST，
         * 真硬件复位）留作后续钩子。 */
        DSO_Watchdog_RequestReboot();
        snprintf(reply, reply_sz,
                 "{\"ver\":\"0.9\",\"type\":\"ack\",\"seq\":%d,"
                 "\"cmd\":\"reboot\",\"result\":\"ok\",\"error\":\"\","
                 "\"data\":{\"mode\":\"soft\"}}",
                 seq);
        return 1;
    }

    return 0;   /* 非本模块命令，交给其它处理器 */
}

/* ---- 串口命令接收（CH340/UART1，1152000） ---- */
#define SW_SERIAL_LINE_MAX 128
static char s_serial_line[SW_SERIAL_LINE_MAX];
static int  s_serial_line_len;
static volatile uint32_t s_last_usb_rx_tick = 0;   /* USB-UART 最近收行 tick（YDS5 源） */

static void vSwitchSerialTask(void *pvParameters)
{
    uint8_t ch;
    uint32_t last_alive = 0;
    (void)pvParameters;
    s_serial_line_len = 0;
    for (;;) {
        if (comGetChar(COM1, &ch)) {
            if (ch == '\n') {
                char reply[256];
                s_serial_line[s_serial_line_len] = '\0';
                s_last_usb_rx_tick = xTaskGetTickCount();
                /* 诊断：打印收到的每一行，确认串口 RX 通路（定位后移除） */
                DebugUartLock();
                printf("[SER] got %d: %s\r\n", s_serial_line_len, s_serial_line);
                DebugUartUnlock();
                if (DSO_Switch_Cmd_HandleLine(s_serial_line, reply, sizeof(reply))) {
                    /* 串口回 ack（与其它 printf 互斥，防字节交错） */
                    DebugUartLock();
                    printf("%s\r\n", reply);
                    DebugUartUnlock();
                }
                s_serial_line_len = 0;
            } else if (ch != '\r') {
                if (s_serial_line_len < SW_SERIAL_LINE_MAX - 1) {
                    s_serial_line[s_serial_line_len++] = (char)ch;
                }
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(2));
        }
        /* 诊断：每 1s 打印存活心跳（tick 调度，不依赖 RX 流量），区分「任务没跑」与「RX 硬件断」 */
        if ((uint32_t)(xTaskGetTickCount() - last_alive) >= pdMS_TO_TICKS(1000)) {
            last_alive = xTaskGetTickCount();
            DebugUartLock();
//            printf("[SER] alive tick=%lu\r\n",
//                   (unsigned long)xTaskGetTickCount());
            DebugUartUnlock();
        }
    }
}

void DSO_Switch_Cmd_Init(void)
{
    DSO_Switch_Init();
    /* 优先级 4（> MainTask 的 2）：云桥模块会使 MainTask 卡死(GUI_Init)并 spin，
     * 饿死 prio-1 任务；提到 4 保证开关串口命令能抢占处理。 */
    if (xTaskCreate(vSwitchSerialTask, "SW_Ser", 512, NULL, 3, NULL) != pdPASS) {
        /* 串口命令任务创建失败不致命：TCP 通道仍可用 */
    }
}

/* ---- 公共 JSON 辅助（供 dso_fmc_telemetry 解析 command.execute） ---- */

int DSO_Switch_Cmd_JsonGetStr(const char *line, const char *key, char *buf, size_t buf_sz)
{
    return json_get_str(line, key, buf, buf_sz);
}

int DSO_Switch_Cmd_JsonGetInt(const char *line, const char *key, int *out)
{
    return json_get_int(line, key, out);
}

/* 蜂鸣器静音状态（GUI set_buzzer 置位；节奏引擎读取） */
uint8_t DSO_Switch_BuzzerMute(void)
{
    return s_buzzer_mute;
}

/* USB-UART 通道 5s 内是否有活动（YDS5 数据源；串口收过行即置位） */
uint8_t DSO_Switch_UsbActive(void)
{
    if (s_last_usb_rx_tick == 0u) {
        return 0u;
    }
    return ((uint32_t)xTaskGetTickCount() - s_last_usb_rx_tick)
           < pdMS_TO_TICKS(5000u) ? 1u : 0u;
}
