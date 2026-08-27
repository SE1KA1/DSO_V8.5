/**
 * @file fmc16_status.h
 * @brief 状态快照解析: STATUS_SNAPSHOT, MODE_SNAPSHOT, IO_SNAPSHOT, HOST_STATUS。
 */
#ifndef FMC16_STATUS_H
#define FMC16_STATUS_H

#include "fmc16.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- STATUS_SNAPSHOT (协议 §8, 10-word payload) ---- */
typedef struct {
    uint16_t schema;              /**< 0x0100 */
    uint16_t validity;            /**< bit0=XO2本地, bit1=PO一致, bit2=PIO一致 */
    uint16_t legacy_w0;           /**< SPI 96-bit Word0 */
    uint16_t legacy_w1;           /**< SPI 96-bit Word1 */
    uint16_t legacy_w2;           /**< SPI 96-bit Word2 */
    uint32_t po;                  /**< PO[31:0] */
    uint16_t pio;                 /**< PIO[15:0] */
    uint32_t sample_timestamp_ms;
} Fmc16StatusSnapshot;

/* ---- MODE_SNAPSHOT (协议 §9A.1, 16-word payload) ---- */
typedef struct {
    uint16_t schema;
    uint8_t  mode;                /**< M0~MC */
    uint8_t  owner;               /**< 0=XO2_LOCAL, 1=STM32_REMOTE */
    uint8_t  clk_sel;
    uint8_t  full_select      : 1;
    uint8_t  pll_locked       : 1;
    uint8_t  clk_out_connected : 1;
    uint16_t key_state;           /**< 消抖后按键 [9:0] */
    uint8_t  experiment_id;
    uint16_t experiment_schema;
    uint16_t experiment_page_id;
    uint16_t business_validity;
    uint32_t legacy_payload;      /**< 4 control bytes */
    uint32_t mode_generation;
    uint32_t clock_generation;
    uint8_t  control_source;
    uint8_t  active_personality;
    uint16_t snapshot_flags;
} Fmc16ModeSnapshot;

/* ---- IO_SNAPSHOT (协议 §8.3, 19-word payload) ---- */
typedef struct {
    uint16_t schema;
    uint16_t validity;
    uint16_t pi_requested;
    uint16_t pi_applied;
    uint32_t po_sample;
    uint16_t pio_sample;
    uint16_t pio_output_enable;
    uint16_t pio_drive_value;
    uint16_t pio_writable_mask;
    uint32_t pi_generation;
    uint32_t po_generation;
    uint32_t pio_generation;
    uint32_t sample_timestamp_ms;
    uint16_t business_flags;
} Fmc16IOSnapshot;

/* ---- HOST_STATUS (协议 §8.4, 15-word payload) ---- */
typedef struct {
    uint16_t schema;
    uint16_t validity;
    uint32_t uptime_ms;
    uint16_t pi_requested;
    uint16_t pi_applied;
    uint32_t pi_generation;
    uint16_t key_led_mask;
    uint16_t host_state_flags;
    uint16_t fmc_service_flags;
    uint32_t last_applied_transaction_id;
    uint16_t app_schema_version;
} Fmc16HostStatus;

/* ---- 解析函数 ---- */
int fmc16_status_parse_snapshot(const uint16_t *payload, Fmc16StatusSnapshot *out);
int fmc16_status_parse_mode(const uint16_t *payload, Fmc16ModeSnapshot *out);
int fmc16_status_parse_io(const uint16_t *payload, Fmc16IOSnapshot *out);

/** 构建 HOST_STATUS payload (15 words)，存入 payload[0..14] */
void fmc16_status_build_host_status(const Fmc16HostStatus *in, uint16_t *payload);

#ifdef __cplusplus
}
#endif
#endif /* FMC16_STATUS_H */
