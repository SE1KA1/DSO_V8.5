/**
 * @file dso_engine_cal.h
 * @brief 引擎校准策略：HS8/HS10 lane cal 与 topology 变更处理。
 */

#ifndef DSO_ENGINE_CAL_H
/* 宏定义: DSO_ENGINE_CAL_H - 头文件保护宏。 */
#define DSO_ENGINE_CAL_H

#include <stdbool.h>
#include <stdint.h>
#include "dso/dso_acq.h"
#include "dso/dso_hs8_lane_cal.h"
#include "dso/dso_process.h"
#include "dso/dso_sample_spec.h"

/* ── HS8 lane bootstrap constants ── */
#define DSO_ENGINE_HS8_LANE_BOOTSTRAP_MAX_FREQ_HZ 1500000.0f
/* 宏定义: DSO_ENGINE_HS8_CAL_MAX_FREQ_HZ - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ENGINE_HS8_CAL_MAX_FREQ_HZ DSO_ENGINE_HS8_LANE_BOOTSTRAP_MAX_FREQ_HZ
/* 宏定义: DSO_ENGINE_HS8_LANE_BOOTSTRAP_MIN_REP - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ENGINE_HS8_LANE_BOOTSTRAP_MIN_REP     0.85f
/* 宏定义: DSO_ENGINE_HS8_LANE_BOOTSTRAP_MIN_SPC - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ENGINE_HS8_LANE_BOOTSTRAP_MIN_SPC     3.0f
/* 宏定义: DSO_ENGINE_HS8_LANE_BOOTSTRAP_FREQ_TOL - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ENGINE_HS8_LANE_BOOTSTRAP_FREQ_TOL    0.15f

/* ── Format queries (inline, pure DSO_SampleSpec wrappers) ── */
static inline bool DSO_EngineCal_IsHs8Active(void)
{
    return DSO_SampleSpec_GetActiveFormat() == DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED;
}
/*
*********************************************************************************************************
*   函 数 名: DSO_EngineCal_Is10bitActive
*   功能说明: 处理引擎侧交织校准服务、校准结果应用和低频参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static inline bool DSO_EngineCal_Is10bitActive(void)
{
    return DSO_SampleSpec_GetActiveFormat() == DSO_SAMPLE_FORMAT_10BIT;
}
/*
*********************************************************************************************************
*   函 数 名: DSO_EngineCal_SupportsRawPreprocess
*   功能说明: 处理引擎侧交织校准服务、校准结果应用和低频参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static inline bool DSO_EngineCal_SupportsRawPreprocess(void)
{
    return DSO_EngineCal_Is10bitActive() || DSO_EngineCal_IsHs8Active();
}
/*
*********************************************************************************************************
*   函 数 名: DSO_EngineCal_Hs8LaneCount
*   功能说明: 处理引擎侧交织校准服务、校准结果应用和低频参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static inline uint8_t DSO_EngineCal_Hs8LaneCount(void)
{
    return DSO_EngineCal_IsHs8Active() ? DSO_Acq_GetLaneCount() : 0u;
}

/* ── One-shot lifecycle ── */
void DSO_EngineCal_Init(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_EngineCal_Reset
*   功能说明: 处理引擎侧交织校准服务、校准结果应用和低频参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_EngineCal_Reset(void);

/* ── Reentry ── */
void DSO_EngineCal_ReentryBegin(void);

/* ── Per-frame ── */
bool DSO_EngineCal_IsCh1Ready(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_EngineCal_ApplyCh1IfReady
*   功能说明: 处理引擎侧交织校准服务、校准结果应用和低频参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_EngineCal_ApplyCh1IfReady(uint16_t *samples, uint32_t count);
/*
*********************************************************************************************************
*   函 数 名: DSO_EngineCal_ServiceFrame
*   功能说明: 处理引擎侧交织校准服务、校准结果应用和低频参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_EngineCal_ServiceFrame(const uint16_t *samples,
                                 uint32_t count,
                                 const DSO_ProcessAnalysis *raw_analysis,
                                 const DSO_TriggerConfig *trig_cfg,
                                 uint32_t sample_rate);
/*
*********************************************************************************************************
*   函 数 名: DSO_EngineCal_RefineLowFreqFromLane0
*   功能说明: 处理引擎侧交织校准服务、校准结果应用和低频参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_EngineCal_RefineLowFreqFromLane0(const uint16_t *samples,
                                           uint32_t count,
                                           uint32_t sample_rate,
                                           const DSO_TriggerConfig *trig_cfg,
                                           DSO_ProcessAnalysis *analysis);
/*
*********************************************************************************************************
*   函 数 名: DSO_EngineCal_SyncTimebase
*   功能说明: 处理引擎侧交织校准服务、校准结果应用和低频参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_EngineCal_SyncTimebase(void);

/* ── Accessors ── */
const DSO_HS8LaneCalState *DSO_EngineCal_GetHs8State(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_EngineCal_GetLane2FailStreak
*   功能说明: 处理引擎侧交织校准服务、校准结果应用和低频参考修正。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t DSO_EngineCal_GetLane2FailStreak(void);

#endif /* DSO_ENGINE_CAL_H */
