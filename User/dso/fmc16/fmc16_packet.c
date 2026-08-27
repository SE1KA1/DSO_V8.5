/**
 * @file fmc16_packet.c
 * @brief FMC16 协议引擎: RX SOF 搜索状态机 + TX 封包。
 *
 * 实现协议 §12 的完整接收状态机:
 *   HUNT_SOF0 → HUNT_SOF1 → READ_HEADER → CHECK_HEADER_CRC
 *   → READ_PAYLOAD → CHECK_PACKET_CRC → DISPATCH
 *
 * 失败处理:
 *   - SOF1 不匹配: 滑动窗口，当前 word 重判 SOF0
 *   - Header CRC 失败: 不信任长度，立即回到 SOF 搜索
 *   - Packet CRC 失败: 整包丢弃
 *   - 任何错误不得触发业务状态更新
 */
#include "fmc16_packet.h"
#include "fmc16_crc.h"
#include "fmc16_conf.h"
#include <string.h>
#include <stdio.h>

/* ---- 辅助: tick 获取 ---- */
/* 使用 HAL_GetTick，后续可切换为 DWT 微秒级时间戳 */
static uint32_t get_tick_ms(void)
{
    return HAL_GetTick();
}

/* ---- RX 状态机 ---- */

void fmc16_packet_rx_init(Fmc16RxCtx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->state = FMC16_RX_HUNT_SOF0;
    ctx->last_word_tick = get_tick_ms();
}

int fmc16_packet_rx_feed(Fmc16RxCtx *ctx, uint16_t word)
{
    uint32_t now = get_tick_ms();
    ctx->last_word_tick = now;

    switch (ctx->state)
    {
    /* ================================================================
     * HUNT_SOF0: 等待 0xA55A
     * ================================================================ */
    case FMC16_RX_HUNT_SOF0:
        if (word == FMC16_SOF0)
        {
            ctx->buf[0] = word;
            ctx->word_index = 1;
            ctx->state = FMC16_RX_HUNT_SOF1;
        }
        return 0;

    /* ================================================================
     * HUNT_SOF1: 等待 0x5AA5，支持滑动窗口
     * ================================================================ */
    case FMC16_RX_HUNT_SOF1:
        if (word == FMC16_SOF1)
        {
            ctx->buf[1] = word;
            ctx->word_index = 2;
            ctx->state = FMC16_RX_READ_HEADER;
        }
        else if (word == FMC16_SOF0)
        {
            /* 滑动窗口: 当前 word 可能是新帧的 SOF0 */
            ctx->buf[0] = word;
            ctx->word_index = 1;
            /* 留在 HUNT_SOF1 */
        }
        else
        {
            /* 非法: 回到 SOF 搜索，但当前 word 可能也是 SOF0 */
            ctx->state = FMC16_RX_HUNT_SOF0;
            if (word == FMC16_SOF0)
            {
                ctx->buf[0] = word;
                ctx->word_index = 1;
                ctx->state = FMC16_RX_HUNT_SOF1;
            }
            ctx->parse_errors++;
        }
        return 0;

    /* ================================================================
     * READ_HEADER: 读 W2~W12 (11 words)
     * ================================================================ */
    case FMC16_RX_READ_HEADER:
        ctx->buf[ctx->word_index++] = word;
        if (ctx->word_index >= FMC16_HEADER_WORDS)
        {
            /* 13 words 收齐 (W0~W12)，进入校验阶段 */
            ctx->state = FMC16_RX_CHECK_HEADER_CRC;
        }
        return 0;

    /* ================================================================
     * CHECK_HEADER_CRC: 校验 Header CRC16 + 字段合法性
     * ================================================================ */
    case FMC16_RX_CHECK_HEADER_CRC:
    {
        /* 1. 计算 W2~W11 的 CRC16 */
        uint16_t computed = fmc16_crc16(&ctx->buf[2], 10);
        uint16_t received = ctx->buf[12];  /* W12 = header_crc16 */

        if (computed != received)
        {
            /* Header CRC 失败 → 不信任任何字段，立即回 SOF */
            ctx->crc_errors++;
            ctx->state = FMC16_RX_HUNT_SOF0;
            goto check_sof0_slide;
        }

        /* 2. 解码并验证关键字段 */
        uint16_t version = ctx->buf[2];  /* W2=VERSION_HEADER */
        uint8_t major = (uint8_t)(version >> 12) & 0x0Fu;
        uint8_t hwords = (uint8_t)(version & 0xFFu);

        ctx->total_words   = ctx->buf[3];  /* W3=TOTAL_WORDS */
        ctx->payload_words = ctx->buf[7];  /* W7=PAYLOAD_WORDS */

        if (major != FMC16_PROTOCOL_MAJOR ||
            hwords != FMC16_HEADER_WORDS ||
            ctx->payload_words > FMC16_MAX_PAYLOAD_WORDS ||
            ctx->total_words != (FMC16_HEADER_WORDS + ctx->payload_words + 2u) ||
            ctx->total_words < FMC16_MIN_PACKET_WORDS ||
            ctx->total_words > FMC16_MAX_PACKET_WORDS)
        {
            /* 字段非法 → 丢弃，回 SOF */
            ctx->parse_errors++;
            ctx->state = FMC16_RX_HUNT_SOF0;
            goto check_sof0_slide;
        }

        /* Header CRC 通过，字段合法。开始读 payload */
        ctx->state = FMC16_RX_READ_PAYLOAD;
        return 0;
    }

    /* ================================================================
     * READ_PAYLOAD: 读 payload + CRC32 (W13 ~ W(N-1))
     * ================================================================ */
    case FMC16_RX_READ_PAYLOAD:
        ctx->buf[ctx->word_index++] = word;
        if (ctx->word_index >= ctx->total_words)
        {
            ctx->state = FMC16_RX_CHECK_PACKET_CRC;
        }
        return 0;

    /* ================================================================
     * CHECK_PACKET_CRC: 校验 Packet CRC32
     * ================================================================ */
    case FMC16_RX_CHECK_PACKET_CRC:
    {
        /* CRC32 覆盖 W2~W(N-3) (不含 W0/W1 SOF, 不含最后 2 words CRC32) */
        uint16_t crc_range = ctx->total_words - 4u;  /* exclude SOF0,SOF1,CRC32_HI,CRC32_LO */
        uint32_t computed  = fmc16_crc32(&ctx->buf[2], crc_range);
        uint32_t received  = FMC16_U32(ctx->buf[ctx->total_words - 2],
                                       ctx->buf[ctx->total_words - 1]);

        if (computed != received)
        {
            ctx->crc_errors++;
            ctx->state = FMC16_RX_HUNT_SOF0;
            goto check_sof0_slide;
        }

        /* 完整合法包 */
        ctx->packets_received++;
        ctx->state = FMC16_RX_DISPATCH;
        return 1;  /* 通知调用者 */
    }

    /* ================================================================
     * DISPATCH / ERROR: 等待 consume
     * ================================================================ */
    case FMC16_RX_DISPATCH:
    case FMC16_RX_ERROR:
        /* 包已就绪但尚未 consume，忽略新输入 */
        return 0;

    default:
        ctx->state = FMC16_RX_HUNT_SOF0;
        return 0;
    }

check_sof0_slide:
    /* 回到 SOF 搜索前检查当前 word 是否碰巧是 SOF0 */
    ctx->buf[0] = word;
    if (word == FMC16_SOF0)
    {
        ctx->word_index = 1;
        ctx->state = FMC16_RX_HUNT_SOF1;
    }
    else
    {
        ctx->word_index = 0;
    }
    return 0;
}

/* ---- 解码 header ---- */

const Fmc16DecodedHeader *fmc16_packet_rx_header(const Fmc16RxCtx *ctx)
{
    if (ctx->state != FMC16_RX_DISPATCH) return NULL;

    static Fmc16DecodedHeader hdr;
    uint16_t vh       = ctx->buf[2];
    hdr.version_major  = (uint8_t)((vh >> 12) & 0x0Fu);
    hdr.version_minor  = (uint8_t)((vh >> 8)  & 0x0Fu);
    hdr.header_words   = (uint8_t)(vh & 0xFFu);
    hdr.total_words    = ctx->buf[3];
    hdr.msg_type       = FMC16_TYPE(ctx->buf[4]);
    hdr.flags          = FMC16_FLAGS(ctx->buf[4]);
    hdr.source         = FMC16_SRC(ctx->buf[5]);
    hdr.destination    = FMC16_DST(ctx->buf[5]);
    hdr.channel        = ctx->buf[6];
    hdr.payload_words  = ctx->buf[7];
    hdr.sequence       = FMC16_U32(ctx->buf[8], ctx->buf[9]);
    hdr.transaction_id = FMC16_U32(ctx->buf[10], ctx->buf[11]);
    hdr.header_crc16   = ctx->buf[12];
    return &hdr;
}

const uint16_t *fmc16_packet_rx_payload(const Fmc16RxCtx *ctx)
{
    if (ctx->state != FMC16_RX_DISPATCH) return NULL;
    return &ctx->buf[FMC16_HEADER_WORDS];  /* W13 起始 */
}

void fmc16_packet_rx_consume(Fmc16RxCtx *ctx)
{
    ctx->state = FMC16_RX_HUNT_SOF0;
    ctx->word_index = 0;
    ctx->total_words = 0;
    ctx->payload_words = 0;
}

/* ---- TX 构建 ---- */

uint16_t fmc16_packet_tx_build(uint16_t *buf,
                               uint8_t msg_type, uint8_t flags,
                               uint8_t dest, uint16_t channel,
                               uint32_t seq, uint32_t trans_id,
                               const uint16_t *payload, uint16_t payload_words)
{
    uint16_t total = FMC16_HEADER_WORDS + payload_words + 2u; /* +2 CRC32 */

    /* W0, W1: SOF */
    buf[0] = FMC16_SOF0;
    buf[1] = FMC16_SOF1;

    /* W2: VERSION_HEADER */
    buf[2] = FMC16_HEADER_VERSION;

    /* W3: TOTAL_WORDS */
    buf[3] = total;

    /* W4: TYPE_FLAGS */
    buf[4] = FMC16_TYPE_FLAGS(msg_type, flags);

    /* W5: SOURCE_DEST */
    buf[5] = FMC16_SRC_DST(FMC16_NODE_STM32, dest);

    /* W6: CHANNEL */
    buf[6] = channel;

    /* W7: PAYLOAD_WORDS */
    buf[7] = payload_words;

    /* W8, W9: SEQUENCE */
    buf[8]  = FMC16_U32_HI(seq);
    buf[9]  = FMC16_U32_LO(seq);

    /* W10, W11: TRANSACTION_ID */
    buf[10] = FMC16_U32_HI(trans_id);
    buf[11] = FMC16_U32_LO(trans_id);

    /* W12: HEADER_CRC16 (先置 0，后填充) */
    buf[12] = fmc16_crc16(&buf[2], 10);

    /* Payload: W13 ~ W(12+payload_words) */
    if (payload != NULL && payload_words > 0)
    {
        for (uint16_t i = 0; i < payload_words; i++)
        {
            buf[FMC16_HEADER_WORDS + i] = payload[i];
        }
    }

    /* Packet CRC32: 最后 2 words */
    uint16_t crc_range = FMC16_HEADER_WORDS + payload_words - 2u; /* W2~W(12+N-1) */
    uint32_t pkt_crc = fmc16_crc32(&buf[2], crc_range);
    buf[total - 2] = FMC16_U32_HI(pkt_crc);
    buf[total - 1] = FMC16_U32_LO(pkt_crc);

    return total;
}

void fmc16_packet_tx_seal_header_crc(uint16_t *header11)
{
    /* header11 = W2~W12 (11 words), CRC16 = W12 位置 */
    header11[10] = fmc16_crc16(header11, 10);
}

uint16_t fmc16_packet_tx_seal_packet_crc(uint16_t *buf, uint16_t total_before_crc)
{
    /* total_before_crc = W2~W(12+N) 的 word 数 (即不含 SOF 和 CRC32 本身) */
    uint32_t crc = fmc16_crc32(&buf[2], total_before_crc - 2u);
    buf[total_before_crc]     = FMC16_U32_HI(crc);
    buf[total_before_crc + 1] = FMC16_U32_LO(crc);
    return 2;
}
