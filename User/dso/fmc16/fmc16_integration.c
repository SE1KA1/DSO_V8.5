/**
 * @file fmc16_integration.c
 * @brief FMC16 系统集成实现: 初始化、服务任务、轮询分发。
 */
#include "fmc16_integration.h"
#include "fmc16_bus.h"
#include "fmc16_crc.h"
#include "fmc16_packet.h"
#include "fmc16_event.h"
#include "fmc16_io.h"
#include "fmc16_conf.h"
#include "../dso_fmc_telemetry.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

/* ---- 全局实例 ---- */
Fmc16System g_fmc16;

/* ---- 初始化 ---- */

int fmc16_system_init(void)
{
    memset(&g_fmc16, 0, sizeof(g_fmc16));

    /* 1. 总线初始化 */
    fmc16_bus_init();

    /* 2. CRC 自检 */
    if (!fmc16_crc_selftest())
    {
        g_fmc16.sys_state = FMC16_STATE_ERROR;
        return 0;
    }

    /* 3. 链路初始化 */
    fmc16_link_init(&g_fmc16.link, FMC16_HELLO_TIMEOUT_MS);

    /* 4. RX 状态机初始化 */
    fmc16_packet_rx_init(&g_fmc16.rx);

    g_fmc16.sys_state = FMC16_STATE_BUS_READY;
    FMC16_LOG("system init OK, waiting for XO2 LINK_HELLO...");
    return 1;
}

/* ---- 内部: 分发接收到的包 ---- */

static void dispatch_packet(Fmc16System *sys)
{
    const Fmc16DecodedHeader *hdr = fmc16_packet_rx_header(&sys->rx);
    const uint16_t *payload = fmc16_packet_rx_payload(&sys->rx);

    if (!hdr) return;

    sys->packets_rx++;

    /* 1. 链路层处理 */
    int link_changed = fmc16_link_process_rx(&sys->link, hdr, payload);
    if (link_changed && sys->link.state == FMC16_LINK_UP)
    {
        sys->sys_state = FMC16_STATE_LINK_UP;
        FMC16_LOG("*** Link UP ***");

        /* 发送 DEVICE_INFO */
        uint16_t n = fmc16_link_send_device_info(&sys->link, sys->tx_buf);
        fmc16_bus_try_lock(FMC16_BUS_LOCK_TIMEOUT_MS);
        fmc16_bus_write_buffer(sys->tx_buf, n);
        fmc16_bus_unlock();
        sys->packets_tx++;
    }

    /* 2. 命令响应处理 */
    fmc16_command_process_rx(sys->cmds, FMC16_MAX_PENDING_CMDS, hdr, payload);

    /* 3. 快照处理 */
    switch (hdr->msg_type)
    {
    case FMC16_MSG_STATUS_SNAPSHOT:
        sys->status_valid = fmc16_status_parse_snapshot(payload, &sys->status_snap);
        break;
    case FMC16_MSG_MODE_SNAPSHOT:
        sys->mode_valid = fmc16_status_parse_mode(payload, &sys->mode_snap);
        break;
    case FMC16_MSG_IO_SNAPSHOT:
        sys->io_valid = fmc16_status_parse_io(payload, &sys->io_snap);
        break;
    case FMC16_MSG_EVENT:
        {
            Fmc16Event evt;
            if (fmc16_event_parse(payload, hdr->payload_words, &evt))
            {
                FMC16_LOG("EVENT: code=0x%04X src=%u ts=%lu",
                          evt.code, evt.source,
                          (unsigned long)evt.timestamp_ms);
                /* 遥测：即时推送事件帧到服务器 */
                DSO_FmcTelemetry_OnEvent(evt.code, evt.timestamp_ms,
                                         evt.data, evt.data_words);
            }
        }
        break;
    default:
        break;
    }

    fmc16_packet_rx_consume(&sys->rx);
}

/* ---- FreeRTOS 任务 ---- */

void FMC16_Service_Task(void *arg)
{
    (void)arg;
    uint32_t last_host_status_tick = 0;

    /* 本地副本，避免频繁访问全局 */
    uint16_t tx_buf[FMC16_MAX_PACKET_WORDS];

    for (;;)
    {
        uint32_t now = HAL_GetTick();

        /* ---- 等待 HELLO ---- */
        if (g_fmc16.sys_state == FMC16_STATE_BUS_READY)
        {
            uint32_t elapsed = now - g_fmc16.link.state_enter_tick;
            if (elapsed > g_fmc16.link.hello_timeout_ms)
            {
                FMC16_WARN("LINK_HELLO timeout (%lums), re-init RX",
                           (unsigned long)elapsed);
                fmc16_packet_rx_init(&g_fmc16.rx);
                g_fmc16.link.state_enter_tick = now;
            }
        }

        /* ---- 轮询 FMC 总线 ---- */
        if (fmc16_bus_try_lock(1))  /* 1ms 超时 */
        {
            uint16_t word = fmc16_bus_read();
            fmc16_bus_unlock();

            if (word != FMC16_IDLE_WORD)
            {
                /* XO2 驱动了数据线 */
                if (fmc16_packet_rx_feed(&g_fmc16.rx, word))
                {
                    /* 完整合法包 */
                    dispatch_packet(&g_fmc16);
                }
            }
        }

        /* ---- 周期 HOST_STATUS (每 500ms) ---- */
        if (g_fmc16.link.state == FMC16_LINK_UP &&
            (now - last_host_status_tick) >= FMC16_LINK_STATUS_PERIOD_MS)
        {
            last_host_status_tick = now;
            uint16_t n = fmc16_link_send_host_status(&g_fmc16.link, tx_buf);
            if (fmc16_bus_try_lock(FMC16_BUS_LOCK_TIMEOUT_MS))
            {
                fmc16_bus_write_buffer(tx_buf, n);
                fmc16_bus_unlock();
                g_fmc16.packets_tx++;
            }
        }

        /* ---- 命令超时检查 ---- */
        uint16_t retry_n = fmc16_command_check_timeouts(
            g_fmc16.cmds, FMC16_MAX_PENDING_CMDS,
            tx_buf, g_fmc16.link.tx_sequence);
        if (retry_n > 0 && fmc16_bus_try_lock(FMC16_BUS_LOCK_TIMEOUT_MS))
        {
            fmc16_bus_write_buffer(tx_buf, retry_n);
            fmc16_bus_unlock();
            g_fmc16.link.tx_sequence++;
            g_fmc16.packets_tx++;
        }

        vTaskDelay(pdMS_TO_TICKS(1));  /* 1ms 轮询周期 */
    }
}

/* ---- 非阻塞轮询 ---- */

int fmc16_poll_once(void)
{
    if (g_fmc16.sys_state < FMC16_STATE_BUS_READY) return 0;

    if (!fmc16_bus_try_lock(1)) return 0;

    uint16_t word = fmc16_bus_read();
    fmc16_bus_unlock();

    if (word == FMC16_IDLE_WORD) return 0;

    if (fmc16_packet_rx_feed(&g_fmc16.rx, word))
    {
        dispatch_packet(&g_fmc16);
        return 1;
    }
    return 0;
}

/* ---- 快照访问 ---- */

const Fmc16StatusSnapshot *fmc16_get_status_snapshot(void)
{
    return g_fmc16.status_valid ? &g_fmc16.status_snap : NULL;
}
const Fmc16ModeSnapshot *fmc16_get_mode_snapshot(void)
{
    return g_fmc16.mode_valid ? &g_fmc16.mode_snap : NULL;
}
const Fmc16IOSnapshot *fmc16_get_io_snapshot(void)
{
    return g_fmc16.io_valid ? &g_fmc16.io_snap : NULL;
}
