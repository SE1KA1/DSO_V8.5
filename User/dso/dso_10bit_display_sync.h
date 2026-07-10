/**
 * @file dso_10bit_display_sync.h
 * @brief CH1/CH2 触发位置按采样率映射；HS10 限 CH1。
 */

#ifndef DSO_10BIT_DISPLAY_SYNC_H
/* 宏定义: DSO_10BIT_DISPLAY_SYNC_H - 头文件保护宏。 */
#define DSO_10BIT_DISPLAY_SYNC_H

#include "dso_process.h"

#include <stdbool.h>
#include <stdint.h>

/*
*********************************************************************************************************
*   函 数 名: DSO_TriggerDisplaySync_MapByRate
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_TriggerDisplaySync_MapByRate(const DSO_ProcessAnalysis *src_analysis,
                                      uint32_t src_rate_hz,
                                      uint32_t dst_rate_hz,
                                      uint32_t dst_sample_count,
                                      DSO_ProcessAnalysis *dst_analysis);

/*
*********************************************************************************************************
*   函 数 名: DSO_10bitDisplaySync_AlignCh2TriggerToCh1
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_10bitDisplaySync_AlignCh2TriggerToCh1(const DSO_ProcessAnalysis *ch1_analysis,
                                               uint32_t ch1_sample_rate_hz,
                                               uint32_t ch2_sample_rate_hz,
                                               uint32_t ch2_sample_count,
                                               DSO_ProcessAnalysis *ch2_analysis);

/*
*********************************************************************************************************
*   函 数 名: DSO_10bitDisplaySync_AllowHs10ForChannel
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_10bitDisplaySync_AllowHs10ForChannel(uint8_t channel);

#endif /* DSO_10BIT_DISPLAY_SYNC_H */
