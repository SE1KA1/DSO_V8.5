/**
 * @file dso_wavegen_timer_math.c
 * @brief timer math 实现。
 */

#include "dso_wavegen_timer_math.h"

#include <stddef.h>

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
                                            DSO_WavegenTimerPlan *out)
{
    uint64_t ticks;
    uint64_t best_error = 0xFFFFFFFFFFFFFFFFull;
    uint32_t best_psc = 0u;
    uint32_t best_arr = 0u;
    uint32_t psc;

    if ((out == NULL) || (timer_clock_hz == 0u) || (target_update_hz == 0u)) {
        return DSO_WAVEGEN_INVALID;
    }

    for (psc = 0u; psc <= 0xFFFFu; ++psc) {
        uint64_t div_clock = (uint64_t)timer_clock_hz / ((uint64_t)psc + 1u);
        uint64_t arr_plus_1;
        uint64_t actual;
        uint64_t error;

        if (div_clock == 0u) {
            break;
        }
        arr_plus_1 = (div_clock + ((uint64_t)target_update_hz / 2u)) / (uint64_t)target_update_hz;
        if ((arr_plus_1 == 0u) || (arr_plus_1 > 0x100000000ull)) {
            continue;
        }
        actual = div_clock / arr_plus_1;
        error = (actual > target_update_hz) ?
                (actual - target_update_hz) :
                ((uint64_t)target_update_hz - actual);
        if (error < best_error) {
            best_error = error;
            best_psc = psc;
            best_arr = (uint32_t)(arr_plus_1 - 1u);
            if (error == 0u) {
                break;
            }
        }
    }

    if (best_error == 0xFFFFFFFFFFFFFFFFull) {
        return DSO_WAVEGEN_TIMER_ERROR;
    }

    ticks = ((uint64_t)best_psc + 1u) * ((uint64_t)best_arr + 1u);
    out->psc = best_psc;
    out->arr = best_arr;
    out->actual_update_hz = (uint32_t)((uint64_t)timer_clock_hz / ticks);
    return DSO_WAVEGEN_OK;
}
