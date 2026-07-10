/**
 * @file dso_process.c
 * @brief Process 分析算法与 DSO_Frame 测量字段填充。
 */

/*
 * dso_process.c
 *
 * Runtime responsibilities are intentionally narrow:
 * - analyze raw samples for trigger/frequency/basic stats
 * - measure voltage metrics on display samples
 * - provide unpack-diagnostic candidate analysis for bring-up
 *
 * Horizontal rendering/ETS lives in dso_timebase.c.
 * Vertical scaling/Y mapping lives in dso_vertical.c.
 */

#include "dso_process.h"
#include "dso_acq.h"
#include "dso_sample_spec.h"

#include <math.h>
#include <string.h>

static uint16_t s_diag_seq[DSO_UNPACK_SIZE];

/* 宏定义: DSO_PROCESS_HS8_DUAL_FREQ_MAX_SPC - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_PROCESS_HS8_DUAL_FREQ_MAX_SPC            8.0f
/* 宏定义: DSO_PROCESS_HS8_DUAL_FREQ_MIN_REP_CONF - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_PROCESS_HS8_DUAL_FREQ_MIN_REP_CONF       0.95f
/* 宏定义: DSO_PROCESS_HS8_DUAL_FREQ_PAIR_MATCH_RATIO - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_PROCESS_HS8_DUAL_FREQ_PAIR_MATCH_RATIO   0.75f
/* 宏定义: DSO_PROCESS_HS8_DUAL_FREQ_PAIR_MATCH_DELTA - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_PROCESS_HS8_DUAL_FREQ_PAIR_MATCH_DELTA   1u

typedef struct {
    float freq_hz;
    bool valid;
    uint32_t crossing_count;
    float first_cross;
    float last_cross;
    float period_jitter_pct;
    bool clipped;
} DSO_FreqStats;

/*
*********************************************************************************************************
*   函 数 名: clamp_u16_from_i32
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t clamp_u16_from_i32(int32_t v)
{
    const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();

    if (v < 0) {
        return 0u;
    }
    if (v > (int32_t)spec->adc_max) {
        return spec->adc_max;
    }
    return (uint16_t)v;
}

/*
*********************************************************************************************************
*   函 数 名: clamp_u16_from_f32
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t clamp_u16_from_f32(float v)
{
    const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();

    if (v < 0.0f) {
        return 0u;
    }
    if (v > (float)spec->adc_max) {
        return spec->adc_max;
    }
    return (uint16_t)(v + 0.5f);
}

/*
*********************************************************************************************************
*   函 数 名: clamp01
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
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
*   函 数 名: hs8_dual_pair_match_ratio
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static float hs8_dual_pair_match_ratio(const uint16_t *samples, uint32_t count)
{
    uint32_t matched = 0u;
    uint32_t pairs = 0u;

    if ((samples == NULL) || (count < 2u)) {
        return 0.0f;
    }

    for (uint32_t i = 0u; (i + 1u) < count; i += 2u) {
        uint16_t a = samples[i];
        uint16_t b = samples[i + 1u];
        uint16_t diff = (a >= b) ? (a - b) : (b - a);

        if (diff <= DSO_PROCESS_HS8_DUAL_FREQ_PAIR_MATCH_DELTA) {
            matched++;
        }
        pairs++;
    }

    if (pairs == 0u) {
        return 0.0f;
    }

    return (float)matched / (float)pairs;
}

/*
*********************************************************************************************************
*   函 数 名: diag_word_master
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t diag_word_master(uint32_t word)
{
    return (uint16_t)(word & 0xFFFFu);
}

/*
*********************************************************************************************************
*   函 数 名: diag_word_slave
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t diag_word_slave(uint32_t word, int16_t cal_offset)
{
    int32_t adc2 = (int32_t)((word >> 16) & 0xFFFFu) - (int32_t)cal_offset;
    return clamp_u16_from_i32(adc2);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Process_DebugCandidateName
*   功能说明: 分析采样数据并计算触发、测量、频率、占空比或诊断统计结果。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const char *DSO_Process_DebugCandidateName(DSO_DiagCandidateId id)
{
    switch (id) {
    case DSO_DIAG_CANDIDATE_SAME_WORD_MASTER_SLAVE:
        return "same_word_master_slave";
    case DSO_DIAG_CANDIDATE_SAME_WORD_SLAVE_MASTER:
        return "same_word_slave_master";
    case DSO_DIAG_CANDIDATE_CROSS_WORD_SLAVE_MASTER_NEXT:
        return "cross_word_slave_master_next";
    case DSO_DIAG_CANDIDATE_CROSS_WORD_MASTER_PREV_SLAVE:
        return "cross_word_master_prev_slave";
    case DSO_DIAG_CANDIDATE_LANE_MASTER_ONLY:
        return "lane_master_only";
    case DSO_DIAG_CANDIDATE_LANE_SLAVE_ONLY:
        return "lane_slave_only";
    default:
        return "unknown";
    }
}

/*
*********************************************************************************************************
*   函 数 名: build_diag_candidate_sequence
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void build_diag_candidate_sequence(const uint32_t *words,
                                          uint32_t word_count,
                                          int16_t cal_offset,
                                          uint32_t sample_rate_hz,
                                          DSO_DiagCandidateId id,
                                          uint16_t *seq,
                                          uint32_t *out_count,
                                          uint32_t *out_rate_hz)
{
    uint32_t limit = word_count;
    uint32_t out_idx = 0u;

    if (limit > (DSO_UNPACK_SIZE / 2u)) {
        limit = DSO_UNPACK_SIZE / 2u;
    }

    *out_count = 0u;
    *out_rate_hz = 0u;

    switch (id) {
    case DSO_DIAG_CANDIDATE_SAME_WORD_MASTER_SLAVE:
        *out_rate_hz = sample_rate_hz;
        for (uint32_t k = 0; k < limit; k++) {
            seq[out_idx++] = diag_word_master(words[k]);
            seq[out_idx++] = diag_word_slave(words[k], cal_offset);
        }
        break;

    case DSO_DIAG_CANDIDATE_SAME_WORD_SLAVE_MASTER:
        *out_rate_hz = sample_rate_hz;
        for (uint32_t k = 0; k < limit; k++) {
            seq[out_idx++] = diag_word_slave(words[k], cal_offset);
            seq[out_idx++] = diag_word_master(words[k]);
        }
        break;

    case DSO_DIAG_CANDIDATE_CROSS_WORD_SLAVE_MASTER_NEXT:
        *out_rate_hz = sample_rate_hz;
        if (limit >= 2u) {
            for (uint32_t k = 0; (k + 1u) < limit; k++) {
                seq[out_idx++] = diag_word_slave(words[k], cal_offset);
                seq[out_idx++] = diag_word_master(words[k + 1u]);
            }
        }
        break;

    case DSO_DIAG_CANDIDATE_CROSS_WORD_MASTER_PREV_SLAVE:
        *out_rate_hz = sample_rate_hz;
        if (limit >= 2u) {
            for (uint32_t k = 1u; k < limit; k++) {
                seq[out_idx++] = diag_word_master(words[k]);
                seq[out_idx++] = diag_word_slave(words[k - 1u], cal_offset);
            }
        }
        break;

    case DSO_DIAG_CANDIDATE_LANE_MASTER_ONLY:
        *out_rate_hz = sample_rate_hz / 2u;
        for (uint32_t k = 0; k < limit; k++) {
            seq[out_idx++] = diag_word_master(words[k]);
        }
        break;

    case DSO_DIAG_CANDIDATE_LANE_SLAVE_ONLY:
        *out_rate_hz = sample_rate_hz / 2u;
        for (uint32_t k = 0; k < limit; k++) {
            seq[out_idx++] = diag_word_slave(words[k], cal_offset);
        }
        break;

    default:
        break;
    }

    *out_count = out_idx;
}

/*
*********************************************************************************************************
*   函 数 名: analyze_diag_sequence
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void analyze_diag_sequence(const uint16_t *seq,
                                  uint32_t count,
                                  uint32_t sample_rate_hz,
                                  DSO_DiagCandidateStats *out)
{
    uint32_t sum = 0u;
    const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();
    const uint16_t clip_margin = (spec->adc_max >= 64u) ? (uint16_t)(spec->adc_max / 64u) : 1u;
    const uint16_t clip_low_thresh = clip_margin;
    const uint16_t clip_high_thresh = (spec->adc_max > clip_margin)
                                    ? (uint16_t)(spec->adc_max - clip_margin)
                                    : spec->adc_max;
    const uint16_t vpp_min_runtime = (uint16_t)((spec->adc_max * 3u) / 100u);
    float avg_f;
    float hyst_f;
    float lo;
    float hi;
    bool armed = false;
    uint32_t prev_cross = 0u;

    memset(out, 0, sizeof(*out));
    out->sample_rate_hz = sample_rate_hz;
    out->sample_count = count;
    if (count == 0u) {
        return;
    }

    out->vmin = spec->adc_max;
    out->sample_preview_count = (count < DSO_DEBUG_DIAG_SAMPLE_PREVIEW)
                              ? count
                              : DSO_DEBUG_DIAG_SAMPLE_PREVIEW;

    for (uint32_t i = 0; i < count; i++) {
        uint16_t v = seq[i];

        if (i < out->sample_preview_count) {
            out->sample_preview[i] = v;
        }
        if (v < out->vmin) out->vmin = v;
        if (v > out->vmax) out->vmax = v;
        if (v <= clip_low_thresh) out->clip_low_count++;
        if (v >= clip_high_thresh) out->clip_high_count++;
        sum += v;
    }

    out->vpp = (uint16_t)(out->vmax - out->vmin);
    out->avg = (uint16_t)((sum + (count / 2u)) / count);

    if ((out->vpp < vpp_min_runtime) || (sample_rate_hz == 0u)) {
        return;
    }

    avg_f = (float)sum / (float)count;
    hyst_f = (float)out->vpp * 0.05f;
    if (hyst_f < 1.0f) {
        hyst_f = 1.0f;
    }

    lo = avg_f - hyst_f;
    hi = avg_f + hyst_f;
    out->lo_level = clamp_u16_from_f32(lo);
    out->hi_level = clamp_u16_from_f32(hi);

    for (uint32_t i = 0; i < count; i++) {
        float val = (float)seq[i];

        if (!armed) {
            if (val < lo) {
                armed = true;
            }
        } else if (val > hi) {
            if (out->crossing_count == 0u) {
                out->first_cross = i;
            } else if (out->cross_gap_preview_count < DSO_DEBUG_DIAG_CROSS_PREVIEW) {
                out->cross_gap_preview[out->cross_gap_preview_count++] = i - prev_cross;
            }

            if (out->cross_preview_count < DSO_DEBUG_DIAG_CROSS_PREVIEW) {
                out->cross_preview[out->cross_preview_count++] = i;
            }

            prev_cross = i;
            out->last_cross = i;
            out->crossing_count++;
            armed = false;
        }
    }

    out->valid = (out->crossing_count >= 3u) && (out->last_cross > out->first_cross);
    if (out->valid) {
        float span_samples = (float)(out->last_cross - out->first_cross);
        float span_seconds = span_samples / (float)sample_rate_hz;

        if (span_seconds > 0.0f) {
            out->freq_hz = (uint32_t)(((float)(out->crossing_count - 1u) / span_seconds) + 0.5f);
        }
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Process_DebugAnalyzeWords
*   功能说明: 分析采样数据并计算触发、测量、频率、占空比或诊断统计结果。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Process_DebugAnalyzeWords(const uint32_t *words,
                                   uint32_t word_count,
                                   int16_t cal_offset,
                                   uint32_t sample_rate_hz,
                                   DSO_DiagReport *out)
{
    uint32_t effective_words = word_count;

    if (out == NULL) {
        return;
    }

    if (effective_words > (DSO_UNPACK_SIZE / 2u)) {
        effective_words = DSO_UNPACK_SIZE / 2u;
    }

    memset(out, 0, sizeof(*out));
    out->word_count = effective_words;
    out->cal_offset = cal_offset;
    out->total_sample_rate_hz = sample_rate_hz;

    for (uint32_t id = 0; id < (uint32_t)DSO_DIAG_CANDIDATE_COUNT; id++) {
        uint32_t seq_count = 0u;
        uint32_t seq_rate_hz = 0u;

        build_diag_candidate_sequence(words,
                                      effective_words,
                                      cal_offset,
                                      sample_rate_hz,
                                      (DSO_DiagCandidateId)id,
                                      s_diag_seq,
                                      &seq_count,
                                      &seq_rate_hz);
        analyze_diag_sequence(s_diag_seq,
                              seq_count,
                              seq_rate_hz,
                              &out->candidate[id]);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Process_Init
*   功能说明: 分析采样数据并计算触发、测量、频率、占空比或诊断统计结果。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Process_Init(void)
{
    /* No persistent runtime state remains here. */
}

/*
*********************************************************************************************************
*   函 数 名: trigger_level_from_volts
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static float trigger_level_from_volts(float volts)
{
    const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();

    if (volts < 0.0f) {
        volts = 0.0f;
    }
    if (volts > spec->vref) {
        volts = spec->vref;
    }
    return (volts / spec->vref) * (float)spec->adc_max;
}

/*
*********************************************************************************************************
*   函 数 名: find_trigger_single_slope
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static int32_t find_trigger_single_slope(const uint16_t *samples,
                                         uint32_t search_limit,
                                         float trig_level,
                                         uint16_t hyst,
                                         DSO_TriggerSlope slope,
                                         float *out_trigger_pos)
{
    float arm_level = trig_level - (float)hyst;
    bool armed = false;

    for (uint32_t i = 1; i < search_limit; i++) {
        float sample = (float)samples[i];

        if (slope == DSO_TRIGGER_SLOPE_FALLING) {
            if (!armed) {
                if (sample > (trig_level + (float)hyst)) {
                    armed = true;
                }
            } else if (sample <= trig_level) {
                if (out_trigger_pos != NULL) {
                    float prev = (float)samples[i - 1u];
                    float denom = sample - prev;
                    float frac = (fabsf(denom) > 1e-6f)
                               ? ((trig_level - prev) / denom)
                               : 1.0f;
                    frac = clamp01(frac);
                    *out_trigger_pos = (float)(i - 1u) + frac;
                }
                return (int32_t)i;
            }
        } else {
            if (!armed) {
                if (sample < arm_level) {
                    armed = true;
                }
            } else if (sample >= trig_level) {
                if (out_trigger_pos != NULL) {
                    float prev = (float)samples[i - 1u];
                    float denom = sample - prev;
                    float frac = (fabsf(denom) > 1e-6f)
                               ? ((trig_level - prev) / denom)
                               : 1.0f;
                    frac = clamp01(frac);
                    *out_trigger_pos = (float)(i - 1u) + frac;
                }
                return (int32_t)i;
            }
        }
    }

    if (out_trigger_pos != NULL) {
        *out_trigger_pos = -1.0f;
    }
    return -1;
}

/*
*********************************************************************************************************
*   函 数 名: find_trigger_in_samples
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static int32_t find_trigger_in_samples(const uint16_t *samples,
                                       uint32_t count,
                                       const DSO_TriggerConfig *trigger,
                                       uint16_t *out_vmin,
                                       uint16_t *out_vmax,
                                       float *out_vavg,
                                       float *out_trigger_pos)
{
    const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();
    const uint16_t vpp_min_runtime = (uint16_t)((spec->adc_max * 3u) / 100u);
    uint16_t vmin = spec->adc_max;
    uint16_t vmax = 0u;
    uint32_t sum = 0u;
    uint32_t search_limit;
    uint16_t vpp;
    uint16_t hyst;
    float avg;
    float trig_level;
    DSO_TriggerSlope slope = DSO_TRIGGER_SLOPE_RISING;

    if ((samples == NULL) || (count == 0u)) {
        if (out_vmin != NULL) *out_vmin = 0u;
        if (out_vmax != NULL) *out_vmax = 0u;
        if (out_vavg != NULL) *out_vavg = 0.0f;
        if (out_trigger_pos != NULL) *out_trigger_pos = -1.0f;
        return -1;
    }

    for (uint32_t i = 0; i < count; i++) {
        uint16_t s = samples[i];
        if (s < vmin) vmin = s;
        if (s > vmax) vmax = s;
        sum += s;
    }

    if (out_vmin != NULL) *out_vmin = vmin;
    if (out_vmax != NULL) *out_vmax = vmax;
    if (out_vavg != NULL) *out_vavg = (float)sum / (float)count;

    vpp = (uint16_t)(vmax - vmin);
    if (vpp < vpp_min_runtime) {
        if (out_trigger_pos != NULL) *out_trigger_pos = -1.0f;
        return -1;
    }

    avg = (float)sum / (float)count;
    hyst = (uint16_t)(vpp / 10u);
    if (hyst < 1u) {
        hyst = 1u;
    }

    trig_level = avg;
    if (trigger != NULL) {
        slope = trigger->slope;
        if (trigger->level_enabled) {
            trig_level = trigger_level_from_volts(trigger->level_volts);
        }
    }

    search_limit = (count > DSO_DISPLAY_WIDTH) ? (count - DSO_DISPLAY_WIDTH) : count;

    if (slope == DSO_TRIGGER_SLOPE_BOTH) {
        float rising_pos = -1.0f;
        float falling_pos = -1.0f;
        int32_t rising_idx = find_trigger_single_slope(samples,
                                                       search_limit,
                                                       trig_level,
                                                       hyst,
                                                       DSO_TRIGGER_SLOPE_RISING,
                                                       &rising_pos);
        int32_t falling_idx = find_trigger_single_slope(samples,
                                                        search_limit,
                                                        trig_level,
                                                        hyst,
                                                        DSO_TRIGGER_SLOPE_FALLING,
                                                        &falling_pos);

        if ((rising_idx >= 0) && (falling_idx >= 0)) {
            if (rising_pos <= falling_pos) {
                if (out_trigger_pos != NULL) {
                    *out_trigger_pos = rising_pos;
                }
                return rising_idx;
            }
            if (out_trigger_pos != NULL) {
                *out_trigger_pos = falling_pos;
            }
            return falling_idx;
        }
        if (rising_idx >= 0) {
            if (out_trigger_pos != NULL) {
                *out_trigger_pos = rising_pos;
            }
            return rising_idx;
        }
        if (falling_idx >= 0) {
            if (out_trigger_pos != NULL) {
                *out_trigger_pos = falling_pos;
            }
            return falling_idx;
        }
        if (out_trigger_pos != NULL) {
            *out_trigger_pos = -1.0f;
        }
        return -1;
    }

    return find_trigger_single_slope(samples,
                                     search_limit,
                                     trig_level,
                                     hyst,
                                     slope,
                                     out_trigger_pos);
}

/*
*********************************************************************************************************
*   函 数 名: detect_freq_in_samples
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void detect_freq_in_samples(const uint16_t *samples,
                                   uint32_t count,
                                   float vavg,
                                   uint16_t vpp,
                                   uint32_t sample_rate,
                                   DSO_FreqStats *out)
{
    const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();
    const uint16_t vpp_min_runtime = (uint16_t)((spec->adc_max * 3u) / 100u);
    const uint16_t clip_margin = (spec->adc_max >= 64u) ? (uint16_t)(spec->adc_max / 64u) : 1u;
    float hyst_f;
    float lo;
    float hi;
    uint32_t crossing_count = 0u;
    float first_cross = 0.0f;
    float last_cross = 0.0f;
    float prev_cross = 0.0f;
    float interval_mean = 0.0f;
    float interval_m2 = 0.0f;
    uint32_t interval_count = 0u;
    bool armed = false;

    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));

    if ((samples == NULL) || (count == 0u) || (vpp < vpp_min_runtime) || (sample_rate == 0u)) {
        return;
    }

    hyst_f = (float)vpp * 0.05f;
    if (hyst_f < 1.0f) {
        hyst_f = 1.0f;
    }

    lo = vavg - hyst_f;
    hi = vavg + hyst_f;

    for (uint32_t i = 0; i < count; i++) {
        float val = (float)samples[i];
        if (!armed) {
            if (val < lo) {
                armed = true;
            }
        } else if (val > hi) {
            float prev = (i > 0u) ? (float)samples[i - 1u] : val;
            float denom = val - prev;
            float frac = (fabsf(denom) > 1e-6f)
                       ? ((hi - prev) / denom)
                       : 1.0f;
            float cross = (i > 0u) ? ((float)(i - 1u) + clamp01(frac)) : 0.0f;
            if (crossing_count == 0u) {
                first_cross = cross;
            }
            last_cross = cross;
            if (crossing_count > 0u) {
                float interval = cross - prev_cross;
                interval_count++;
                {
                    float delta = interval - interval_mean;
                    interval_mean += delta / (float)interval_count;
                    interval_m2 += delta * (interval - interval_mean);
                }
            }
            prev_cross = cross;
            crossing_count++;
            armed = false;
        }
    }

    {
        uint16_t clip_threshold = (spec->adc_max > (uint16_t)(2u * clip_margin))
                                ? (uint16_t)(spec->adc_max - (2u * clip_margin))
                                : spec->adc_max;
        out->clipped = (vpp >= clip_threshold);
    }
    out->crossing_count = crossing_count;
    out->first_cross = first_cross;
    out->last_cross = last_cross;

    if (crossing_count < 3u) {
        return;
    }

    {
        float span_samples = last_cross - first_cross;
        float span_seconds = span_samples / (float)sample_rate;

        if (span_seconds <= 0.0f) {
            return;
        }

        out->valid = true;
        out->freq_hz = (float)(crossing_count - 1u) / span_seconds;
        if (interval_count >= 2u && interval_mean > 1e-6f) {
            float variance = interval_m2 / (float)(interval_count - 1u);
            if (variance < 0.0f) {
                variance = 0.0f;
            }
            out->period_jitter_pct = sqrtf(variance) / interval_mean;
        }
    }
}

/*
*********************************************************************************************************
*   函 数 名: measure
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void measure(const uint16_t *samples,
                    uint32_t count,
                    uint16_t *out_vmin,
                    uint16_t *out_vmax,
                    uint16_t *out_vpp,
                    float *out_vavg,
                    float *out_vrms,
                    float *out_duty)
{
    const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();
    const uint16_t vpp_min_runtime = (uint16_t)((spec->adc_max * 3u) / 100u);
    uint16_t vmin = spec->adc_max;
    uint16_t vmax = 0u;
    float sum = 0.0f;
    float sum_sq = 0.0f;

    for (uint32_t i = 0; i < count; i++) {
        uint16_t s = samples[i];
        if (s < vmin) vmin = s;
        if (s > vmax) vmax = s;
        sum += (float)s;
        sum_sq += (float)s * (float)s;
    }

    *out_vmin = vmin;
    *out_vmax = vmax;
    *out_vpp = (uint16_t)(vmax - vmin);
    *out_vavg = sum / (float)count;
    *out_vrms = sqrtf(sum_sq / (float)count);

    if (*out_vpp < vpp_min_runtime) {
        *out_duty = -1.0f;
        return;
    }

    {
        float mid = (float)(vmin + vmax) * 0.5f;
        uint32_t high_count = 0u;

        for (uint32_t i = 0; i < count; i++) {
            if ((float)samples[i] >= mid) {
                high_count++;
            }
        }

        *out_duty = (float)high_count / (float)count;
    }
}

/*
*********************************************************************************************************
*   函 数 名: measure_envelope_columns
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void measure_envelope_columns(const uint16_t *col_lo,
                                    const uint16_t *col_hi,
                                    uint32_t count,
                                    uint16_t *out_vmin,
                                    uint16_t *out_vmax,
                                    uint16_t *out_vpp,
                                    float *out_vavg,
                                    float *out_vrms,
                                    float *out_duty)
{
    const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();
    const uint16_t vpp_min_runtime = (uint16_t)((spec->adc_max * 3u) / 100u);
    uint16_t vmin = spec->adc_max;
    uint16_t vmax = 0u;
    float sum = 0.0f;
    float sum_sq = 0.0f;

    for (uint32_t i = 0; i < count; i++) {
        uint16_t s_lo = col_lo[i];
        uint16_t s_hi = col_hi[i];
        if (s_lo < vmin) {
            vmin = s_lo;
        }
        if (s_hi > vmax) {
            vmax = s_hi;
        }
        {
            float mid = 0.5f * ((float)s_lo + (float)s_hi);
            sum += mid;
            sum_sq += mid * mid;
        }
    }

    *out_vmin = vmin;
    *out_vmax = vmax;
    *out_vpp = (uint16_t)(vmax - vmin);
    *out_vavg = sum / (float)count;
    *out_vrms = sqrtf(sum_sq / (float)count);

    if (*out_vpp < vpp_min_runtime) {
        *out_duty = -1.0f;
        return;
    }

    {
        float mid = (float)(vmin + vmax) * 0.5f;
        uint32_t high_count = 0u;

        for (uint32_t i = 0; i < count; i++) {
            if (0.5f * ((float)col_lo[i] + (float)col_hi[i]) >= mid) {
                high_count++;
            }
        }

        *out_duty = (float)high_count / (float)count;
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Process_AnalyzeSamples
*   功能说明: 分析采样数据并计算触发、测量、频率、占空比或诊断统计结果。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Process_AnalyzeSamples(const uint16_t *samples,
                                uint32_t count,
                                uint32_t sample_rate,
                                DSO_ProcessAnalysis *out)
{
    DSO_Process_AnalyzeSamplesEx(samples, count, sample_rate, NULL, out);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Process_AnalyzeSamplesEx
*   功能说明: 分析采样数据并计算触发、测量、频率、占空比或诊断统计结果。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Process_AnalyzeSamplesEx(const uint16_t *samples,
                                  uint32_t count,
                                  uint32_t sample_rate,
                                  const DSO_TriggerConfig *trigger,
                                  DSO_ProcessAnalysis *out)
{
    uint16_t vmin;
    uint16_t vmax;
    float vavg;
    float trig_pos;
    DSO_FreqStats freq = {0};
    int32_t trig_idx;

    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    if ((samples == NULL) || (count == 0u)) {
        out->trigger_index = -1;
        out->trigger_pos_samples = -1.0f;
        return;
    }

    trig_idx = find_trigger_in_samples(samples, count, trigger, &vmin, &vmax, &vavg, &trig_pos);
    detect_freq_in_samples(samples,
                           count,
                           vavg,
                           (uint16_t)(vmax - vmin),
                           sample_rate,
                           &freq);

    out->freq_hz = freq.freq_hz;
    out->freq_valid = freq.valid;
    out->vmin = vmin;
    out->vmax = vmax;
    out->vavg = vavg;
    out->trigger_index = trig_idx;
    out->trigger_pos_samples = trig_pos;
    out->triggered = (trig_idx >= 0);
    out->samples_per_cycle = (freq.valid && (freq.freq_hz > 0.0f))
                           ? ((float)sample_rate / freq.freq_hz)
                           : 0.0f;
    out->crossing_count = freq.crossing_count;
    out->period_jitter_pct = freq.period_jitter_pct;
    out->clipped = freq.clipped;
    if (freq.valid) {
        float cross_score = clamp01(((float)freq.crossing_count - 2.0f) / 6.0f);
        /* Denominator 0.5 keeps 8-bit bootstrap paths viable:
         *  - pre-calibration, lane mismatch inflates jitter to ~50%, so the
         *    score tends to 0 and rep sits near 0.55 (cross dominates);
         *  - post-calibration, natural jitter is ~5-15% so the score lands in
         *    the 0.7-0.9 band and rep climbs well above the 0.7 render gate.
         * 0.25 (previous value) was too strict for uncalibrated 8-bit. */
        float jitter_score = clamp01(1.0f - (freq.period_jitter_pct / 0.50f));
        /* clip no longer degrades rep_conf: a flat-topped repetitive signal is
         * still repetitive. Clipping is handled at the measurement / render
         * policy level, not as a confidence penalty. */
        out->repetitive_confidence = (0.55f * cross_score) + (0.45f * jitter_score);
    } else {
        out->repetitive_confidence = 0.0f;
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Process_IsHs8DualFreqReliable
*   功能说明: 分析采样数据并计算触发、测量、频率、占空比或诊断统计结果。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_Process_IsHs8DualFreqReliable(const uint16_t *samples,
                                       uint32_t count,
                                       const DSO_ProcessAnalysis *analysis)
{
    uint8_t lane_count;

    if ((analysis == NULL) || !analysis->freq_valid) {
        return true;
    }

    if (DSO_SampleSpec_GetActiveFormat() != DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED) {
        return true;
    }

    lane_count = DSO_Acq_GetLaneCount();
    if (lane_count < 2u) {
        return true;
    }

    if (lane_count >= 3u) {
        return true;
    }

    if ((samples == NULL) || (count < 32u)) {
        return true;
    }

    if ((analysis->samples_per_cycle <= 0.0f) ||
        (analysis->samples_per_cycle > DSO_PROCESS_HS8_DUAL_FREQ_MAX_SPC)) {
        return true;
    }

    if (analysis->repetitive_confidence >= DSO_PROCESS_HS8_DUAL_FREQ_MIN_REP_CONF) {
        return true;
    }

    return hs8_dual_pair_match_ratio(samples, count) < DSO_PROCESS_HS8_DUAL_FREQ_PAIR_MATCH_RATIO;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Process_MeasureDisplay
*   功能说明: 分析采样数据并计算触发、测量、频率、占空比或诊断统计结果。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Process_MeasureDisplay(const uint16_t *column_lo,
                                const uint16_t *column_hi,
                                uint32_t count,
                                DSO_Frame *out)
{
    uint16_t m_vmin;
    uint16_t m_vmax;
    uint16_t m_vpp;
    float m_vavg;
    float m_vrms;
    float m_duty;

    if (out == NULL) {
        return;
    }

    if ((column_lo == NULL) || (count == 0u)) {
        out->vmin = 0u;
        out->vmax = 0u;
        out->vpp = 0u;
        out->vrms = 0.0f;
        out->duty_cycle = -1.0f;
        return;
    }

    if (column_hi == NULL) {
        DSO_Process_MeasureSamples(column_lo, count, out);
        return;
    }

    measure_envelope_columns(column_lo, column_hi, count, &m_vmin, &m_vmax, &m_vpp, &m_vavg, &m_vrms, &m_duty);
    out->vmin = m_vmin;
    out->vmax = m_vmax;
    out->vpp = m_vpp;
    out->vrms = m_vrms;
    out->duty_cycle = m_duty;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Process_MeasureSamples
*   功能说明: 分析采样数据并计算触发、测量、频率、占空比或诊断统计结果。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Process_MeasureSamples(const uint16_t *samples,
                                uint32_t count,
                                DSO_Frame *out)
{
    uint16_t m_vmin;
    uint16_t m_vmax;
    uint16_t m_vpp;
    float m_vavg;
    float m_vrms;
    float m_duty;

    if (out == NULL) {
        return;
    }

    if ((samples == NULL) || (count == 0u)) {
        out->vmin = 0u;
        out->vmax = 0u;
        out->vpp = 0u;
        out->vrms = 0.0f;
        out->duty_cycle = -1.0f;
        return;
    }

    measure(samples, count, &m_vmin, &m_vmax, &m_vpp, &m_vavg, &m_vrms, &m_duty);
    out->vmin = m_vmin;
    out->vmax = m_vmax;
    out->vpp = m_vpp;
    out->vrms = m_vrms;
    out->duty_cycle = m_duty;
}
