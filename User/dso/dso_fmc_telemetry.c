/**
 * @file dso_fmc_telemetry.c
 * @brief STM32 与服务器设备协议 v1.0 客户端（对齐 Markdown/STM32 与服务器设备协议.md）。
 *
 * - STM32 为 TCP 客户端主动连服务器（IP/端口见 fmc16_conf.h），单连接双向。
 * - 上行：device.status（1s 心跳）、telemetry.io（周期+变化即时）、telemetry.mode、
 *         event.key（FMC16 事件）、command.result。
 * - 下行：command.execute（set_sw / get_status / 其它 → NOT_SUPPORTED），经
 *         DSO_Switch_Cmd_JsonGetStr/Int 解析，DSO_Switch_Set 执行。
 */
#include "dso_fmc_telemetry.h"
#include "dso_switch_cmd.h"
#include "dso_switch_panel.h"
#include "fmc16_integration.h"
#include "fmc16_link.h"
#include "fmc16_status.h"
#include "fmc16_conf.h"
#include "lwip/sockets.h"
#include "lwip/netif.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "bsp.h"
#include <string.h>
#include <stdio.h>

#define FMC_TEL_TASK_STACK   1024
#define FMC_TEL_RECONNECT_MS 5000U
#define FMC_TEL_STATUS_MS    1000U
#define FMC_TEL_DEVICE_ID    "board-001"
#define FMC_TEL_BUILD_ID     "v0.9.1"

static int s_fd = -1;
static uint32_t s_seq = 0;
static uint32_t s_boot_id = 1;
static SemaphoreHandle_t s_mtx;

static void tel_lock(void)
{
    if (s_mtx != NULL && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        (void)xSemaphoreTake(s_mtx, portMAX_DELAY);
    }
}

static void tel_unlock(void)
{
    if (s_mtx != NULL && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        (void)xSemaphoreGive(s_mtx);
    }
}

static void tel_disconnect(void)
{
    if (s_fd >= 0) {
        lwip_close(s_fd);
        s_fd = -1;
    }
}

static int tel_connect(void)
{
    struct sockaddr_in s;
    ip4_addr_t ip;

    tel_disconnect();
    s_fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (s_fd < 0) {
        return -1;
    }
    IP4_ADDR(&ip, FMC_TEL_SERVER_IP0, FMC_TEL_SERVER_IP1,
             FMC_TEL_SERVER_IP2, FMC_TEL_SERVER_IP3);
    memset(&s, 0, sizeof(s));
    s.sin_family = AF_INET;
    s.sin_addr.s_addr = ip4_addr_get_u32(&ip);
    s.sin_port = lwip_htons(FMC_TEL_SERVER_PORT);
    if (lwip_connect(s_fd, (struct sockaddr *)&s, sizeof(s)) < 0) {
        tel_disconnect();
        return -1;
    }
    return 0;
}

/* 发送一行 JSON（带 CRLF），失败则断开（下次 tick 重连） */
static void tel_send_line(const char *line)
{
    size_t n;

    if (s_fd < 0 || line == NULL) return;
    n = strlen(line);
    if (lwip_send(s_fd, line, n, 0) < 0) {
        tel_disconnect();
        return;
    }
    (void)lwip_send(s_fd, "\r\n", 2, 0);
}

int DSO_FmcTelemetry_SendRaw(const char *line)
{
    if (line == NULL) {
        return -1;
    }
    tel_lock();
    if (s_fd < 0) {
        tel_unlock();
        return -1;
    }
    tel_send_line(line);
    tel_unlock();
    return 0;
}

/* 通用信封帧发送 */
static void tel_send_frame(const char *mt, const char *cmd_id, const char *payload)
{
    char buf[700];

    if (cmd_id != NULL && cmd_id[0] != '\0') {
        snprintf(buf, sizeof(buf),
                 "{\"protocolVersion\":\"1.0\",\"messageType\":\"%s\","
                 "\"deviceId\":\"%s\",\"sessionId\":\"\",\"bootId\":%lu,\"seq\":%lu,"
                 "\"timestampMs\":%lu,\"commandId\":\"%s\",\"payload\":%s}",
                 mt, FMC_TEL_DEVICE_ID, (unsigned long)s_boot_id,
                 (unsigned long)++s_seq, (unsigned long)HAL_GetTick(), cmd_id, payload);
    } else {
        snprintf(buf, sizeof(buf),
                 "{\"protocolVersion\":\"1.0\",\"messageType\":\"%s\","
                 "\"deviceId\":\"%s\",\"sessionId\":\"\",\"bootId\":%lu,\"seq\":%lu,"
                 "\"timestampMs\":%lu,\"payload\":%s}",
                 mt, FMC_TEL_DEVICE_ID, (unsigned long)s_boot_id,
                 (unsigned long)++s_seq, (unsigned long)HAL_GetTick(), payload);
    }
    tel_send_line(buf);
}

/* ---- 上行帧 ---- */

static void tel_send_device_status(void)
{
    char payload[200];

    snprintf(payload, sizeof(payload),
             "{\"online\":true,\"buildId\":\"%s\",\"fmcReady\":%s,\"bootId\":%lu,"
             "\"uptimeMs\":%lu,\"lastError\":\"\"}",
             FMC_TEL_BUILD_ID,
             (g_fmc16.link.state == FMC16_LINK_UP) ? "true" : "false",
             (unsigned long)s_boot_id, (unsigned long)HAL_GetTick());
    tel_send_frame("device.status", NULL, payload);
}

static void tel_send_snapshots(void)
{
    const Fmc16IOSnapshot  *io   = fmc16_get_io_snapshot();
    const Fmc16ModeSnapshot *mode = fmc16_get_mode_snapshot();
    char payload[300];

    if (io != NULL) {
        snprintf(payload, sizeof(payload),
                 "{\"pi\":%u,\"po\":%lu,\"pio\":%u,\"piGeneration\":%lu,"
                 "\"poGeneration\":%lu,\"pioGeneration\":%lu,\"validity\":%u,"
                 "\"timestampMs\":%lu}",
                 (unsigned)io->pi_applied, (unsigned long)io->po_sample,
                 (unsigned)io->pio_sample, (unsigned long)io->pi_generation,
                 (unsigned long)io->po_generation, (unsigned long)io->pio_generation,
                 (unsigned)io->validity, (unsigned long)io->sample_timestamp_ms);
        tel_send_frame("telemetry.io", NULL, payload);
    }

    if (mode != NULL) {
        snprintf(payload, sizeof(payload),
                 "{\"mode\":%u,\"experimentId\":%u,\"clockProfile\":\"%s\","
                 "\"hz\":%lu,\"keyState\":%u,\"generation\":%lu}",
                 (unsigned)mode->mode, (unsigned)mode->experiment_id,
                 (mode->clk_sel != 0) ? "PLL_50M" : "PLL_25M",
                 (unsigned long)((mode->clk_sel != 0) ? 50000000UL : 25000000UL),
                 (unsigned)mode->key_state, (unsigned long)mode->mode_generation);
        tel_send_frame("telemetry.mode", NULL, payload);
    }
}

/* ---- 下行命令处理 ---- */

static void tel_send_result(const char *cmd_id, const char *status,
                            const char *detail, uint32_t gen, uint32_t txn)
{
    char payload[220];

    snprintf(payload, sizeof(payload),
             "{\"status\":\"%s\",\"detail\":\"%s\",\"appliedGeneration\":%lu,"
             "\"fmcTransactionId\":%lu}",
             status, detail, (unsigned long)gen, (unsigned long)txn);
    tel_send_frame("command.result", cmd_id, payload);
}

static void tel_handle_command(const char *line)
{
    char cmd_id[64];
    char cmd_type[32];
    const char *status = "NOT_SUPPORTED";
    const char *detail = "";
    int sw = 0;
    int on = 0;

    cmd_id[0] = '\0';
    (void)DSO_Switch_Cmd_JsonGetStr(line, "commandId", cmd_id, sizeof(cmd_id));

    if (!DSO_Switch_Cmd_JsonGetStr(line, "type", cmd_type, sizeof(cmd_type))) {
        tel_send_result(cmd_id, "BAD_REQUEST", "missing type", 0, 0);
        return;
    }

    if (strcmp(cmd_type, "set_sw") == 0) {
        if (DSO_Switch_Cmd_JsonGetInt(line, "sw", &sw) &&
            DSO_Switch_Cmd_JsonGetInt(line, "on", &on) &&
            sw >= 0 && sw < SW_TOTAL && (on == 0 || on == 1)) {
            DSO_Switch_Set(sw, on);
            status = "OK";
        } else {
            status = "BAD_REQUEST";
            detail = "bad sw/on";
        }
    } else if (strcmp(cmd_type, "get_status") == 0) {
        status = "OK";
        detail = "see telemetry";
    } else {
        detail = cmd_type;
    }

    tel_send_result(cmd_id, status, detail, 0, 0);
}

/* ---- 推送任务 ---- */

static void vFmcTelTask(void *pvParameters)
{
    char line_buf[256];
    int  line_len = 0;
    uint32_t last_status = 0;
    uint32_t last_io = 0;
    (void)pvParameters;

    /* 等待 LwIP netif up（ETH 任务 3s 后 init） */
    while (netif_default == NULL || !netif_is_link_up(netif_default)) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    for (;;) {
        if (s_fd < 0) {
            if (tel_connect() == 0) {
                printf("[FTEL] connected to server\r\n");
                last_status = 0;
                last_io = 0;
            } else {
                vTaskDelay(pdMS_TO_TICKS(FMC_TEL_RECONNECT_MS));
                continue;
            }
        }

        /* 读服务器下行命令（非阻塞） */
        if (s_fd >= 0) {
            char rbuf[256];
            int n;
            while ((n = lwip_recv(s_fd, rbuf, (u16_t)sizeof(rbuf) - 1, MSG_DONTWAIT)) > 0) {
                int i;
                for (i = 0; i < n; i++) {
                    char c = rbuf[i];
                    if (c == '\n') {
                        line_buf[line_len] = '\0';
                        if (line_len > 0) {
                            tel_lock();
                            tel_handle_command(line_buf);
                            tel_unlock();
                        }
                        line_len = 0;
                    } else if (c != '\r' && line_len < (int)sizeof(line_buf) - 1) {
                        line_buf[line_len++] = c;
                    }
                }
            }
            if (n < 0) {
                tel_disconnect();
            }
        }

        {
            uint32_t now = HAL_GetTick();
            tel_lock();
            if (s_fd >= 0 && (now - last_status) >= FMC_TEL_STATUS_MS) {
                last_status = now;
                tel_send_device_status();
            }
            if (s_fd >= 0 && (now - last_io) >= FMC_TEL_PERIOD_MS) {
                last_io = now;
                tel_send_snapshots();
            }
            tel_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* ---- 公共 API ---- */

void DSO_FmcTelemetry_Init(void)
{
    if (s_mtx == NULL) {
        s_mtx = xSemaphoreCreateMutex();
    }
    if (xTaskCreate(vFmcTelTask, "FMC_Tel", FMC_TEL_TASK_STACK, NULL, 2, NULL)
        != pdPASS) {
        /* 遥测推送任务创建失败不致命：FMC16 链路本身仍工作 */
    }
}

void DSO_FmcTelemetry_OnEvent(uint16_t code, uint32_t ts,
                              const uint16_t *data, uint16_t n)
{
    char payload[220];
    int pos;
    uint16_t i;

    tel_lock();
    if (s_fd < 0) {
        tel_unlock();
        return;
    }
    pos = snprintf(payload, sizeof(payload),
                   "{\"keyState\":0,\"changedMask\":0x%04X,\"pressMask\":0,"
                   "\"source\":\"fmc\",\"generation\":%lu,\"eventTs\":%lu,\"data\":[",
                   (unsigned)code, (unsigned long)ts, (unsigned long)ts);
    for (i = 0; i < n && i < 8 && pos < (int)sizeof(payload) - 16; i++) {
        pos += snprintf(payload + pos, sizeof(payload) - (size_t)pos, "%s%u",
                        (i ? "," : ""), (unsigned)data[i]);
    }
    snprintf(payload + pos, sizeof(payload) - (size_t)pos, "]}");
    tel_send_frame("event.key", NULL, payload);
    tel_unlock();
}
