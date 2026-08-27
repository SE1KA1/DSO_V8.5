/**
 * @file fmc16_status.c
 * @brief 状态快照解析实现。
 */
#include "fmc16_status.h"
#include <string.h>

int fmc16_status_parse_snapshot(const uint16_t *payload, Fmc16StatusSnapshot *out)
{
    if (!payload || !out) return 0;
    memset(out, 0, sizeof(*out));

    out->schema   = payload[0];
    out->validity = payload[1];
    out->legacy_w0 = payload[2];
    out->legacy_w1 = payload[3];
    out->legacy_w2 = payload[4];
    out->po       = FMC16_U32(payload[5], payload[6]);
    out->pio      = payload[7];
    out->sample_timestamp_ms = FMC16_U32(payload[8], payload[9]);
    return 1;
}

int fmc16_status_parse_mode(const uint16_t *payload, Fmc16ModeSnapshot *out)
{
    if (!payload || !out) return 0;
    memset(out, 0, sizeof(*out));

    uint16_t p1 = payload[1];
    uint16_t p2 = payload[2];

    out->schema       = payload[0];
    out->mode         = (uint8_t)(p1 & 0x0Fu);
    out->owner        = (uint8_t)((p1 >> 8) & 0x03u);
    out->clk_sel      = (uint8_t)(p2 & 0x1Fu);
    out->full_select  = (uint8_t)((p2 >> 5) & 1u);
    out->pll_locked   = (uint8_t)((p2 >> 6) & 1u);
    out->clk_out_connected = (uint8_t)((p2 >> 7) & 1u);
    out->key_state    = payload[3] & 0x03FFu;
    out->experiment_id = (uint8_t)(payload[4] & 0xFFu);
    out->experiment_schema = payload[5];
    out->experiment_page_id = payload[6];
    out->business_validity  = payload[7];
    out->legacy_payload     = FMC16_U32(payload[8], payload[9]);
    out->mode_generation    = FMC16_U32(payload[10], payload[11]);
    out->clock_generation   = FMC16_U32(payload[12], payload[13]);
    out->control_source     = (uint8_t)(payload[14] & 0xFFu);
    out->active_personality = (uint8_t)((payload[14] >> 8) & 0xFFu);
    out->snapshot_flags     = payload[15];
    return 1;
}

int fmc16_status_parse_io(const uint16_t *payload, Fmc16IOSnapshot *out)
{
    if (!payload || !out) return 0;
    memset(out, 0, sizeof(*out));

    out->schema         = payload[0];
    out->validity       = payload[1];
    out->pi_requested   = payload[2];
    out->pi_applied     = payload[3];
    out->po_sample      = FMC16_U32(payload[4], payload[5]);
    out->pio_sample     = payload[6];
    out->pio_output_enable = payload[7];
    out->pio_drive_value   = payload[8];
    out->pio_writable_mask = payload[9];
    out->pi_generation  = FMC16_U32(payload[10], payload[11]);
    out->po_generation  = FMC16_U32(payload[12], payload[13]);
    out->pio_generation = FMC16_U32(payload[14], payload[15]);
    out->sample_timestamp_ms = FMC16_U32(payload[16], payload[17]);
    out->business_flags = payload[18];
    return 1;
}

void fmc16_status_build_host_status(const Fmc16HostStatus *in, uint16_t *payload)
{
    memset(payload, 0, 15 * sizeof(uint16_t));
    payload[0]  = in->schema;
    payload[1]  = in->validity;
    payload[2]  = FMC16_U32_HI(in->uptime_ms);
    payload[3]  = FMC16_U32_LO(in->uptime_ms);
    payload[4]  = in->pi_requested;
    payload[5]  = in->pi_applied;
    payload[6]  = FMC16_U32_HI(in->pi_generation);
    payload[7]  = FMC16_U32_LO(in->pi_generation);
    payload[8]  = in->key_led_mask;
    payload[9]  = in->host_state_flags;
    payload[10] = in->fmc_service_flags;
    payload[11] = FMC16_U32_HI(in->last_applied_transaction_id);
    payload[12] = FMC16_U32_LO(in->last_applied_transaction_id);
    payload[13] = in->app_schema_version;
    payload[14] = 0x0000;  /* reserved */
}
