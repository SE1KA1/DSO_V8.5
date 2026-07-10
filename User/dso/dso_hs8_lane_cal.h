/**
 * @file dso_hs8_lane_cal.h
 * @brief 8-bit 三路 lane 校准。
 */

#ifndef DSO_HS8_LANE_CAL_H
/* 宏定义: DSO_HS8_LANE_CAL_H - 头文件保护宏。 */
#define DSO_HS8_LANE_CAL_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float offset_lsb;
    float gain;
    float skew;
    bool valid;
} DSO_HS8LaneCoeffs;

/* Diagnostic stage reached on the most recent DSO_HS8LaneCal_ServiceFrame
 * call. Lets us see where the calibration pipeline is dying without
 * instrumenting every decision point with printf. */
typedef enum {
    DSO_HS8_LANE_CAL_STAGE_IDLE          = 0,  /* not called yet */
    DSO_HS8_LANE_CAL_STAGE_REJECT_ARG    = 1,  /* null / short buffer */
    DSO_HS8_LANE_CAL_STAGE_REJECT_FREQ   = 2,  /* freq_invalid / freq<=0 */
    DSO_HS8_LANE_CAL_STAGE_REJECT_REP    = 3,  /* rep_conf below threshold */
    DSO_HS8_LANE_CAL_STAGE_RESET_FREQ    = 4,  /* freq jumped >10%, state reset */
    DSO_HS8_LANE_CAL_STAGE_SOLVE_FAIL_1  = 5,  /* solve_lane(1) failed */
    DSO_HS8_LANE_CAL_STAGE_SOLVE_FAIL_2  = 6,  /* solve_lane(2) failed */
    DSO_HS8_LANE_CAL_STAGE_SOLVED        = 7,  /* EMA accumulated a frame */
    DSO_HS8_LANE_CAL_STAGE_READY         = 8   /* ready=true */
} DSO_HS8LaneCalStage;

typedef enum {
    DSO_HS8_LANE_SOLVE_FAIL_NONE       = 0,
    DSO_HS8_LANE_SOLVE_FAIL_ARG        = 1,
    DSO_HS8_LANE_SOLVE_FAIL_USED       = 2,  /* used < min */
    DSO_HS8_LANE_SOLVE_FAIL_SINGULAR   = 3,  /* solve3x3 rejected */
    DSO_HS8_LANE_SOLVE_FAIL_GAIN       = 4,  /* gain out of bounds */
    DSO_HS8_LANE_SOLVE_FAIL_SKEW       = 5,  /* skew out of bounds */
    DSO_HS8_LANE_SOLVE_FAIL_OFFSET     = 6   /* offset out of bounds */
} DSO_HS8LaneSolveFail;

typedef struct {
    DSO_HS8LaneCoeffs phase[3];
    float locked_freq_hz;
    uint8_t good_frames;
    bool ready;
    /* Diagnostics — populated each ServiceFrame/solve_lane call. */
    uint8_t  last_stage;            /* DSO_HS8LaneCalStage */
    uint8_t  last_lane1_fail;       /* DSO_HS8LaneSolveFail for lane 1 */
    uint8_t  last_lane2_fail;       /* DSO_HS8LaneSolveFail for lane 2 */
    uint32_t last_lane1_used;       /* samples accepted by LS for lane 1 */
    uint32_t last_lane2_used;       /* samples accepted by LS for lane 2 */
    uint32_t service_calls;         /* total ServiceFrame invocations */
} DSO_HS8LaneCalState;

/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_IsSampleDensityUsable
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_HS8LaneCal_IsSampleDensityUsable(float total_samples_per_cycle,
                                          uint8_t lane_count);
/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_Reset
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_HS8LaneCal_Reset(DSO_HS8LaneCalState *state);
/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_IsReady
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_HS8LaneCal_IsReady(const DSO_HS8LaneCalState *state);
/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_ResetForLaneCount
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_HS8LaneCal_ResetForLaneCount(DSO_HS8LaneCalState *state, uint8_t lane_count);
/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_IsReadyForLaneCount
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_HS8LaneCal_IsReadyForLaneCount(const DSO_HS8LaneCalState *state,
                                        uint8_t lane_count);
/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_SolveFrame
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_HS8LaneCal_SolveFrame(const uint16_t *samples,
                               uint32_t count,
                               uint16_t adc_max,
                               DSO_HS8LaneCoeffs phase[3]);
/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_SolveFrameEx
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_HS8LaneCal_SolveFrameEx(const uint16_t *samples,
                                 uint32_t count,
                                 uint16_t adc_max,
                                 uint8_t lane_count,
                                 DSO_HS8LaneCoeffs phase[3]);
/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_ServiceFrame
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_HS8LaneCal_ServiceFrame(DSO_HS8LaneCalState *state,
                                 const uint16_t *samples,
                                 uint32_t count,
                                 uint16_t adc_max,
                                 float freq_hz,
                                 bool freq_valid,
                                 bool clipped,
                                 float repetitive_confidence);
/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_ServiceFrameEx
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_HS8LaneCal_ServiceFrameEx(DSO_HS8LaneCalState *state,
                                   const uint16_t *samples,
                                   uint32_t count,
                                   uint16_t adc_max,
                                   float freq_hz,
                                   bool freq_valid,
                                   bool clipped,
                                   float repetitive_confidence,
                                   uint8_t lane_count);
/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_ApplyWithCoeffs
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_HS8LaneCal_ApplyWithCoeffs(const DSO_HS8LaneCoeffs phase[3],
                                    uint16_t adc_max,
                                    uint16_t *samples,
                                    uint32_t count);
/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_ApplyWithCoeffsEx
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_HS8LaneCal_ApplyWithCoeffsEx(const DSO_HS8LaneCoeffs phase[3],
                                      uint16_t adc_max,
                                      uint16_t *samples,
                                      uint32_t count,
                                      uint8_t lane_count);
/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_Apply
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_HS8LaneCal_Apply(const DSO_HS8LaneCalState *state,
                          uint16_t adc_max,
                          uint16_t *samples,
                          uint32_t count);
/*
*********************************************************************************************************
*   函 数 名: DSO_HS8LaneCal_ApplyEx
*   功能说明: 处理 8-bit 高速多 lane 幅度/相位校准求解、状态维护和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_HS8LaneCal_ApplyEx(const DSO_HS8LaneCalState *state,
                            uint16_t adc_max,
                            uint16_t *samples,
                            uint32_t count,
                            uint8_t lane_count);

#endif /* DSO_HS8_LANE_CAL_H */
