/**
 * @file dso_serial_debug.c
 * @brief 串口调试主逻辑与命令解析。
 */

#include "dso_serial_debug.h"
#include "dso_acq.h"
#include "dso_sample_spec.h"

#include "dso_config.h"
#include "dso_freq_meter_hw_tim12.h"
#include "dso_serial_debug_math.h"
#include "dso_serial_debug_format.h"
#include "stm32h7xx_hal.h"
#include "bsp_uart_fifo.h"

#include <stdio.h>
#include <string.h>

#if DSO_SERIAL_DEBUG_ENABLE

typedef struct {
    uint32_t count;
    float min;
    float max;
    float sum;
} DSO_SerialDebugFloatStats;

typedef struct {
    uint32_t count;
    float min;
    float max;
    float sum;
    float anchor;
    float period;
    bool anchor_valid;
} DSO_SerialDebugPeriodicStats;

typedef struct {
    bool raw_valid;
    DSO_SerialDebugAcqSummary raw_summary;
    uint16_t raw_preview[DSO_SERIAL_DEBUG_SNAPSHOT_POINTS];
    uint32_t raw_preview_count;
    uint32_t raw_preview_start;
    int32_t raw_focus_index;
    uint8_t raw_adc_bits;
    uint8_t raw_lane_count;
    float prev_freq_hz;
    uint8_t prev_recon_mode;
    bool prev_ets_active;
    uint32_t last_emit_ms;
    uint32_t last_snapshot_ms;
    uint32_t last_raw_dump_ms;
    uint32_t last_hs8_window_ms;
    DSO_SerialDebugFloatStats phase_grid_stats;
    DSO_SerialDebugPeriodicStats interleave_common_phase_stats;
    DSO_SerialDebugPeriodicStats interleave_rel_lane0_stats;
    DSO_SerialDebugPeriodicStats interleave_rel_lane1_stats;
    DSO_SerialDebugFloatStats interleave_sep_stats;
    bool freq_log_valid;
    uint8_t freq_log_state;
    uint8_t freq_log_mode;
    uint32_t freq_log_gate_us;
    uint32_t last_freq_log_ms;
} DSO_SerialDebugChannelState;

static DSO_SerialDebugChannelState s_dbg_ch[2];
static char s_dbg_line[DSO_SERIAL_DEBUG_LINE_BUF_SIZE];
static bool s_printf_silenced;

/* 宏定义: DSO_SERIAL_DEBUG_HS8_STAGE_NONE - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_SERIAL_DEBUG_HS8_STAGE_NONE          0u
/* 宏定义: DSO_SERIAL_DEBUG_HS8_STAGE_LANE_SUMMARY - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_SERIAL_DEBUG_HS8_STAGE_LANE_SUMMARY  1u
/* 宏定义: DSO_SERIAL_DEBUG_HS8_STAGE_LANE_WINDOWS - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_SERIAL_DEBUG_HS8_STAGE_LANE_WINDOWS  2u

/*
*********************************************************************************************************
*   函 数 名: channel_state
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static DSO_SerialDebugChannelState *channel_state(uint8_t channel)
{
    if ((channel < 1u) || (channel > 2u)) {
        return NULL;
    }
    return &s_dbg_ch[channel - 1u];
}

/*
*********************************************************************************************************
*   函 数 名: freq_meter_serial_only_active
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool freq_meter_serial_only_active(void)
{
    return DSO_FREQ_METER_SERIAL_DEBUG_ONLY != 0u;
}

/*
*********************************************************************************************************
*   函 数 名: raw_lane_count_for_active_format
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t raw_lane_count_for_active_format(const DSO_SampleSpec *spec)
{
    if (DSO_SampleSpec_GetActiveFormat() == DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED) {
        uint8_t lane_count = DSO_Acq_GetLaneCount();
        return (lane_count == 0u) ? 1u : lane_count;
    }

    return (spec != NULL && spec->lane_count != 0u) ? spec->lane_count : 1u;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_SendLine
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_SerialDebug_SendLine(const char *text)
{
    size_t len;

    if (text == NULL) {
        return;
    }

    len = strlen(text);
    if (len == 0u) {
        return;
    }

    if (len > 0xFFFFu) {
        len = 0xFFFFu;
    }
    comSendBuf(COM1, (uint8_t *)text, (uint16_t)len);
}

/*
*********************************************************************************************************
*   函 数 名: send_line
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void send_line(const char *text)
{
    DSO_SerialDebug_SendLine(text);
}

/*
*********************************************************************************************************
*   函 数 名: analyze_u16_window
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void analyze_u16_window(const uint16_t *samples,
                               uint32_t count,
                               DSO_SerialDebugAcqSummary *out)
{
    uint32_t sum = 0u;
    uint32_t clip_low = 0u;
    uint32_t clip_high = 0u;
    const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();
    const uint16_t clip_margin = (spec->adc_max >= 64u) ? (uint16_t)(spec->adc_max / 64u) : 1u;
    const uint16_t clip_low_thresh = clip_margin;
    const uint16_t clip_high_thresh = (spec->adc_max > clip_margin)
                                    ? (uint16_t)(spec->adc_max - clip_margin)
                                    : spec->adc_max;
    uint16_t vmin = spec->adc_max;
    uint16_t vmax = 0u;
    uint16_t vpp_local;
    uint16_t clip_threshold;
    float mean;
    float hyst;
    float lo;
    float hi;
    bool armed = false;

    if ((out == NULL) || (samples == NULL) || (count == 0u)) {
        return;
    }

    for (uint32_t i = 0u; i < count; ++i) {
        uint16_t v = samples[i];
        if (v < vmin) {
            vmin = v;
        }
        if (v > vmax) {
            vmax = v;
        }
        if (v <= clip_low_thresh) {
            clip_low++;
        }
        if (v >= clip_high_thresh) {
            clip_high++;
        }
        sum += v;
    }

    out->vmin = vmin;
    out->vmax = vmax;
    out->mean = (uint16_t)(sum / count);

    /* Flag clip only when vpp actually spans >= (adc_max - 2*margin) ≈ 96.9% FS.
     * A single stray sample near the rail no longer trips the flag, which was
     * previously spuriously firing on 8-bit near-full-scale sines. */
    vpp_local = (uint16_t)(vmax - vmin);
    clip_threshold = (spec->adc_max > (uint16_t)(2u * clip_margin))
                   ? (uint16_t)(spec->adc_max - (2u * clip_margin))
                   : spec->adc_max;
    (void)clip_low;
    (void)clip_high;
    out->clip_low = (vpp_local >= clip_threshold) && (vmin <= clip_low_thresh);
    out->clip_high = (vpp_local >= clip_threshold) && (vmax >= clip_high_thresh);

    mean = (float)sum / (float)count;
    hyst = ((float)vmax - (float)vmin) * 0.05f;
    if (hyst < 1.0f) {
        hyst = 1.0f;
    }
    lo = mean - hyst;
    hi = mean + hyst;

    out->zero_crossings = 0u;
    for (uint32_t i = 0u; i < count; ++i) {
        float v = (float)samples[i];
        if (!armed) {
            if (v < lo) {
                armed = true;
            }
        } else if (v > hi) {
            out->zero_crossings++;
            armed = false;
        }
    }
}

/*
*********************************************************************************************************
*   函 数 名: choose_preview_start
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t choose_preview_start(uint32_t total_count, int32_t focus_index)
{
    uint32_t window = DSO_SERIAL_DEBUG_SNAPSHOT_POINTS;
    uint32_t start = 0u;

    if (total_count <= window) {
        return 0u;
    }

    if (focus_index > 0) {
        int32_t candidate = focus_index - (int32_t)(window / 4u);
        if (candidate > 0) {
            start = (uint32_t)candidate;
        }
    }

    if ((start + window) > total_count) {
        start = total_count - window;
    }

    return start;
}

/*
*********************************************************************************************************
*   函 数 名: copy_preview_u16
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t copy_preview_u16(const uint16_t *samples,
                                 uint32_t count,
                                 int32_t focus_index,
                                 uint16_t *dst,
                                 uint32_t *start_out)
{
    uint32_t start;
    uint32_t copy_count;

    if ((samples == NULL) || (dst == NULL) || (count == 0u)) {
        if (start_out != NULL) {
            *start_out = 0u;
        }
        return 0u;
    }

    start = choose_preview_start(count, focus_index);
    copy_count = count - start;
    if (copy_count > DSO_SERIAL_DEBUG_SNAPSHOT_POINTS) {
        copy_count = DSO_SERIAL_DEBUG_SNAPSHOT_POINTS;
    }
    memcpy(dst, &samples[start], sizeof(uint16_t) * copy_count);

    if (start_out != NULL) {
        *start_out = start;
    }
    return copy_count;
}

/*
*********************************************************************************************************
*   函 数 名: append_values_u16
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t append_values_u16(char *buf,
                                  uint32_t used,
                                  uint32_t buf_size,
                                  const uint16_t *samples,
                                  uint32_t count)
{
    for (uint32_t i = 0u; i < count; ++i) {
        int written = snprintf(&buf[used],
                               buf_size - used,
                               (i + 1u < count) ? "%u," : "%u\r\n",
                               (unsigned)samples[i]);
        if ((written < 0) || ((uint32_t)written >= (buf_size - used))) {
            return 0u;
        }
        used += (uint32_t)written;
    }

    return used;
}

/*
*********************************************************************************************************
*   函 数 名: append_values_i16
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t append_values_i16(char *buf,
                                  uint32_t used,
                                  uint32_t buf_size,
                                  const int16_t *samples,
                                  uint32_t count)
{
    for (uint32_t i = 0u; i < count; ++i) {
        int written = snprintf(&buf[used],
                               buf_size - used,
                               (i + 1u < count) ? "%d," : "%d\r\n",
                               (int)samples[i]);
        if ((written < 0) || ((uint32_t)written >= (buf_size - used))) {
            return 0u;
        }
        used += (uint32_t)written;
    }

    return used;
}

/*
*********************************************************************************************************
*   函 数 名: build_reason_text
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void build_reason_text(const DSO_SerialDebugSnapshotDecision *decision,
                              char *buf,
                              size_t buf_size)
{
    size_t used = 0u;

    if ((buf == NULL) || (buf_size == 0u) || (decision == NULL)) {
        return;
    }

    buf[0] = '\0';

/* 宏定义: APPEND_REASON - DSO 采集、处理、渲染或调试相关配置。 */
#define APPEND_REASON(flag, text)                                                     \
    do {                                                                              \
        if (decision->flag) {                                                         \
            int written = snprintf(&buf[used],                                        \
                                   buf_size - used,                                   \
                                   (used > 0u) ? "|%s" : "%s",                       \
                                   text);                                             \
            if ((written < 0) || ((size_t)written >= (buf_size - used))) {           \
                return;                                                               \
            }                                                                         \
            used += (size_t)written;                                                  \
        }                                                                             \
    } while (0)

    APPEND_REASON(forced, "force");
    APPEND_REASON(clipped, "clip");
    APPEND_REASON(freq_jump, "fjump");
    APPEND_REASON(recon_changed, "recon");
    APPEND_REASON(ets_toggled, "ets");
    APPEND_REASON(limited_quality, "limit");

#undef APPEND_REASON

    if (used == 0u) {
        (void)snprintf(buf, buf_size, "manual");
    }
}

/*
*********************************************************************************************************
*   函 数 名: emit_snapshot_u16
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void emit_snapshot_u16(const char *tag,
                              uint32_t frame_id,
                              uint8_t channel,
                              const char *reason,
                              const uint16_t *samples,
                              uint32_t count,
                              int32_t focus_index)
{
    uint16_t preview[DSO_SERIAL_DEBUG_SNAPSHOT_POINTS];
    uint32_t start = 0u;
    uint32_t preview_count = copy_preview_u16(samples, count, focus_index, preview, &start);
    uint32_t used;

    if (preview_count == 0u) {
        return;
    }

    used = (uint32_t)snprintf(s_dbg_line,
                              sizeof(s_dbg_line),
                              "%s frame=%lu ch=%u reason=%s ofs=%lu count=%lu data=",
                              tag,
                              (unsigned long)frame_id,
                              (unsigned)channel,
                              reason,
                              (unsigned long)start,
                              (unsigned long)preview_count);
    if ((used == 0u) || (used >= sizeof(s_dbg_line))) {
        return;
    }

    used = append_values_u16(s_dbg_line, used, sizeof(s_dbg_line), preview, preview_count);
    if (used == 0u) {
        return;
    }
    send_line(s_dbg_line);
}

/*
*********************************************************************************************************
*   函 数 名: emit_snapshot_i16
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void emit_snapshot_i16(const char *tag,
                              uint32_t frame_id,
                              uint8_t channel,
                              const char *reason,
                              const int16_t *samples,
                              uint32_t count,
                              int32_t focus_index)
{
    int16_t preview[DSO_SERIAL_DEBUG_SNAPSHOT_POINTS];
    uint32_t start = choose_preview_start(count, focus_index);
    uint32_t preview_count = count - start;
    uint32_t used;

    if ((samples == NULL) || (count == 0u)) {
        return;
    }

    if (preview_count > DSO_SERIAL_DEBUG_SNAPSHOT_POINTS) {
        preview_count = DSO_SERIAL_DEBUG_SNAPSHOT_POINTS;
    }
    memcpy(preview, &samples[start], sizeof(int16_t) * preview_count);

    used = (uint32_t)snprintf(s_dbg_line,
                              sizeof(s_dbg_line),
                              "%s frame=%lu ch=%u reason=%s ofs=%lu count=%lu data=",
                              tag,
                              (unsigned long)frame_id,
                              (unsigned)channel,
                              reason,
                              (unsigned long)start,
                              (unsigned long)preview_count);
    if ((used == 0u) || (used >= sizeof(s_dbg_line))) {
        return;
    }

    used = append_values_i16(s_dbg_line, used, sizeof(s_dbg_line), preview, preview_count);
    if (used == 0u) {
        return;
    }
    send_line(s_dbg_line);
}

/*
*********************************************************************************************************
*   函 数 名: emit_raw_dump
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void emit_raw_dump(uint32_t frame_id,
                          uint8_t channel,
                          const DSO_SerialDebugChannelState *state)
{
    DSO_SerialDebugRawDumpSummary rawd = {0};

    if ((state == NULL) || !state->raw_valid || (state->raw_preview_count == 0u)) {
        return;
    }

    rawd.frame_id = frame_id;
    rawd.channel = channel;
    rawd.adc_bits = state->raw_adc_bits;
    rawd.lane_count = state->raw_lane_count;
    rawd.sample_rate_hz = state->raw_summary.sample_rate_hz;
    rawd.total_count = state->raw_summary.sample_count;
    rawd.trigger_index = state->raw_focus_index;
    rawd.window_start = state->raw_preview_start;
    rawd.window_count = state->raw_preview_count;
    rawd.samples = state->raw_preview;

    if (DSO_SerialDebug_FormatRawDumpLine(&rawd, s_dbg_line, sizeof(s_dbg_line)) > 0u) {
        send_line(s_dbg_line);
    }
}

/*
*********************************************************************************************************
*   函 数 名: y_min_value
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static int16_t y_min_value(const int16_t *samples, uint32_t count)
{
    int16_t vmin = 0;

    if ((samples == NULL) || (count == 0u)) {
        return 0;
    }

    vmin = samples[0];
    for (uint32_t i = 1u; i < count; ++i) {
        if (samples[i] < vmin) {
            vmin = samples[i];
        }
    }
    return vmin;
}

/*
*********************************************************************************************************
*   函 数 名: y_max_value
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static int16_t y_max_value(const int16_t *samples, uint32_t count)
{
    int16_t vmax = 0;

    if ((samples == NULL) || (count == 0u)) {
        return 0;
    }

    vmax = samples[0];
    for (uint32_t i = 1u; i < count; ++i) {
        if (samples[i] > vmax) {
            vmax = samples[i];
        }
    }
    return vmax;
}

/*
*********************************************************************************************************
*   函 数 名: capture_hs8_snapshot
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool capture_hs8_snapshot(DSO_AcqDebugSnapshot *snap)
{
    if (snap == NULL) {
        return false;
    }

    memset(snap, 0, sizeof(*snap));
    return DSO_Acq_GetDebugSnapshot(snap) &&
           (snap->format == DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
}

/*
*********************************************************************************************************
*   函 数 名: trigger_pos_pct_from_analysis
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static float trigger_pos_pct_from_analysis(const DSO_ProcessAnalysis *analysis,
                                           uint32_t sample_count)
{
    float trig_pct;

    if ((analysis == NULL) || !analysis->triggered || (sample_count == 0u)) {
        return 0.0f;
    }

    trig_pct = (100.0f * analysis->trigger_pos_samples) / (float)sample_count;
    if (trig_pct < 0.0f) {
        return 0.0f;
    }
    if (trig_pct > 100.0f) {
        return 100.0f;
    }
    return trig_pct;
}

/*
*********************************************************************************************************
*   函 数 名: trigger_grid_phase_from_analysis
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static float trigger_grid_phase_from_analysis(const DSO_ProcessAnalysis *analysis)
{
    float phase;
    int32_t whole_samples;

    if ((analysis == NULL) || !analysis->triggered || (analysis->trigger_pos_samples < 0.0f)) {
        return -1.0f;
    }

    whole_samples = (int32_t)analysis->trigger_pos_samples;
    phase = analysis->trigger_pos_samples - (float)whole_samples;
    if (phase < 0.0f) {
        phase += 1.0f;
    }
    return phase;
}

/*
*********************************************************************************************************
*   函 数 名: rate_acceptance_per_mille
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t rate_acceptance_per_mille(uint32_t actual_rate_hz,
                                          uint32_t requested_rate_hz)
{
    if (requested_rate_hz == 0u) {
        return 0u;
    }

    return (uint32_t)((((uint64_t)actual_rate_hz * 1000u) +
                       ((uint64_t)requested_rate_hz / 2u)) /
                      (uint64_t)requested_rate_hz);
}

/*
*********************************************************************************************************
*   函 数 名: compact_phase_mode_active
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool compact_phase_mode_active(uint8_t channel, const DSO_AcqDebugSnapshot *snap)
{
    uint8_t hs8_topology;
    uint8_t hs8_lane_count;

    if ((DSO_SERIAL_DEBUG_PHASE_ONLY == 0u) ||
        (channel != 1u)) {
        return false;
    }

    if (snap != NULL) {
        hs8_topology = snap->hs8_topology;
        hs8_lane_count = snap->hs8_lane_count;
    } else {
        if (DSO_SampleSpec_GetActiveFormat() != DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED) {
            return false;
        }
        hs8_topology = DSO_Acq_GetHs8Topology();
        hs8_lane_count = DSO_Acq_GetLaneCount();
    }

    return (hs8_topology == DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED) &&
           (hs8_lane_count >= 3u);
}

/*
*********************************************************************************************************
*   函 数 名: reset_float_stats
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void reset_float_stats(DSO_SerialDebugFloatStats *stats)
{
    if (stats == NULL) {
        return;
    }

    memset(stats, 0, sizeof(*stats));
}

/*
*********************************************************************************************************
*   函 数 名: accumulate_float_stats
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void accumulate_float_stats(DSO_SerialDebugFloatStats *stats, float value)
{
    if (stats == NULL) {
        return;
    }

    if (stats->count == 0u) {
        stats->min = value;
        stats->max = value;
        stats->sum = value;
        stats->count = 1u;
        return;
    }

    if (value < stats->min) {
        stats->min = value;
    }
    if (value > stats->max) {
        stats->max = value;
    }
    stats->sum += value;
    stats->count++;
}

/*
*********************************************************************************************************
*   函 数 名: reset_periodic_stats
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void reset_periodic_stats(DSO_SerialDebugPeriodicStats *stats)
{
    if (stats == NULL) {
        return;
    }

    memset(stats, 0, sizeof(*stats));
}

/*
*********************************************************************************************************
*   函 数 名: accumulate_periodic_stats
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void accumulate_periodic_stats(DSO_SerialDebugPeriodicStats *stats,
                                      float value,
                                      float period)
{
    float sample;

    if (stats == NULL) {
        return;
    }

    if ((period <= 0.0f) || !stats->anchor_valid) {
        stats->count = 1u;
        stats->min = value;
        stats->max = value;
        stats->sum = value;
        stats->anchor = value;
        stats->period = period;
        stats->anchor_valid = true;
        return;
    }

    sample = DSO_SerialDebug_UnwrapToAnchor(value, stats->anchor, stats->period);
    if (sample < stats->min) {
        stats->min = sample;
    }
    if (sample > stats->max) {
        stats->max = sample;
    }
    stats->sum += sample;
    stats->count++;
}

/*
*********************************************************************************************************
*   函 数 名: periodic_stats_ready_to_emit
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool periodic_stats_ready_to_emit(const DSO_SerialDebugPeriodicStats *stats)
{
    uint32_t window = (DSO_SERIAL_DEBUG_PHASE_STATS_WINDOW == 0u)
                    ? 1u
                    : DSO_SERIAL_DEBUG_PHASE_STATS_WINDOW;

    return (stats != NULL) && (stats->count >= window);
}

/*
*********************************************************************************************************
*   函 数 名: float_stats_ready_to_emit
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool float_stats_ready_to_emit(const DSO_SerialDebugFloatStats *stats)
{
    uint32_t window = (DSO_SERIAL_DEBUG_PHASE_STATS_WINDOW == 0u)
                    ? 1u
                    : DSO_SERIAL_DEBUG_PHASE_STATS_WINDOW;

    return (stats != NULL) && (stats->count >= window);
}

/*
*********************************************************************************************************
*   函 数 名: reset_phase_stats
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void reset_phase_stats(DSO_SerialDebugChannelState *state)
{
    if (state == NULL) {
        return;
    }

    reset_float_stats(&state->phase_grid_stats);
}

/*
*********************************************************************************************************
*   函 数 名: reset_interleave_stats
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void reset_interleave_stats(DSO_SerialDebugChannelState *state)
{
    if (state == NULL) {
        return;
    }

    reset_periodic_stats(&state->interleave_common_phase_stats);
    reset_periodic_stats(&state->interleave_rel_lane0_stats);
    reset_periodic_stats(&state->interleave_rel_lane1_stats);
    reset_float_stats(&state->interleave_sep_stats);
}

/*
*********************************************************************************************************
*   函 数 名: accumulate_interleave_stats
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void accumulate_interleave_stats(DSO_SerialDebugChannelState *state,
                                        float common_phase,
                                        float common_period,
                                        float rel_lane0_phase,
                                        float rel_lane1_phase,
                                        float lane_phase_sep,
                                        float lane_period)
{
    if (state == NULL) {
        return;
    }

    accumulate_periodic_stats(&state->interleave_common_phase_stats,
                              common_phase,
                              common_period);
    accumulate_periodic_stats(&state->interleave_rel_lane0_stats,
                              rel_lane0_phase,
                              lane_period);
    accumulate_periodic_stats(&state->interleave_rel_lane1_stats,
                              rel_lane1_phase,
                              lane_period);
    accumulate_float_stats(&state->interleave_sep_stats, lane_phase_sep);
}

/*
*********************************************************************************************************
*   函 数 名: interleave_stats_ready_to_emit
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool interleave_stats_ready_to_emit(const DSO_SerialDebugChannelState *state)
{
    return (state != NULL) &&
           periodic_stats_ready_to_emit(&state->interleave_common_phase_stats) &&
           periodic_stats_ready_to_emit(&state->interleave_rel_lane0_stats) &&
           periodic_stats_ready_to_emit(&state->interleave_rel_lane1_stats) &&
           float_stats_ready_to_emit(&state->interleave_sep_stats);
}

/*
*********************************************************************************************************
*   函 数 名: normalize_centered_cycle_offset
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static float normalize_centered_cycle_offset(float phase)
{
    while (phase < -0.5f) {
        phase += 1.0f;
    }
    while (phase >= 0.5f) {
        phase -= 1.0f;
    }
    return phase;
}

/*
*********************************************************************************************************
*   函 数 名: nearest_lane_sample_index
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static int32_t nearest_lane_sample_index(float trigger_pos_samples, uint8_t lane_index)
{
    float pair_pos;
    int32_t pair_index;

    pair_pos = (trigger_pos_samples - (float)lane_index) * 0.5f;
    pair_index = (int32_t)(pair_pos + 0.5f);
    return (pair_index * 2) + (int32_t)lane_index;
}

/*
*********************************************************************************************************
*   函 数 名: calculate_interleave_phase_metrics
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool calculate_interleave_phase_metrics(const DSO_ProcessAnalysis *analysis,
                                               float *common_phase,
                                               float *rel_lane0_phase,
                                               float *rel_lane1_phase,
                                               float *lane_phase_sep,
                                               float *lane_period)
{
    int32_t lane0_index;
    int32_t lane1_index;
    float diff;
    float rel0;
    float rel1;
    float sample_period;
    float common;

    if (common_phase != NULL) {
        *common_phase = 0.0f;
    }

    if (rel_lane0_phase != NULL) {
        *rel_lane0_phase = 0.0f;
    }
    if (rel_lane1_phase != NULL) {
        *rel_lane1_phase = 0.0f;
    }
    if (lane_phase_sep != NULL) {
        *lane_phase_sep = 0.0f;
    }
    if (lane_period != NULL) {
        *lane_period = 0.0f;
    }

    if ((analysis == NULL) ||
        !analysis->triggered ||
        (analysis->trigger_pos_samples < 0.0f) ||
        (analysis->samples_per_cycle <= 0.0f)) {
        return false;
    }

    lane0_index = nearest_lane_sample_index(analysis->trigger_pos_samples, 0u);
    lane1_index = nearest_lane_sample_index(analysis->trigger_pos_samples, 1u);
    if ((lane0_index < 0) || (lane1_index < 0)) {
        return false;
    }

    sample_period = 1.0f / analysis->samples_per_cycle;
    if (lane_period != NULL) {
        *lane_period = 2.0f / analysis->samples_per_cycle;
    }

    rel0 = normalize_centered_cycle_offset(((float)lane0_index -
                                            analysis->trigger_pos_samples) /
                                           analysis->samples_per_cycle);
    rel1 = normalize_centered_cycle_offset(((float)lane1_index -
                                            analysis->trigger_pos_samples) /
                                           analysis->samples_per_cycle);
    common = DSO_SerialDebug_NormalizeToCenteredPeriod(rel0, sample_period);

    if (common_phase != NULL) {
        *common_phase = common;
    }

    if (rel_lane0_phase != NULL) {
        *rel_lane0_phase = rel0;
    }
    if (rel_lane1_phase != NULL) {
        *rel_lane1_phase = rel1;
    }
    if (lane_phase_sep != NULL) {
        diff = rel1 - rel0;
        if (diff < 0.0f) {
            diff = -diff;
        }
        if (diff > 0.5f) {
            diff = 1.0f - diff;
        }
        *lane_phase_sep = diff;
    }

    return true;
}

/*
*********************************************************************************************************
*   函 数 名: emit_hs8_summary
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void emit_hs8_summary(uint32_t frame_id, const DSO_AcqDebugSnapshot *snap)
{
    DSO_SerialDebugHs8Summary hs8;

    if (snap == NULL) {
        return;
    }

    memset(&hs8, 0, sizeof(hs8));
    hs8.frame_id = frame_id;
    hs8.running = snap->running;
    hs8.sample_rate_hz = snap->sample_rate_hz;
    hs8.ch2_sample_rate_hz = snap->ch2_sample_rate_hz;
    hs8.adc_kernel_clock_hz = snap->adc_kernel_clock_hz;
    hs8.dma_pos = snap->dma_pos;
    hs8.lane0_pos = snap->lane0_pos;
    hs8.lane1_pos = snap->lane1_pos;
    hs8.lane2_pos = snap->lane2_pos;
    hs8.ch2_valid = snap->ch2_valid;
    hs8.ch2_capture_active = snap->ch2_capture_active;
    hs8.ch2_count = snap->ch2_count;
    hs8.phase_sync_errors = snap->phase_sync_errors;
    hs8.overrun_count = snap->overrun_count;
    hs8.lane0_vmin = snap->lane0_vmin;
    hs8.lane0_vmax = snap->lane0_vmax;
    hs8.lane0_mean = snap->lane0_mean;
    hs8.lane1_vmin = snap->lane1_vmin;
    hs8.lane1_vmax = snap->lane1_vmax;
    hs8.lane1_mean = snap->lane1_mean;
    hs8.lane2_vmin = snap->lane2_vmin;
    hs8.lane2_vmax = snap->lane2_vmax;
    hs8.lane2_mean = snap->lane2_mean;
    hs8.ch2_vmin = snap->ch2_vmin;
    hs8.ch2_vmax = snap->ch2_vmax;
    hs8.ch2_mean = snap->ch2_mean;

    if (DSO_SerialDebug_FormatHs8Line(&hs8, s_dbg_line, sizeof(s_dbg_line)) > 0u) {
        send_line(s_dbg_line);
    }
}

/*
*********************************************************************************************************
*   函 数 名: emit_hs8_registers
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void emit_hs8_registers(uint32_t frame_id, const DSO_AcqDebugSnapshot *snap)
{
    DSO_SerialDebugHs8RegsSummary hs8r;

    if (snap == NULL) {
        return;
    }

    memset(&hs8r, 0, sizeof(hs8r));
    hs8r.frame_id = frame_id;
    hs8r.tim_enabled = snap->tim_enabled;
    hs8r.tim_counter = snap->tim_counter;
    hs8r.tim_arr = snap->tim_arr;
    hs8r.tim_ccr1 = snap->tim_ccr1;
    hs8r.tim_ccr2 = snap->tim_ccr2;
    hs8r.tim_ccr3 = snap->tim_ccr3;
    hs8r.tim_cr1 = snap->tim_cr1;
    hs8r.tim_ccmr1 = snap->tim_ccmr1;
    hs8r.tim_ccmr2 = snap->tim_ccmr2;
    hs8r.tim_ccer = snap->tim_ccer;
    hs8r.tim_bdtr = snap->tim_bdtr;
    hs8r.tim_sr = snap->tim_sr;
    hs8r.dma_ndtr0 = snap->dma_ndtr0;
    hs8r.dma_ndtr1 = snap->dma_ndtr1;
    hs8r.dma_ndtr2 = snap->dma_ndtr2;
    hs8r.adc1_cr = snap->adc1_cr;
    hs8r.adc2_cr = snap->adc2_cr;
    hs8r.adc3_cr = snap->adc3_cr;
    hs8r.adc1_cfgr = snap->adc1_cfgr;
    hs8r.adc2_cfgr = snap->adc2_cfgr;
    hs8r.adc3_cfgr = snap->adc3_cfgr;
    hs8r.adc1_isr = snap->adc1_isr;
    hs8r.adc2_isr = snap->adc2_isr;
    hs8r.adc3_isr = snap->adc3_isr;

    if (DSO_SerialDebug_FormatHs8RegsLine(&hs8r, s_dbg_line, sizeof(s_dbg_line)) > 0u) {
        send_line(s_dbg_line);
    }
}

/*
*********************************************************************************************************
*   函 数 名: emit_hs8_diag
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void emit_hs8_diag(uint32_t frame_id,
                          const DSO_AcqDebugSnapshot *snap,
                          const DSO_ProcessAnalysis *analysis,
                          const DSO_RenderMeta *render_meta,
                          uint32_t source_count)
{
    DSO_SerialDebugHs8DiagSummary hs8d;

    if ((snap == NULL) || (analysis == NULL) || (render_meta == NULL)) {
        return;
    }

    memset(&hs8d, 0, sizeof(hs8d));
    hs8d.frame_id = frame_id;
    hs8d.hs8_topology = snap->hs8_topology;
    hs8d.hs8_lane_count = snap->hs8_lane_count;
    hs8d.data_ready = snap->data_ready;
    hs8d.data_ready_lane_count = snap->data_ready_lane_count;
    hs8d.sample_rate_hz = snap->sample_rate_hz;
    hs8d.dma_sample_rate_hz = snap->dma_sample_rate_hz;
    hs8d.dma_sample_count = snap->dma_sample_count;
    hs8d.lane0_dma_sample_rate_hz = snap->lane0_dma_sample_rate_hz;
    hs8d.lane1_dma_sample_rate_hz = snap->lane1_dma_sample_rate_hz;
    hs8d.lane2_dma_sample_rate_hz = snap->lane2_dma_sample_rate_hz;
    hs8d.lane0_dma_pos_sample_rate_hz = snap->lane0_dma_pos_sample_rate_hz;
    hs8d.lane1_dma_pos_sample_rate_hz = snap->lane1_dma_pos_sample_rate_hz;
    hs8d.lane2_dma_pos_sample_rate_hz = snap->lane2_dma_pos_sample_rate_hz;
    hs8d.lane0_dma_accept_per_mille =
        rate_acceptance_per_mille(snap->lane0_dma_sample_rate_hz,
                                  snap->trigger_rate_hz);
    hs8d.lane1_dma_accept_per_mille =
        rate_acceptance_per_mille(snap->lane1_dma_sample_rate_hz,
                                  snap->trigger_rate_hz);
    hs8d.lane2_dma_accept_per_mille =
        rate_acceptance_per_mille(snap->lane2_dma_sample_rate_hz,
                                  snap->trigger_rate_hz);
    hs8d.lane0_dma_pos_accept_per_mille =
        rate_acceptance_per_mille(snap->lane0_dma_pos_sample_rate_hz,
                                  snap->trigger_rate_hz);
    hs8d.lane1_dma_pos_accept_per_mille =
        rate_acceptance_per_mille(snap->lane1_dma_pos_sample_rate_hz,
                                  snap->trigger_rate_hz);
    hs8d.lane2_dma_pos_accept_per_mille =
        rate_acceptance_per_mille(snap->lane2_dma_pos_sample_rate_hz,
                                  snap->trigger_rate_hz);
    hs8d.dma_pos_rate_max_elapsed_ms = snap->dma_pos_rate_max_elapsed_ms;
    hs8d.lane0_dma_sample_count = snap->lane0_dma_sample_count;
    hs8d.lane1_dma_sample_count = snap->lane1_dma_sample_count;
    hs8d.lane2_dma_sample_count = snap->lane2_dma_sample_count;
    hs8d.lane0_dma_half_count = snap->lane0_dma_half_count;
    hs8d.lane1_dma_half_count = snap->lane1_dma_half_count;
    hs8d.lane2_dma_half_count = snap->lane2_dma_half_count;
    hs8d.lane0_dma_full_count = snap->lane0_dma_full_count;
    hs8d.lane1_dma_full_count = snap->lane1_dma_full_count;
    hs8d.lane2_dma_full_count = snap->lane2_dma_full_count;
    hs8d.trigger_rate_hz = snap->trigger_rate_hz;
    hs8d.accepted_trigger_div = snap->accepted_trigger_div;
    hs8d.common_pos = snap->dma_pos;
    hs8d.lane0_pos = snap->lane0_pos;
    hs8d.lane1_pos = snap->lane1_pos;
    hs8d.lane2_pos = snap->lane2_pos;
    hs8d.lane0_delta = snap->lane0_delta;
    hs8d.lane1_delta = snap->lane1_delta;
    hs8d.lane2_delta = snap->lane2_delta;
    hs8d.common_delta = snap->common_delta;
    hs8d.phase_sync_error = snap->phase_sync_error;
    hs8d.overrun_count = snap->overrun_count;
    hs8d.lane0_overrun_count = snap->lane0_overrun_count;
    hs8d.lane1_overrun_count = snap->lane1_overrun_count;
    hs8d.lane2_overrun_count = snap->lane2_overrun_count;
    hs8d.freq_valid = analysis->freq_valid;
    hs8d.freq_hz = analysis->freq_hz;
    hs8d.samples_per_cycle = analysis->samples_per_cycle;
    hs8d.trig_pos_pct = trigger_pos_pct_from_analysis(analysis, source_count);
    hs8d.trigger_pos_samples = analysis->trigger_pos_samples;
    hs8d.trigger_grid_phase = trigger_grid_phase_from_analysis(analysis);
    hs8d.allow_ets_requested = render_meta->allow_ets_requested;
    hs8d.allow_ets_effective = render_meta->allow_ets_effective;
    hs8d.fast_mode = render_meta->fast_mode;
    hs8d.recon_fast_mode = render_meta->recon_fast_mode;
    hs8d.sample_mode = render_meta->sample_mode;
    hs8d.rep_gate_ok = render_meta->rep_gate_ok;
    hs8d.repetitive_confidence_min = render_meta->repetitive_confidence_min;
    hs8d.hs8_spc_gate_ok = render_meta->hs8_spc_gate_ok;
    hs8d.ets_spc_gate_ok = render_meta->ets_spc_gate_ok;
    hs8d.hs8_candidate = render_meta->hs8_candidate;
    hs8d.hs8_ready = render_meta->hs8_ready;
    hs8d.ets_candidate = render_meta->ets_candidate;
    hs8d.hs8_frame_local = render_meta->hs8_frame_local;
    hs8d.hs8_accum_start = render_meta->hs8_accum_start;
    hs8d.hs8_accum_count = render_meta->hs8_accum_count;

    if (DSO_SerialDebug_FormatHs8DiagLine(&hs8d, s_dbg_line, sizeof(s_dbg_line)) > 0u) {
        send_line(s_dbg_line);
    }
}

/*
*********************************************************************************************************
*   函 数 名: emit_interleave_phase_line
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void emit_interleave_phase_line(uint32_t frame_id,
                                       const DSO_AcqDebugSnapshot *snap,
                                       const DSO_ProcessAnalysis *analysis)
{
    DSO_SerialDebugInterleavePhaseSummary phase = {0};

    if ((snap == NULL) || (analysis == NULL)) {
        return;
    }

    phase.frame_id = frame_id;
    phase.hs8_topology = snap->hs8_topology;
    phase.hs8_lane_count = snap->hs8_lane_count;
    phase.sample_rate_hz = snap->sample_rate_hz;
    phase.lane0_pos = snap->lane0_pos;
    phase.lane1_pos = snap->lane1_pos;
    phase.common_delta = snap->common_delta;
    phase.phase_sync_error = snap->phase_sync_error;
    phase.freq_valid = analysis->freq_valid;
    phase.triggered = analysis->triggered;
    phase.trigger_index = analysis->trigger_index;
    phase.trigger_pos_samples = analysis->trigger_pos_samples;
    phase.samples_per_cycle = analysis->samples_per_cycle;
    phase.freq_hz = analysis->freq_hz;
    (void)calculate_interleave_phase_metrics(analysis,
                                             &phase.common_phase,
                                             &phase.rel_lane0_phase,
                                             &phase.rel_lane1_phase,
                                             &phase.lane_phase_sep,
                                             NULL);

    if (DSO_SerialDebug_FormatInterleavePhaseLine(&phase, s_dbg_line, sizeof(s_dbg_line)) > 0u) {
        send_line(s_dbg_line);
    }
}

/*
*********************************************************************************************************
*   函 数 名: emit_interleave_stats_line
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void emit_interleave_stats_line(uint32_t frame_id,
                                       const DSO_AcqDebugSnapshot *snap,
                                       const DSO_ProcessAnalysis *analysis,
                                       const DSO_SerialDebugChannelState *state)
{
    DSO_SerialDebugInterleaveStatsSummary stats = {0};

    if ((snap == NULL) || (analysis == NULL) || (state == NULL) ||
        (state->interleave_rel_lane0_stats.count == 0u) ||
        (state->interleave_rel_lane1_stats.count == 0u) ||
        (state->interleave_sep_stats.count == 0u)) {
        return;
    }

    stats.frame_id = frame_id;
    stats.hs8_topology = snap->hs8_topology;
    stats.hs8_lane_count = snap->hs8_lane_count;
    stats.sample_rate_hz = snap->sample_rate_hz;
    stats.freq_hz = analysis->freq_hz;
    stats.valid_count = state->interleave_sep_stats.count;
    stats.sep_min = state->interleave_sep_stats.min;
    stats.sep_max = state->interleave_sep_stats.max;
    stats.sep_span = state->interleave_sep_stats.max -
                     state->interleave_sep_stats.min;
    stats.sep_mean = state->interleave_sep_stats.sum /
                     (float)state->interleave_sep_stats.count;
    stats.common_min = state->interleave_common_phase_stats.min;
    stats.common_max = state->interleave_common_phase_stats.max;
    stats.common_span = state->interleave_common_phase_stats.max -
                        state->interleave_common_phase_stats.min;
    stats.common_mean = state->interleave_common_phase_stats.sum /
                        (float)state->interleave_common_phase_stats.count;
    stats.rel_lane0_min = state->interleave_rel_lane0_stats.min;
    stats.rel_lane0_max = state->interleave_rel_lane0_stats.max;
    stats.rel_lane0_span = state->interleave_rel_lane0_stats.max -
                           state->interleave_rel_lane0_stats.min;
    stats.rel_lane0_mean = state->interleave_rel_lane0_stats.sum /
                           (float)state->interleave_rel_lane0_stats.count;
    stats.rel_lane1_min = state->interleave_rel_lane1_stats.min;
    stats.rel_lane1_max = state->interleave_rel_lane1_stats.max;
    stats.rel_lane1_span = state->interleave_rel_lane1_stats.max -
                           state->interleave_rel_lane1_stats.min;
    stats.rel_lane1_mean = state->interleave_rel_lane1_stats.sum /
                           (float)state->interleave_rel_lane1_stats.count;

    if (DSO_SerialDebug_FormatInterleaveStatsLine(&stats, s_dbg_line, sizeof(s_dbg_line)) > 0u) {
        send_line(s_dbg_line);
    }
}

/*
*********************************************************************************************************
*   函 数 名: emit_hs8_window
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void emit_hs8_window(uint32_t frame_id,
                            const DSO_AcqDebugSnapshot *snap,
                            const DSO_ProcessAnalysis *analysis)
{
    DSO_SerialDebugHs8Window hs8w;
    DSO_SerialDebugHs8OrderSummary order;

    if ((snap == NULL) || (snap->lane_window_count == 0u)) {
        return;
    }

    memset(&hs8w, 0, sizeof(hs8w));
    hs8w.frame_id = frame_id;
    hs8w.hs8_topology = snap->hs8_topology;
    hs8w.hs8_lane_count = snap->hs8_lane_count;
    hs8w.lane0_start = snap->lane0_window_start;
    hs8w.lane1_start = snap->lane1_window_start;
    hs8w.lane2_start = snap->lane2_window_start;
    hs8w.sample_count = snap->lane_window_count;
    hs8w.lane0 = snap->lane0_window;
    hs8w.lane1 = snap->lane1_window;
    hs8w.lane2 = snap->lane2_window;

    if (DSO_SerialDebug_FormatHs8WindowLine(&hs8w, s_dbg_line, sizeof(s_dbg_line)) > 0u) {
        send_line(s_dbg_line);
    }

    if (snap->hs8_lane_count < 3u) {
        return;
    }

    memset(&order, 0, sizeof(order));
    order.frame_id = frame_id;
    order.hs8_topology = snap->hs8_topology;
    order.hs8_lane_count = snap->hs8_lane_count;
    order.sample_rate_hz = snap->sample_rate_hz;
    order.current_freq_valid = (analysis != NULL) && analysis->freq_valid;
    order.current_freq_hz = (analysis != NULL) ? analysis->freq_hz : 0.0f;
    order.lane0_start = snap->lane0_window_start;
    order.lane1_start = snap->lane1_window_start;
    order.lane2_start = snap->lane2_window_start;
    order.sample_count = snap->lane_window_count;
    order.lane0 = snap->lane0_window;
    order.lane1 = snap->lane1_window;
    order.lane2 = snap->lane2_window;

    if (DSO_SerialDebug_FormatHs8OrderLine(&order, s_dbg_line, sizeof(s_dbg_line)) > 0u) {
        send_line(s_dbg_line);
    }

    if (snap->lane_physical_window_count == 0u) {
        return;
    }

    memset(&order, 0, sizeof(order));
    order.line_tag = "ORDP";
    order.frame_id = frame_id;
    order.hs8_topology = snap->hs8_topology;
    order.hs8_lane_count = snap->hs8_lane_count;
    order.sample_rate_hz = snap->sample_rate_hz;
    order.current_freq_valid = (analysis != NULL) && analysis->freq_valid;
    order.current_freq_hz = (analysis != NULL) ? analysis->freq_hz : 0.0f;
    order.lane0_start = snap->lane0_physical_window_start;
    order.lane1_start = snap->lane1_physical_window_start;
    order.lane2_start = snap->lane2_physical_window_start;
    order.sample_count = snap->lane_physical_window_count;
    order.lane0 = snap->lane0_physical_window;
    order.lane1 = snap->lane1_physical_window;
    order.lane2 = snap->lane2_physical_window;

    if (DSO_SerialDebug_FormatHs8OrderLine(&order, s_dbg_line, sizeof(s_dbg_line)) > 0u) {
        send_line(s_dbg_line);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_Init
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_SerialDebug_Init(void)
{
    DSO_SerialDebugInitSummary init = {
        .hs8_stage = (uint8_t)DSO_SERIAL_DEBUG_HS8_STAGE,
        .hs8_topology = (uint8_t)DSO_ACQ8_TOPOLOGY_DEFAULT,
        .hs8_lane_count = DSO_Acq_GetLaneCount(),
        .hs8_ch2_realtime = false,
        .acq8_lane2_adc_channel = 1u,
    };

    memset(s_dbg_ch, 0, sizeof(s_dbg_ch));
    s_printf_silenced = false;
#if !DSO_FREQ_METER_SERIAL_DEBUG_ONLY
    if (DSO_SerialDebug_FormatInitLine(&init, s_dbg_line, sizeof(s_dbg_line)) > 0u) {
        send_line(s_dbg_line);
    }
#else
    (void)init;
#endif
    s_printf_silenced = (DSO_SERIAL_DEBUG_PHASE_ONLY != 0u) ||
                         (DSO_FREQ_METER_SERIAL_DEBUG_ONLY != 0u);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_CaptureRawStage
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_SerialDebug_CaptureRawStage(uint8_t channel,
                                     uint32_t frame_id,
                                     const uint16_t *samples,
                                     uint32_t sample_count,
                                     uint32_t sample_rate_hz,
                                     bool cal_active,
                                     uint8_t cal_state)
{
    DSO_SerialDebugChannelState *state = channel_state(channel);
    const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();

    if (state == NULL) {
        return;
    }

    memset(&state->raw_summary, 0, sizeof(state->raw_summary));
    state->raw_valid = false;
    state->raw_preview_count = 0u;
    state->raw_preview_start = 0u;
    state->raw_focus_index = -1;
    state->raw_adc_bits = 0u;
    state->raw_lane_count = 0u;

    if ((samples == NULL) || (sample_count == 0u)) {
        return;
    }

    state->raw_summary.frame_id = frame_id;
    state->raw_summary.channel = channel;
    state->raw_summary.sample_count = sample_count;
    state->raw_summary.sample_rate_hz = sample_rate_hz;
    state->raw_summary.cal_applied = cal_active;
    state->raw_summary.cal_state = cal_state;
    if (spec != NULL) {
        state->raw_adc_bits = spec->adc_bits;
        state->raw_lane_count = raw_lane_count_for_active_format(spec);
    }
    analyze_u16_window(samples, sample_count, &state->raw_summary);
    state->raw_preview_count = copy_preview_u16(samples,
                                                sample_count,
                                                -1,
                                                state->raw_preview,
                                                &state->raw_preview_start);
    state->raw_valid = true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_RefineRawStageFocus
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_SerialDebug_RefineRawStageFocus(uint8_t channel,
                                         const uint16_t *samples,
                                         uint32_t sample_count,
                                         int32_t focus_index)
{
    DSO_SerialDebugChannelState *state = channel_state(channel);

    if ((state == NULL) || !state->raw_valid ||
        (samples == NULL) || (sample_count == 0u)) {
        return;
    }

    state->raw_preview_count = copy_preview_u16(samples,
                                                sample_count,
                                                focus_index,
                                                state->raw_preview,
                                                &state->raw_preview_start);
    state->raw_focus_index = focus_index;
}

/*
*********************************************************************************************************
*   函 数 名: freq_log_due
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool freq_log_due(const DSO_SerialDebugChannelState *state,
                         const DSO_FreqMeterSnapshot *snap,
                         uint32_t now_ms)
{
    if ((state == NULL) || (snap == NULL)) {
        return false;
    }

    if (!state->freq_log_valid) {
        return true;
    }
    if (state->freq_log_state != (uint8_t)snap->state) {
        return true;
    }
    if (state->freq_log_mode != (uint8_t)snap->mode) {
        return true;
    }
    if (state->freq_log_gate_us != snap->gate_us) {
        return true;
    }

    return (uint32_t)(now_ms - state->last_freq_log_ms) >= DSO_FREQ_METER_SERIAL_DEBUG_INTERVAL_MS;
}

/*
*********************************************************************************************************
*   函 数 名: remember_freq_log
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void remember_freq_log(DSO_SerialDebugChannelState *state,
                              const DSO_FreqMeterSnapshot *snap,
                              uint32_t now_ms)
{
    if ((state == NULL) || (snap == NULL)) {
        return;
    }

    state->freq_log_valid = true;
    state->freq_log_state = (uint8_t)snap->state;
    state->freq_log_mode = (uint8_t)snap->mode;
    state->freq_log_gate_us = snap->gate_us;
    state->last_freq_log_ms = now_ms;
}

/*
*********************************************************************************************************
*   函 数 名: emit_freq_meter_line
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void emit_freq_meter_line(DSO_SerialDebugChannelState *state, uint32_t now_ms)
{
#if DSO_FREQ_METER_SERIAL_DEBUG_ENABLE
    DSO_SerialDebugFreqSummary summary = {0};

    summary.now_ms = now_ms;
    (void)DSO_FreqMeter_GetSnapshot(&summary.meter);
    DSO_FreqMeterHwTim12_ReadDebug(&summary.hw);
    if (!freq_log_due(state, &summary.meter, now_ms)) {
        return;
    }
    if (DSO_SerialDebug_FormatFreqDebugLine(&summary, s_dbg_line, sizeof(s_dbg_line)) > 0u) {
        send_line(s_dbg_line);
        remember_freq_log(state, &summary.meter, now_ms);
    }
#else
    (void)state;
    (void)now_ms;
#endif
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_LogFrame
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_SerialDebug_LogFrame(uint8_t channel,
                              uint32_t frame_id,
                              bool cal_active,
                              uint8_t cal_state,
                              const uint16_t *source_samples,
                              uint32_t source_count,
                              uint32_t sample_rate_hz,
                              const DSO_ProcessAnalysis *analysis,
                              const DSO_RenderMeta *render_meta,
                              const uint16_t *render_samples,
                              uint32_t render_count,
                              const DSO_Frame *frame,
                              const int16_t *y_coords,
                              uint32_t now_ms,
                              uint8_t trig_pos_pct)
{
    DSO_SerialDebugChannelState *state = channel_state(channel);
    DSO_SerialDebugProcSummary proc = {0};
    DSO_SerialDebugUiSummary ui = {0};
    DSO_SerialDebugSnapshotInput in = {0};
    DSO_SerialDebugSnapshotDecision decision = {0};
    DSO_AcqDebugSnapshot hs8_snap = {0};
    char reason[48];
    bool summary_due;
    bool snapshot_due;
    bool hs8_snapshot_valid = false;
    bool hs8_stage_active = false;
    bool hs8_window_due = false;
    bool raw_dump_due = false;
    bool phase_only_active = false;
    uint8_t hs8_stage = DSO_SERIAL_DEBUG_HS8_STAGE_NONE;
    int32_t raw_focus;
    int32_t ui_focus;

    if (state == NULL) {
        return;
    }

    if (freq_meter_serial_only_active()) {
        (void)frame_id;
        (void)cal_active;
        (void)cal_state;
        (void)source_samples;
        (void)source_count;
        (void)sample_rate_hz;
        (void)analysis;
        (void)render_meta;
        (void)render_samples;
        (void)render_count;
        (void)frame;
        (void)y_coords;
        if (channel == 1u) {
            emit_freq_meter_line(state, now_ms);
        }
        return;
    }

    if ((analysis == NULL) || (render_meta == NULL) ||
        (frame == NULL) || (y_coords == NULL)) {
        return;
    }

    if (!state->raw_valid && (source_samples != NULL) && (source_count > 0u)) {
        DSO_SerialDebug_CaptureRawStage(channel,
                                        frame_id,
                                        source_samples,
                                        source_count,
                                        sample_rate_hz,
                                        cal_active,
                                        cal_state);
    }

    proc.frame_id = frame_id;
    proc.channel = channel;
    proc.freq_valid = analysis->freq_valid;
    proc.freq_hz = analysis->freq_hz;
    proc.triggered = analysis->triggered;
    proc.trigger_index = analysis->trigger_index;
    proc.trigger_pos_samples = analysis->trigger_pos_samples;
    proc.samples_per_cycle = analysis->samples_per_cycle;
    proc.period_jitter_pct = analysis->period_jitter_pct;
    proc.repetitive_confidence = analysis->repetitive_confidence;
    proc.recon_mode = (uint8_t)render_meta->recon_mode;
    proc.display_quality = (uint8_t)render_meta->display_quality;
    proc.ets_active = render_meta->ets_active;
    proc.ets_building = render_meta->ets_building;

    ui.frame_id = frame_id;
    ui.channel = channel;
    ui.tb_sec_div_ns = frame->tb_sec_div_ns;
    ui.y_min = y_min_value(y_coords, DSO_DISPLAY_WIDTH);
    ui.y_max = y_max_value(y_coords, DSO_DISPLAY_WIDTH);

    if (channel == 1u) {
        ui.ui_freq_hz = frame->freq_hz;
        ui.ui_freq_valid = frame->freq_valid;
        ui.display_vpp = frame->vpp;
        ui.display_vrms = frame->vrms;
        ui.effective_sample_rate_hz = frame->effective_sample_rate_hz;
        ui.vert_mv_per_div = frame->vert_mv_per_div;
    } else {
        ui.ui_freq_hz = frame->ch2_freq_hz;
        ui.ui_freq_valid = frame->ch2_freq_valid;
        ui.display_vpp = frame->ch2_vpp;
        ui.display_vrms = frame->ch2_vrms;
        ui.effective_sample_rate_hz = (float)frame->ch2_sample_rate;
        ui.vert_mv_per_div = frame->ch2_vert_mv_per_div;
    }

    summary_due = (state->last_emit_ms == 0u) ||
                  ((uint32_t)(now_ms - state->last_emit_ms) >= DSO_SERIAL_DEBUG_INTERVAL_MS);

    in.force_snapshot = (DSO_SERIAL_DEBUG_FORCE_SNAPSHOT != 0u);
    in.clipped = analysis->clipped;
    in.freq_valid = analysis->freq_valid;
    in.prev_freq_hz = state->prev_freq_hz;
    in.cur_freq_hz = analysis->freq_hz;
    in.prev_recon_mode = state->prev_recon_mode;
    in.cur_recon_mode = (uint8_t)render_meta->recon_mode;
    in.prev_ets_active = state->prev_ets_active;
    in.cur_ets_active = render_meta->ets_active;
    in.cur_display_quality = (uint8_t)render_meta->display_quality;
    DSO_SerialDebug_DecideSnapshot(&in, &decision);
    snapshot_due = DSO_SerialDebug_ShouldEmitSnapshot(&decision,
                                                      now_ms,
                                                      state->last_snapshot_ms,
                                                      DSO_SERIAL_DEBUG_SNAPSHOT_INTERVAL_MS);
    phase_only_active = compact_phase_mode_active(channel, NULL);

    if ((channel == 1u) && (summary_due || snapshot_due) && capture_hs8_snapshot(&hs8_snap)) {
        hs8_snapshot_valid = true;
        hs8_stage = (uint8_t)DSO_SERIAL_DEBUG_HS8_STAGE;
        hs8_stage_active = (hs8_stage != DSO_SERIAL_DEBUG_HS8_STAGE_NONE);
        phase_only_active = compact_phase_mode_active(channel, &hs8_snap);
        hs8_window_due = hs8_stage_active &&
                         !phase_only_active &&
                         (hs8_stage >= DSO_SERIAL_DEBUG_HS8_STAGE_LANE_WINDOWS) &&
                         (snapshot_due ||
                          (state->last_hs8_window_ms == 0u) ||
                          ((uint32_t)(now_ms - state->last_hs8_window_ms) >=
                           DSO_SERIAL_DEBUG_SNAPSHOT_INTERVAL_MS));
    }

    raw_dump_due = (DSO_SERIAL_DEBUG_RAW_DUMP_ENABLE != 0u) &&
                   !phase_only_active &&
                   state->raw_valid &&
                   (state->raw_preview_count > 0u) &&
                   ((state->last_raw_dump_ms == 0u) ||
                    ((uint32_t)(now_ms - state->last_raw_dump_ms) >=
                     DSO_SERIAL_DEBUG_RAW_DUMP_INTERVAL_MS));

    if (phase_only_active && (summary_due || snapshot_due)) {
        float common_phase = 0.0f;
        float common_period = 0.0f;
        float rel_lane0_phase = 0.0f;
        float rel_lane1_phase = 0.0f;
        float lane_phase_sep = 0.0f;
        float lane_period = 0.0f;
        bool phase_valid;

        reset_phase_stats(state);
        emit_interleave_phase_line(frame_id, &hs8_snap, analysis);
        phase_valid = calculate_interleave_phase_metrics(analysis,
                                                         &common_phase,
                                                         &rel_lane0_phase,
                                                         &rel_lane1_phase,
                                                         &lane_phase_sep,
                                                         &lane_period);
        if (analysis->freq_valid && phase_valid) {
            common_period = 1.0f / analysis->samples_per_cycle;
            accumulate_interleave_stats(state,
                                        common_phase,
                                        common_period,
                                        rel_lane0_phase,
                                        rel_lane1_phase,
                                        lane_phase_sep,
                                        lane_period);
            if (interleave_stats_ready_to_emit(state)) {
                emit_interleave_stats_line(frame_id, &hs8_snap, analysis, state);
                reset_interleave_stats(state);
            }
        }
        state->last_emit_ms = now_ms;
        if (snapshot_due) {
            state->last_snapshot_ms = now_ms;
        }
        state->prev_freq_hz = analysis->freq_valid ? analysis->freq_hz : 0.0f;
        state->prev_recon_mode = (uint8_t)render_meta->recon_mode;
        state->prev_ets_active = render_meta->ets_active;
        state->raw_valid = false;
        state->raw_preview_count = 0u;
        return;
    }

    if (summary_due || snapshot_due) {
        if (channel == 1u) {
            if (hs8_stage_active) {
                emit_hs8_diag(frame_id, &hs8_snap, analysis, render_meta, source_count);
                if (hs8_window_due) {
                    emit_hs8_window(frame_id, &hs8_snap, analysis);
                    state->last_hs8_window_ms = now_ms;
                }
            } else if (hs8_snapshot_valid) {
                emit_hs8_summary(frame_id, &hs8_snap);
                emit_hs8_registers(frame_id, &hs8_snap);
            }
        }
        if (state->raw_valid) {
            state->raw_summary.frame_id = frame_id;
            state->raw_summary.channel = channel;
            state->raw_summary.cal_applied = cal_active;
            state->raw_summary.cal_state = cal_state;
            if (DSO_SerialDebug_FormatAcqLine(&state->raw_summary, s_dbg_line, sizeof(s_dbg_line)) > 0u) {
                send_line(s_dbg_line);
            }
        }
        if (DSO_SerialDebug_FormatProcLine(&proc, s_dbg_line, sizeof(s_dbg_line)) > 0u) {
            send_line(s_dbg_line);
        }
        if (DSO_SerialDebug_FormatUiLine(&ui, s_dbg_line, sizeof(s_dbg_line)) > 0u) {
            send_line(s_dbg_line);
        }
        state->last_emit_ms = now_ms;
    }

    if (raw_dump_due) {
        emit_raw_dump(frame_id, channel, state);
        state->last_raw_dump_ms = now_ms;
    }

    if (snapshot_due) {
        build_reason_text(&decision, reason, sizeof(reason));
        raw_focus = analysis->triggered ? analysis->trigger_index : -1;
        ui_focus = (int32_t)(((uint32_t)trig_pos_pct * DSO_DISPLAY_WIDTH) / 100u);

        (void)snprintf(s_dbg_line,
                       sizeof(s_dbg_line),
                       "EVT frame=%lu ch=%u reason=%s\r\n",
                       (unsigned long)frame_id,
                       (unsigned)channel,
                       reason);
        send_line(s_dbg_line);

        if (!hs8_stage_active && state->raw_valid && (state->raw_preview_count > 0u)) {
            uint32_t used = (uint32_t)snprintf(s_dbg_line,
                                               sizeof(s_dbg_line),
                                               "RAW frame=%lu ch=%u reason=%s ofs=%lu count=%lu data=",
                                               (unsigned long)frame_id,
                                               (unsigned)channel,
                                               reason,
                                               (unsigned long)state->raw_preview_start,
                                               (unsigned long)state->raw_preview_count);
            if ((used > 0u) && (used < sizeof(s_dbg_line))) {
                used = append_values_u16(s_dbg_line,
                                         used,
                                         sizeof(s_dbg_line),
                                         state->raw_preview,
                                         state->raw_preview_count);
                if (used > 0u) {
                    send_line(s_dbg_line);
                }
            }
        }
        if (!hs8_stage_active && cal_active && (source_samples != NULL) && (source_count > 0u)) {
            emit_snapshot_u16("CAL", frame_id, channel, reason, source_samples, source_count, raw_focus);
        }
        if (!hs8_stage_active) {
            emit_snapshot_u16("REN", frame_id, channel, reason, render_samples, render_count, ui_focus);
            emit_snapshot_i16("UIY", frame_id, channel, reason, y_coords, DSO_DISPLAY_WIDTH, ui_focus);
        }
        state->last_snapshot_ms = now_ms;
    }

    state->prev_freq_hz = analysis->freq_valid ? analysis->freq_hz : 0.0f;
    state->prev_recon_mode = (uint8_t)render_meta->recon_mode;
    state->prev_ets_active = render_meta->ets_active;
    state->raw_valid = false;
    state->raw_preview_count = 0u;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_ShouldBypassVofa
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_SerialDebug_ShouldBypassVofa(void)
{
    return DSO_SERIAL_DEBUG_ALLOW_VOFA == 0u;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_IsPhaseOnlyModeEnabled
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_SerialDebug_IsPhaseOnlyModeEnabled(void)
{
    return DSO_SERIAL_DEBUG_PHASE_ONLY != 0u;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_ShouldSilencePrintf
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_SerialDebug_ShouldSilencePrintf(void)
{
    return s_printf_silenced;
}

#else

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_SendLine
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_SerialDebug_SendLine(const char *text)
{
    (void)text;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_Init
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_SerialDebug_Init(void)
{
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_CaptureRawStage
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_SerialDebug_CaptureRawStage(uint8_t channel,
                                     uint32_t frame_id,
                                     const uint16_t *samples,
                                     uint32_t sample_count,
                                     uint32_t sample_rate_hz,
                                     bool cal_active,
                                     uint8_t cal_state)
{
    (void)channel;
    (void)frame_id;
    (void)samples;
    (void)sample_count;
    (void)sample_rate_hz;
    (void)cal_active;
    (void)cal_state;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_RefineRawStageFocus
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_SerialDebug_RefineRawStageFocus(uint8_t channel,
                                         const uint16_t *samples,
                                         uint32_t sample_count,
                                         int32_t focus_index)
{
    (void)channel;
    (void)samples;
    (void)sample_count;
    (void)focus_index;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_IsPhaseOnlyModeEnabled
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_SerialDebug_IsPhaseOnlyModeEnabled(void)
{
    return false;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_ShouldSilencePrintf
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_SerialDebug_ShouldSilencePrintf(void)
{
    return false;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_LogFrame
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_SerialDebug_LogFrame(uint8_t channel,
                              uint32_t frame_id,
                              bool cal_active,
                              uint8_t cal_state,
                              const uint16_t *source_samples,
                              uint32_t source_count,
                              uint32_t sample_rate_hz,
                              const DSO_ProcessAnalysis *analysis,
                              const DSO_RenderMeta *render_meta,
                              const uint16_t *render_samples,
                              uint32_t render_count,
                              const DSO_Frame *frame,
                              const int16_t *y_coords,
                              uint32_t now_ms,
                              uint8_t trig_pos_pct)
{
    (void)channel;
    (void)frame_id;
    (void)cal_active;
    (void)cal_state;
    (void)source_samples;
    (void)source_count;
    (void)sample_rate_hz;
    (void)analysis;
    (void)render_meta;
    (void)render_samples;
    (void)render_count;
    (void)frame;
    (void)y_coords;
    (void)now_ms;
    (void)trig_pos_pct;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_ShouldBypassVofa
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_SerialDebug_ShouldBypassVofa(void)
{
    return false;
}

#endif
