/**
 * @file dso_sample_spec.c
 * @brief 采样格式表与默认上电格式（8-bit high-speed）。
 */

#include "dso_sample_spec.h"

#include <stddef.h>

/* 采样格式静态规格表：下标必须与 DSO_SampleFormat 枚举值保持一致。 */
static const DSO_SampleSpec s_specs[] = {
    /* 10-bit 模式：CH1 双 ADC lane 交织，CH2 由独立 ADC 实时采集。 */
    {
        .adc_bits = 10u,
        .adc_max = 1023u,
        .midscale = 512u,
        .vref = 3.3f,
        .lane_count = 2u,
        .ch2_mode = DSO_CH2_MODE_REALTIME,
        .interleaved = true,
        .start_immediately = true,
    },
    /* 8-bit 高速模式：CH1 三路交替采样，CH2 在该格式下关闭。 */
    {
        .adc_bits = 8u,
        .adc_max = 255u,
        .midscale = 128u,
        .vref = 3.3f,
        .lane_count = 3u,
        .ch2_mode = DSO_CH2_MODE_DISABLED,
        .interleaved = true,
        .start_immediately = true,
    },
};

/* 当前活动采样格式；由采集 facade 在初始化和格式切换时维护。 */
static DSO_SampleFormat s_active_format = DSO_SAMPLE_FORMAT_DEFAULT;

/*
*********************************************************************************************************
*   函 数 名: DSO_SampleSpec_Get
*   功能说明: 按格式枚举读取采样规格，非法枚举回退到表项 0，避免空指针传播。
*   形    参: format - 采样格式枚举值。
*   返 回 值: 采样规格表项地址。
*********************************************************************************************************
*/
const DSO_SampleSpec *DSO_SampleSpec_Get(DSO_SampleFormat format)
{
    if ((uint32_t)format >= (sizeof(s_specs) / sizeof(s_specs[0]))) {
        return &s_specs[0];
    }

    return &s_specs[(uint32_t)format];
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SampleSpec_SetActive
*   功能说明: 更新当前活动采样格式；非法枚举恢复为默认格式。
*   形    参: format - 需要设为活动状态的采样格式。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_SampleSpec_SetActive(DSO_SampleFormat format)
{
    if ((uint32_t)format >= (sizeof(s_specs) / sizeof(s_specs[0]))) {
        s_active_format = DSO_SAMPLE_FORMAT_DEFAULT;
        return;
    }

    s_active_format = format;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SampleSpec_GetActiveFormat
*   功能说明: 返回当前活动采样格式，供 UI 状态显示和采集路由判断。
*   形    参: 无。
*   返 回 值: 当前活动格式枚举。
*********************************************************************************************************
*/
DSO_SampleFormat DSO_SampleSpec_GetActiveFormat(void)
{
    return s_active_format;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SampleSpec_GetActive
*   功能说明: 返回当前活动采样规格，集中提供 adc_bits、lane_count、CH2 模式等信息。
*   形    参: 无。
*   返 回 值: 当前活动采样规格表项地址。
*********************************************************************************************************
*/
const DSO_SampleSpec *DSO_SampleSpec_GetActive(void)
{
    return DSO_SampleSpec_Get(s_active_format);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SampleSpec_ClampFromI32
*   功能说明: 将中间计算值限制在采样格式的 ADC code 范围内，防止溢出到非法显示/测量值。
*   形    参: spec  - 采样格式规格；为空时返回 0。
*             value - 待限制的有符号中间值。
*   返 回 值: 0 到 spec->adc_max 之间的 ADC code。
*********************************************************************************************************
*/
uint16_t DSO_SampleSpec_ClampFromI32(const DSO_SampleSpec *spec, int32_t value)
{
    if (spec == NULL) {
        return 0u;
    }

    if (value < 0) {
        return 0u;
    }
    if (value > (int32_t)spec->adc_max) {
        return spec->adc_max;
    }

    return (uint16_t)value;
}
