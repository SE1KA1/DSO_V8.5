/**
 * @file dso_8bit_vpp_cal.h
 * @brief 8-bit Vpp measurement frequency-response calibration.
 */

#ifndef DSO_8BIT_VPP_CAL_H
/* 宏定义: DSO_8BIT_VPP_CAL_H - 头文件保护宏。 */
#define DSO_8BIT_VPP_CAL_H

#include "dso_config.h"

typedef struct {
    float freq_hz;
    float blend_to_hf_scale;
} DSO_8bitVppCalPoint;

/*
*********************************************************************************************************
*   函 数 名: DSO_8bitVppCal_Clamp01
*   功能说明: 为 8-bit Vpp 标定链路提供校准系数查询、应用或有效性判断。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static inline float DSO_8bitVppCal_Clamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_8bitVppCal_BlendScale
*   功能说明: 为 8-bit Vpp 标定链路提供校准系数查询、应用或有效性判断。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static inline float DSO_8bitVppCal_BlendScale(float high_freq_scale, float blend)
{
    return 1.0f + (DSO_8bitVppCal_Clamp01(blend) * (high_freq_scale - 1.0f));
}

/*
*********************************************************************************************************
*   函 数 名: DSO_8bitVppCal_ScaleForFreq
*   功能说明: 为 8-bit Vpp 标定链路提供校准系数查询、应用或有效性判断。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static inline float DSO_8bitVppCal_ScaleForFreq(float freq_hz, float high_freq_scale)
{
    static const DSO_8bitVppCalPoint points[] = {
        { 1000000.0f, 0.00f },
        { 5000000.0f, 0.20f },
        { 8000000.0f, 0.55f },
        { 8500000.0f, 0.775f },
        { 9000000.0f, 1.00f },
    };
    unsigned int i;

    if (high_freq_scale <= 0.0f) {
        high_freq_scale = DSO_MEASURE_AMP_SCALE;
    }
    if (freq_hz <= 0.0f) {
        return high_freq_scale;
    }
    if (freq_hz <= points[0].freq_hz) {
        return 1.0f;
    }

    for (i = 1u; i < (sizeof(points) / sizeof(points[0])); ++i) {
        if (freq_hz <= points[i].freq_hz) {
            float f0 = points[i - 1u].freq_hz;
            float f1 = points[i].freq_hz;
            float b0 = points[i - 1u].blend_to_hf_scale;
            float b1 = points[i].blend_to_hf_scale;
            float t = (freq_hz - f0) / (f1 - f0);

            return DSO_8bitVppCal_BlendScale(high_freq_scale,
                                             b0 + ((b1 - b0) * t));
        }
    }

    return high_freq_scale;
}

#endif /* DSO_8BIT_VPP_CAL_H */
