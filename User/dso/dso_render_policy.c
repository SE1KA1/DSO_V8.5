/**
 * @file dso_render_policy.c
 * @brief render policy 实现。
 */

#include "dso_render_policy.h"

#include <string.h>

/* Lowered from 0.85 to keep parity with DSO_HS8_RENDER_MIN_REPETITIVE: ETS is
 * a fallback reconstruction path for 8-bit when HS8 render is not active, and
 * an over-tight gate prevents the ETS path from ever building on live data. */
/* ETS 允许构建的默认最低重复性置信度。 */
#define DSO_RENDER_POLICY_ETS_MIN_REPETITIVE_CONFIDENCE 0.70f

/* ETS 候选允许的最大每周期样本数。 */
#define DSO_RENDER_POLICY_ETS_MAX_SAMPLES_PER_CYCLE     4.5f

/* 低于该每周期样本数时，原始显示降级为 limited。 */
#define DSO_RENDER_POLICY_LIMITED_MAX_SAMPLES_PER_CYCLE 6.0f

/*
*********************************************************************************************************
*   函 数 名: clamp01
*   功能说明: 将浮点值限制在 0..1 范围内。
*   形    参: v - 输入值。
*   返 回 值: 钳位后的值。
*********************************************************************************************************
*/
static float clamp01(float v)
{
    if (v < 0.0f) {
        return 0.0f;
    }
    if (v > 1.0f) {
        return 1.0f;
    }
    return v;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_RenderPolicy_Decide
*   功能说明: 依据输入状态选择 raw/limited/ETS 等重建模式，并标记显示质量。
*   形    参: in  - 渲染策略输入。
*             out - 渲染策略输出。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_RenderPolicy_Decide(const DSO_RenderPolicyInput *in,
                             DSO_RenderPolicyDecision *out)
{
    float spc;
    float rep;
    float rep_min;
    bool ets_candidate;

    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    out->recon_mode = DSO_RECON_MODE_RAW_LINEAR;
    out->display_quality = DSO_DISPLAY_QUALITY_GOOD;

    if (in == NULL || !in->freq_valid || in->samples_per_cycle <= 0.0f) {
        out->recon_mode = DSO_RECON_MODE_RAW_LIMITED;
        out->display_quality = DSO_DISPLAY_QUALITY_LIMITED;
        return;
    }

    spc = in->samples_per_cycle;
    rep = clamp01(in->repetitive_confidence);
    rep_min = (in->repetitive_confidence_min > 0.0f)
            ? in->repetitive_confidence_min
            : DSO_RENDER_POLICY_ETS_MIN_REPETITIVE_CONFIDENCE;
    /* Clipped signals are still reconstructible (flat-topped but repetitive);
     * do not gate ETS on clip state. */
    ets_candidate = in->fast_mode &&
                    in->sample_mode &&
                    in->allow_ets &&
                    (rep >= rep_min) &&
                    (spc < DSO_RENDER_POLICY_ETS_MAX_SAMPLES_PER_CYCLE);

    if (ets_candidate) {
        out->display_quality = DSO_DISPLAY_QUALITY_REPEAT_REQUIRED;
        out->recon_mode = DSO_RECON_MODE_RAW_LIMITED;
        out->ets_building = !in->ets_ready;
        if (in->ets_ready) {
            out->recon_mode = DSO_RECON_MODE_ETS;
            out->display_quality = DSO_DISPLAY_QUALITY_GOOD;
            out->ets_building = false;
        }
        return;
    }

    if (spc < DSO_RENDER_POLICY_LIMITED_MAX_SAMPLES_PER_CYCLE) {
        out->recon_mode = DSO_RECON_MODE_RAW_LIMITED;
        out->display_quality = DSO_DISPLAY_QUALITY_LIMITED;
        return;
    }

    out->recon_mode = DSO_RECON_MODE_RAW_LINEAR;
    out->display_quality = DSO_DISPLAY_QUALITY_GOOD;
}
