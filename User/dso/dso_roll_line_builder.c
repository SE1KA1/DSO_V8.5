/**
 * @file dso_roll_line_builder.c
 * @brief RollLineBuilder 状态机与像素列聚合。
 */

#include "dso_roll_line_builder.h"

#include "dso_roll_physics.h"

#include "dso_config.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

/* 宏定义: DSO_ROLL_NEED_CAP - 计数、阈值、容量或深度配置常量。 */
#define DSO_ROLL_NEED_CAP  (UINT32_MAX / 8u)

/*
*********************************************************************************************************
*   函 数 名: DSO_RollLineBuilder_Begin
*   功能说明: 在 Roll 显示链路中完成滚动列构建、物理参数计算、历史缓冲或重同步处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_RollLineBuilder_Begin(DSO_RollLineBuilderState *st,
                               uint32_t sample_rate_hz,
                               uint32_t sec_div_ns,
                               uint32_t width)
{
    if (st == NULL) {
        return;
    }

    (void)memset(st, 0, sizeof(*st));
    st->width = width;

    if ((width == 0u) || (sample_rate_hz == 0u) || (sec_div_ns == 0u)) {
        st->samples_per_pixel = 1.0f;
        return;
    }

    st->samples_per_pixel = DSO_RollPhysics_SamplesPerPixel(sec_div_ns, sample_rate_hz, width);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_RollLineBuilder_Feed
*   功能说明: 在 Roll 显示链路中完成滚动列构建、物理参数计算、历史缓冲或重同步处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_RollLineBuilder_Feed(DSO_RollLineBuilderState *st,
                              const uint16_t *samples,
                              uint32_t sample_count,
                              uint16_t *out_roll)
{
    uint32_t i;

    if ((st == NULL) || (out_roll == NULL) || (samples == NULL) || (sample_count == 0u) ||
        (st->width == 0u)) {
        return;
    }

    for (i = 0u; (i < sample_count) && (st->wix < st->width); i++) {
        st->lastv = samples[i];
        st->acc_sum += (float)samples[i];
        st->acc_count += 1.0f;

        if (st->acc_count >= st->samples_per_pixel) {
            out_roll[st->wix++] = (uint16_t)((st->acc_sum / st->acc_count) + 0.5f);
            st->acc_sum = 0.0f;
            st->acc_count = 0.0f;
        }
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_RollLineBuilder_FinishPad
*   功能说明: 在 Roll 显示链路中完成滚动列构建、物理参数计算、历史缓冲或重同步处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_RollLineBuilder_FinishPad(DSO_RollLineBuilderState *st, uint16_t *out_roll)
{
    if ((st == NULL) || (out_roll == NULL) || (st->width == 0u)) {
        return;
    }

    while (st->wix < st->width) {
        out_roll[st->wix++] = st->lastv;
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_RollLineBuilder_FinishNoPad
*   功能说明: 在 Roll 显示链路中完成滚动列构建、物理参数计算、历史缓冲或重同步处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_RollLineBuilder_FinishNoPad(const DSO_RollLineBuilderState *st)
{
    (void)st;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_RollLineBuilder_ExportRemainder
*   功能说明: 在 Roll 显示链路中完成滚动列构建、物理参数计算、历史缓冲或重同步处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_RollLineBuilder_ExportRemainder(const DSO_RollLineBuilderState *st,
                                         float *out_acc_sum,
                                         float *out_acc_count)
{
    if ((st == NULL) || (out_acc_sum == NULL) || (out_acc_count == NULL)) {
        return;
    }
    *out_acc_sum   = st->acc_sum;
    *out_acc_count = st->acc_count;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_RollLineBuilder_Build
*   功能说明: 在 Roll 显示链路中完成滚动列构建、物理参数计算、历史缓冲或重同步处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_RollLineBuilder_Build(const uint16_t *samples,
                               uint32_t sample_count,
                               uint32_t sample_rate_hz,
                               uint32_t sec_div_ns,
                               uint16_t *out_roll,
                               uint32_t width)
{
    DSO_RollLineBuilderState st;

    if ((out_roll == NULL) || (width == 0u) || (sample_rate_hz == 0u) || (sec_div_ns == 0u)) {
        if ((out_roll != NULL) && (width > 0u)) {
            (void)memset(out_roll, 0, sizeof(uint16_t) * width);
        }
        return;
    }

    DSO_RollLineBuilder_Begin(&st, sample_rate_hz, sec_div_ns, width);
    if ((samples != NULL) && (sample_count > 0u)) {
        DSO_RollLineBuilder_Feed(&st, samples, sample_count, out_roll);
    }
    DSO_RollLineBuilder_FinishPad(&st, out_roll);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_RollLineBuilder_NeededSampleCountFull
*   功能说明: 在 Roll 显示链路中完成滚动列构建、物理参数计算、历史缓冲或重同步处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_RollLineBuilder_NeededSampleCountFull(float time_per_pixel_ns,
                                                  uint32_t sample_rate_hz,
                                                  uint32_t display_width)
{
    float tpp_s;
    float spp;
    uint32_t need;

    if ((time_per_pixel_ns <= 0.0f) || (sample_rate_hz == 0u) || (display_width == 0u)) {
        return 0u;
    }

    tpp_s = time_per_pixel_ns * 1e-9f;
    spp = (float)sample_rate_hz * tpp_s;
    if (spp < 1.0f) {
        spp = 1.0f;
    }

    need = (uint32_t)ceilf(spp * (float)display_width) + 128u;
    if (need > DSO_ROLL_NEED_CAP) {
        need = DSO_ROLL_NEED_CAP;
    }
    return need;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_RollLineBuilder_NeededSampleCount
*   功能说明: 在 Roll 显示链路中完成滚动列构建、物理参数计算、历史缓冲或重同步处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_RollLineBuilder_NeededSampleCount(float time_per_pixel_ns,
                                               uint32_t sample_rate_hz,
                                               uint32_t display_width,
                                               uint32_t pull_max)
{
    uint32_t full;

    if (pull_max == 0u) {
        return 0u;
    }

    full = DSO_RollLineBuilder_NeededSampleCountFull(time_per_pixel_ns,
                                                     sample_rate_hz,
                                                     display_width);
    if (full > pull_max) {
        return pull_max;
    }
    return full;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_RollLineBuilder_CappedNeedSamples
*   功能说明: 在 Roll 显示链路中完成滚动列构建、物理参数计算、历史缓冲或重同步处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_RollLineBuilder_CappedNeedSamples(float time_per_pixel_ns,
                                              uint32_t sample_rate_hz,
                                              uint32_t sec_div_ns,
                                              uint32_t display_width,
                                              uint32_t ring_depth,
                                              uint32_t pull_max)
{
    uint32_t full;
    uint32_t cap_by_slow;
    float spp;
    uint32_t need;

    if ((pull_max == 0u) || (ring_depth == 0u)) {
        return 0u;
    }

    full = DSO_RollLineBuilder_NeededSampleCountFull(time_per_pixel_ns,
                                                     sample_rate_hz,
                                                     display_width);
    if (full == 0u) {
        return 0u;
    }

    cap_by_slow = full;
    if ((sec_div_ns >= DSO_ROLL_SLOW_SEC_DIV_NS) && (display_width > 0u) && (sample_rate_hz > 0u)) {
        spp = DSO_RollPhysics_SamplesPerPixel(sec_div_ns, sample_rate_hz, display_width);
        cap_by_slow = (uint32_t)ceilf(spp * (float)display_width) * DSO_ROLL_SEED_MAX_SCREENS + 128u;
        if (cap_by_slow > DSO_ROLL_NEED_CAP) {
            cap_by_slow = DSO_ROLL_NEED_CAP;
        }
        if (full > cap_by_slow) {
            full = cap_by_slow;
        }
        {
            uint32_t min_row;

            min_row = (uint32_t)ceilf(spp * (float)display_width);
            if (min_row < 1u) {
                min_row = 1u;
            }
            if (full < min_row) {
                full = min_row;
                if (full > DSO_ROLL_NEED_CAP) {
                    full = DSO_ROLL_NEED_CAP;
                }
            }
        }
    }

    need = full;
    if (need > ring_depth) {
        need = ring_depth;
    }
    if (need > pull_max) {
        need = pull_max;
    }
    return need;
}
