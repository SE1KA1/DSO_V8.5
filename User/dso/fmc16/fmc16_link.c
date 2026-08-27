/**
 * @file fmc16_link.c
 * @brief FMC16 链路层实现。
 */
#include "fmc16_link.h"
#include "fmc16_conf.h"
#include <string.h>
#include <stdio.h>

static uint32_t tick_now(void) { return HAL_GetTick(); }

void fmc16_link_init(Fmc16LinkCtx *ctx, uint32_t hello_timeout_ms)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->state = FMC16_LINK_WAIT_HELLO;
    ctx->hello_timeout_ms = hello_timeout_ms;
    ctx->state_enter_tick = tick_now();
}

int fmc16_link_process_rx(Fmc16LinkCtx *ctx,
                          const Fmc16DecodedHeader *hdr,
                          const uint16_t *payload)
{
    if (!hdr || !ctx) return 0;

    /* 只处理发给 STM32 的包 */
    if (hdr->destination != FMC16_NODE_STM32 &&
        hdr->destination != FMC16_NODE_BROADCAST)
        return 0;

    /* 更新序列号追踪 */
    if (hdr->sequence > ctx->last_rx_seq)
        ctx->last_rx_seq = hdr->sequence;

    switch (hdr->msg_type)
    {
    /* ---- LINK_HELLO (XO2→STM32) ---- */
    case FMC16_MSG_LINK_HELLO:
        if (hdr->payload_words >= 11)
        {
            ctx->peer.device_type       = payload[0];
            ctx->peer.capabilities      = FMC16_U32(payload[4], payload[5]);
            ctx->peer.max_payload_words = payload[6];
            ctx->peer.max_outstanding   = payload[7];
            ctx->peer.rx_queue_words    = payload[8];
            ctx->peer.tx_queue_words    = payload[9];
            ctx->peer.reset_reason      = (uint8_t)(payload[10] & 0xFFu);
            ctx->peer.build_id          = FMC16_U32(payload[2], payload[3]);

            FMC16_LOG("LINK_HELLO: dev=0x%04X proto=%d.%d caps=0x%08lX "
                      "rxQ=%u txQ=%u outstanding=%u",
                      ctx->peer.device_type,
                      (payload[1] >> 8) & 0xFF, payload[1] & 0xFF,
                      (unsigned long)ctx->peer.capabilities,
                      ctx->peer.rx_queue_words, ctx->peer.tx_queue_words,
                      ctx->peer.max_outstanding);

            if (ctx->state == FMC16_LINK_WAIT_HELLO ||
                ctx->state == FMC16_LINK_DOWN)
            {
                ctx->state = FMC16_LINK_HANDSHAKING;
                ctx->state_enter_tick = tick_now();
                return 1;  /* 状态变化 */
            }
        }
        break;

    /* ---- RESET_NOTICE (XO2→STM32) ---- */
    case FMC16_MSG_RESET_NOTICE:
        FMC16_WARN("RESET_NOTICE: reason=%u", payload[1]);
        /* 作废未完成事务、PI/PIO 镜像、旧 credit */
        ctx->status.link_ready = 0;
        ctx->state = FMC16_LINK_WAIT_HELLO;
        ctx->state_enter_tick = tick_now();
        return 1;

    /* ---- LINK_STATUS (XO2→STM32) ---- */
    case FMC16_MSG_LINK_STATUS:
        if (hdr->payload_words >= 8)
        {
            uint16_t flags = payload[1];
            ctx->status.rx_free_words  = payload[2];
            ctx->status.tx_used_words  = payload[3];
            ctx->status.rx_free_slots  = payload[4];
            ctx->status.tx_pending     = payload[5];
            ctx->status.link_ready     = (flags & FMC16_LINK_READY) != 0;
            ctx->status.rx_accepting   = (flags & FMC16_LINK_RX_ACCEPTING) != 0;
            ctx->status.tx_available   = (flags & FMC16_LINK_TX_AVAILABLE) != 0;
            ctx->status.resync_needed  = (flags & FMC16_LINK_RESYNC_REQUIRED) != 0;
            ctx->status.uptime_ms      = FMC16_U32(payload[6], payload[7]);

            if (ctx->status.resync_needed)
            {
                FMC16_WARN("XO2 requesting resync");
                ctx->state = FMC16_LINK_RESYNC;
            }
        }
        break;

    /* ---- DEVICE_INFO (双向) ---- */
    case FMC16_MSG_DEVICE_INFO:
        if (hdr->source == FMC16_NODE_XO2 && hdr->payload_words >= 16)
        {
            if (ctx->state == FMC16_LINK_HANDSHAKING)
            {
                ctx->state = FMC16_LINK_UP;
                ctx->state_enter_tick = tick_now();
                FMC16_LOG("Link UP (XO2 DEVICE_INFO received)");
                return 1;
            }
        }
        break;

    default:
        break;
    }

    return 0;
}

uint16_t fmc16_link_send_device_info(Fmc16LinkCtx *ctx, uint16_t *tx_buf)
{
    (void)ctx;
    uint16_t payload[16];
    memset(payload, 0, sizeof(payload));

    payload[0]  = 0x0100;  /* device_info_schema */
    payload[1]  = FMC16_DEVICE_TYPE_STM32;  /* "H7" */
    payload[2]  = FMC16_PROTOCOL_VERSION;
    payload[3]  = FMC16_SRC_DST(FMC16_ROLE_MASTER, FMC16_PERSONALITY_FMC16);

    /* 能力位: async read+write, credit flow, device info */
    uint32_t caps = FMC16_CAP_ASYNC_READ | FMC16_CAP_ASYNC_WRITE |
                    FMC16_CAP_CREDIT_FLOW;
    payload[4]  = FMC16_U32_HI(0);       /* build_id = 0 (stub) */
    payload[5]  = FMC16_U32_LO(0);
    payload[6]  = 0x0100;  /* mode_schema */
    payload[7]  = 0x0100;  /* io_schema */
    payload[8]  = 0x0100;  /* experiment_schema */
    payload[9]  = 0x0000;  /* board_revision: unknown */
    payload[10] = FMC16_U32_HI(caps);
    payload[11] = FMC16_U32_LO(caps);
    payload[12] = 0x0000;  /* experiment class mask hi */
    payload[13] = 0x0000;  /* experiment class mask lo */
    payload[14] = 0x000F;  /* public feature flags: status+IO+mode+event */
    payload[15] = 0x0000;  /* reserved */

    return fmc16_packet_tx_build(tx_buf,
                                 FMC16_MSG_DEVICE_INFO, 0,
                                 FMC16_NODE_XO2, FMC16_CH_LINK_MGMT,
                                 ctx->tx_sequence++, 0,
                                 payload, 16);
}

uint16_t fmc16_link_send_host_status(Fmc16LinkCtx *ctx, uint16_t *tx_buf)
{
    uint16_t payload[15];
    memset(payload, 0, sizeof(payload));

    payload[0]  = 0x0100;  /* host_status_schema */
    payload[1]  = 0x0001;  /* validity: uptime valid */

    uint32_t uptime = tick_now();
    payload[2]  = FMC16_U32_HI(uptime);
    payload[3]  = FMC16_U32_LO(uptime);

    /* PI, KEY_LED 等 stubbed（待对接 MCP23017 驱动） */
    payload[4]  = 0x0000;  /* pi_requested */
    payload[5]  = 0x0000;  /* pi_applied */
    payload[6]  = 0x0000;  /* pi_gen hi */
    payload[7]  = 0x0000;  /* pi_gen lo */
    payload[8]  = 0x0000;  /* key_led_mask */
    payload[9]  = 0x0000;  /* host_state_flags */
    payload[10] = (ctx->state == FMC16_LINK_UP) ? 0x07u : 0x00u; /* fmc_service_flags */
    payload[11] = FMC16_U32_HI(0);  /* last_applied_trans hi */
    payload[12] = FMC16_U32_LO(0);
    payload[13] = 0x0100;  /* application_schema_version */
    payload[14] = 0x0000;  /* reserved */

    return fmc16_packet_tx_build(tx_buf,
                                 FMC16_MSG_HOST_STATUS, 0,
                                 FMC16_NODE_XO2, FMC16_CH_LINK_MGMT,
                                 ctx->tx_sequence++, 0,
                                 payload, 15);
}

int fmc16_link_has_credit(const Fmc16LinkCtx *ctx, uint16_t total_words)
{
    if (!ctx->status.link_ready || !ctx->status.rx_accepting)
        return 0;
    return (ctx->status.rx_free_words >= total_words &&
            ctx->status.rx_free_slots > 0);
}

int fmc16_link_is_up(const Fmc16LinkCtx *ctx)
{
    return (ctx->state == FMC16_LINK_UP && ctx->status.link_ready);
}
