/**
 * @file fmc16_command.h
 * @brief 命令层: COMMAND/RESPONSE/ACK/NACK 构建与解析。
 */
#ifndef FMC16_COMMAND_H
#define FMC16_COMMAND_H

#include "fmc16.h"
#include "fmc16_packet.h"   /* Fmc16DecodedHeader */

#ifdef __cplusplus
extern "C" {
#endif

#define FMC16_MAX_PENDING_CMDS 4

/* ---- 待处理命令 ---- */
typedef struct {
    uint32_t transaction_id;
    uint16_t opcode;
    uint32_t send_tick;
    uint16_t timeout_ms;
    uint8_t  retry_count;
    uint8_t  max_retries;
    uint8_t  pending      : 1;
    uint8_t  ack_received : 1;
    uint8_t  response_rcvd : 1;
    uint16_t resp_status;
    uint16_t resp_detail;
    uint16_t resp_payload[32];  /**< 响应 payload */
    uint16_t resp_words;
} Fmc16PendingCmd;

/* ---- API ---- */

/** 分配一个命令槽并构建 COMMAND 包。返回 transaction_id。 */
uint32_t fmc16_command_send(uint16_t *tx_buf,
                            Fmc16PendingCmd *slots, uint8_t num_slots,
                            uint16_t opcode,
                            const uint16_t *args, uint16_t arg_words,
                            uint16_t timeout_ms, uint8_t max_retries,
                            uint32_t seq);

/** 处理接收到的 ACK/NACK/RESPONSE。返回 1 如果命令完成。 */
int fmc16_command_process_rx(Fmc16PendingCmd *slots, uint8_t num_slots,
                             const Fmc16DecodedHeader *hdr,
                             const uint16_t *payload);

/** 检查超时并触发重试。返回重试所需 tx 包 word 数 (0=无操作)。 */
uint16_t fmc16_command_check_timeouts(Fmc16PendingCmd *slots, uint8_t num_slots,
                                      uint16_t *tx_buf, uint32_t seq);

/** 获取已完成的命令响应 */
const Fmc16PendingCmd *fmc16_command_get_completed(Fmc16PendingCmd *slots,
                                                    uint8_t num_slots);

#ifdef __cplusplus
}
#endif
#endif /* FMC16_COMMAND_H */
