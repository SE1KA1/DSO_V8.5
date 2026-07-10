/**
 * @file dso_serial_debug.h
 * @brief 结构化串口调试：snapshot、raw dump、frame log。
 */

#ifndef DSO_SERIAL_DEBUG_H
/* 宏定义: DSO_SERIAL_DEBUG_H - 头文件保护宏。 */
#define DSO_SERIAL_DEBUG_H

#include "dso_timebase.h"

#include <stdbool.h>
#include <stdint.h>

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_SendLine
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_SerialDebug_SendLine(const char *text);
/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_Init
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_SerialDebug_Init(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_CaptureRawStage
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_SerialDebug_CaptureRawStage(uint8_t channel,
                                     uint32_t frame_id,
                                     const uint16_t *samples,
                                     uint32_t sample_count,
                                     uint32_t sample_rate_hz,
                                     bool cal_active,
                                     uint8_t cal_state);
/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_RefineRawStageFocus
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_SerialDebug_RefineRawStageFocus(uint8_t channel,
                                         const uint16_t *samples,
                                         uint32_t sample_count,
                                         int32_t focus_index);
/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_IsPhaseOnlyModeEnabled
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_SerialDebug_IsPhaseOnlyModeEnabled(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_ShouldSilencePrintf
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_SerialDebug_ShouldSilencePrintf(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_LogFrame
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_SerialDebug_LogFrame(uint8_t channel,
                              uint32_t frame_id,
                              bool cal_active,
                              uint8_t cal_state,
                              const uint16_t *source_samples,
                              uint32_t source_count,
                              uint32_t sample_rate_hz,
                              const DSO_ProcessAnalysis *analysis,
                              const DSO_RenderMeta *render_meta,
                              const uint16_t *render_samples,
                              uint32_t render_count,
                              const DSO_Frame *frame,
                              const int16_t *y_coords,
                              uint32_t now_ms,
                              uint8_t trig_pos_pct);
/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_ShouldBypassVofa
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_SerialDebug_ShouldBypassVofa(void);

#endif /* DSO_SERIAL_DEBUG_H */
