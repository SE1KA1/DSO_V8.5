/**
 * @file dso_10bit_display_sync.c
 * @brief 10-bit 显示同步实现。
 */

#include "dso_10bit_display_sync.h"

#include <math.h>
#include <stddef.h>

/*
*********************************************************************************************************
*   函 数 名: DSO_TriggerDisplaySync_MapByRate
*   功能说明: 在 10-bit 显示同步链路中对齐 CH1/CH2 触发位置和显示分析结果。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_TriggerDisplaySync_MapByRate(const DSO_ProcessAnalysis *src_analysis,
                                      uint32_t src_rate_hz,
                                      uint32_t dst_rate_hz,
                                      uint32_t dst_sample_count,
                                      DSO_ProcessAnalysis *dst_analysis)
{
    float src_trigger_pos;
    float mapped_pos;

    if ((src_analysis == NULL) || (dst_analysis == NULL) ||
        !src_analysis->triggered || (src_rate_hz == 0u) ||
        (dst_rate_hz == 0u) || (dst_sample_count == 0u)) {
        return false;
    }

    src_trigger_pos = src_analysis->trigger_pos_samples;
    if (src_trigger_pos < 0.0f) {
        if (src_analysis->trigger_index < 0) {
            return false;
        }
        src_trigger_pos = (float)src_analysis->trigger_index;
    }

    mapped_pos = src_trigger_pos * ((float)dst_rate_hz / (float)src_rate_hz);
    if (mapped_pos < 0.0f) {
        mapped_pos = 0.0f;
    }
    if (mapped_pos > (float)(dst_sample_count - 1u)) {
        mapped_pos = (float)(dst_sample_count - 1u);
    }

    dst_analysis->triggered = true;
    dst_analysis->trigger_pos_samples = mapped_pos;
    dst_analysis->trigger_index = (int32_t)floorf(mapped_pos);
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_10bitDisplaySync_AlignCh2TriggerToCh1
*   功能说明: 在 10-bit 显示同步链路中对齐 CH1/CH2 触发位置和显示分析结果。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_10bitDisplaySync_AlignCh2TriggerToCh1(const DSO_ProcessAnalysis *ch1_analysis,
                                               uint32_t ch1_sample_rate_hz,
                                               uint32_t ch2_sample_rate_hz,
                                               uint32_t ch2_sample_count,
                                               DSO_ProcessAnalysis *ch2_analysis)
{
    return DSO_TriggerDisplaySync_MapByRate(ch1_analysis,
                                            ch1_sample_rate_hz,
                                            ch2_sample_rate_hz,
                                            ch2_sample_count,
                                            ch2_analysis);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_10bitDisplaySync_AllowHs10ForChannel
*   功能说明: 在 10-bit 显示同步链路中对齐 CH1/CH2 触发位置和显示分析结果。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_10bitDisplaySync_AllowHs10ForChannel(uint8_t channel)
{
    return channel == 0u;
}
