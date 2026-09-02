/**
 * @file dso_switch_cmd.c
 * @brief 开关面板 JSON 控制命令处理（对齐 dsosim protocol v0.9）。
 *
 * 协议（CRLF 定界，一行一 JSON）：
 *   请求  {"cmd":"set_sw","params":{"sw":5,"on":1},"seq":7}
 *   请求  {"cmd":"get_switch","params":{},"seq":8}
 *   应答  {"ver":"0.9","type":"ack","seq":7,"cmd":"set_sw","result":"ok","error":""}
 *   应答  {"ver":"0.9","type":"ack","seq":8,"cmd":"get_switch","result":"ok","error":"","data":{"sw":16383,"led":5}}
 *
 * sw = 14-bit bitmap（bit0=SW1 … bit13=SW14），led = 3-bit（bit0=心跳 bit1=电源 bit2=报警）。
 * 解析为最小 JSON 子集：容忍 json.dumps 默认分隔符（冒号前后空格）。
 *
 * LED0（心跳）不设独立任务：FreeRTOS 堆已被任务栈占满（148KB 临界），
 * 心跳在 get_switch 应答时按系统 tick 实时计算（1s 周期翻转），零堆开销。
 */
#include "dso_switch_cmd.h"
#include "dso_switch_panel.h"
#include "bsp.h"              /* stm32h7xx_hal + bsp_uart_fifo（提供 USART_TypeDef/__IO 前置） */
#include "debug_uart_lock.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

/* ---- 最小 JSON 子集辅助（供本模块与 dso_fmc_telemetry 共用） ---- */

/* 返回 "key" 出现的位置（含引号），找不到返回 NULL */
static const char *json_find_key(const char *line, const char *key)
{
    char pat[32];
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

    if (strcmp(cmd, "get_switch") == 0) {
        uint8_t led = DSO_Switch_GetLedBitmap();
        /* LED0 心跳按系统 tick 实时计算（1s 翻转），避免独立任务占用堆 */
        led = (uint8_t)((led & ~0x01u) |
                        ((uint8_t)((xTaskGetTickCount() / pdMS_TO_TICKS(1000)) & 1u)));
        snprintf(reply, reply_sz,
                 "{\"ver\":\"0.9\",\"type\":\"ack\",\"seq\":%d,"
                 "\"cmd\":\"get_switch\",\"result\":\"ok\",\"error\":\"\","
                 "\"data\":{\"sw\":%u,\"led\":%u}}",
                 seq, (unsigned)DSO_Switch_GetBitmap(),
                 (unsigned)led);
        return 1;
    }

    return 0;   /* 非本模块命令，交给其它处理器 */
}

/* ---- 串口命令接收（CH340/UART1，1152000） ---- */
#define SW_SERIAL_LINE_MAX 128
static char s_serial_line[SW_SERIAL_LINE_MAX];
static int  s_serial_line_len;

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
