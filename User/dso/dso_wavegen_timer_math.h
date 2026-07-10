/**
 * @file dso_wavegen_timer_math.h
 * @brief 波形发生器 TIM/DAC 周期计算。
 */

#ifndef DSO_WAVEGEN_TIMER_MATH_H
/* 宏定义: DSO_WAVEGEN_TIMER_MATH_H - 头文件保护宏。 */
#define DSO_WAVEGEN_TIMER_MATH_H

#include <stdint.h>

#include "dso_wavegen_types.h"

typedef struct {
    uint32_t psc;
    uint32_t arr;
    uint32_t actual_update_hz;
} DSO_WavegenTimerPlan;

/*
*********************************************************************************************************
*   函 数 名: DSO_WavegenTimer_CalcPlan
*   功能说明: 处理 PA4/DAC 波形发生器的配置、启动停止、查表或定时器参数计算。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_WavegenStatus DSO_WavegenTimer_CalcPlan(uint32_t timer_clock_hz,
                                            uint32_t target_update_hz,
                                            DSO_WavegenTimerPlan *out);

#endif /* DSO_WAVEGEN_TIMER_MATH_H */
