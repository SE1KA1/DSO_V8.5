/**
 * @file fmc16_io.h
 * @brief IO 控制: IO_CONTROL 命令构建, IO_SNAPSHOT 解析。
 */
#ifndef FMC16_IO_H
#define FMC16_IO_H

#include "fmc16.h"
#include "fmc16_status.h"  /* Fmc16IOSnapshot */

#ifdef __cplusplus
extern "C" {
#endif

/* ---- IO_CONTROL 输入参数 ---- */
typedef struct {
    uint16_t pi_update_mask;
    uint16_t pi_value;
    uint16_t pio_update_mask;
    uint16_t pio_output_enable;
    uint16_t pio_drive_value;
    uint32_t expected_generation;
    uint32_t new_generation;
    uint16_t timeout_ms;
} Fmc16IOControl;

/* ---- IO_CONTROL 响应 ---- */
typedef struct {
    uint16_t pi_mirror;
    uint16_t pio_output_enable;
    uint16_t pio_drive_value;
    uint32_t generation;
    uint16_t status;
} Fmc16IOControlResp;

/* ---- API ---- */

/** 构建 IO_CONTROL 命令 payload (13 words) */
void fmc16_io_build_control_payload(const Fmc16IOControl *ctrl, uint16_t *payload);

/** 解析 IO_CONTROL 响应 */
int fmc16_io_parse_control_response(const uint16_t *payload,
                                    uint16_t payload_words,
                                    Fmc16IOControlResp *out);

#ifdef __cplusplus
}
#endif
#endif /* FMC16_IO_H */
