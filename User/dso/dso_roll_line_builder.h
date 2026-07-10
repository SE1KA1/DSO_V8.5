/**
 * @file dso_roll_line_builder.h
 * @brief 从 RollHistory ring 构造 decimated 波形线。
 */

#ifndef DSO_ROLL_LINE_BUILDER_H
/* 宏定义: DSO_ROLL_LINE_BUILDER_H - 头文件保护宏。 */
#define DSO_ROLL_LINE_BUILDER_H

#include <stdint.h>

/* Must match DSO_TIMEBASE grid column divisor in dso_timebase.c RollRender. */
#define DSO_ROLL_LINE_GRID_COL_DIV 10u

/**
 * Slow Roll: cap seed pull to at most this many full-screen boxcar lengths (time axis).
 * If [DSO] roll ring: nc1/nc2 stay small on slow sec/div while rd/ring_cap are ample,
 * need may be limited by this cap (see CappedNeedSamples) — increase cautiously (CPU/bandwidth).
 */
#define DSO_ROLL_SEED_MAX_SCREENS    3u
/** When sec/div >= this (ns), apply slow-roll seed length cap (e.g. 100 ms/div). */
#define DSO_ROLL_SLOW_SEC_DIV_NS     100000000u

typedef struct {
    float acc_sum;
    float acc_count;
    uint32_t wix;
    uint16_t lastv;
    float samples_per_pixel;
    uint32_t width;
} DSO_RollLineBuilderState;

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
                               uint32_t width);

/** Feed samples in chronological order; may be called multiple times between Begin and FinishPad. */
void DSO_RollLineBuilder_Feed(DSO_RollLineBuilderState *st,
                              const uint16_t *samples,
                              uint32_t sample_count,
                              uint16_t *out_roll);

/** Pad remaining columns with lastv (legacy / tests). */
void DSO_RollLineBuilder_FinishPad(DSO_RollLineBuilderState *st, uint16_t *out_roll);

/** Do not pad; valid decimated columns are out_roll[0 .. wix-1] only. */
void DSO_RollLineBuilder_FinishNoPad(const DSO_RollLineBuilderState *st);

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
                                         float *out_acc_count);

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
                               uint32_t width);

/** Theoretical sample count for full window (not clamped to pull buffer). */
uint32_t DSO_RollLineBuilder_NeededSampleCountFull(float time_per_pixel_ns,
                                                   uint32_t sample_rate_hz,
                                                   uint32_t display_width);

/** Samples to pull from ring: min(Full(...), optional slow cap, ring_depth, pull_max). */
uint32_t DSO_RollLineBuilder_NeededSampleCount(float time_per_pixel_ns,
                                               uint32_t sample_rate_hz,
                                               uint32_t display_width,
                                               uint32_t pull_max);

/** Same as NeededSampleCount but applies slow-roll time-window cap using sec_div_ns. */
uint32_t DSO_RollLineBuilder_CappedNeedSamples(float time_per_pixel_ns,
                                              uint32_t sample_rate_hz,
                                              uint32_t sec_div_ns,
                                              uint32_t display_width,
                                              uint32_t ring_depth,
                                              uint32_t pull_max);

#endif /* DSO_ROLL_LINE_BUILDER_H */
