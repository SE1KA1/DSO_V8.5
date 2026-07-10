/**
 * @file dso_10bit_lane_cal.h
 * @brief 10-bit CH1 双 lane 校准：offset/gain/skew，EEPROM 持久化。
 */

#ifndef DSO_10BIT_LANE_CAL_H
/* 宏定义: DSO_10BIT_LANE_CAL_H - 头文件保护宏。 */
#define DSO_10BIT_LANE_CAL_H

#include "dso_process.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float offset_lsb;
    float gain;
    float skew;
    bool valid;
} DSO_10bitLaneCalCoeffs;

typedef enum {
    DSO_10BIT_LANE_CAL_STATE_IDLE = 0,
    DSO_10BIT_LANE_CAL_STATE_LOADED = 1,
    DSO_10BIT_LANE_CAL_STATE_COLLECTING = 2,
    DSO_10BIT_LANE_CAL_STATE_SUCCESS = 3,
    DSO_10BIT_LANE_CAL_STATE_FAILED = 4,
} DSO_10bitLaneCalState;

typedef enum {
    DSO_10BIT_LANE_CAL_ERR_NONE = 0,
    DSO_10BIT_LANE_CAL_ERR_NO_SIGNAL = 1,
    DSO_10BIT_LANE_CAL_ERR_NOT_REPETITIVE = 2,
    DSO_10BIT_LANE_CAL_ERR_CLIPPED = 3,
    DSO_10BIT_LANE_CAL_ERR_SOLVE = 4,
    DSO_10BIT_LANE_CAL_ERR_STORE = 5,
} DSO_10bitLaneCalError;

typedef struct {
    DSO_10bitLaneCalCoeffs coeffs;
    DSO_10bitLaneCalState state;
    DSO_10bitLaneCalError error;
    bool valid;
    uint8_t progress_pct;
    uint8_t good_frames;
    uint8_t target_frames;
    float last_freq_hz;
} DSO_10bitLaneCalInfo;

/*
*********************************************************************************************************
*   函 数 名: DSO_10bitLaneCal_Init
*   功能说明: 处理 10-bit 双 lane 校准参数加载、求解、保存和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_10bitLaneCal_Init(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_10bitLaneCal_RequestRun
*   功能说明: 处理 10-bit 双 lane 校准参数加载、求解、保存和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_10bitLaneCal_RequestRun(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_10bitLaneCal_Cancel
*   功能说明: 处理 10-bit 双 lane 校准参数加载、求解、保存和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_10bitLaneCal_Cancel(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_10bitLaneCal_GetInfo
*   功能说明: 处理 10-bit 双 lane 校准参数加载、求解、保存和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_10bitLaneCal_GetInfo(DSO_10bitLaneCalInfo *out);
/*
*********************************************************************************************************
*   函 数 名: DSO_10bitLaneCal_IsValid
*   功能说明: 处理 10-bit 双 lane 校准参数加载、求解、保存和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_10bitLaneCal_IsValid(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_10bitLaneCal_ShouldApply
*   功能说明: 处理 10-bit 双 lane 校准参数加载、求解、保存和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_10bitLaneCal_ShouldApply(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_10bitLaneCal_ApplyInPlace
*   功能说明: 处理 10-bit 双 lane 校准参数加载、求解、保存和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_10bitLaneCal_ApplyInPlace(uint16_t *samples,
                                   uint32_t count,
                                   uint16_t adc_max);
/*
*********************************************************************************************************
*   函 数 名: DSO_10bitLaneCal_ApplyWithCoeffs
*   功能说明: 处理 10-bit 双 lane 校准参数加载、求解、保存和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_10bitLaneCal_ApplyWithCoeffs(uint16_t *samples,
                                      uint32_t count,
                                      const DSO_10bitLaneCalCoeffs *coeffs,
                                      uint16_t adc_max);
/*
*********************************************************************************************************
*   函 数 名: DSO_10bitLaneCal_SolveFrame
*   功能说明: 处理 10-bit 双 lane 校准参数加载、求解、保存和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_10bitLaneCal_SolveFrame(const uint16_t *samples,
                                 uint32_t count,
                                 DSO_10bitLaneCalCoeffs *out);
/*
*********************************************************************************************************
*   函 数 名: DSO_10bitLaneCal_ServiceFrame
*   功能说明: 处理 10-bit 双 lane 校准参数加载、求解、保存和样本修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_10bitLaneCal_ServiceFrame(const uint16_t *samples,
                                   uint32_t count,
                                   const DSO_ProcessAnalysis *analysis);

#endif /* DSO_10BIT_LANE_CAL_H */
