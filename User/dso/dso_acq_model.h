/**
 * @file dso_acq_model.h
 * @brief ADC/TIM 时钟模型：估算 lane 采样率与 TIM ARR。
 */

#ifndef DSO_ACQ_MODEL_H
/* 宏定义: DSO_ACQ_MODEL_H - 头文件保护宏。 */
#define DSO_ACQ_MODEL_H

#include "dso_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* STM32H743 Rev.Y 的 DBGMCU->IDCODE 修订号。 */
#define DSO_ACQ_REVID_Y 0x1003u

/* STM32H743 Rev.V 的 DBGMCU->IDCODE 修订号。 */
#define DSO_ACQ_REVID_V 0x2003u

typedef enum
{
    /* 未识别或未读取到有效芯片修订号。 */
    DSO_ACQ_REVISION_UNKNOWN = 0,
    /* Rev.Y 硬件版本。 */
    DSO_ACQ_REVISION_Y,
    /* Rev.V 硬件版本。 */
    DSO_ACQ_REVISION_V,
} DSO_AcqRevision;

typedef struct
{
    /* 原始芯片修订号，通常来自 DBGMCU->IDCODE。 */
    uint32_t revid;
    /* 解析后的芯片修订枚举。 */
    DSO_AcqRevision revision;
    /* 用户或编译配置请求的采集 profile。 */
    uint32_t requested_profile;
    /* 请求 profile 的字符串名称，供串口诊断输出。 */
    const char *requested_profile_name;
    /* 经过时序约束降档后的实际 profile。 */
    uint32_t active_profile;
    /* 实际 profile 的字符串名称。 */
    const char *active_profile_name;
    /* ADC 输入时钟，Rev.V 可先输入 150 MHz 再由 ADC 内部分频。 */
    uint32_t adc_input_clk_hz;
    /* ADC 实际 core clock。 */
    uint32_t adc_core_clk_hz;
    /* ADC core 分频系数。 */
    uint32_t adc_core_divider;
    /* tim8_* names are legacy field names; current acquisition backends use TIM1. */
    /* 触发定时器时钟；字段名保留 tim8 兼容旧诊断输出，当前后端实际使用 TIM1。 */
    uint32_t tim8_clk_hz;
    /* 触发定时器 ARR。 */
    uint32_t tim8_arr;
    /* 一个触发周期的定时器 clock 数，即 ARR + 1。 */
    uint32_t period_clocks;
    /* 根据 ADC 转换时间和触发延迟计算得到的最低周期 clock 数。 */
    uint32_t required_period_clocks;
    /* ADC 采样时间，单位为 0.1 个 ADC clock。 */
    uint32_t sample_cycles_x10;
    /* ADC SAR 转换时间，单位为 0.1 个 ADC clock。 */
    uint32_t sar_cycles_x10;
    /* 触发延迟保护周期，单位为 ADC clock。 */
    uint32_t delay_cycles;
    /* 定时器产生的触发频率。 */
    uint32_t trigger_rate_hz;
    /* 单个 lane 的有效采样率。 */
    uint32_t lane_rate_hz;
    /* 合并后的 CH1 等效采样率。 */
    uint32_t sample_rate_hz;
    /* 是否为单 ADC 模式；false 表示多 lane 交织。 */
    bool single_adc;
    /* 当前 active profile 是否满足最低转换周期要求。 */
    bool profile_valid;
} DSO_AcqRuntimeInfo;

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqModel_RevisionFromRevid
*   功能说明: 将芯片修订号转换为采集模型使用的修订枚举。
*   形    参: revid - 原始芯片修订号。
*   返 回 值: DSO_AcqRevision 枚举。
*********************************************************************************************************
*/
static inline DSO_AcqRevision DSO_AcqModel_RevisionFromRevid(uint32_t revid)
{
    switch (revid) {
    case DSO_ACQ_REVID_Y:
        return DSO_ACQ_REVISION_Y;
    case DSO_ACQ_REVID_V:
        return DSO_ACQ_REVISION_V;
    default:
        return DSO_ACQ_REVISION_UNKNOWN;
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqModel_RevisionName
*   功能说明: 返回芯片修订枚举对应的诊断字符串。
*   形    参: revision - 芯片修订枚举。
*   返 回 值: 字符串常量指针。
*********************************************************************************************************
*/
static inline const char *DSO_AcqModel_RevisionName(DSO_AcqRevision revision)
{
    switch (revision) {
    case DSO_ACQ_REVISION_Y:
        return "Rev.Y";
    case DSO_ACQ_REVISION_V:
        return "Rev.V";
    default:
        return "unknown";
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqModel_TargetInputClockHz
*   功能说明: 根据芯片修订选择 ADC 输入时钟目标。
*   形    参: revision - 芯片修订枚举。
*   返 回 值: ADC 输入时钟，单位 Hz。
*********************************************************************************************************
*/
static inline uint32_t DSO_AcqModel_TargetInputClockHz(DSO_AcqRevision revision)
{
    return (revision == DSO_ACQ_REVISION_V)
         ? DSO_ADC_INPUT_CLK_REVV_HZ
         : DSO_ADC_INPUT_CLK_DEFAULT_HZ;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqModel_AdcCoreDivider
*   功能说明: 根据芯片修订选择 ADC core 分频系数。
*   形    参: revision - 芯片修订枚举。
*   返 回 值: ADC core 分频系数。
*********************************************************************************************************
*/
static inline uint32_t DSO_AcqModel_AdcCoreDivider(DSO_AcqRevision revision)
{
    return (revision == DSO_ACQ_REVISION_V) ? 2u : 1u;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqModel_AdcCoreClockHz
*   功能说明: 根据输入时钟和芯片修订计算 ADC 实际 core clock。
*   形    参: adc_input_clk_hz - ADC 输入时钟，单位 Hz。
*             revision         - 芯片修订枚举。
*   返 回 值: ADC core clock，单位 Hz。
*********************************************************************************************************
*/
static inline uint32_t DSO_AcqModel_AdcCoreClockHz(uint32_t adc_input_clk_hz,
                                                   DSO_AcqRevision revision)
{
    uint32_t divider = DSO_AcqModel_AdcCoreDivider(revision);
    return (divider == 0u) ? 0u : (adc_input_clk_hz / divider);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqModel_ProfileArr
*   功能说明: 返回指定采集 profile 对应的触发定时器 ARR。
*   形    参: profile - 采集 profile 编号。
*   返 回 值: TIM ARR。
*********************************************************************************************************
*/
static inline uint32_t DSO_AcqModel_ProfileArr(uint32_t profile)
{
    switch (profile) {
    case DSO_DUAL_SWEEP_PROFILE_HS11:
        return DSO_DUAL_SWEEP_PROFILE_HS11_TIM8_ARR;
    case DSO_DUAL_SWEEP_PROFILE_HS10:
        return DSO_DUAL_SWEEP_PROFILE_HS10_TIM8_ARR;
    case DSO_DUAL_SWEEP_PROFILE_STABLE93:
        return DSO_DUAL_SWEEP_PROFILE_STABLE93_TIM8_ARR;
    case DSO_DUAL_SWEEP_PROFILE_SAFE9:
    default:
        return DSO_DUAL_SWEEP_PROFILE_SAFE9_TIM8_ARR;
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqModel_ProfileName
*   功能说明: 返回指定采集 profile 的诊断字符串。
*   形    参: profile - 采集 profile 编号。
*   返 回 值: 字符串常量指针。
*********************************************************************************************************
*/
static inline const char *DSO_AcqModel_ProfileName(uint32_t profile)
{
    switch (profile) {
    case DSO_DUAL_SWEEP_PROFILE_HS11:
        return "dual_hs11_d1";
    case DSO_DUAL_SWEEP_PROFILE_HS10:
        return "dual_hs10_d1";
    case DSO_DUAL_SWEEP_PROFILE_STABLE93:
        return "dual_stable93_d1";
    case DSO_DUAL_SWEEP_PROFILE_SAFE9:
    default:
        return "dual_safe9_d1";
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqModel_ProfileStepDown
*   功能说明: 当请求 profile 不满足 ADC 转换时序时，返回下一档更保守 profile。
*   形    参: profile - 当前 profile 编号。
*   返 回 值: 降档后的 profile 编号；最低档保持不变。
*********************************************************************************************************
*/
static inline uint32_t DSO_AcqModel_ProfileStepDown(uint32_t profile)
{
    switch (profile) {
    case DSO_DUAL_SWEEP_PROFILE_HS11:
        return DSO_DUAL_SWEEP_PROFILE_HS10;
    case DSO_DUAL_SWEEP_PROFILE_HS10:
        return DSO_DUAL_SWEEP_PROFILE_STABLE93;
    case DSO_DUAL_SWEEP_PROFILE_STABLE93:
        return DSO_DUAL_SWEEP_PROFILE_SAFE9;
    case DSO_DUAL_SWEEP_PROFILE_SAFE9:
    default:
        return DSO_DUAL_SWEEP_PROFILE_SAFE9;
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqModel_RequiredPeriodClocks
*   功能说明: 根据 ADC 转换时间和触发延迟计算一个 lane 所需的最小定时器周期。
*   形    参: tim8_clk_hz       - 触发定时器时钟，单位 Hz。
*             adc_core_clk_hz   - ADC core clock，单位 Hz。
*             sample_cycles_x10 - 采样周期，单位 0.1 ADC clock。
*             sar_cycles_x10    - SAR 转换周期，单位 0.1 ADC clock。
*             delay_cycles      - 额外延迟周期，单位 ADC clock。
*   返 回 值: 最小定时器周期 clock 数；参数无效时返回 0。
*********************************************************************************************************
*/
static inline uint32_t DSO_AcqModel_RequiredPeriodClocks(uint32_t tim8_clk_hz,
                                                         uint32_t adc_core_clk_hz,
                                                         uint32_t sample_cycles_x10,
                                                         uint32_t sar_cycles_x10,
                                                         uint32_t delay_cycles)
{
    uint64_t lane_cycles_x10;

    if ((tim8_clk_hz == 0u) || (adc_core_clk_hz == 0u)) {
        return 0u;
    }

    /*
     * High-speed paths use independent ADC lane timing. Each lane receives one
     * trigger per timer period, so the minimum period is one conversion plus the
     * explicit trigger-delay margin.
     */
    lane_cycles_x10 = (uint64_t)sample_cycles_x10 +
                      (uint64_t)sar_cycles_x10 +
                      ((uint64_t)delay_cycles * 10u);

    return (uint32_t)((lane_cycles_x10 * (uint64_t)tim8_clk_hz +
                       ((uint64_t)adc_core_clk_hz * 10ull) - 1ull) /
                      ((uint64_t)adc_core_clk_hz * 10ull));
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqModel_AcceptedTriggerRateHz
*   功能说明: 根据安全采样率限制计算可接受触发率及触发分频。
*   形    参: trigger_rate_hz - 原始触发率，单位 Hz。
*             safe_rate_hz    - 安全上限，单位 Hz。
*             out_trigger_div - 输出触发分频；允许为 NULL。
*   返 回 值: 分频后的可接受触发率，单位 Hz。
*********************************************************************************************************
*/
static inline uint32_t DSO_AcqModel_AcceptedTriggerRateHz(uint32_t trigger_rate_hz,
                                                          uint32_t safe_rate_hz,
                                                          uint32_t *out_trigger_div)
{
    uint32_t trigger_div;

    if ((trigger_rate_hz == 0u) || (safe_rate_hz == 0u)) {
        if (out_trigger_div != NULL) {
            *out_trigger_div = 0u;
        }
        return 0u;
    }

    trigger_div = (trigger_rate_hz + safe_rate_hz - 1u) / safe_rate_hz;
    if (trigger_div == 0u) {
        trigger_div = 1u;
    }
    if (out_trigger_div != NULL) {
        *out_trigger_div = trigger_div;
    }

    return trigger_rate_hz / trigger_div;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqModel_Build
*   功能说明: 构建采集运行时模型，完成芯片修订、ADC 时钟、profile 降档和采样率计算。
*   形    参: revid              - 芯片修订号。
*             requested_profile  - 请求 profile。
*             adc_input_clk_hz   - ADC 输入时钟，单位 Hz。
*             tim8_clk_hz        - 触发定时器时钟，单位 Hz。
*             sample_cycles_x10  - 采样周期，单位 0.1 ADC clock。
*             sar_cycles_x10     - SAR 转换周期，单位 0.1 ADC clock。
*             delay_cycles       - 触发延迟保护周期。
*             single_adc         - 是否为单 ADC 模式。
*   返 回 值: 填充完整的 DSO_AcqRuntimeInfo。
*********************************************************************************************************
*/
static inline DSO_AcqRuntimeInfo DSO_AcqModel_Build(uint32_t revid,
                                                    uint32_t requested_profile,
                                                    uint32_t adc_input_clk_hz,
                                                    uint32_t tim8_clk_hz,
                                                    uint32_t sample_cycles_x10,
                                                    uint32_t sar_cycles_x10,
                                                    uint32_t delay_cycles,
                                                    bool single_adc)
{
    DSO_AcqRuntimeInfo out;
    uint32_t active_profile;
    uint32_t required_period_clocks;

    out.revid = revid;
    out.revision = DSO_AcqModel_RevisionFromRevid(revid);
    out.requested_profile = requested_profile;
    out.requested_profile_name = DSO_AcqModel_ProfileName(requested_profile);
    out.adc_input_clk_hz = adc_input_clk_hz;
    out.adc_core_divider = DSO_AcqModel_AdcCoreDivider(out.revision);
    out.adc_core_clk_hz = DSO_AcqModel_AdcCoreClockHz(adc_input_clk_hz, out.revision);
    out.tim8_clk_hz = tim8_clk_hz;
    out.sample_cycles_x10 = sample_cycles_x10;
    out.sar_cycles_x10 = sar_cycles_x10;
    out.delay_cycles = delay_cycles;
    out.single_adc = single_adc;

    required_period_clocks = DSO_AcqModel_RequiredPeriodClocks(tim8_clk_hz,
                                                               out.adc_core_clk_hz,
                                                               sample_cycles_x10,
                                                               sar_cycles_x10,
                                                               delay_cycles);
    active_profile = requested_profile;
    while (((DSO_AcqModel_ProfileArr(active_profile) + 1u) < required_period_clocks) &&
           (active_profile != DSO_DUAL_SWEEP_PROFILE_SAFE9)) {
        active_profile = DSO_AcqModel_ProfileStepDown(active_profile);
    }

    out.active_profile = active_profile;
    out.active_profile_name = DSO_AcqModel_ProfileName(active_profile);
    out.tim8_arr = DSO_AcqModel_ProfileArr(active_profile);
    out.period_clocks = out.tim8_arr + 1u;
    out.required_period_clocks = required_period_clocks;
    out.trigger_rate_hz = (out.period_clocks != 0u) ? (tim8_clk_hz / out.period_clocks) : 0u;
    if (single_adc) {
        out.lane_rate_hz = out.trigger_rate_hz;
        out.sample_rate_hz = out.trigger_rate_hz;
    } else {
        out.lane_rate_hz = out.trigger_rate_hz;
        out.sample_rate_hz = (out.period_clocks != 0u)
                           ? ((2u * tim8_clk_hz) / out.period_clocks)
                           : 0u;
    }
    out.profile_valid = (required_period_clocks != 0u) &&
                        (out.period_clocks >= required_period_clocks);

    return out;
}

#endif /* DSO_ACQ_MODEL_H */
