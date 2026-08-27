/**
 * @file fmc16_command.c
 * @brief 命令层实现: COMMAND 发送 + RESPONSE/ACK/NACK 处理 + 超时重试。
 */
#include "fmc16_command.h"
#include "fmc16_packet.h"
#include "fmc16_conf.h"
#include <string.h>

/* ---- 内部: 事务号生成 ---- */
static uint32_t s_next_trans_id = 1;

static uint32_t next_trans_id(void)
{
    uint32_t id = s_next_trans_id++;
    if (s_next_trans_id == 0) s_next_trans_id = 1;
    return id;
}

uint32_t fmc16_command_send(uint16_t *tx_buf,
                            Fmc16PendingCmd *slots, uint8_t num_slots,
                            uint16_t opcode,
                            const uint16_t *args, uint16_t arg_words,
                            uint16_t timeout_ms, uint8_t max_retries,
                            uint32_t seq)
{
    /* 找空闲槽 */
    Fmc16PendingCmd *slot = NULL;
    for (uint8_t i = 0; i < num_slots; i++)
    {
        if (!slots[i].pending)
        {
            slot = &slots[i];
            break;
        }
    }
    if (!slot) return 0;  /* 无空闲槽 */

    uint32_t tid = next_trans_id();

    /* 构建 COMMAND payload: P0=opcode, P1=schema, P2=options, P3=timeout_ms, P4..=args */
    uint16_t cmd_payload[FMC16_MAX_PAYLOAD_WORDS];
    uint16_t total_args = 4u + arg_words;
    if (total_args > FMC16_MAX_PAYLOAD_WORDS) return 0;

    cmd_payload[0] = opcode;
    cmd_payload[1] = 0x0100;      /* command_schema_version */
    cmd_payload[2] = 0x0000;      /* options */
    cmd_payload[3] = timeout_ms;
    if (args && arg_words > 0)
    {
        memcpy(&cmd_payload[4], args, arg_words * sizeof(uint16_t));
    }

    uint8_t flags = 0;
    /* 长命令 (IO_CONTROL) 需要 ACK_REQUIRED */
    if (opcode == FMC16_CMD_IO_CONTROL)
        flags |= FMC16_FLAG_ACK_REQUIRED;

    uint16_t pkt_words = fmc16_packet_tx_build(tx_buf,
                                                FMC16_MSG_COMMAND, flags,
                                                FMC16_NODE_XO2, FMC16_CH_CONTROL,
                                                seq, tid,
                                                cmd_payload, total_args);

    /* 记录命令 */
    memset(slot, 0, sizeof(*slot));
    slot->transaction_id = tid;
    slot->opcode         = opcode;
    slot->send_tick      = HAL_GetTick();
    slot->timeout_ms     = timeout_ms > 0 ? timeout_ms : FMC16_CMD_DEFAULT_TIMEOUT_MS;
    slot->max_retries    = max_retries > 0 ? max_retries : FMC16_MAX_RETRIES;
    slot->pending        = 1;

    return tid;
}

int fmc16_command_process_rx(Fmc16PendingCmd *slots, uint8_t num_slots,
                             const Fmc16DecodedHeader *hdr,
                             const uint16_t *payload)
{
    if (!hdr || !payload) return 0;
    if (hdr->transaction_id == 0) return 0;

    /* 找到对应命令槽 */
    Fmc16PendingCmd *slot = NULL;
    for (uint8_t i = 0; i < num_slots; i++)
    {
        if (slots[i].pending &&
            slots[i].transaction_id == hdr->transaction_id)
        {
            slot = &slots[i];
            break;
        }
    }
    if (!slot) return 0;

    switch (hdr->msg_type)
    {
    case FMC16_MSG_ACK:
        /* ACK payload: P0=original_type, P1=original_opcode, P2=ACCEPTED, P3=est_ms */
        if (hdr->payload_words >= 4)
        {
            slot->ack_received = 1;
            FMC16_LOG("ACK: opcode=0x%04X accepted, est=%ums",
                      payload[1], payload[3]);
        }
        return 0;  /* 等待 RESPONSE */

    case FMC16_MSG_NACK:
        if (hdr->payload_words >= 4)
        {
            slot->resp_status = payload[2];  /* error code */
            slot->resp_detail = payload[3];
            slot->response_rcvd = 1;
            FMC16_WARN("NACK: opcode=0x%04X err=0x%04X detail=0x%04X",
                       payload[1], slot->resp_status, slot->resp_detail);
        }
        return 1;  /* 命令失败完成 */

    case FMC16_MSG_RESPONSE:
        /* RESPONSE payload: P0=opcode, P1=status, P2=detail, P3=result_words, P4..=result */
        if (hdr->payload_words >= 4)
        {
            slot->resp_status = payload[1];
            slot->resp_detail = payload[2];
            slot->resp_words  = payload[3];
            if (slot->resp_words > 0 && slot->resp_words <= 32)
            {
                memcpy(slot->resp_payload, &payload[4],
                       slot->resp_words * sizeof(uint16_t));
            }
            slot->response_rcvd = 1;
            FMC16_LOG("RESPONSE: opcode=0x%04X status=0x%04X",
                      payload[0], slot->resp_status);
        }
        return 1;  /* 命令完成 */

    default:
        break;
    }

    return 0;
}

uint16_t fmc16_command_check_timeouts(Fmc16PendingCmd *slots, uint8_t num_slots,
                                      uint16_t *tx_buf, uint32_t seq)
{
    uint32_t now = HAL_GetTick();

    for (uint8_t i = 0; i < num_slots; i++)
    {
        if (!slots[i].pending) continue;
        if (slots[i].response_rcvd)
        {
            slots[i].pending = 0;  /* 标记已完成 */
            continue;
        }

        uint32_t elapsed = now - slots[i].send_tick;
        if (elapsed >= slots[i].timeout_ms)
        {
            if (slots[i].retry_count < slots[i].max_retries)
            {
                slots[i].retry_count++;
                slots[i].send_tick = now;
                FMC16_WARN("cmd 0x%04X timeout, retry %u/%u",
                           slots[i].opcode,
                           slots[i].retry_count, slots[i].max_retries);

                /* 重试: 重建 COMMAND 包（RETRY 标志） */
                /* 简化: 置 RETRY 标志，复用原 transaction_id */
                uint16_t cmd_payload[4];
                cmd_payload[0] = slots[i].opcode;
                cmd_payload[1] = 0x0100;
                cmd_payload[2] = 0x0000;
                cmd_payload[3] = slots[i].timeout_ms;

                return fmc16_packet_tx_build(tx_buf,
                                              FMC16_MSG_COMMAND,
                                              FMC16_FLAG_RETRY,
                                              FMC16_NODE_XO2,
                                              FMC16_CH_CONTROL,
                                              seq,
                                              slots[i].transaction_id,
                                              cmd_payload, 4);
            }
            else
            {
                FMC16_ERR("cmd 0x%04X FAILED after %u retries",
                          slots[i].opcode, slots[i].max_retries);
                slots[i].resp_status = FMC16_TIMEOUT;
                slots[i].response_rcvd = 1;
            }
        }
    }
    return 0;
}

const Fmc16PendingCmd *fmc16_command_get_completed(Fmc16PendingCmd *slots,
                                                    uint8_t num_slots)
{
    for (uint8_t i = 0; i < num_slots; i++)
    {
        if (slots[i].pending && slots[i].response_rcvd)
        {
            slots[i].pending = 0;  /* 消费 */
            return &slots[i];
        }
    }
    return NULL;
}
