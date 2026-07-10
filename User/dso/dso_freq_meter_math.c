/**
 * @file dso_freq_meter_math.c
 * @brief freq meter math 实现。
 */

#include "dso_freq_meter_math.h"

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterMath_PeriodToMilliHz
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint64_t DSO_FreqMeterMath_PeriodToMilliHz(uint32_t ref_hz,
                                           uint32_t periods,
                                           uint64_t ref_ticks)
{
    uint64_t numerator;

    if ((ref_hz == 0u) || (periods == 0u) || (ref_ticks == 0ull)) {
        return 0ull;
    }

    numerator = (uint64_t)ref_hz * (uint64_t)periods * 1000ull;
    return (numerator + (ref_ticks / 2ull)) / ref_ticks;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterMath_GateToMilliHz
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint64_t DSO_FreqMeterMath_GateToMilliHz(uint64_t edge_count,
                                         uint32_t gate_us)
{
    uint64_t numerator;

    if ((edge_count == 0ull) || (gate_us == 0u)) {
        return 0ull;
    }

    numerator = edge_count * 1000000000ull;
    return (numerator + ((uint64_t)gate_us / 2ull)) / (uint64_t)gate_us;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterMath_Extend16
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint64_t DSO_FreqMeterMath_Extend16(uint32_t overflow_count,
                                    uint16_t counter)
{
    return ((uint64_t)overflow_count << 16) | (uint64_t)counter;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterMath_ExtendCapture
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint64_t DSO_FreqMeterMath_ExtendCapture(uint32_t overflow_count,
                                         uint16_t capture,
                                         bool update_pending)
{
    uint32_t ovf = overflow_count;

    if (update_pending && (capture < 0x8000u)) {
        ovf++;
    }

    return DSO_FreqMeterMath_Extend16(ovf, capture);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterMath_SelectMode
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_FreqMeterMode DSO_FreqMeterMath_SelectMode(DSO_FreqMeterMode current,
                                               uint64_t freq_millihz,
                                               uint32_t edges_in_gate)
{
    if (current == DSO_FREQ_METER_MODE_GATED_COUNT) {
        if ((edges_in_gate < DSO_FREQ_METER_GATE_MIN_EDGES) ||
            (freq_millihz <= DSO_FREQ_METER_GATED_TO_CAPTURE_MILLIHZ)) {
            return DSO_FREQ_METER_MODE_LOW_CAPTURE;
        }
        return DSO_FREQ_METER_MODE_GATED_COUNT;
    }

    if (freq_millihz >= DSO_FREQ_METER_CAPTURE_TO_GATED_MILLIHZ) {
        return DSO_FREQ_METER_MODE_GATED_COUNT;
    }

    return DSO_FREQ_METER_MODE_LOW_CAPTURE;
}
