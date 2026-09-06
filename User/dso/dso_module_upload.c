/**
 * @file dso_module_upload.c
 * @brief module_telemetry 各模块周期/事件上报（spec-00 信封，自持 TCP 客户端）。
 *
 * 本模块自带一条 TCP 客户端连接（服务器 IP/端口见 fmc16_conf.h，
 * 默认 192.168.100.100:5005），不依赖 dso_fmc_telemetry 的连接状态。
 *
 * 已接入：
 *  - sht40      1s 周期 data + sensor_lost 事件（读 dso_sht40 缓存）
 *  - ina226_u10 1s 周期 data + overcurrent/recovered 事件（读 dso_ina226 缓存）
 * 后续模块（pcal6524/ina226_u44/bgm_x/watchdog）在本任务扩展，模式相同。
 */
#include "dso_module_upload.h"
#include "dso_sht40.h"
#include "dso_ina226.h"
#include "dso_ina226_u44.h"
#include "dso_switch_panel.h"
#include "dso_switch_cmd.h"
#include "dso_freq_meter.h"
#include "dso_acq.h"
#include "dso_watchdog.h"
#include "bsp_pcal6524.h"
#include "fmc16_conf.h"
#include "lwip/sockets.h"
#include "lwip/netif.h"
#include "lwip/errno.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "bsp.h"
#include <string.h>
#include <stdio.h>

#define MUP_TASK_STACK    1024
#define MUP_RECONNECT_MS  500U
#define MUP_PERIOD_MS     1000U

static int s_fd = -1;
static SemaphoreHandle_t s_mtx;
static uint32_t s_seq = 0;

static void up_lock(void)
{
    if (s_mtx != NULL && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        (void)xSemaphoreTake(s_mtx, portMAX_DELAY);
    }
}

static void up_unlock(void)
{
    if (s_mtx != NULL && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        (void)xSemaphoreGive(s_mtx);
    }
}

static void up_disconnect(void)
{
    if (s_fd >= 0) {
        lwip_close(s_fd);
        s_fd = -1;
    }
}

static uint32_t s_fail_cnt = 0;

static void up_connect(void)
{
    struct sockaddr_in a;
    ip4_addr_t ip;

    up_disconnect();
    s_fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (s_fd < 0) {
        if ((++s_fail_cnt % 20u) == 1u) {
            printf("[MUPL] socket fail errno=%d\r\n", errno);
        }
        return;
    }
    IP4_ADDR(&ip, FMC_TEL_SERVER_IP0, FMC_TEL_SERVER_IP1,
             FMC_TEL_SERVER_IP2, FMC_TEL_SERVER_IP3);
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = ip4_addr_get_u32(&ip);
    a.sin_port = lwip_htons(FMC_TEL_SERVER_PORT);
    if (lwip_connect(s_fd, (struct sockaddr *)&a, sizeof(a)) < 0) {
        if ((++s_fail_cnt % 20u) == 1u) {
            printf("[MUPL] connect fail errno=%d\r\n", errno);
        }
        up_disconnect();
        return;
    }
    s_fail_cnt = 0u;
    printf("[MUPL] connected to server\r\n");
}

/* 发一行（自动补 CRLF；断连即断开重连由主循环负责） */
static void up_send_line(const char *line)
{
    int n, off = 0;

    if (s_fd < 0 || line == NULL) {
        return;
    }
    up_lock();
    n = (int)strlen(line);
    while (off < n) {
        int w = lwip_send(s_fd, line + off, (size_t)(n - off), 0);
        if (w <= 0) {
            up_disconnect();
            break;
        }
        off += w;
    }
    if (off > 0 && s_fd >= 0) {
        (void)lwip_send(s_fd, "\r\n", 2, 0);
    }
    up_unlock();
}

/* YDS1-5 逻辑点亮掩码（声光联动分级映射，颜色按用户定义）：
 *   YDS1 红  = 电流告警闪烁（等级1/2/3 → 慢闪500/快闪250/爆闪100ms 半周期）
 *   YDS2 绿  = 电流正常常亮（两路电流均 level=0）
 *   YDS3 蓝  = 温度正常常亮（SHT40 level=0）
 *   YDS4 橙  = 温度告警闪烁（同三频分级）
 *   YDS5     = USB 活动（保留）
 */
static uint8_t mup_yds_state(void)
{
    uint32_t t = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint8_t cur_lv = DSO_Ina226_GetLevel();
    uint8_t u44_lv = DSO_Ina226U44_GetLevel();
    uint8_t tmp_lv = DSO_Sht40_GetLevel();
    uint8_t m = 0u;
    uint32_t half;

    if (u44_lv > cur_lv) {
        cur_lv = u44_lv;
    }
    /* YDS1 红：电流告警闪烁 */
    if (cur_lv > 0u) {
        half = (cur_lv == 1u) ? 500u : (cur_lv == 2u) ? 250u : 100u;
        if (((t / half) & 1u) == 0u) {
            m |= 0x01u;
        }
    }
    /* YDS2 绿：电流正常 */
    if (cur_lv == 0u) {
        m |= 0x02u;
    }
    /* YDS3 蓝：温度正常 */
    if (tmp_lv == 0u) {
        m |= 0x04u;
    }
    /* YDS4 橙：温度告警闪烁 */
    if (tmp_lv > 0u) {
        half = (tmp_lv == 1u) ? 500u : (tmp_lv == 2u) ? 250u : 100u;
        if (((t / half) & 1u) == 0u) {
            m |= 0x08u;
        }
    }
    /* YDS5：USB 活动 */
    if (DSO_Switch_UsbActive()) {
        m |= 0x10u;
    }
    return m;
}

/* spec-00 信封组装并发一行（type: module.data / module.event） */
static void mup_send(const char *module, const char *type, const char *data_json)
{
    char buf[512];
    int n;

    n = snprintf(buf, sizeof(buf),
                 "{\"ver\":\"1.0\",\"type\":\"%s\",\"module\":\"%s\","
                 "\"seq\":%lu,\"ts_ms\":%lu,\"data\":%s}",
                 type, module, (unsigned long)(++s_seq),
                 (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS),
                 data_json);
    if (n > 0 && n < (int)sizeof(buf)) {
        up_send_line(buf);
    }
}

/* ---- 主循环：连接维护 + 每 1s 周期上报 + 边沿事件 ---- */

static void mup_task(void *pv)
{
    uint8_t prev_sht_ok = 1u;
    uint8_t prev_over = 0u;
    uint8_t last_state = 0u;   /* 0=未连接 1=已连接 */
    (void)pv;

    printf("[MUPL] task start\r\n");
    for (;;) {
        uint8_t state;
        /* 等 LwIP netif 出现即可（MX_LWIP_Init 后）；link-up 标志本固件未必置位，
         * 故不依赖它——connect 失败会每 500ms 重试直到服务器就绪 */
        if (netif_default == NULL) {
            vTaskDelay(pdMS_TO_TICKS(MUP_RECONNECT_MS));
            continue;
        }
        state = (s_fd >= 0) ? 1u : 0u;
        if (state != last_state) {
            if (state == 1u) {
                printf("[MUPL] link up\r\n");
            }
            last_state = state;
        }
        if (s_fd < 0) {
            up_connect();
            vTaskDelay(pdMS_TO_TICKS(MUP_RECONNECT_MS));
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(MUP_PERIOD_MS));
        if (s_fd < 0) {
            continue;
        }

        /* 模块 3：sht40 */
        {
            char js[160];
            uint8_t ok = DSO_Sht40_IsValid();
            snprintf(js, sizeof(js),
                     "{\"temp_dezi\":%ld,\"temp_ok\":%u,\"hi_alarm\":%u,"
                     "\"level\":%u}",
                     (long)DSO_Sht40_GetTempDezi(),
                     (unsigned)ok,
                     (unsigned)DSO_Sht40_HiAlarm(),
                     (unsigned)DSO_Sht40_GetLevel());
            mup_send("sht40", "module.data", js);
            if (prev_sht_ok && !ok) {
                mup_send("sht40", "module.event",
                         "{\"event\":\"sensor_lost\",\"level\":\"warning\","
                         "\"detail\":\"10 consecutive read fails\"}");
            }
            prev_sht_ok = ok;
        }

        /* 模块 4：ina226_u10 */
        {
            char js[128];
            uint8_t ok = DSO_Ina226_PowerOk();
            uint8_t over = (ok == 0u) ? 1u : 0u;
            snprintf(js, sizeof(js),
                     "{\"cur_ma\":%ld,\"over_cur\":%u,\"valid\":%u,"
                     "\"level\":%u}",
                     (long)DSO_Ina226_GetCurMa(), (unsigned)over,
                     (unsigned)1u,    /* TODO: 有效标志 API 就绪后取真实值 */
                     (unsigned)DSO_Ina226_GetLevel());
            mup_send("ina226_u10", "module.data", js);
            if (!prev_over && over) {
                mup_send("ina226_u10", "module.event",
                         "{\"event\":\"overcurrent\",\"level\":\"warning\","
                         "\"detail\":\"shunt over limit\"}");
            } else if (prev_over && !over) {
                mup_send("ina226_u10", "module.event",
                         "{\"event\":\"recovered\",\"level\":\"info\","
                         "\"detail\":\"current back to normal\"}");
            }
            prev_over = over;
        }

        /* 模块 5：ina226_u44（核心板 U44，0x40/20mΩ） */
        {
            char js[128];
            uint8_t ok44 = DSO_Ina226U44_PowerOk();
            uint8_t over44 = (ok44 == 0u) ? 1u : 0u;
            snprintf(js, sizeof(js),
                     "{\"cur_ma\":%ld,\"over_cur\":%u,\"valid\":%u,"
                     "\"level\":%u}",
                     (long)DSO_Ina226U44_GetCurMa(), (unsigned)over44,
                     (unsigned)1u,    /* TODO: 有效标志 API 就绪后取真实值 */
                     (unsigned)DSO_Ina226U44_GetLevel());
            mup_send("ina226_u44", "module.data", js);
        }

        /* 模块 6：bgm_adc_dac（频率计快照 + 采样运行态） */
        {
            char js[320];
            DSO_FreqMeterSnapshot fs;
            if (DSO_FreqMeter_GetSnapshot(&fs)) {
                snprintf(js, sizeof(js),
                         "{\"freq_valid\":%u,\"freq_state\":%u,"
                         "\"freq_millihz\":%lu,\"gate_us\":%lu,"
                         "\"edge_count\":%lu,\"ref_ticks\":%lu,"
                         "\"overflow_count\":%lu,\"last_edge_age_ms\":%lu,"
                         "\"overrange\":%u,\"sample_format\":%u,"
                         "\"sample_rate\":%lu,\"overrun_count\":%lu}",
                         (unsigned)(fs.valid ? 1u : 0u), (unsigned)fs.state,
                         (unsigned long)fs.freq_millihz,
                         (unsigned long)fs.gate_us,
                         (unsigned long)fs.edge_count,
                         (unsigned long)fs.ref_ticks,
                         (unsigned long)fs.overflow_count,
                         (unsigned long)fs.last_edge_age_ms,
                         (unsigned)(fs.overrange ? 1u : 0u),
                         (unsigned)DSO_Acq_GetSampleFormat(),
                         (unsigned long)DSO_Acq_GetSampleRate(),
                         (unsigned long)DSO_Acq_GetOverrunCount());
            } else {
                snprintf(js, sizeof(js),
                         "{\"freq_valid\":0,\"freq_state\":0,\"freq_millihz\":0,"
                         "\"gate_us\":0,\"edge_count\":0,\"ref_ticks\":0,"
                         "\"overflow_count\":0,\"last_edge_age_ms\":0,"
                         "\"overrange\":0,\"sample_format\":%u,"
                         "\"sample_rate\":%lu,\"overrun_count\":%lu}",
                         (unsigned)DSO_Acq_GetSampleFormat(),
                         (unsigned long)DSO_Acq_GetSampleRate(),
                         (unsigned long)DSO_Acq_GetOverrunCount());
            }
            mup_send("bgm_adc_dac", "module.data", js);
        }

        /* 模块 8：watchdog（SP706T 喂狗/复位原因） */
        {
            char js[192];
            snprintf(js, sizeof(js),
                     "{\"feed_en\":%u,\"kick_count\":%lu,\"pfo\":%u,"
                     "\"reset_cause\":%u,\"uptime_ms\":%lu}",
                     (unsigned)DSO_WDOG_FEED_PB9,
                     (unsigned long)DSO_Watchdog_GetKickCount(),
                     (unsigned)DSO_Watchdog_Pfo(),
                     (unsigned)DSO_Watchdog_ResetCause(),
                     (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS));
            mup_send("watchdog", "module.data", js);
        }

        /* 模块 2：pcal6524（13 开关位图 + YDS1-5 逻辑点亮状态） */
        {
            char js[192];
            uint16_t sw = DSO_Switch_GetBitmap();
            uint8_t yds = mup_yds_state();
            snprintf(js, sizeof(js),
                     "{\"sw\":%u,\"yds\":%u,\"eth_link\":%u,\"usb_active\":%u}",
                     (unsigned)sw, (unsigned)yds,
                     (unsigned)((yds & 0x08u) ? 1u : 0u),
                     (unsigned)((yds & 0x10u) ? 1u : 0u));
            mup_send("pcal6524", "module.data", js);
        }
    }
}

/* YDS 硬件点灯任务（100ms 刷新：爆闪 100ms 半周期需 ≥100ms 粒度） */
static void mup_yds_task(void *pv)
{
    (void)pv;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(100u));
        (void)BSP_PCAL6524_WriteP2Leds(mup_yds_state());
    }
}

/* 蜂鸣器节奏引擎（100ms 粒度，驱动 U10 ALERT 通断）：
 *   等级 = max(U10 电流, U44 电流, SHT40 温度) —— 温度报警与电流报警
 *   同级联动、不同级取更严重者（温度报警也走同一蜂鸣硬件）。
 *   level0/静音: 停（限值=0x7FFF 抑制）
 *   level1 预警: 响 250ms → 停 1750ms（2s 周期）
 *   level2 告警: 响 250ms → 停 250ms（500ms 周期，与快闪同步）
 *   level3 紧急: 响 100ms → 停 100ms（200ms 周期，与爆闪同步）
 * 响窗口把 U10 ALERT_LIMIT 压到 200 LSB 强制触发；停窗口置 0x7FFF 并清锁存。
 */
#define BUZ_TRIG_LSB   200u
#define BUZ_SUPPRESS   0x7FFFu

static void mup_buz_task(void *pv)
{
    (void)pv;
    for (;;) {
        uint32_t t = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint8_t lv = DSO_Ina226_GetLevel();
        uint8_t lv44 = DSO_Ina226U44_GetLevel();
        uint8_t lvT = DSO_Sht40_GetLevel();
        uint8_t on = 0u;
        if (lv44 > lv) {
            lv = lv44;
        }
        if (lvT > lv) {
            lv = lvT;
        }
        if (!DSO_Switch_BuzzerMute()) {
            if (lv == 1u) {
                on = ((t % 2000u) < 250u) ? 1u : 0u;
            } else if (lv == 2u) {
                on = ((t % 500u) < 250u) ? 1u : 0u;
            } else if (lv == 3u) {
                on = ((t % 200u) < 100u) ? 1u : 0u;
            }
        }
        if (on) {
            DSO_Ina226_SetAlertLimit(BUZ_TRIG_LSB);
        } else {
            DSO_Ina226_SetAlertLimit(BUZ_SUPPRESS);
            DSO_Ina226_ClearAlertLatch();
        }
        vTaskDelay(pdMS_TO_TICKS(100u));
    }
}

void DSO_ModuleUpload_Init(void)
{
    if (s_mtx == NULL) {
        s_mtx = xSemaphoreCreateMutex();
    }
    if (xTaskCreate(mup_task, "MUpTcp", MUP_TASK_STACK, NULL, 3, NULL) != pdPASS) {
        printf("[BOOT] FAIL MUpTcp\r\n");
    }
    if (xTaskCreate(mup_yds_task, "MUpYds", 512, NULL, 3, NULL) != pdPASS) {
        printf("[BOOT] FAIL MUpYds\r\n");
    }
    if (xTaskCreate(mup_buz_task, "MUpBuz", 512, NULL, 3, NULL) != pdPASS) {
        printf("[BOOT] FAIL MUpBuz\r\n");
    }
}
