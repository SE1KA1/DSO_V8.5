/**
 * @file dso_freq_meter_math.h
 * @brief 频率计纯数学：period/gate、overflow 扩展、模式选择。
 */

#ifndef DSO_FREQ_METER_MATH_H
/* 宏定义: DSO_FREQ_METER_MATH_H - 头文件保护宏。 */
#define DSO_FREQ_METER_MATH_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DSO_FREQ_METER_MODE_LOW_CAPTURE = 0,
    DSO_FREQ_METER_MODE_GATED_COUNT,
} DSO_FreqMeterMode;

/* 宏定义: DSO_FREQ_METER_GATE_MIN_EDGES - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_FREQ_METER_GATE_MIN_EDGES             8u
/* 宏定义: DSO_FREQ_METER_GATED_TO_CAPTURE_MILLIHZ - 计数、阈值、容量或深度配置常量。 */
#define DSO_FREQ_METER_GATED_TO_CAPTURE_MILLIHZ   1000000ull
/* 宏定义: DSO_FREQ_METER_CAPTURE_TO_GATED_MILLIHZ - 计数、阈值、容量或深度配置常量。 */
#define DSO_FREQ_METER_CAPTURE_TO_GATED_MILLIHZ   2000000ull

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
                                           uint64_t ref_ticks);
/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterMath_GateToMilliHz
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint64_t DSO_FreqMeterMath_GateToMilliHz(uint64_t edge_count,
                                         uint32_t gate_us);
/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterMath_Extend16
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint64_t DSO_FreqMeterMath_Extend16(uint32_t overflow_count,
                                    uint16_t counter);
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
                                         bool update_pending);
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
                                               uint32_t edges_in_gate);

#endif /* DSO_FREQ_METER_MATH_H */
