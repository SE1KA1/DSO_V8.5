/**
 * @file fmc16_packet.h
 * @brief FMC16 协议层: 封包/解包、SOF 搜索状态机、收发接口。
 */
#ifndef FMC16_PACKET_H
#define FMC16_PACKET_H

#include "fmc16.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 接收状态机状态（协议 §12）
 * ======================================================================== */

typedef enum {
    FMC16_RX_HUNT_SOF0,
    FMC16_RX_HUNT_SOF1,
    FMC16_RX_READ_HEADER,       /* 读 W2~W12 */
    FMC16_RX_CHECK_HEADER_CRC,  /* 校验 Header CRC16 + 长度合法性 */
    FMC16_RX_READ_PAYLOAD,      /* 读 payload + CRC32 */
    FMC16_RX_CHECK_PACKET_CRC,  /* 校验 Packet CRC32 */
    FMC16_RX_DISPATCH,          /* 完整合法包就绪 */
    FMC16_RX_ERROR,             /* 进入错误恢复 */
} Fmc16RxState;

/* ========================================================================
 * 接收上下文
 * ======================================================================== */

typedef struct {
    Fmc16RxState state;
    uint16_t buf[FMC16_MAX_PACKET_WORDS];  /**< 接收缓冲区 */
    uint16_t word_index;                   /**< 当前写入位置 */
    uint16_t total_words;                  /**< 从 header 解析的包长 */
    uint16_t payload_words;                /**< 从 header 解析的 payload 长度 */
    uint16_t header[FMC16_HEADER_WORDS];   /**< W0~W12 原始 header */
    uint32_t last_word_tick;               /**< 上一个 word 到达时刻(ms) */
    /* 诊断计数器 */
    uint32_t parse_errors;
    uint32_t crc_errors;
    uint32_t resync_count;
    uint32_t packets_received;
} Fmc16RxCtx;

/* ========================================================================
 * 解码后的包头（比 Fmc16RawHeader 更友好）
 * ======================================================================== */

typedef struct {
    uint8_t  version_major;
    uint8_t  version_minor;
    uint8_t  header_words;
    uint16_t total_words;
    uint8_t  msg_type;       /* Fmc16MsgType */
    uint8_t  flags;
    uint8_t  source;
    uint8_t  destination;
    uint16_t channel;
    uint16_t payload_words;
    uint32_t sequence;
    uint32_t transaction_id;
    uint16_t header_crc16;
} Fmc16DecodedHeader;

/* ========================================================================
 * API
 * ======================================================================== */

/** 初始化接收状态机 */
void fmc16_packet_rx_init(Fmc16RxCtx *ctx);

/**
 * @brief 喂入一个 word 到接收状态机。
 * @param ctx   接收上下文
 * @param word  从 FMC 总线读入的 16-bit word
 * @return 1 = 完整合法包就绪 (DISPATCH 状态), 0 = 继续
 */
int fmc16_packet_rx_feed(Fmc16RxCtx *ctx, uint16_t word);

/**
 * @brief 获取解码后的 packet header。
 *        仅在 fmc16_packet_rx_feed 返回 1 后有效。
 */
const Fmc16DecodedHeader *fmc16_packet_rx_header(const Fmc16RxCtx *ctx);

/**
 * @brief 获取 payload 指针。
 *        payload = ctx->buf 中 W13 起始的位置。
 */
const uint16_t *fmc16_packet_rx_payload(const Fmc16RxCtx *ctx);

/**
 * @brief 消耗当前包，回到 HUNT_SOF0 准备接收下一个。
 */
void fmc16_packet_rx_consume(Fmc16RxCtx *ctx);

/**
 * @brief 构建一个完整 TX 包。
 *        填充 SOF0/SOF1, 所有 header 字段, payload, 并计算 CRC。
 * @param buf           输出缓冲区 (至少 FMC16_MAX_PACKET_WORDS)
 * @param msg_type      消息类型
 * @param flags         标志位
 * @param dest          目标节点 ID
 * @param channel       逻辑通道
 * @param seq           32-bit 传输序号
 * @param trans_id      32-bit 事务号 (非事务包填 0)
 * @param payload       payload 数据 (可为 NULL 若 payload_words==0)
 * @param payload_words payload word 数
 * @return 整包总 word 数 (含 header + payload + CRC32)
 */
uint16_t fmc16_packet_tx_build(uint16_t *buf,
                               uint8_t msg_type, uint8_t flags,
                               uint8_t dest, uint16_t channel,
                               uint32_t seq, uint32_t trans_id,
                               const uint16_t *payload, uint16_t payload_words);

/**
 * @brief 在给定 buffer 上单独计算并写入 Header CRC16。
 *        用于增量构建的场景。
 */
void fmc16_packet_tx_seal_header_crc(uint16_t *header11);

/**
 * @brief 在给定 buffer 上单独计算并追加 Packet CRC32。
 *        返回 append 的 CRC32 word 数 (=2)。
 */
uint16_t fmc16_packet_tx_seal_packet_crc(uint16_t *buf, uint16_t total_before_crc);

#ifdef __cplusplus
}
#endif
#endif /* FMC16_PACKET_H */
