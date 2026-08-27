/**
 * @file fmc16_io.c
 * @brief IO_CONTROL 命令实现（协议 §8.2）。
 */
#include "fmc16_io.h"
#include <string.h>

void fmc16_io_build_control_payload(const Fmc16IOControl *ctrl, uint16_t *payload)
{
    memset(payload, 0, 13 * sizeof(uint16_t));
    payload[0]  = 0x0100;  /* opcode = IO_CONTROL */
    payload[1]  = 0x0100;  /* schema */
    payload[2]  = (ctrl->pi_update_mask ? 1u : 0u) |
                  (ctrl->pio_update_mask ? 2u : 0u);  /* options */
    payload[3]  = ctrl->timeout_ms;
    payload[4]  = ctrl->pi_update_mask;
    payload[5]  = ctrl->pi_value;
    payload[6]  = ctrl->pio_update_mask;
    payload[7]  = ctrl->pio_output_enable;
    payload[8]  = ctrl->pio_drive_value;
    payload[9]  = FMC16_U32_HI(ctrl->expected_generation);
    payload[10] = FMC16_U32_LO(ctrl->expected_generation);
    payload[11] = FMC16_U32_HI(ctrl->new_generation);
    payload[12] = FMC16_U32_LO(ctrl->new_generation);
}

int fmc16_io_parse_control_response(const uint16_t *payload,
                                    uint16_t payload_words,
                                    Fmc16IOControlResp *out)
{
    if (!payload || payload_words < 5) return 0;
    memset(out, 0, sizeof(*out));
    out->pi_mirror        = payload[0];
    out->pio_output_enable = payload[1];
    out->pio_drive_value   = payload[2];
    out->generation        = FMC16_U32(payload[3], payload[4]);
    out->status            = payload[5];
    return 1;
}
