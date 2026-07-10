/**
 * @file dso_wavegen_lut.c
 * @brief LUT 数据与插值。
 */

#include "dso_wavegen_lut.h"

#include <math.h>
#include <stddef.h>

/* 宏定义: DSO_WAVEGEN_DAC_MAX_CODE - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_DAC_MAX_CODE 4095
/* 宏定义: DSO_WAVEGEN_TWO_PI - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_WAVEGEN_TWO_PI       6.28318530717958647692f

/*
*********************************************************************************************************
*   函 数 名: clamp_code
*   功能说明: 在波形发生器模块中完成 DAC/定时器配置、波形查表、参数换算或运行状态控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t clamp_code(int32_t value)
{
    if (value < 0) {
        return 0u;
    }
    if (value > DSO_WAVEGEN_DAC_MAX_CODE) {
        return (uint16_t)DSO_WAVEGEN_DAC_MAX_CODE;
    }
    return (uint16_t)value;
}

/*
*********************************************************************************************************
*   函 数 名: high_count_from_duty
*   功能说明: 在波形发生器模块中完成 DAC/定时器配置、波形查表、参数换算或运行状态控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t high_count_from_duty(uint16_t lut_count, uint16_t duty_permille)
{
    uint32_t high_count = ((uint32_t)lut_count * (uint32_t)duty_permille + 999u) / 1000u;

    if (high_count == 0u) {
        high_count = 1u;
    }
    if (high_count >= lut_count) {
        high_count = (uint32_t)lut_count - 1u;
    }
    return (uint16_t)high_count;
}

/*
*********************************************************************************************************
*   函 数 名: validate
*   功能说明: 在波形发生器模块中完成 DAC/定时器配置、波形查表、参数换算或运行状态控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static DSO_WavegenStatus validate(const DSO_WavegenLutConfig *cfg,
                                  const uint16_t *out_lut,
                                  uint16_t out_capacity)
{
    if ((cfg == NULL) || (out_lut == NULL) ||
        (cfg->lut_count < 2u) ||
        (cfg->lut_count > DSO_WAVEGEN_MAX_LUT_SAMPLES) ||
        (out_capacity < cfg->lut_count) ||
        (cfg->duty_permille == 0u) ||
        (cfg->duty_permille >= 1000u) ||
        (cfg->offset > DSO_WAVEGEN_DAC_MAX_CODE) ||
        (cfg->amplitude > DSO_WAVEGEN_DAC_MAX_CODE)) {
        return DSO_WAVEGEN_INVALID;
    }
    if ((cfg->wave == DSO_WAVEGEN_WAVE_KIND_TRIANGLE) && ((cfg->lut_count % 2u) != 0u)) {
        return DSO_WAVEGEN_INVALID;
    }
    if (cfg->wave > DSO_WAVEGEN_WAVE_KIND_SQUARE) {
        return DSO_WAVEGEN_INVALID;
    }
    return DSO_WAVEGEN_OK;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_WavegenLut_Build
*   功能说明: 处理 PA4/DAC 波形发生器的配置、启动停止、查表或定时器参数计算。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_WavegenStatus DSO_WavegenLut_Build(const DSO_WavegenLutConfig *cfg,
                                       uint16_t *out_lut,
                                       uint16_t out_capacity,
                                       uint16_t *out_count)
{
    DSO_WavegenStatus status;
    uint16_t i;
    int32_t low;
    int32_t high;

    status = validate(cfg, out_lut, out_capacity);
    if (status != DSO_WAVEGEN_OK) {
        if (out_count != NULL) {
            *out_count = 0u;
        }
        return status;
    }

    low = (int32_t)cfg->offset - (int32_t)cfg->amplitude;
    high = (int32_t)cfg->offset + (int32_t)cfg->amplitude;

    switch (cfg->wave) {
    case DSO_WAVEGEN_WAVE_KIND_SINE:
        for (i = 0u; i < cfg->lut_count; ++i) {
            float phase = DSO_WAVEGEN_TWO_PI * (float)i / (float)cfg->lut_count;
            float value = (float)cfg->offset + ((float)cfg->amplitude * sinf(phase));
            out_lut[i] = clamp_code((int32_t)(value + 0.5f));
        }
        break;

    case DSO_WAVEGEN_WAVE_KIND_TRIANGLE: {
        uint16_t half = cfg->lut_count / 2u;
        uint16_t denom = (half > 1u) ? (uint16_t)(half - 1u) : 1u;

        for (i = 0u; i < half; ++i) {
            int32_t value = low + (((high - low) * (int32_t)i) / (int32_t)denom);
            out_lut[i] = clamp_code(value);
        }
        for (i = 0u; i < half; ++i) {
            int32_t value = high - (((high - low) * (int32_t)i) / (int32_t)denom);
            out_lut[half + i] = clamp_code(value);
        }
        break;
    }

    case DSO_WAVEGEN_WAVE_KIND_SQUARE: {
        uint16_t high_count = high_count_from_duty(cfg->lut_count, cfg->duty_permille);

        for (i = 0u; i < cfg->lut_count; ++i) {
            out_lut[i] = (i < high_count) ? clamp_code(high) : clamp_code(low);
        }
        break;
    }

    default:
        if (out_count != NULL) {
            *out_count = 0u;
        }
        return DSO_WAVEGEN_INVALID;
    }

    if (out_count != NULL) {
        *out_count = cfg->lut_count;
    }
    return DSO_WAVEGEN_OK;
}
