/**
 * @file dso_timebase.c
 * @brief 时基 Apply、RenderEx、RollRender 实现。
 */

#include "dso_timebase.h"
#include "dso_acq.h"
#include "dso_10bit_display_sync.h"
#include "dso_hs8_render.h"
#include "dso_hs10_render.h"
#include "dso_roll_physics.h"
#include "dso_sample_spec.h"

#include <math.h>
#include <string.h>

/* 宏定义: DSO_RAM_D1 - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_RAM_D1 __attribute__((section(".RAM_D1")))

/* 宏定义: DSO_TIMEBASE_COUNT - 时间间隔、周期或时基相关配置常量。 */
#define DSO_TIMEBASE_COUNT      22u
/* 宏定义: DSO_GRID_COL_DIV - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_GRID_COL_DIV        10u
/* 宏定义: DSO_VIEW_SAMPLES_TARGET - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_VIEW_SAMPLES_TARGET 18432u
/* 宏定义: DSO_TIMEBASE_8BIT_REP_MIN - 时间间隔、周期或时基相关配置常量。 */
#define DSO_TIMEBASE_8BIT_REP_MIN 0.50f
/* 宏定义: DSO_TIMEBASE_DEFAULT_REP_MIN - 时间间隔、周期或时基相关配置常量。 */
#define DSO_TIMEBASE_DEFAULT_REP_MIN 0.70f
/* 宏定义: DSO_TIMEBASE_HS8_MAX_SPC - 时间间隔、周期或时基相关配置常量。 */
#define DSO_TIMEBASE_HS8_MAX_SPC 8.0f
/* 宏定义: DSO_TIMEBASE_HS10_MAX_SPC - 时间间隔、周期或时基相关配置常量。 */
#define DSO_TIMEBASE_HS10_MAX_SPC 6.5f
/* 宏定义: DSO_TIMEBASE_ETS_MAX_SPC - 时间间隔、周期或时基相关配置常量。 */
#define DSO_TIMEBASE_ETS_MAX_SPC 4.5f

typedef struct {
    float ets_bin_sum[DSO_ETS_PHASE_BINS];
    uint16_t ets_bin_count[DSO_ETS_PHASE_BINS];
    float ets_locked_freq;
    bool ets_ready;
} DSO_TimebaseETSState;

typedef struct {
    uint8_t active_index;
    uint8_t pending_index;
    bool pending_change;
    bool hs8_cal_ready[2];
    DSO_TimebaseETSState ets[2];
    DSO_HS8RenderState hs8[2];
    DSO_HS10RenderState hs10[2];
    uint16_t display_buf[2][DSO_DISPLAY_WIDTH];
    uint16_t display_buf_hi[2][DSO_DISPLAY_WIDTH];
    uint16_t roll_buf[2][DSO_DISPLAY_WIDTH];
    uint32_t roll_fill[2];
    float roll_accum_sum[2];
    float roll_accum_count[2];
    uint16_t roll_plot_valid[2];
} DSO_TimebaseState;

static const DSO_TimebaseEntry s_table_10bit[DSO_TIMEBASE_COUNT] = {
    {  0u,        100u, TB_MODE_FAST,   42u, 9302325u },
    {  1u,        200u, TB_MODE_FAST,   42u, 9302325u },
    {  2u,        500u, TB_MODE_FAST,   42u, 9302325u },
    {  3u,       1000u, TB_MODE_FAST,   42u, 9302325u },
    {  4u,       2000u, TB_MODE_FAST,   42u, 9302325u },
    {  5u,       5000u, TB_MODE_FAST,   42u, 9302325u },
    {  6u,      10000u, TB_MODE_NORMAL, 42u, 9302325u },
    {  7u,      20000u, TB_MODE_NORMAL, 42u, 9302325u },
    {  8u,      50000u, TB_MODE_NORMAL, 42u, 9302325u },
    {  9u,     100000u, TB_MODE_NORMAL, 42u, 9302325u },
    { 10u,     200000u, TB_MODE_NORMAL, 42u, 9302325u },
    { 11u,     500000u, TB_MODE_NORMAL, 108u, 3669724u },
    { 12u,    1000000u, TB_MODE_NORMAL, 216u, 1843317u },
    { 13u,    2000000u, TB_MODE_NORMAL, 433u, 921658u  },
    { 14u,    5000000u, TB_MODE_NORMAL, 1084u, 368664u },
    { 15u,   10000000u, TB_MODE_NORMAL, 2169u, 184332u },
    { 16u,   20000000u, TB_MODE_ROLL,   19999u, 20000u },
    { 17u,   50000000u, TB_MODE_ROLL,   19999u, 20000u },
    { 18u,  100000000u, TB_MODE_ROLL,   19999u, 20000u },
    { 19u,  200000000u, TB_MODE_ROLL,   19999u, 20000u },
    { 20u,  500000000u, TB_MODE_ROLL,   19999u, 20000u },
    { 21u, 1000000000u, TB_MODE_ROLL,   19999u, 20000u },
};

/*
 * Tri-lane 8-bit capture is the only 8-bit product path. The fastest entries
 * use the V2 uniform timing target: TIM1 ARR=17, period=18, CC1/2/3 at
 * 3/9/15 ticks, 11.111 MSPS per ADC and 33.333 MSPS after merging.
 */
static const DSO_TimebaseEntry s_table_8bit_tri[DSO_TIMEBASE_COUNT] = {
    {  0u,        100u, TB_MODE_FAST,      DSO_ACQ8_TRI_INTERLEAVED_TIM_ARR, DSO_ACQ8_TRI_INTERLEAVED_SAMPLE_RATE_HZ },
    {  1u,        200u, TB_MODE_FAST,      DSO_ACQ8_TRI_INTERLEAVED_TIM_ARR, DSO_ACQ8_TRI_INTERLEAVED_SAMPLE_RATE_HZ },
    {  2u,        500u, TB_MODE_FAST,      DSO_ACQ8_TRI_INTERLEAVED_TIM_ARR, DSO_ACQ8_TRI_INTERLEAVED_SAMPLE_RATE_HZ },
    {  3u,       1000u, TB_MODE_FAST,      DSO_ACQ8_TRI_INTERLEAVED_TIM_ARR, DSO_ACQ8_TRI_INTERLEAVED_SAMPLE_RATE_HZ },
    {  4u,       2000u, TB_MODE_FAST,      DSO_ACQ8_TRI_INTERLEAVED_TIM_ARR, DSO_ACQ8_TRI_INTERLEAVED_SAMPLE_RATE_HZ },
    {  5u,       5000u, TB_MODE_FAST,      DSO_ACQ8_TRI_INTERLEAVED_TIM_ARR, DSO_ACQ8_TRI_INTERLEAVED_SAMPLE_RATE_HZ },
    {  6u,      10000u, TB_MODE_NORMAL,    DSO_ACQ8_TRI_INTERLEAVED_TIM_ARR, DSO_ACQ8_TRI_INTERLEAVED_SAMPLE_RATE_HZ },
    {  7u,      20000u, TB_MODE_NORMAL,    DSO_ACQ8_TRI_INTERLEAVED_TIM_ARR, DSO_ACQ8_TRI_INTERLEAVED_SAMPLE_RATE_HZ },
    {  8u,      50000u, TB_MODE_NORMAL,    DSO_ACQ8_TRI_INTERLEAVED_TIM_ARR, DSO_ACQ8_TRI_INTERLEAVED_SAMPLE_RATE_HZ },
    {  9u,     100000u, TB_MODE_NORMAL,    DSO_ACQ8_TRI_INTERLEAVED_TIM_ARR, DSO_ACQ8_TRI_INTERLEAVED_SAMPLE_RATE_HZ },
    { 10u,     200000u, TB_MODE_NORMAL,    DSO_ACQ8_TRI_INTERLEAVED_TIM_ARR, DSO_ACQ8_TRI_INTERLEAVED_SAMPLE_RATE_HZ },
    { 11u,     500000u, TB_MODE_NORMAL,    71u,  8333331u },
    { 12u,    1000000u, TB_MODE_NORMAL,   143u,  4166664u },
    { 13u,    2000000u, TB_MODE_NORMAL,   287u,  2083332u },
    { 14u,    5000000u, TB_MODE_NORMAL,   719u,   833331u },
    { 15u,   10000000u, TB_MODE_NORMAL,  1439u,   416664u },
    { 16u,   20000000u, TB_MODE_ROLL,   29999u,    19998u },
    { 17u,   50000000u, TB_MODE_ROLL,   29999u,    19998u },
    { 18u,  100000000u, TB_MODE_ROLL,   29999u,    19998u },
    { 19u,  200000000u, TB_MODE_ROLL,   29999u,    19998u },
    { 20u,  500000000u, TB_MODE_ROLL,   29999u,    19998u },
    { 21u, 1000000000u, TB_MODE_ROLL,   29999u,    19998u },
};

static DSO_TimebaseState s_tb DSO_RAM_D1;

/*
*********************************************************************************************************
*   函 数 名: active_8bit_table
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static const DSO_TimebaseEntry *active_8bit_table(void)
{
    return s_table_8bit_tri;
}

/*
*********************************************************************************************************
*   函 数 名: active_table
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static const DSO_TimebaseEntry *active_table(void)
{
    return (DSO_SampleSpec_GetActiveFormat() == DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED)
         ? active_8bit_table()
         : s_table_10bit;
}

/*
*********************************************************************************************************
*   函 数 名: active_lane_count_from_spec
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t active_lane_count_from_spec(const DSO_SampleSpec *spec)
{
    if (DSO_SampleSpec_GetActiveFormat() == DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED) {
        uint8_t lane_count = DSO_Acq_GetLaneCount();
        return (lane_count == 0u) ? 1u : lane_count;
    }

    return (spec != NULL && spec->lane_count != 0u) ? spec->lane_count : 1u;
}

/*
*********************************************************************************************************
*   函 数 名: active_sample_rate_from_entry
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t active_sample_rate_from_entry(const DSO_TimebaseEntry *entry,
                                              const DSO_SampleSpec *spec)
{
    const DSO_AcqRuntimeInfo *runtime;
    uint32_t tim_clk_hz;
    uint32_t period_clocks;
    uint32_t trigger_rate_hz;
    uint8_t lane_count;

    if (entry == NULL) {
        return 0u;
    }

    if (DSO_SampleSpec_GetActiveFormat() != DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED) {
        return entry->hw_sample_rate;
    }

    runtime = DSO_Acq_GetRuntimeInfo();
    if ((runtime != NULL) &&
        (runtime->tim8_arr == entry->tim8_arr) &&
        (runtime->sample_rate_hz != 0u)) {
        return runtime->sample_rate_hz;
    }

    tim_clk_hz = DSO_Acq_GetTimClkHz();
    period_clocks = entry->tim8_arr + 1u;
    lane_count = active_lane_count_from_spec(spec);

    if ((tim_clk_hz == 0u) || (period_clocks == 0u)) {
        return entry->hw_sample_rate;
    }

    trigger_rate_hz = tim_clk_hz / period_clocks;
    return trigger_rate_hz * (uint32_t)lane_count;
}

/*
*********************************************************************************************************
*   函 数 名: active_repetitive_confidence_min
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static float active_repetitive_confidence_min(void)
{
    return (DSO_SampleSpec_GetActiveFormat() == DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED)
         ? DSO_TIMEBASE_8BIT_REP_MIN
         : DSO_TIMEBASE_DEFAULT_REP_MIN;
}

/*
*********************************************************************************************************
*   函 数 名: clamp_channel
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t clamp_channel(uint8_t channel)
{
    return (channel != 0u) ? 1u : 0u;
}

/*
*********************************************************************************************************
*   函 数 名: time_per_pixel_ns_from_entry
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static float time_per_pixel_ns_from_entry(const DSO_TimebaseEntry *entry)
{
    return ((float)entry->sec_div_ns * (float)DSO_GRID_COL_DIV) / (float)DSO_DISPLAY_WIDTH;
}

/*
*********************************************************************************************************
*   函 数 名: render_linear_upsample
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void render_linear_upsample(const uint16_t *src, uint32_t count, uint16_t *dst)
{
    const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();

    if (count == 0u) {
        memset(dst, 0, sizeof(uint16_t) * DSO_DISPLAY_WIDTH);
        return;
    }

    if (count == 1u) {
        for (uint32_t i = 0; i < DSO_DISPLAY_WIDTH; i++) {
            dst[i] = src[0];
        }
        return;
    }

    for (uint32_t i = 0; i < DSO_DISPLAY_WIDTH; i++) {
        float pos = ((float)i * (float)(count - 1u)) / (float)(DSO_DISPLAY_WIDTH - 1u);
        uint32_t idx0 = (uint32_t)pos;
        uint32_t idx1 = (idx0 + 1u < count) ? (idx0 + 1u) : idx0;
        float frac = pos - (float)idx0;
        float v = ((1.0f - frac) * (float)src[idx0]) + (frac * (float)src[idx1]);
        if (v < 0.0f) v = 0.0f;
        if (v > (float)spec->adc_max) v = (float)spec->adc_max;
        dst[i] = (uint16_t)(v + 0.5f);
    }
}

/*
*********************************************************************************************************
*   函 数 名: render_minmax_lo_hi
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void render_minmax_lo_hi(const uint16_t *src, uint32_t count, uint16_t *dst_lo, uint16_t *dst_hi)
{
    for (uint32_t px = 0; px < DSO_DISPLAY_WIDTH; px++) {
        uint32_t seg_s = (px * count) / DSO_DISPLAY_WIDTH;
        uint32_t seg_e = ((px + 1u) * count) / DSO_DISPLAY_WIDTH;
        uint16_t lo;
        uint16_t hi;

        if (seg_e <= seg_s) {
            seg_e = seg_s + 1u;
            if (seg_e > count) seg_e = count;
            if (seg_s >= count) seg_s = count - 1u;
        }

        lo = src[seg_s];
        hi = src[seg_s];
        for (uint32_t j = seg_s + 1u; j < seg_e; j++) {
            if (src[j] < lo) {
                lo = src[j];
            }
            if (src[j] > hi) {
                hi = src[j];
            }
        }

        dst_lo[px] = lo;
        dst_hi[px] = hi;
    }
}

/*
*********************************************************************************************************
*   函 数 名: render_sample_downsample
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void render_sample_downsample(const uint16_t *src, uint32_t count, uint16_t *dst)
{
    for (uint32_t px = 0; px < DSO_DISPLAY_WIDTH; px++) {
        uint32_t seg_s = (px * count) / DSO_DISPLAY_WIDTH;
        uint32_t seg_e = ((px + 1u) * count) / DSO_DISPLAY_WIDTH;
        uint32_t idx;

        if (seg_e <= seg_s) {
            seg_e = seg_s + 1u;
            if (seg_e > count) seg_e = count;
            if (seg_s >= count) seg_s = count - 1u;
        }

        idx = seg_s + ((seg_e - seg_s) / 2u);
        if (idx >= count) {
            idx = count - 1u;
        }
        dst[px] = src[idx];
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_HiResBoxcar
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Timebase_HiResBoxcar(const uint16_t *samples,
                                  uint32_t sample_count,
                                  uint32_t sample_rate_hz,
                                  uint16_t *out_samples,
                                  uint32_t out_capacity,
                                  uint32_t *out_effective_rate_hz)
{
    uint32_t bucket;
    uint32_t out_count = 0u;
    const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();

    if (out_effective_rate_hz != NULL) {
        *out_effective_rate_hz = sample_rate_hz;
    }
    if ((samples == NULL) || (out_samples == NULL) || (sample_count == 0u) || (out_capacity == 0u)) {
        return 0u;
    }

    bucket = sample_count / DSO_DISPLAY_WIDTH;
    if (bucket < 2u) {
        out_count = (sample_count < out_capacity) ? sample_count : out_capacity;
        memcpy(out_samples, samples, sizeof(uint16_t) * out_count);
        return out_count;
    }

    if (out_effective_rate_hz != NULL) {
        *out_effective_rate_hz = sample_rate_hz / bucket;
    }

    for (uint32_t start = 0u; (start < sample_count) && (out_count < out_capacity); start += bucket) {
        uint32_t end = start + bucket;
        uint32_t span;
        uint32_t sum = 0u;
        uint32_t avg;

        if (end > sample_count) {
            end = sample_count;
        }
        span = end - start;
        if (span == 0u) {
            break;
        }

        for (uint32_t i = start; i < end; ++i) {
            sum += (uint32_t)samples[i];
        }

        avg = (sum + (span / 2u)) / span;
        if (avg > spec->adc_max) {
            avg = spec->adc_max;
        }
        out_samples[out_count++] = (uint16_t)avg;
    }

    return out_count;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_SetHs8CalReadyForChannel
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Timebase_SetHs8CalReadyForChannel(uint8_t channel, bool ready)
{
    s_tb.hs8_cal_ready[clamp_channel(channel)] = ready;
}

/*
*********************************************************************************************************
*   函 数 名: render_direct_window_to_dst
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void render_direct_window_to_dst(const uint16_t *samples,
                                        uint32_t sample_count,
                                        uint32_t window_start,
                                        uint32_t window_count,
                                        DSO_AcquireMode acquire_mode,
                                        uint16_t *dst,
                                        uint16_t *dst_hi,
                                        bool *column_envelope_out)
{
    bool env = false;

    if ((samples == NULL) || (sample_count == 0u) || (dst == NULL)) {
        if (dst != NULL) {
            memset(dst, 0, sizeof(uint16_t) * DSO_DISPLAY_WIDTH);
        }
        if ((dst_hi != NULL) && (dst != NULL)) {
            memset(dst_hi, 0, sizeof(uint16_t) * DSO_DISPLAY_WIDTH);
        }
        if (column_envelope_out != NULL) {
            *column_envelope_out = false;
        }
        return;
    }

    if (dst_hi == NULL) {
        dst_hi = dst;
    }

    if (window_start >= sample_count) {
        window_start = sample_count - 1u;
    }
    if (window_count == 0u) {
        window_count = 1u;
    }
    if ((window_start + window_count) > sample_count) {
        window_count = sample_count - window_start;
    }

    if (window_count <= DSO_DISPLAY_WIDTH) {
        render_linear_upsample(&samples[window_start], window_count, dst);
        (void)memcpy(dst_hi, dst, sizeof(uint16_t) * DSO_DISPLAY_WIDTH);
        env = false;
    } else if ((acquire_mode == DSO_ACQUIRE_MODE_PEAK_DETECT) ||
               (window_count >= (uint32_t)(DSO_DISPLAY_WIDTH * 4u))) {
        /* Whenever a pixel spans >=4 samples, mid-sample downsampling aliases
         * high-frequency content badly. Fall back to min/max envelope even in
         * SAMPLE mode — this matches how every modern DSO handles dense views. */
        render_minmax_lo_hi(&samples[window_start], window_count, dst, dst_hi);
        env = true;
    } else {
        render_sample_downsample(&samples[window_start], window_count, dst);
        (void)memcpy(dst_hi, dst, sizeof(uint16_t) * DSO_DISPLAY_WIDTH);
        env = false;
    }

    if (column_envelope_out != NULL) {
        *column_envelope_out = env;
    }
}

/*
*********************************************************************************************************
*   函 数 名: ets_reset_channel
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ets_reset_channel(uint8_t channel)
{
    DSO_TimebaseETSState *ets = &s_tb.ets[clamp_channel(channel)];

    memset(ets->ets_bin_sum, 0, sizeof(ets->ets_bin_sum));
    memset(ets->ets_bin_count, 0, sizeof(ets->ets_bin_count));
    ets->ets_locked_freq = 0.0f;
    ets->ets_ready = false;
}

/*
*********************************************************************************************************
*   函 数 名: ets_reset_all
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ets_reset_all(void)
{
    ets_reset_channel(0u);
    ets_reset_channel(1u);
}

/*
*********************************************************************************************************
*   函 数 名: ets_freq_matches
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool ets_freq_matches(const DSO_TimebaseETSState *ets, float freq_hz)
{
    float diff;

    if ((ets == NULL) || (freq_hz <= 0.0f) || (ets->ets_locked_freq <= 0.0f)) {
        return true;
    }

    diff = fabsf(freq_hz - ets->ets_locked_freq) / ets->ets_locked_freq;
    return diff <= DSO_ETS_FREQ_RESET_TOL;
}

/*
*********************************************************************************************************
*   函 数 名: hs8_reset_channel
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void hs8_reset_channel(uint8_t channel)
{
    DSO_HS8Render_Reset(&s_tb.hs8[clamp_channel(channel)]);
}

/*
*********************************************************************************************************
*   函 数 名: hs8_reset_all
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void hs8_reset_all(void)
{
    hs8_reset_channel(0u);
    hs8_reset_channel(1u);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_ResetHs8ReconstructionForChannel
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Timebase_ResetHs8ReconstructionForChannel(uint8_t channel)
{
    uint8_t idx = clamp_channel(channel);

    DSO_HS8Render_Reset(&s_tb.hs8[idx]);
    ets_reset_channel(idx);
}

/*
*********************************************************************************************************
*   函 数 名: hs10_reset_channel
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void hs10_reset_channel(uint8_t channel)
{
    DSO_HS10Render_Reset(&s_tb.hs10[clamp_channel(channel)]);
}

/*
*********************************************************************************************************
*   函 数 名: hs10_reset_all
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void hs10_reset_all(void)
{
    hs10_reset_channel(0u);
    hs10_reset_channel(1u);
}

/*
*********************************************************************************************************
*   函 数 名: ets_accumulate
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ets_accumulate(DSO_TimebaseETSState *ets,
                           const uint16_t *samples,
                           uint32_t sample_count,
                           uint32_t sample_rate_hz,
                           float freq_hz,
                           float trig_pos_samples)
{
    if ((samples == NULL) || (sample_count == 0u) || (sample_rate_hz == 0u) ||
        (freq_hz <= 0.0f) || (trig_pos_samples < 0.0f) || (trig_pos_samples >= (float)sample_count)) {
        return;
    }

    if (ets->ets_locked_freq <= 0.0f) {
        ets->ets_locked_freq = freq_hz;
    } else {
        float diff = fabsf(freq_hz - ets->ets_locked_freq) / ets->ets_locked_freq;
        if (diff > DSO_ETS_FREQ_RESET_TOL) {
            uint8_t channel = (uint8_t)(ets - &s_tb.ets[0]);
            ets_reset_channel(channel);
            ets->ets_locked_freq = freq_hz;
        } else {
            ets->ets_locked_freq = freq_hz;
        }
    }

    for (uint32_t i = 0; i < sample_count; i++) {
        float dt = ((float)i - trig_pos_samples) / (float)sample_rate_hz;
        float phase = dt * ets->ets_locked_freq;
        phase -= floorf(phase);
        if (phase < 0.0f) phase += 1.0f;

        {
            uint32_t bin = (uint32_t)(phase * (float)DSO_ETS_PHASE_BINS);
            if (bin >= DSO_ETS_PHASE_BINS) bin = DSO_ETS_PHASE_BINS - 1u;
            ets->ets_bin_sum[bin] += (float)samples[i];
            if (ets->ets_bin_count[bin] < UINT16_MAX) {
                ets->ets_bin_count[bin]++;
            }
        }
    }

    {
        uint32_t filled = 0u;
        for (uint32_t i = 0; i < DSO_ETS_PHASE_BINS; i++) {
            if (ets->ets_bin_count[i] != 0u) filled++;
        }
        ets->ets_ready = ((float)filled / (float)DSO_ETS_PHASE_BINS) >= DSO_ETS_COVERAGE_READY;
    }
}

/*
*********************************************************************************************************
*   函 数 名: ets_render_to_display
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool ets_render_to_display(DSO_TimebaseETSState *ets,
                                  float sec_div_ns,
                                  uint16_t *out_display)
{
    float cycle[DSO_ETS_PHASE_BINS];
    float time_per_pixel_ns;
    float width_mid;

    if ((out_display == NULL) || !ets->ets_ready || (ets->ets_locked_freq <= 0.0f)) {
        return false;
    }

    for (uint32_t i = 0; i < DSO_ETS_PHASE_BINS; i++) {
        if (ets->ets_bin_count[i] != 0u) {
            cycle[i] = ets->ets_bin_sum[i] / (float)ets->ets_bin_count[i];
        } else {
            cycle[i] = (i == 0u) ? (float)DSO_SampleSpec_GetActive()->midscale : cycle[i - 1u];
        }
    }

    time_per_pixel_ns = (sec_div_ns * (float)DSO_GRID_COL_DIV) / (float)DSO_DISPLAY_WIDTH;
    width_mid = ((float)DSO_DISPLAY_WIDTH - 1.0f) * 0.5f;

    for (uint32_t px = 0; px < DSO_DISPLAY_WIDTH; px++) {
        float rel_t_ns = ((float)px - width_mid) * time_per_pixel_ns;
        float phase = (rel_t_ns * 1e-9f) * ets->ets_locked_freq;
        phase -= floorf(phase);
        if (phase < 0.0f) phase += 1.0f;

        {
            float pos = phase * (float)DSO_ETS_PHASE_BINS;
            uint32_t idx0 = (uint32_t)pos % DSO_ETS_PHASE_BINS;
            uint32_t idx1 = (idx0 + 1u) % DSO_ETS_PHASE_BINS;
            float frac = pos - floorf(pos);
            float v = ((1.0f - frac) * cycle[idx0]) + (frac * cycle[idx1]);

            if (v < 0.0f) v = 0.0f;
            if (v > (float)DSO_SampleSpec_GetActive()->adc_max) {
                v = (float)DSO_SampleSpec_GetActive()->adc_max;
            }
            out_display[px] = (uint16_t)(v + 0.5f);
        }
    }

    return true;
}

/*
*********************************************************************************************************
*   函 数 名: render_meta_reset
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void render_meta_reset(DSO_RenderMeta *meta, uint32_t sample_rate_hz)
{
    if (meta == NULL) {
        return;
    }

    memset(meta, 0, sizeof(*meta));
    meta->recon_mode = DSO_RECON_MODE_RAW_LINEAR;
    meta->display_quality = DSO_DISPLAY_QUALITY_GOOD;
    meta->effective_sample_rate_hz = (float)sample_rate_hz;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_Init
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Timebase_Init(void)
{
    memset(&s_tb, 0, sizeof(s_tb));
    s_tb.active_index = 12u;
    s_tb.pending_index = 12u;
    s_tb.hs8_cal_ready[0] = true;
    s_tb.hs8_cal_ready[1] = true;
    s_tb.roll_plot_valid[0] = (uint16_t)DSO_DISPLAY_WIDTH;
    s_tb.roll_plot_valid[1] = (uint16_t)DSO_DISPLAY_WIDTH;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_GetCount
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t DSO_Timebase_GetCount(void)
{
    return (uint8_t)DSO_TIMEBASE_COUNT;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_GetTable
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const DSO_TimebaseEntry *DSO_Timebase_GetTable(void)
{
    return active_table();
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_GetEntryAt
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const DSO_TimebaseEntry *DSO_Timebase_GetEntryAt(uint8_t idx)
{
    if (idx >= DSO_TIMEBASE_COUNT) {
        return NULL;
    }
    return &active_table()[idx];
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_SetIndex
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Timebase_SetIndex(uint8_t idx)
{
    if (idx >= DSO_TIMEBASE_COUNT) {
        return;
    }
    /* Encoder at min/max still calls SetIndex with clamped value: avoid queuing a no-op
     * apply (would run ets/hs8 reset + roll resync every frame and stutter, e.g. 1 s/div). */
    if (idx == s_tb.active_index) {
        s_tb.pending_change = false;
        return;
    }
    s_tb.pending_index = idx;
    s_tb.pending_change = true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_ApplyPending
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_Timebase_ApplyPending(void)
{
    uint8_t pending;

    if (!s_tb.pending_change) {
        return false;
    }

    pending = s_tb.pending_index;
    if (pending == s_tb.active_index) {
        s_tb.pending_change = false;
        return false;
    }

    s_tb.active_index = pending;
    s_tb.pending_change = false;
    /* Roll does not use ETS/HS8; skipping resets avoids useless work on every Roll step. */
    if (active_table()[s_tb.active_index].mode != TB_MODE_FAST &&
        active_table()[s_tb.active_index].mode != TB_MODE_ROLL) {
        ets_reset_all();
        hs8_reset_all();
        hs10_reset_all();
    }
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_GetMode
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_TimebaseMode DSO_Timebase_GetMode(void)
{
    return active_table()[s_tb.active_index].mode;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_GetInfo
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Timebase_GetInfo(DSO_TimebaseInfo *out)
{
    const DSO_TimebaseEntry *entry;
    const DSO_SampleSpec *spec;

    if (out == NULL) {
        return;
    }

    entry = &active_table()[s_tb.active_index];
    spec = DSO_SampleSpec_GetActive();
    out->index = entry->index;
    out->sec_div_ns = entry->sec_div_ns;
    out->mode = entry->mode;
    out->hw_sample_rate = active_sample_rate_from_entry(entry, spec);
    out->time_per_pixel_ns = time_per_pixel_ns_from_entry(entry);
    out->roll_active = (entry->mode == TB_MODE_ROLL);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_GetIndex
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t DSO_Timebase_GetIndex(void)
{
    return s_tb.active_index;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_GetTimArr
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Timebase_GetTimArr(void)
{
    return active_table()[s_tb.active_index].tim8_arr;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_GetSampleRateHz
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Timebase_GetSampleRateHz(void)
{
    const DSO_TimebaseEntry *entry = &active_table()[s_tb.active_index];
    const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();

    return active_sample_rate_from_entry(entry, spec);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_GetWindowSampleCount
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Timebase_GetWindowSampleCount(uint32_t sample_rate_hz)
{
    const DSO_TimebaseEntry *entry = &active_table()[s_tb.active_index];
    uint32_t window_samples;

    if (sample_rate_hz == 0u) {
        return 0u;
    }

    window_samples = (uint32_t)llround(((double)sample_rate_hz *
                                        ((double)entry->sec_div_ns * (double)DSO_GRID_COL_DIV * 1e-9)));
    if (window_samples == 0u) {
        window_samples = 1u;
    }
    return window_samples;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_ResetRollColumnState
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Timebase_ResetRollColumnState(void)
{
    uint32_t i;

    memset(s_tb.roll_buf, 0, sizeof(s_tb.roll_buf));
    memset(s_tb.roll_fill, 0, sizeof(s_tb.roll_fill));
    memset(s_tb.roll_accum_sum, 0, sizeof(s_tb.roll_accum_sum));
    memset(s_tb.roll_accum_count, 0, sizeof(s_tb.roll_accum_count));
    for (i = 0u; i < 2u; i++) {
        s_tb.roll_plot_valid[i] = (uint16_t)DSO_DISPLAY_WIDTH;
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_ResetRollColumnStateForChannel
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Timebase_ResetRollColumnStateForChannel(uint8_t channel)
{
    uint8_t idx = clamp_channel(channel);

    (void)memset(s_tb.roll_buf[idx], 0, sizeof(s_tb.roll_buf[idx]));
    s_tb.roll_fill[idx] = 0u;
    s_tb.roll_accum_sum[idx] = 0.0f;
    s_tb.roll_accum_count[idx] = 0.0f;
    s_tb.roll_plot_valid[idx] = (uint16_t)DSO_DISPLAY_WIDTH;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_ResetFastState
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Timebase_ResetFastState(void)
{
    ets_reset_all();
    hs8_reset_all();
    hs10_reset_all();
    DSO_Timebase_ResetRollColumnState();
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_SnapshotRollDisplayCh1
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Timebase_SnapshotRollDisplayCh1(uint16_t *dst)
{
    if (dst == NULL) {
        return;
    }
    (void)memcpy(dst, s_tb.roll_buf[0u], sizeof(uint16_t) * DSO_DISPLAY_WIDTH);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_SetRollAccumRemainder
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Timebase_SetRollAccumRemainder(uint8_t channel, float acc_sum, float acc_count)
{
    uint8_t idx = clamp_channel(channel);

    s_tb.roll_accum_sum[idx]   = acc_sum;
    s_tb.roll_accum_count[idx] = acc_count;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_SeedRollDecimatedLineEx
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Timebase_SeedRollDecimatedLineEx(uint8_t channel,
                                          const uint16_t *decimated_line,
                                          uint32_t valid_px,
                                          int right_align,
                                          float remainder_sum,
                                          float remainder_count)
{
    uint8_t idx = clamp_channel(channel);
    uint32_t i;
    uint16_t mid;
    const DSO_SampleSpec *spec;

    spec = DSO_SampleSpec_GetActive();
    mid  = spec->midscale;

    if ((decimated_line == NULL) || (valid_px == 0u)) {
        s_tb.roll_fill[idx] = 0u;
        s_tb.roll_plot_valid[idx] = (uint16_t)DSO_DISPLAY_WIDTH;
        DSO_Timebase_SetRollAccumRemainder(idx, remainder_sum, remainder_count);
        return;
    }

    if (valid_px > DSO_DISPLAY_WIDTH) {
        valid_px = DSO_DISPLAY_WIDTH;
    }

    if (right_align != 0) {
        uint32_t left = DSO_DISPLAY_WIDTH - valid_px;

        for (i = 0u; i < left; i++) {
            s_tb.roll_buf[idx][i] = mid;
        }
        (void)memcpy(&s_tb.roll_buf[idx][left], decimated_line, valid_px * sizeof(uint16_t));
        s_tb.roll_plot_valid[idx] = (uint16_t)valid_px;
    } else {
        (void)memcpy(s_tb.roll_buf[idx], decimated_line, valid_px * sizeof(uint16_t));
        for (i = valid_px; i < DSO_DISPLAY_WIDTH; i++) {
            s_tb.roll_buf[idx][i] = mid;
        }
        s_tb.roll_plot_valid[idx] = (uint16_t)DSO_DISPLAY_WIDTH;
    }

    s_tb.roll_fill[idx] = DSO_DISPLAY_WIDTH;
    DSO_Timebase_SetRollAccumRemainder(idx, remainder_sum, remainder_count);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_SeedRollDecimatedLine
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Timebase_SeedRollDecimatedLine(uint8_t channel,
                                        const uint16_t *decimated_line,
                                        uint32_t width)
{
    /* Legacy: full-width line; pad short with last sample (tests / explicit hold). */
    uint8_t idx = clamp_channel(channel);
    uint32_t copy_w;
    uint32_t k;
    uint16_t pad;
    uint16_t scratch[DSO_DISPLAY_WIDTH];

    if ((decimated_line == NULL) || (width == 0u)) {
        s_tb.roll_fill[idx] = 0u;
        DSO_Timebase_SetRollAccumRemainder(idx, 0.0f, 0.0f);
        return;
    }

    copy_w = (width > DSO_DISPLAY_WIDTH) ? DSO_DISPLAY_WIDTH : width;
    pad    = decimated_line[copy_w - 1u];
    (void)memcpy(scratch, decimated_line, copy_w * sizeof(uint16_t));
    for (k = copy_w; k < DSO_DISPLAY_WIDTH; k++) {
        scratch[k] = pad;
    }
    DSO_Timebase_SeedRollDecimatedLineEx(channel, scratch, DSO_DISPLAY_WIDTH, 0, 0.0f, 0.0f);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_GetLastColumnHiForChannel
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const uint16_t *DSO_Timebase_GetLastColumnHiForChannel(uint8_t channel)
{
    uint8_t idx = clamp_channel(channel);

    return s_tb.display_buf_hi[idx];
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_GetRollPlotValidPx
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint16_t DSO_Timebase_GetRollPlotValidPx(uint8_t channel)
{
    uint8_t idx = clamp_channel(channel);

    return s_tb.roll_plot_valid[idx];
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_GetRollFillPx
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Timebase_GetRollFillPx(uint8_t channel)
{
    uint8_t idx = clamp_channel(channel);

    return s_tb.roll_fill[idx];
}

/*
*********************************************************************************************************
*   函 数 名: roll_pad_unfilled_tail
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void roll_pad_unfilled_tail(uint8_t idx)
{
    const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();
    uint16_t mid = spec->midscale;
    uint32_t k;

    for (k = s_tb.roll_fill[idx]; k < DSO_DISPLAY_WIDTH; k++) {
        s_tb.roll_buf[idx][k] = mid;
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_RenderExForChannel
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const uint16_t *DSO_Timebase_RenderExForChannel(uint8_t channel,
                                                const uint16_t *samples,
                                                uint32_t sample_count,
                                                uint32_t sample_rate_hz,
                                                const DSO_ProcessAnalysis *analysis,
                                                uint8_t trig_position_pct,
                                                DSO_AcquireMode acquire_mode,
                                                DSO_RenderMeta *out_meta)
{
    uint8_t idx = clamp_channel(channel);

    DSO_Timebase_RenderToBufferForChannel(idx,
                                          samples,
                                          sample_count,
                                          sample_rate_hz,
                                          analysis,
                                          trig_position_pct,
                                          acquire_mode,
                                          (DSO_DISABLE_ETS_FOR_SWEEP == 0u),
                                          s_tb.display_buf[idx],
                                          s_tb.display_buf_hi[idx],
                                          out_meta);
    return s_tb.display_buf[idx];
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_RenderEx
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const uint16_t *DSO_Timebase_RenderEx(const uint16_t *samples,
                                      uint32_t sample_count,
                                      uint32_t sample_rate_hz,
                                      const DSO_ProcessAnalysis *analysis,
                                      uint8_t trig_position_pct,
                                      DSO_AcquireMode acquire_mode,
                                      DSO_RenderMeta *out_meta)
{
    return DSO_Timebase_RenderExForChannel(0u,
                                           samples,
                                           sample_count,
                                           sample_rate_hz,
                                           analysis,
                                           trig_position_pct,
                                           acquire_mode,
                                           out_meta);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_RenderToBufferForChannel
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Timebase_RenderToBufferForChannel(uint8_t channel,
                                           const uint16_t *samples,
                                           uint32_t sample_count,
                                           uint32_t sample_rate_hz,
                                           const DSO_ProcessAnalysis *analysis,
                                           uint8_t trig_position_pct,
                                           DSO_AcquireMode acquire_mode,
                                           bool allow_ets,
                                           uint16_t *out_display,
                                           uint16_t *out_column_hi,
                                           DSO_RenderMeta *out_meta)
{
    const DSO_TimebaseEntry *entry = &active_table()[s_tb.active_index];
    uint8_t ch_idx = clamp_channel(channel);
    uint16_t *hi_sink = (out_column_hi != NULL) ? out_column_hi : s_tb.display_buf_hi[ch_idx];
    DSO_TimebaseETSState *ets = &s_tb.ets[ch_idx];
    DSO_HS8RenderState *hs8 = &s_tb.hs8[ch_idx];
    DSO_HS10RenderState *hs10 = &s_tb.hs10[ch_idx];
    DSO_RenderPolicyDecision decision = {0};
    DSO_RenderPolicyInput policy = {0};
    const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();
    bool format_is_hs8 = (DSO_SampleSpec_GetActiveFormat() == DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
    bool format_is_10bit = (DSO_SampleSpec_GetActiveFormat() == DSO_SAMPLE_FORMAT_10BIT);
    uint8_t active_lane_count = active_lane_count_from_spec(spec);
    bool hs8_multi_lane = format_is_hs8 && (active_lane_count > 1u);
    bool hs8_cal_ready = s_tb.hs8_cal_ready[ch_idx];
    bool enable_advanced_recon = !format_is_hs8 || hs8_multi_lane;
    bool hs8_candidate = false;
    bool hs10_candidate = false;
    bool ets_candidate = false;
    bool fast_mode;
    bool recon_fast_mode;
    bool sample_mode;
    bool allow_ets_requested;
    bool allow_ets_effective;
    bool rep_gate_ok = false;
    bool hs8_spc_gate_ok = false;
    bool hs10_spc_gate_ok = false;
    bool ets_spc_gate_ok = false;
    float rep_min = active_repetitive_confidence_min();
    uint32_t window_samples;
    uint32_t start;
    int32_t trig_index = (analysis != NULL) ? analysis->trigger_index : -1;
    float trig_pos_samples = (analysis != NULL) ? analysis->trigger_pos_samples : -1.0f;

    if (out_display == NULL) {
        return;
    }

    render_meta_reset(out_meta, sample_rate_hz);

    if ((samples == NULL) || (sample_count == 0u) || (sample_rate_hz == 0u)) {
        memset(out_display, 0, sizeof(uint16_t) * DSO_DISPLAY_WIDTH);
        memset(hi_sink, 0, sizeof(uint16_t) * DSO_DISPLAY_WIDTH);
        return;
    }

    if (!hs8_multi_lane) {
        DSO_HS8Render_Reset(hs8);
    }
    if (!format_is_10bit) {
        DSO_HS10Render_Reset(hs10);
    }

    fast_mode = (entry->mode == TB_MODE_FAST);
    recon_fast_mode = fast_mode;
    sample_mode = (acquire_mode == DSO_ACQUIRE_MODE_SAMPLE);
    allow_ets_requested = allow_ets && enable_advanced_recon;
    allow_ets_effective = allow_ets_requested;

    if (hs8_multi_lane && !hs8_cal_ready) {
        allow_ets_effective = false;
        DSO_HS8Render_Reset(hs8);
        ets_reset_channel(ch_idx);
    }

    if (out_meta != NULL) {
        out_meta->allow_ets_requested = allow_ets_requested;
        out_meta->allow_ets_effective = allow_ets_effective;
        out_meta->fast_mode = fast_mode;
        out_meta->recon_fast_mode = recon_fast_mode;
        out_meta->sample_mode = sample_mode;
        out_meta->repetitive_confidence_min = rep_min;
        out_meta->hs8_accum_start = 0u;
        out_meta->hs8_accum_count = sample_count;
    }

    if (out_meta != NULL && analysis != NULL) {
        out_meta->samples_per_cycle = analysis->samples_per_cycle;
    }

    if (trig_position_pct < 10u) {
        trig_position_pct = 10u;
    }
    if (trig_position_pct > 90u) {
        trig_position_pct = 90u;
    }

    if (analysis != NULL) {
        rep_gate_ok = analysis->repetitive_confidence >= rep_min;
        hs8_spc_gate_ok = (analysis->samples_per_cycle > 0.0f) &&
                          (analysis->samples_per_cycle < DSO_TIMEBASE_HS8_MAX_SPC);
        hs10_spc_gate_ok = (analysis->samples_per_cycle > 0.0f) &&
                           (analysis->samples_per_cycle < DSO_TIMEBASE_HS10_MAX_SPC);
        ets_spc_gate_ok = (analysis->samples_per_cycle > 0.0f) &&
                          (analysis->samples_per_cycle < DSO_TIMEBASE_ETS_MAX_SPC);

        ets_candidate = recon_fast_mode &&
                        sample_mode &&
                        allow_ets_effective &&
                        analysis->freq_valid &&
                        analysis->triggered &&
                        !analysis->clipped &&
                        rep_gate_ok &&
                        ets_spc_gate_ok &&
                        (trig_pos_samples >= 0.0f) &&
                        (trig_pos_samples < (float)sample_count);
        hs8_candidate = hs8_multi_lane &&
                        hs8_cal_ready &&
                        DSO_HS8Render_ShouldBuild(recon_fast_mode,
                                                  sample_mode,
                                                  analysis->freq_valid,
                                                  analysis->clipped,
                                                  analysis->repetitive_confidence,
                                                  analysis->samples_per_cycle) &&
                        analysis->triggered &&
                        (trig_pos_samples >= 0.0f) &&
                        (trig_pos_samples < (float)sample_count);
        hs10_candidate = format_is_10bit &&
                         DSO_10bitDisplaySync_AllowHs10ForChannel(channel) &&
                         recon_fast_mode &&
                         sample_mode &&
                         analysis->freq_valid &&
                         analysis->triggered &&
                         rep_gate_ok &&
                         hs10_spc_gate_ok &&
                         (trig_pos_samples >= 0.0f) &&
                         (trig_pos_samples < (float)sample_count);

        if (analysis->freq_valid &&
            analysis->freq_hz > 0.0f &&
            !ets_freq_matches(ets, analysis->freq_hz)) {
            ets_reset_channel(ch_idx);
        }

        policy.fast_mode = recon_fast_mode;
        policy.sample_mode = sample_mode;
        policy.allow_ets = ets_candidate;
        policy.freq_valid = analysis->freq_valid;
        policy.clipped = analysis->clipped;
        policy.ets_ready = ets->ets_ready;
        policy.repetitive_confidence = analysis->repetitive_confidence;
        policy.repetitive_confidence_min = rep_min;
        policy.samples_per_cycle = analysis->samples_per_cycle;
        DSO_RenderPolicy_Decide(&policy, &decision);
        if (out_meta != NULL) {
            out_meta->recon_mode = decision.recon_mode;
            out_meta->display_quality = decision.display_quality;
            out_meta->ets_building = decision.ets_building;
            out_meta->rep_gate_ok = rep_gate_ok;
            out_meta->hs8_spc_gate_ok = hs8_spc_gate_ok;
            out_meta->ets_spc_gate_ok = ets_spc_gate_ok;
            out_meta->hs8_candidate = hs8_candidate;
            out_meta->hs8_ready = DSO_HS8Render_IsReady(hs8);
            out_meta->ets_candidate = ets_candidate;
        }
        if (hs10_candidate &&
            DSO_HS10Render_ShouldBuild(recon_fast_mode,
                                       sample_mode,
                                       analysis->freq_valid,
                                       analysis->clipped,
                                       analysis->repetitive_confidence,
                                       analysis->samples_per_cycle) &&
            (analysis->freq_hz > 0.0f)) {
            DSO_HS10Render_Accumulate(hs10,
                                      samples,
                                      sample_count,
                                      sample_rate_hz,
                                      analysis->freq_hz,
                                      trig_pos_samples);
            if (DSO_HS10Render_Render(hs10,
                                      (float)entry->sec_div_ns,
                                      spec->adc_max,
                                      spec->midscale,
                                      out_display,
                                      DSO_DISPLAY_WIDTH)) {
                if (out_meta != NULL) {
                    out_meta->recon_mode = DSO_RECON_MODE_HS10_ENHANCED;
                    out_meta->display_quality = DSO_DISPLAY_QUALITY_GOOD;
                    out_meta->ets_building = false;
                    out_meta->effective_sample_rate_hz = DSO_HS10Render_EffectiveRateHz(hs10);
                    out_meta->column_envelope = false;
                }
                (void)memcpy(hi_sink, out_display, sizeof(uint16_t) * DSO_DISPLAY_WIDTH);
                return;
            }
        }
        if (hs8_candidate && (analysis->freq_hz > 0.0f)) {
            DSO_HS8Render_Accumulate(hs8,
                                     samples,
                                     sample_count,
                                     sample_rate_hz,
                                     analysis->freq_hz,
                                     trig_pos_samples);
            if (out_meta != NULL) {
                out_meta->hs8_ready = DSO_HS8Render_IsReady(hs8);
            }
            if (DSO_HS8Render_Render(hs8,
                                     (float)entry->sec_div_ns,
                                     spec->adc_max,
                                     spec->midscale,
                                     out_display,
                                     DSO_DISPLAY_WIDTH)) {
                if (out_meta != NULL) {
                    out_meta->recon_mode = DSO_RECON_MODE_HS8_ENHANCED;
                    out_meta->display_quality = DSO_DISPLAY_QUALITY_GOOD;
                    out_meta->ets_building = false;
                    out_meta->effective_sample_rate_hz = DSO_HS8Render_EffectiveRateHz(hs8);
                    out_meta->column_envelope = false;
                }
                (void)memcpy(hi_sink, out_display, sizeof(uint16_t) * DSO_DISPLAY_WIDTH);
                return;
            }
            if (out_meta != NULL) {
                out_meta->recon_mode = DSO_RECON_MODE_RAW_LIMITED;
                out_meta->display_quality = DSO_DISPLAY_QUALITY_REPEAT_REQUIRED;
                out_meta->ets_building = true;
            }
        }
        if (ets_candidate &&
            decision.ets_building &&
            (analysis->freq_hz > 0.0f)) {
            ets_accumulate(ets, samples, sample_count, sample_rate_hz, analysis->freq_hz, trig_pos_samples);
            if (ets->ets_ready) {
                policy.ets_ready = true;
                DSO_RenderPolicy_Decide(&policy, &decision);
                if (out_meta != NULL) {
                    out_meta->recon_mode = decision.recon_mode;
                    out_meta->display_quality = decision.display_quality;
                    out_meta->ets_building = decision.ets_building;
                }
            }
        }
        if (decision.recon_mode == DSO_RECON_MODE_ETS &&
            ets_render_to_display(ets, (float)entry->sec_div_ns, out_display)) {
            if (out_meta != NULL) {
                out_meta->ets_active = true;
                out_meta->ets_building = false;
                out_meta->effective_sample_rate_hz = analysis->freq_hz * (float)DSO_ETS_PHASE_BINS;
                out_meta->column_envelope = false;
            }
            (void)memcpy(hi_sink, out_display, sizeof(uint16_t) * DSO_DISPLAY_WIDTH);
            return;
        }
    } else {
        DSO_HS8Render_Reset(hs8);
        ets_reset_channel(ch_idx);
    }

    window_samples = DSO_Timebase_GetWindowSampleCount(sample_rate_hz);
    if (window_samples == 0u) {
        window_samples = 1u;
    }
    if (window_samples > sample_count) {
        window_samples = sample_count;
    }

    if ((trig_index >= 0) && ((uint32_t)trig_index < sample_count)) {
        start = (uint32_t)trig_index;
        if (window_samples < sample_count) {
            uint32_t left = (window_samples * (uint32_t)trig_position_pct) / 100u;
            if (left >= window_samples) {
                left = window_samples - 1u;
            }
            start = ((uint32_t)trig_index > left) ? ((uint32_t)trig_index - left) : 0u;
            if ((start + window_samples) > sample_count) {
                start = sample_count - window_samples;
            }
        }
    } else {
        start = (sample_count > window_samples) ? (sample_count - window_samples) : 0u;
    }

    {
        bool col_env = false;

        render_direct_window_to_dst(samples,
                                    sample_count,
                                    start,
                                    window_samples,
                                    acquire_mode,
                                    out_display,
                                    hi_sink,
                                    &col_env);
        if (out_meta != NULL) {
            out_meta->column_envelope = col_env;
        }
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_RenderToBuffer
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Timebase_RenderToBuffer(const uint16_t *samples,
                                 uint32_t sample_count,
                                 uint32_t sample_rate_hz,
                                 const DSO_ProcessAnalysis *analysis,
                                 uint8_t trig_position_pct,
                                 DSO_AcquireMode acquire_mode,
                                 bool allow_ets,
                                 uint16_t *out_display,
                                 uint16_t *out_column_hi,
                                 DSO_RenderMeta *out_meta)
{
    DSO_Timebase_RenderToBufferForChannel(0u,
                                          samples,
                                          sample_count,
                                          sample_rate_hz,
                                          analysis,
                                          trig_position_pct,
                                          acquire_mode,
                                          allow_ets,
                                          out_display,
                                          out_column_hi,
                                          out_meta);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_Render
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const uint16_t *DSO_Timebase_Render(const uint16_t *samples,
                                    uint32_t sample_count,
                                    uint32_t sample_rate_hz,
                                    const DSO_ProcessAnalysis *analysis,
                                    DSO_RenderMeta *out_meta)
{
    return DSO_Timebase_RenderEx(samples,
                                 sample_count,
                                 sample_rate_hz,
                                 analysis,
                                 50u,
                                 DSO_ACQUIRE_MODE_SAMPLE,
                                 out_meta);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_RollRender
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const uint16_t *DSO_Timebase_RollRender(const uint16_t *samples,
                                        uint32_t sample_count,
                                        uint32_t sample_rate_hz)
{
    return DSO_Timebase_RollRenderForChannel(0u, samples, sample_count, sample_rate_hz);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_RollRenderForChannel
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const uint16_t *DSO_Timebase_RollRenderForChannel(uint8_t channel,
                                                  const uint16_t *samples,
                                                  uint32_t sample_count,
                                                  uint32_t sample_rate_hz)
{
    const DSO_TimebaseEntry *entry = &active_table()[s_tb.active_index];
    float samples_per_pixel;
    uint8_t idx = (channel != 0u) ? 1u : 0u;

    if ((samples == NULL) || (sample_count == 0u) || (sample_rate_hz == 0u)) {
        roll_pad_unfilled_tail(idx);
        return s_tb.roll_buf[idx];
    }

    samples_per_pixel = DSO_RollPhysics_SamplesPerPixel(entry->sec_div_ns,
                                                          sample_rate_hz,
                                                          DSO_DISPLAY_WIDTH);

    for (uint32_t i = 0; i < sample_count; i++) {
        s_tb.roll_accum_sum[idx] += (float)samples[i];
        s_tb.roll_accum_count[idx] += 1.0f;

        if (s_tb.roll_accum_count[idx] >= samples_per_pixel) {
            uint16_t v = (uint16_t)((s_tb.roll_accum_sum[idx] / s_tb.roll_accum_count[idx]) + 0.5f);
            if (s_tb.roll_fill[idx] < DSO_DISPLAY_WIDTH) {
                s_tb.roll_buf[idx][s_tb.roll_fill[idx]++] = v;
            } else {
                memmove(&s_tb.roll_buf[idx][0],
                        &s_tb.roll_buf[idx][1],
                        sizeof(uint16_t) * (DSO_DISPLAY_WIDTH - 1u));
                s_tb.roll_buf[idx][DSO_DISPLAY_WIDTH - 1u] = v;
            }
            s_tb.roll_accum_sum[idx] = 0.0f;
            s_tb.roll_accum_count[idx] = 0.0f;
            s_tb.roll_plot_valid[idx] = (uint16_t)DSO_DISPLAY_WIDTH;
        }
    }

    roll_pad_unfilled_tail(idx);
    return s_tb.roll_buf[idx];
}
