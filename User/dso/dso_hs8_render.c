/**
 * @file dso_hs8_render.c
 * @brief HS8 render 实现。
 */

#include "dso_hs8_render.h"

#include <math.h>
#include <string.h>

/* Lowered from 0.85 so HS8 reconstruction can engage as soon as lane
 * calibration cleans up the interleaved spurs (post-calibration rep_conf
 * typically lands in the 0.80~0.95 band for near-full-scale sines). */
#define DSO_HS8_RENDER_MIN_REPETITIVE  0.70f
/* 宏定义: DSO_HS8_RENDER_MAX_SPC - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_HS8_RENDER_MAX_SPC         8.0f
/* 宏定义: DSO_HS8_RENDER_FREQ_RESET_TOL - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_HS8_RENDER_FREQ_RESET_TOL  0.10f
/* 宏定义: DSO_HS8_RENDER_READY_COVERAGE - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_HS8_RENDER_READY_COVERAGE  0.40f
/* 宏定义: DSO_HS8_GRID_DIV - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_HS8_GRID_DIV               10.0f

/*
*********************************************************************************************************
*   函 数 名: DSO_HS8Render_Reset
*   功能说明: 处理 8-bit 高速重复信号增强重建的累积、状态判断和渲染输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_HS8Render_Reset(DSO_HS8RenderState *state)
{
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
}

/*
*********************************************************************************************************
*   函 数 名: DSO_HS8Render_ShouldBuild
*   功能说明: 处理 8-bit 高速重复信号增强重建的累积、状态判断和渲染输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_HS8Render_ShouldBuild(bool fast_mode,
                               bool sample_mode,
                               bool freq_valid,
                               bool clipped,
                               float repetitive_confidence,
                               float samples_per_cycle)
{
    /* `clipped` intentionally unused: a near-full-scale repetitive signal is
     * still a valid reconstruction target. Clipped peaks just saturate in the
     * bin average, which is visually correct for a railed input. */
    (void)clipped;
    return fast_mode &&
           sample_mode &&
           freq_valid &&
           (repetitive_confidence >= DSO_HS8_RENDER_MIN_REPETITIVE) &&
           (samples_per_cycle > 0.0f) &&
           (samples_per_cycle < DSO_HS8_RENDER_MAX_SPC);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_HS8Render_Accumulate
*   功能说明: 处理 8-bit 高速重复信号增强重建的累积、状态判断和渲染输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_HS8Render_Accumulate(DSO_HS8RenderState *state,
                              const uint16_t *samples,
                              uint32_t sample_count,
                              uint32_t sample_rate_hz,
                              float freq_hz,
                              float trig_pos_samples)
{
    if ((state == NULL) || (samples == NULL) || (sample_count == 0u) ||
        (sample_rate_hz == 0u) || (freq_hz <= 0.0f) ||
        (trig_pos_samples < 0.0f) || (trig_pos_samples >= (float)sample_count)) {
        return;
    }

    if (state->locked_freq_hz > 0.0f) {
        float diff = fabsf(freq_hz - state->locked_freq_hz) / state->locked_freq_hz;
        if (diff > DSO_HS8_RENDER_FREQ_RESET_TOL) {
            DSO_HS8Render_Reset(state);
            return;
        }
    }
    state->locked_freq_hz = freq_hz;

    for (uint32_t i = 0u; i < sample_count; ++i) {
        float dt = ((float)i - trig_pos_samples) / (float)sample_rate_hz;
        float phase = dt * state->locked_freq_hz;
        uint32_t bin;

        phase -= floorf(phase);
        if (phase < 0.0f) {
            phase += 1.0f;
        }

        bin = (uint32_t)(phase * (float)DSO_HS8_RENDER_PHASE_BINS);
        if (bin >= DSO_HS8_RENDER_PHASE_BINS) {
            bin = DSO_HS8_RENDER_PHASE_BINS - 1u;
        }

        state->bin_sum[bin] += (float)samples[i];
        if (state->bin_count[bin] < UINT16_MAX) {
            state->bin_count[bin]++;
        }
    }

    {
        uint32_t filled = 0u;
        for (uint32_t i = 0u; i < DSO_HS8_RENDER_PHASE_BINS; ++i) {
            if (state->bin_count[i] != 0u) {
                filled++;
            }
        }
        state->ready = ((float)filled / (float)DSO_HS8_RENDER_PHASE_BINS) >= DSO_HS8_RENDER_READY_COVERAGE;
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_HS8Render_IsReady
*   功能说明: 处理 8-bit 高速重复信号增强重建的累积、状态判断和渲染输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_HS8Render_IsReady(const DSO_HS8RenderState *state)
{
    return (state != NULL) && state->ready && (state->locked_freq_hz > 0.0f);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_HS8Render_Render
*   功能说明: 处理 8-bit 高速重复信号增强重建的累积、状态判断和渲染输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_HS8Render_Render(const DSO_HS8RenderState *state,
                          float sec_div_ns,
                          uint16_t adc_max,
                          uint16_t midscale,
                          uint16_t *out_display,
                          uint32_t display_width)
{
    float cycle[DSO_HS8_RENDER_PHASE_BINS];
    float time_per_pixel_ns;
    float width_mid;

    if ((out_display == NULL) || (display_width == 0u) || !DSO_HS8Render_IsReady(state)) {
        return false;
    }

    for (uint32_t i = 0u; i < DSO_HS8_RENDER_PHASE_BINS; ++i) {
        if (state->bin_count[i] != 0u) {
            cycle[i] = state->bin_sum[i] / (float)state->bin_count[i];
        } else {
            cycle[i] = (i == 0u) ? (float)midscale : cycle[i - 1u];
        }
    }

    time_per_pixel_ns = (sec_div_ns * DSO_HS8_GRID_DIV) / (float)display_width;
    width_mid = ((float)display_width - 1.0f) * 0.5f;

    for (uint32_t px = 0u; px < display_width; ++px) {
        float rel_t_ns = ((float)px - width_mid) * time_per_pixel_ns;
        float phase = (rel_t_ns * 1e-9f) * state->locked_freq_hz;
        float pos;
        float frac;
        float v;
        uint32_t idx0;
        uint32_t idx1;

        phase -= floorf(phase);
        if (phase < 0.0f) {
            phase += 1.0f;
        }

        pos = phase * (float)DSO_HS8_RENDER_PHASE_BINS;
        idx0 = (uint32_t)pos % DSO_HS8_RENDER_PHASE_BINS;
        idx1 = (idx0 + 1u) % DSO_HS8_RENDER_PHASE_BINS;
        frac = pos - floorf(pos);
        v = ((1.0f - frac) * cycle[idx0]) + (frac * cycle[idx1]);

        if (v < 0.0f) {
            v = 0.0f;
        }
        if (v > (float)adc_max) {
            v = (float)adc_max;
        }
        out_display[px] = (uint16_t)(v + 0.5f);
    }

    return true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_HS8Render_EffectiveRateHz
*   功能说明: 处理 8-bit 高速重复信号增强重建的累积、状态判断和渲染输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
float DSO_HS8Render_EffectiveRateHz(const DSO_HS8RenderState *state)
{
    if (!DSO_HS8Render_IsReady(state)) {
        return 0.0f;
    }

    return state->locked_freq_hz * (float)DSO_HS8_RENDER_PHASE_BINS;
}
