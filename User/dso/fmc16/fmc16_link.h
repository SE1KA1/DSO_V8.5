/**
 * @file fmc16_link.h
 * @brief FMC16 链路层: HELLO 握手、能力协商、LINK_STATUS、HOST_STATUS。
 */
#ifndef FMC16_LINK_H
#define FMC16_LINK_H

#include "fmc16.h"
#include "fmc16_packet.h"   /* Fmc16DecodedHeader */

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 链路状态 ---- */
typedef enum {
    FMC16_LINK_DOWN = 0,
    FMC16_LINK_WAIT_HELLO,
    FMC16_LINK_HANDSHAKING,
    FMC16_LINK_UP,
    FMC16_LINK_RESYNC,
} Fmc16LinkState;

/* ---- XO2 能力信息 ---- */
typedef struct {
    uint32_t capabilities;
    uint16_t max_payload_words;
    uint16_t max_outstanding;
    uint16_t rx_queue_words;
    uint16_t tx_queue_words;
    uint8_t  reset_reason;
    uint32_t build_id;
    uint16_t device_type;
} Fmc16PeerInfo;

/* ---- 信用/流控状态 ---- */
typedef struct {
    uint16_t rx_free_words;
    uint16_t tx_used_words;
    uint16_t rx_free_slots;
    uint16_t tx_pending;
    uint8_t  link_ready     : 1;
    uint8_t  rx_accepting   : 1;
    uint8_t  tx_available   : 1;
    uint8_t  resync_needed  : 1;
    uint32_t uptime_ms;
} Fmc16LinkStatus;

/* ---- 链路上下文 ---- */
typedef struct {
    Fmc16LinkState  state;
    Fmc16PeerInfo   peer;
    Fmc16LinkStatus status;
    uint32_t tx_sequence;
    uint32_t last_rx_seq;
    uint32_t last_rx_uptime;
    uint32_t hello_timeout_ms;
    uint32_t state_enter_tick;
    uint32_t resync_count;
} Fmc16LinkCtx;

/* ---- API ---- */

void fmc16_link_init(Fmc16LinkCtx *ctx, uint32_t hello_timeout_ms);

/**
 * @brief 处理接收到的包（链路层）。
 *        自动处理 LINK_HELLO, RESET_NOTICE, LINK_STATUS, DEVICE_INFO。
 *        更新信用状态。
 * @return 若链路状态发生变化返回 1
 */
int fmc16_link_process_rx(Fmc16LinkCtx *ctx,
                          const Fmc16DecodedHeader *hdr,
                          const uint16_t *payload);

/** 发送 DEVICE_INFO */
uint16_t fmc16_link_send_device_info(Fmc16LinkCtx *ctx, uint16_t *tx_buf);

/** 发送 HOST_STATUS */
uint16_t fmc16_link_send_host_status(Fmc16LinkCtx *ctx, uint16_t *tx_buf);

/** 检查是否有足够信用发送 packets */
int fmc16_link_has_credit(const Fmc16LinkCtx *ctx, uint16_t total_words);

/** 查询链路是否就绪 */
int fmc16_link_is_up(const Fmc16LinkCtx *ctx);

#ifdef __cplusplus
}
#endif
#endif /* FMC16_LINK_H */
