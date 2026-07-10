/**
 * @file dso_serial_debug_format.c
 * @brief 格式化实现。
 */

#include "dso_serial_debug_format.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*
*********************************************************************************************************
*   函 数 名: recon_mode_name
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static const char *recon_mode_name(uint8_t mode)
{
    switch (mode) {
    case 0u:
        return "RAW";
    case 1u:
        return "LIMIT";
    case 2u:
        return "ETS";
    case 3u:
        return "HS8";
    case 4u:
        return "HS10";
    default:
        return "?";
    }
}

/*
*********************************************************************************************************
*   函 数 名: display_quality_name
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static const char *display_quality_name(uint8_t quality)
{
    switch (quality) {
    case 0u:
        return "GOOD";
    case 1u:
        return "LIMITED";
    case 2u:
        return "REPEAT";
    default:
        return "?";
    }
}

/*
*********************************************************************************************************
*   函 数 名: freq_state_name
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static const char *freq_state_name(DSO_FreqMeterState state)
{
    switch (state) {
    case DSO_FREQ_METER_STATE_STOPPED:
        return "STOPPED";
    case DSO_FREQ_METER_STATE_WAIT_EDGE:
        return "WAIT_EDGE";
    case DSO_FREQ_METER_STATE_MEASURING:
        return "MEASURING";
    case DSO_FREQ_METER_STATE_VALID:
        return "VALID";
    case DSO_FREQ_METER_STATE_NO_SIGNAL:
        return "NO_SIGNAL";
    case DSO_FREQ_METER_STATE_OVERRANGE:
        return "OVERRANGE";
    case DSO_FREQ_METER_STATE_FAULT:
        return "FAULT";
    default:
        return "UNKNOWN";
    }
}

/*
*********************************************************************************************************
*   函 数 名: freq_mode_name
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static const char *freq_mode_name(DSO_FreqMeterMode mode)
{
    switch (mode) {
    case DSO_FREQ_METER_MODE_LOW_CAPTURE:
        return "CAP";
    case DSO_FREQ_METER_MODE_GATED_COUNT:
        return "GATE";
    default:
        return "UNKNOWN";
    }
}

/*
*********************************************************************************************************
*   函 数 名: format_line
*   功能说明: 完成当前 UI 模块的绘制、格式化或局部状态辅助处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static size_t format_line(char *buf, size_t buf_size, const char *fmt, ...)
{
    va_list ap;
    int written;

    if ((buf == NULL) || (buf_size == 0u) || (fmt == NULL)) {
        return 0u;
    }

    va_start(ap, fmt);
    written = vsnprintf(buf, buf_size, fmt, ap);
    va_end(ap);

    if ((written < 0) || ((size_t)written >= buf_size)) {
        if (buf_size > 0u) {
            buf[0] = '\0';
        }
        return 0u;
    }

    return (size_t)written;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_FormatInitLine
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
size_t DSO_SerialDebug_FormatInitLine(const DSO_SerialDebugInitSummary *summary,
                                      char *buf,
                                      size_t buf_size)
{
    if (summary == NULL) {
        return 0u;
    }

    return format_line(buf,
                       buf_size,
                       "DBG init=1 mode=summary+snapshot hs8_stage=%u hs8_top=%u lc=%u hs8_ch2=%u lane2_ch=%lu\r\n",
                       (unsigned)summary->hs8_stage,
                       (unsigned)summary->hs8_topology,
                       (unsigned)summary->hs8_lane_count,
                       summary->hs8_ch2_realtime ? 1u : 0u,
                       (unsigned long)summary->acq8_lane2_adc_channel);
}

/*
*********************************************************************************************************
*   函 数 名: append_text
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool append_text(char *buf, size_t buf_size, size_t *used, const char *text)
{
    int written;

    if ((buf == NULL) || (used == NULL) || (text == NULL) || (*used >= buf_size)) {
        return false;
    }

    written = snprintf(&buf[*used], buf_size - *used, "%s", text);
    if ((written < 0) || ((size_t)written >= (buf_size - *used))) {
        if (buf_size > 0u) {
            buf[0] = '\0';
        }
        return false;
    }

    *used += (size_t)written;
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: append_u16_values
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool append_u16_values(char *buf,
                              size_t buf_size,
                              size_t *used,
                              const uint16_t *values,
                              uint32_t count)
{
    for (uint32_t i = 0u; i < count; ++i) {
        int written;

        if ((buf == NULL) || (used == NULL) || (values == NULL) || (*used >= buf_size)) {
            return false;
        }

        written = snprintf(&buf[*used],
                           buf_size - *used,
                           (i + 1u < count) ? "%u," : "%u",
                           (unsigned)values[i]);
        if ((written < 0) || ((size_t)written >= (buf_size - *used))) {
            if (buf_size > 0u) {
                buf[0] = '\0';
            }
            return false;
        }
        *used += (size_t)written;
    }

    return true;
}

/*
*********************************************************************************************************
*   函 数 名: append_interleaved3
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool append_interleaved3(char *buf,
                                size_t buf_size,
                                size_t *used,
                                const uint16_t *lane0,
                                const uint16_t *lane1,
                                const uint16_t *lane2,
                                uint32_t count)
{
    for (uint32_t i = 0u; i < count; ++i) {
        int written;

        if ((buf == NULL) || (used == NULL) || (lane0 == NULL) ||
            (lane1 == NULL) || (lane2 == NULL) || (*used >= buf_size)) {
            return false;
        }

        written = snprintf(&buf[*used],
                           buf_size - *used,
                           (i + 1u < count) ? "%u,%u,%u," : "%u,%u,%u",
                           (unsigned)lane0[i],
                           (unsigned)lane1[i],
                           (unsigned)lane2[i]);
        if ((written < 0) || ((size_t)written >= (buf_size - *used))) {
            if (buf_size > 0u) {
                buf[0] = '\0';
            }
            return false;
        }
        *used += (size_t)written;
    }

    return true;
}

/*
*********************************************************************************************************
*   函 数 名: append_interleaved2
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool append_interleaved2(char *buf,
                                size_t buf_size,
                                size_t *used,
                                const uint16_t *lane0,
                                const uint16_t *lane1,
                                uint32_t count)
{
    for (uint32_t i = 0u; i < count; ++i) {
        int written;

        if ((buf == NULL) || (used == NULL) || (lane0 == NULL) ||
            (lane1 == NULL) || (*used >= buf_size)) {
            return false;
        }

        written = snprintf(&buf[*used],
                           buf_size - *used,
                           (i + 1u < count) ? "%u,%u," : "%u,%u",
                           (unsigned)lane0[i],
                           (unsigned)lane1[i]);
        if ((written < 0) || ((size_t)written >= (buf_size - *used))) {
            if (buf_size > 0u) {
                buf[0] = '\0';
            }
            return false;
        }
        *used += (size_t)written;
    }

    return true;
}

/*
*********************************************************************************************************
*   函 数 名: order_lane_ptr
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static const uint16_t *order_lane_ptr(const DSO_SerialDebugHs8OrderSummary *summary,
                                      uint8_t lane_index)
{
    switch (lane_index) {
    case 0u:
        return summary->lane0;
    case 1u:
        return summary->lane1;
    case 2u:
        return summary->lane2;
    default:
        return NULL;
    }
}

/*
*********************************************************************************************************
*   函 数 名: order_sample_value
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static float order_sample_value(const DSO_SerialDebugHs8OrderSummary *summary,
                                const uint8_t order[3],
                                uint32_t merged_index)
{
    const uint16_t *lane;
    uint32_t lane_sample = merged_index / 3u;
    uint8_t lane_index = order[merged_index % 3u];

    lane = order_lane_ptr(summary, lane_index);
    if (lane == NULL) {
        return 0.0f;
    }
    return (float)lane[lane_sample];
}

/*
*********************************************************************************************************
*   函 数 名: estimate_order_freq_hz
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static float estimate_order_freq_hz(const DSO_SerialDebugHs8OrderSummary *summary,
                                    uint8_t a,
                                    uint8_t b,
                                    uint8_t c)
{
    uint8_t order[3] = {a, b, c};
    uint32_t total_count;
    float mean;
    float prev;
    float crossings[64];
    uint32_t crossing_count = 0u;
    float period_sum = 0.0f;
    uint32_t period_count = 0u;

    if ((summary == NULL) || (summary->sample_rate_hz == 0u) ||
        (summary->sample_count == 0u) || (summary->sample_count > 21u) ||
        (summary->lane0 == NULL) || (summary->lane1 == NULL) ||
        (summary->lane2 == NULL)) {
        return 0.0f;
    }

    total_count = summary->sample_count * 3u;
    mean = 0.0f;
    for (uint32_t i = 0u; i < total_count; ++i) {
        mean += order_sample_value(summary, order, i);
    }
    mean /= (float)total_count;

    prev = order_sample_value(summary, order, 0u) - mean;
    for (uint32_t i = 1u; i < total_count; ++i) {
        float cur = order_sample_value(summary, order, i) - mean;
        if (((prev < 0.0f) && (cur >= 0.0f)) ||
            ((prev > 0.0f) && (cur <= 0.0f))) {
            float denom = prev;
            float frac = 0.0f;

            if (denom < 0.0f) {
                denom = -denom;
            }
            if (cur < 0.0f) {
                denom -= cur;
            } else {
                denom += cur;
            }
            if (denom > 0.0f) {
                float mag = prev;
                if (mag < 0.0f) {
                    mag = -mag;
                }
                frac = mag / denom;
            }
            if (crossing_count < (sizeof(crossings) / sizeof(crossings[0]))) {
                crossings[crossing_count++] = (float)(i - 1u) + frac;
            }
        }
        prev = cur;
    }

    if (crossing_count < 4u) {
        return 0.0f;
    }

    for (uint32_t i = 2u; i < crossing_count; ++i) {
        float period = crossings[i] - crossings[i - 2u];
        if (period > 0.0f) {
            period_sum += period;
            period_count++;
        }
    }

    if ((period_count == 0u) || (period_sum <= 0.0f)) {
        return 0.0f;
    }

    return (float)summary->sample_rate_hz / (period_sum / (float)period_count);
}

/*
*********************************************************************************************************
*   函 数 名: rounded_hz
*   功能说明: 在串口诊断模块中完成采集快照、测量数据、格式化文本或调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static unsigned long rounded_hz(float hz)
{
    if (hz <= 0.0f) {
        return 0ul;
    }
    return (unsigned long)(hz + 0.5f);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_FormatAcqLine
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
size_t DSO_SerialDebug_FormatAcqLine(const DSO_SerialDebugAcqSummary *summary,
                                     char *buf,
                                     size_t buf_size)
{
    if (summary == NULL) {
        return 0u;
    }

    return format_line(buf,
                       buf_size,
                       "ACQ frame=%lu ch=%u count=%lu fs=%lu min=%u max=%u mean=%u zc=%lu clip=%u/%u cal=%u state=%u\r\n",
                       (unsigned long)summary->frame_id,
                       (unsigned)summary->channel,
                       (unsigned long)summary->sample_count,
                       (unsigned long)summary->sample_rate_hz,
                       (unsigned)summary->vmin,
                       (unsigned)summary->vmax,
                       (unsigned)summary->mean,
                       (unsigned long)summary->zero_crossings,
                       summary->clip_low ? 1u : 0u,
                       summary->clip_high ? 1u : 0u,
                       summary->cal_applied ? 1u : 0u,
                       (unsigned)summary->cal_state);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_FormatProcLine
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
size_t DSO_SerialDebug_FormatProcLine(const DSO_SerialDebugProcSummary *summary,
                                      char *buf,
                                      size_t buf_size)
{
    if (summary == NULL) {
        return 0u;
    }

    return format_line(buf,
                       buf_size,
                       "PROC frame=%lu ch=%u freq=%.2f valid=%u trig=%u trig_i=%ld trig_p=%.2f spc=%.2f jitter=%.2f rep=%.2f mode=%s quality=%s ets=%u/%u\r\n",
                       (unsigned long)summary->frame_id,
                       (unsigned)summary->channel,
                       (double)summary->freq_hz,
                       summary->freq_valid ? 1u : 0u,
                       summary->triggered ? 1u : 0u,
                       (long)summary->trigger_index,
                       (double)summary->trigger_pos_samples,
                       (double)summary->samples_per_cycle,
                       (double)summary->period_jitter_pct,
                       (double)summary->repetitive_confidence,
                       recon_mode_name(summary->recon_mode),
                       display_quality_name(summary->display_quality),
                       summary->ets_active ? 1u : 0u,
                       summary->ets_building ? 1u : 0u);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_FormatUiLine
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
size_t DSO_SerialDebug_FormatUiLine(const DSO_SerialDebugUiSummary *summary,
                                    char *buf,
                                    size_t buf_size)
{
    if (summary == NULL) {
        return 0u;
    }

    return format_line(buf,
                       buf_size,
                       "UI frame=%lu ch=%u freq=%.2f valid=%u vpp=%u vrms=%.2f eff_fs=%.2f tb_ns_div=%lu mv_div=%lu y=%d:%d\r\n",
                       (unsigned long)summary->frame_id,
                       (unsigned)summary->channel,
                       (double)summary->ui_freq_hz,
                       summary->ui_freq_valid ? 1u : 0u,
                       (unsigned)summary->display_vpp,
                       (double)summary->display_vrms,
                       (double)summary->effective_sample_rate_hz,
                       (unsigned long)summary->tb_sec_div_ns,
                       (unsigned long)summary->vert_mv_per_div,
                       (int)summary->y_min,
                       (int)summary->y_max);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_FormatRawDumpLine
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
size_t DSO_SerialDebug_FormatRawDumpLine(const DSO_SerialDebugRawDumpSummary *summary,
                                         char *buf,
                                         size_t buf_size)
{
    size_t used;

    if ((summary == NULL) || (buf == NULL) || (buf_size == 0u) ||
        (summary->samples == NULL) || (summary->window_count == 0u)) {
        return 0u;
    }

    used = format_line(buf,
                       buf_size,
                       "RAWD frame=%lu ch=%u bits=%u lanes=%u fs=%lu total=%lu trig=%ld ofs=%lu count=%lu data=",
                       (unsigned long)summary->frame_id,
                       (unsigned)summary->channel,
                       (unsigned)summary->adc_bits,
                       (unsigned)summary->lane_count,
                       (unsigned long)summary->sample_rate_hz,
                       (unsigned long)summary->total_count,
                       (long)summary->trigger_index,
                       (unsigned long)summary->window_start,
                       (unsigned long)summary->window_count);
    if (used == 0u) {
        return 0u;
    }
    if (!append_u16_values(buf,
                           buf_size,
                           &used,
                           summary->samples,
                           summary->window_count)) {
        return 0u;
    }
    if (!append_text(buf, buf_size, &used, "\r\n")) {
        return 0u;
    }

    return used;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_FormatHs8Line
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
size_t DSO_SerialDebug_FormatHs8Line(const DSO_SerialDebugHs8Summary *summary,
                                     char *buf,
                                     size_t buf_size)
{
    if (summary == NULL) {
        return 0u;
    }

    return format_line(buf,
                       buf_size,
                       "HS8 frame=%lu run=%u fs=%lu ch2_fs=%lu adc=%lu pos=%lu lanes=%lu/%lu/%lu ch2=%u/%u/%lu phase=%lu ovr=%lu l0=%u:%u:%u l1=%u:%u:%u l2=%u:%u:%u ch2v=%u:%u:%u\r\n",
                       (unsigned long)summary->frame_id,
                       summary->running ? 1u : 0u,
                       (unsigned long)summary->sample_rate_hz,
                       (unsigned long)summary->ch2_sample_rate_hz,
                       (unsigned long)summary->adc_kernel_clock_hz,
                       (unsigned long)summary->dma_pos,
                       (unsigned long)summary->lane0_pos,
                       (unsigned long)summary->lane1_pos,
                       (unsigned long)summary->lane2_pos,
                       summary->ch2_valid ? 1u : 0u,
                       summary->ch2_capture_active ? 1u : 0u,
                       (unsigned long)summary->ch2_count,
                       (unsigned long)summary->phase_sync_errors,
                       (unsigned long)summary->overrun_count,
                       (unsigned)summary->lane0_vmin,
                       (unsigned)summary->lane0_vmax,
                       (unsigned)summary->lane0_mean,
                       (unsigned)summary->lane1_vmin,
                       (unsigned)summary->lane1_vmax,
                       (unsigned)summary->lane1_mean,
                       (unsigned)summary->lane2_vmin,
                       (unsigned)summary->lane2_vmax,
                       (unsigned)summary->lane2_mean,
                       (unsigned)summary->ch2_vmin,
                       (unsigned)summary->ch2_vmax,
                       (unsigned)summary->ch2_mean);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_FormatHs8RegsLine
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
size_t DSO_SerialDebug_FormatHs8RegsLine(const DSO_SerialDebugHs8RegsSummary *summary,
                                         char *buf,
                                         size_t buf_size)
{
    if (summary == NULL) {
        return 0u;
    }

    return format_line(buf,
                       buf_size,
                       "HS8R frame=%lu tim=%u/%lu/%lu ccr=%lu/%lu/%lu timr=0x%08lX/0x%08lX/0x%08lX/0x%08lX/0x%08lX/0x%08lX ndtr=%lu/%lu/%lu cr=0x%08lX/0x%08lX/0x%08lX cfgr=0x%08lX/0x%08lX/0x%08lX isr=0x%08lX/0x%08lX/0x%08lX\r\n",
                       (unsigned long)summary->frame_id,
                       summary->tim_enabled ? 1u : 0u,
                       (unsigned long)summary->tim_counter,
                       (unsigned long)summary->tim_arr,
                       (unsigned long)summary->tim_ccr1,
                       (unsigned long)summary->tim_ccr2,
                       (unsigned long)summary->tim_ccr3,
                       (unsigned long)summary->tim_cr1,
                       (unsigned long)summary->tim_ccmr1,
                       (unsigned long)summary->tim_ccmr2,
                       (unsigned long)summary->tim_ccer,
                       (unsigned long)summary->tim_bdtr,
                       (unsigned long)summary->tim_sr,
                       (unsigned long)summary->dma_ndtr0,
                       (unsigned long)summary->dma_ndtr1,
                       (unsigned long)summary->dma_ndtr2,
                       (unsigned long)summary->adc1_cr,
                       (unsigned long)summary->adc2_cr,
                       (unsigned long)summary->adc3_cr,
                       (unsigned long)summary->adc1_cfgr,
                       (unsigned long)summary->adc2_cfgr,
                       (unsigned long)summary->adc3_cfgr,
                       (unsigned long)summary->adc1_isr,
                       (unsigned long)summary->adc2_isr,
                       (unsigned long)summary->adc3_isr);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_FormatHs8DiagLine
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
size_t DSO_SerialDebug_FormatHs8DiagLine(const DSO_SerialDebugHs8DiagSummary *summary,
                                         char *buf,
                                         size_t buf_size)
{
    if (summary == NULL) {
        return 0u;
    }

    return format_line(buf,
                       buf_size,
                       "HS8D frame=%lu top=%u lc=%u ready=%u rlanes=%u fs=%lu dmafs=%lu dmac=%lu dmafs_l=%lu/%lu/%lu posfs_l=%lu/%lu/%lu acc_l=%lu/%lu/%lu pacc_l=%lu/%lu/%lu posdt=%lu dmac_l=%lu/%lu/%lu ht_l=%lu/%lu/%lu tc_l=%lu/%lu/%lu trigfs=%lu div=%lu pos=%lu lanes=%lu/%lu/%lu d=%lu/%lu/%lu cd=%lu phase_err=%u ovr=%lu ovr_l=%lu/%lu/%lu freq=%.2f valid=%u spc=%.2f trig=%.2f tpos=%.2f grid=%.3f req=%u eff=%u fast=%u rfast=%u samp=%u repok=%u repmin=%.2f hs8spc=%u etsspc=%u hs8=%u/%u etsc=%u hloc=%u hwin=%lu+%lu\r\n",
                       (unsigned long)summary->frame_id,
                       (unsigned)summary->hs8_topology,
                       (unsigned)summary->hs8_lane_count,
                       summary->data_ready ? 1u : 0u,
                       (unsigned)summary->data_ready_lane_count,
                       (unsigned long)summary->sample_rate_hz,
                       (unsigned long)summary->dma_sample_rate_hz,
                       (unsigned long)summary->dma_sample_count,
                       (unsigned long)summary->lane0_dma_sample_rate_hz,
                       (unsigned long)summary->lane1_dma_sample_rate_hz,
                       (unsigned long)summary->lane2_dma_sample_rate_hz,
                       (unsigned long)summary->lane0_dma_pos_sample_rate_hz,
                       (unsigned long)summary->lane1_dma_pos_sample_rate_hz,
                       (unsigned long)summary->lane2_dma_pos_sample_rate_hz,
                       (unsigned long)summary->lane0_dma_accept_per_mille,
                       (unsigned long)summary->lane1_dma_accept_per_mille,
                       (unsigned long)summary->lane2_dma_accept_per_mille,
                       (unsigned long)summary->lane0_dma_pos_accept_per_mille,
                       (unsigned long)summary->lane1_dma_pos_accept_per_mille,
                       (unsigned long)summary->lane2_dma_pos_accept_per_mille,
                       (unsigned long)summary->dma_pos_rate_max_elapsed_ms,
                       (unsigned long)summary->lane0_dma_sample_count,
                       (unsigned long)summary->lane1_dma_sample_count,
                       (unsigned long)summary->lane2_dma_sample_count,
                       (unsigned long)summary->lane0_dma_half_count,
                       (unsigned long)summary->lane1_dma_half_count,
                       (unsigned long)summary->lane2_dma_half_count,
                       (unsigned long)summary->lane0_dma_full_count,
                       (unsigned long)summary->lane1_dma_full_count,
                       (unsigned long)summary->lane2_dma_full_count,
                       (unsigned long)summary->trigger_rate_hz,
                       (unsigned long)summary->accepted_trigger_div,
                       (unsigned long)summary->common_pos,
                       (unsigned long)summary->lane0_pos,
                       (unsigned long)summary->lane1_pos,
                       (unsigned long)summary->lane2_pos,
                       (unsigned long)summary->lane0_delta,
                       (unsigned long)summary->lane1_delta,
                       (unsigned long)summary->lane2_delta,
                       (unsigned long)summary->common_delta,
                       summary->phase_sync_error ? 1u : 0u,
                       (unsigned long)summary->overrun_count,
                       (unsigned long)summary->lane0_overrun_count,
                       (unsigned long)summary->lane1_overrun_count,
                       (unsigned long)summary->lane2_overrun_count,
                       (double)summary->freq_hz,
                       summary->freq_valid ? 1u : 0u,
                       (double)summary->samples_per_cycle,
                       (double)summary->trig_pos_pct,
                       (double)summary->trigger_pos_samples,
                       (double)summary->trigger_grid_phase,
                       summary->allow_ets_requested ? 1u : 0u,
                       summary->allow_ets_effective ? 1u : 0u,
                       summary->fast_mode ? 1u : 0u,
                       summary->recon_fast_mode ? 1u : 0u,
                       summary->sample_mode ? 1u : 0u,
                       summary->rep_gate_ok ? 1u : 0u,
                       (double)summary->repetitive_confidence_min,
                       summary->hs8_spc_gate_ok ? 1u : 0u,
                       summary->ets_spc_gate_ok ? 1u : 0u,
                       summary->hs8_candidate ? 1u : 0u,
                       summary->hs8_ready ? 1u : 0u,
                       summary->ets_candidate ? 1u : 0u,
                       summary->hs8_frame_local ? 1u : 0u,
                       (unsigned long)summary->hs8_accum_start,
                       (unsigned long)summary->hs8_accum_count);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_FormatPhaseLine
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
size_t DSO_SerialDebug_FormatPhaseLine(const DSO_SerialDebugPhaseSummary *summary,
                                       char *buf,
                                       size_t buf_size)
{
    if (summary == NULL) {
        return 0u;
    }

    return format_line(buf,
                       buf_size,
                       "PHASE frame=%lu top=%u lc=%u fs=%lu pos=%lu lane=%lu d=%lu phase_err=%u valid=%u trig=%u trig_i=%ld tpos=%.2f grid=%.3f spc=%.2f freq=%.2f\r\n",
                       (unsigned long)summary->frame_id,
                       (unsigned)summary->hs8_topology,
                       (unsigned)summary->hs8_lane_count,
                       (unsigned long)summary->sample_rate_hz,
                       (unsigned long)summary->dma_pos,
                       (unsigned long)summary->lane0_pos,
                       (unsigned long)summary->common_delta,
                       summary->phase_sync_error ? 1u : 0u,
                       summary->freq_valid ? 1u : 0u,
                       summary->triggered ? 1u : 0u,
                       (long)summary->trigger_index,
                       (double)summary->trigger_pos_samples,
                       (double)summary->trigger_grid_phase,
                       (double)summary->samples_per_cycle,
                       (double)summary->freq_hz);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_FormatPhaseStatsLine
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
size_t DSO_SerialDebug_FormatPhaseStatsLine(const DSO_SerialDebugPhaseStatsSummary *summary,
                                            char *buf,
                                            size_t buf_size)
{
    if (summary == NULL) {
        return 0u;
    }

    return format_line(buf,
                       buf_size,
                       "PSTAT frame=%lu top=%u lc=%u fs=%lu freq=%.2f count=%lu min=%.3f max=%.3f span=%.3f mean=%.3f\r\n",
                       (unsigned long)summary->frame_id,
                       (unsigned)summary->hs8_topology,
                       (unsigned)summary->hs8_lane_count,
                       (unsigned long)summary->sample_rate_hz,
                       (double)summary->freq_hz,
                       (unsigned long)summary->valid_count,
                       (double)summary->grid_min,
                       (double)summary->grid_max,
                       (double)summary->grid_span,
                       (double)summary->grid_mean);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_FormatInterleavePhaseLine
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
size_t DSO_SerialDebug_FormatInterleavePhaseLine(const DSO_SerialDebugInterleavePhaseSummary *summary,
                                                 char *buf,
                                                 size_t buf_size)
{
    if (summary == NULL) {
        return 0u;
    }

    return format_line(buf,
                       buf_size,
                       "IPHASE frame=%lu top=%u lc=%u fs=%lu lanes=%lu/%lu cd=%lu phase_err=%u valid=%u trig=%u trig_i=%ld tpos=%.2f spc=%.2f freq=%.2f sep=%.3f cm=%.3f r0=%.3f r1=%.3f\r\n",
                       (unsigned long)summary->frame_id,
                       (unsigned)summary->hs8_topology,
                       (unsigned)summary->hs8_lane_count,
                       (unsigned long)summary->sample_rate_hz,
                       (unsigned long)summary->lane0_pos,
                       (unsigned long)summary->lane1_pos,
                       (unsigned long)summary->common_delta,
                       summary->phase_sync_error ? 1u : 0u,
                       summary->freq_valid ? 1u : 0u,
                       summary->triggered ? 1u : 0u,
                       (long)summary->trigger_index,
                       (double)summary->trigger_pos_samples,
                       (double)summary->samples_per_cycle,
                       (double)summary->freq_hz,
                       (double)summary->lane_phase_sep,
                       (double)summary->common_phase,
                       (double)summary->rel_lane0_phase,
                       (double)summary->rel_lane1_phase);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_FormatInterleaveStatsLine
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
size_t DSO_SerialDebug_FormatInterleaveStatsLine(const DSO_SerialDebugInterleaveStatsSummary *summary,
                                                 char *buf,
                                                 size_t buf_size)
{
    if (summary == NULL) {
        return 0u;
    }

    return format_line(buf,
                       buf_size,
                       "ISTAT frame=%lu top=%u lc=%u fs=%lu freq=%.2f count=%lu sep=%.3f/%.3f/%.3f/%.3f cm=%.3f/%.3f/%.3f/%.3f r0=%.3f/%.3f/%.3f/%.3f r1=%.3f/%.3f/%.3f/%.3f\r\n",
                       (unsigned long)summary->frame_id,
                       (unsigned)summary->hs8_topology,
                       (unsigned)summary->hs8_lane_count,
                       (unsigned long)summary->sample_rate_hz,
                       (double)summary->freq_hz,
                       (unsigned long)summary->valid_count,
                       (double)summary->sep_min,
                       (double)summary->sep_max,
                       (double)summary->sep_span,
                       (double)summary->sep_mean,
                       (double)summary->common_min,
                       (double)summary->common_max,
                       (double)summary->common_span,
                       (double)summary->common_mean,
                       (double)summary->rel_lane0_min,
                       (double)summary->rel_lane0_max,
                       (double)summary->rel_lane0_span,
                       (double)summary->rel_lane0_mean,
                       (double)summary->rel_lane1_min,
                       (double)summary->rel_lane1_max,
                       (double)summary->rel_lane1_span,
                       (double)summary->rel_lane1_mean);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_FormatHs8WindowLine
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
size_t DSO_SerialDebug_FormatHs8WindowLine(const DSO_SerialDebugHs8Window *summary,
                                           char *buf,
                                           size_t buf_size)
{
    size_t used;

    if ((summary == NULL) || (buf == NULL) || (buf_size == 0u) ||
        (summary->sample_count == 0u) || (summary->lane0 == NULL) ||
        (summary->lane1 == NULL) || (summary->lane2 == NULL)) {
        return 0u;
    }

    used = format_line(buf,
                       buf_size,
                       "HS8W frame=%lu top=%u lc=%u ofs=%lu/%lu/%lu count=%lu l0=",
                       (unsigned long)summary->frame_id,
                       (unsigned)summary->hs8_topology,
                       (unsigned)summary->hs8_lane_count,
                       (unsigned long)summary->lane0_start,
                       (unsigned long)summary->lane1_start,
                       (unsigned long)summary->lane2_start,
                       (unsigned long)summary->sample_count);
    if (used == 0u) {
        return 0u;
    }
    if (!append_u16_values(buf, buf_size, &used, summary->lane0, summary->sample_count)) {
        return 0u;
    }
    if (!append_text(buf, buf_size, &used, " l1=")) {
        return 0u;
    }
    if (!append_u16_values(buf, buf_size, &used, summary->lane1, summary->sample_count)) {
        return 0u;
    }
    if (!append_text(buf, buf_size, &used, " l2=")) {
        return 0u;
    }
    if (!append_u16_values(buf, buf_size, &used, summary->lane2, summary->sample_count)) {
        return 0u;
    }
    if (!append_text(buf, buf_size, &used, " m=")) {
        return 0u;
    }
    if (summary->hs8_lane_count >= 3u) {
        if (!append_interleaved3(buf,
                                 buf_size,
                                 &used,
                                 summary->lane0,
                                 summary->lane1,
                                 summary->lane2,
                                 summary->sample_count)) {
            return 0u;
        }
    } else if (summary->hs8_lane_count == 2u) {
        if (!append_interleaved2(buf,
                                 buf_size,
                                 &used,
                                 summary->lane0,
                                 summary->lane1,
                                 summary->sample_count)) {
            return 0u;
        }
    } else {
        if (!append_u16_values(buf, buf_size, &used, summary->lane0, summary->sample_count)) {
            return 0u;
        }
    }
    if (!append_text(buf, buf_size, &used, "\r\n")) {
        return 0u;
    }

    return used;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_FormatHs8OrderLine
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
size_t DSO_SerialDebug_FormatHs8OrderLine(const DSO_SerialDebugHs8OrderSummary *summary,
                                          char *buf,
                                          size_t buf_size)
{
    const char *tag;
    float p012;
    float p021;
    float p102;
    float p120;
    float p201;
    float p210;

    if ((summary == NULL) || (summary->hs8_lane_count < 3u) ||
        (summary->lane0 == NULL) || (summary->lane1 == NULL) ||
        (summary->lane2 == NULL) || (summary->sample_count == 0u)) {
        return 0u;
    }

    p012 = estimate_order_freq_hz(summary, 0u, 1u, 2u);
    p021 = estimate_order_freq_hz(summary, 0u, 2u, 1u);
    p102 = estimate_order_freq_hz(summary, 1u, 0u, 2u);
    p120 = estimate_order_freq_hz(summary, 1u, 2u, 0u);
    p201 = estimate_order_freq_hz(summary, 2u, 0u, 1u);
    p210 = estimate_order_freq_hz(summary, 2u, 1u, 0u);
    tag = (summary->line_tag != NULL) ? summary->line_tag : "ORD";

    return format_line(buf,
                       buf_size,
                       "%s frame=%lu top=%u lc=%u fs=%lu n=%lu ofs=%lu/%lu/%lu cur=%lu p012=%lu p021=%lu p102=%lu p120=%lu p201=%lu p210=%lu\r\n",
                       tag,
                       (unsigned long)summary->frame_id,
                       (unsigned)summary->hs8_topology,
                       (unsigned)summary->hs8_lane_count,
                       (unsigned long)summary->sample_rate_hz,
                       (unsigned long)summary->sample_count,
                       (unsigned long)summary->lane0_start,
                       (unsigned long)summary->lane1_start,
                       (unsigned long)summary->lane2_start,
                       summary->current_freq_valid ? rounded_hz(summary->current_freq_hz) : 0ul,
                       rounded_hz(p012),
                       rounded_hz(p021),
                       rounded_hz(p102),
                       rounded_hz(p120),
                       rounded_hz(p201),
                       rounded_hz(p210));
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_FormatHs8CalLine
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
size_t DSO_SerialDebug_FormatHs8CalLine(const DSO_SerialDebugHs8CalSummary *summary,
                                        char *buf,
                                        size_t buf_size)
{
    if (summary == NULL) {
        return 0u;
    }

    return format_line(buf,
                       buf_size,
                       "[HS8CAL] f=%lu top=%u lc=%u calls=%lu stage=%u good=%u ready=%u l1u=%lu l1f=%u l2u=%lu l2f=%u g1=%.3f o1=%.1f s1=%.2f g2=%.3f o2=%.1f s2=%.2f rep=%.2f lock=%.0f\r\n",
                       (unsigned long)summary->frame_id,
                       (unsigned)summary->hs8_topology,
                       (unsigned)summary->hs8_lane_count,
                       (unsigned long)summary->service_calls,
                       (unsigned)summary->stage,
                       (unsigned)summary->good_frames,
                       summary->ready ? 1u : 0u,
                       (unsigned long)summary->lane1_used,
                       (unsigned)summary->lane1_fail,
                       (unsigned long)summary->lane2_used,
                       (unsigned)summary->lane2_fail,
                       (double)summary->gain1,
                       (double)summary->offset1,
                       (double)summary->skew1,
                       (double)summary->gain2,
                       (double)summary->offset2,
                       (double)summary->skew2,
                       (double)summary->repetitive_confidence,
                       (double)summary->locked_freq_hz);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_FormatFreqLine
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
size_t DSO_SerialDebug_FormatFreqLine(const DSO_FreqMeterSnapshot *snap,
                                      char *buf,
                                      size_t buf_size)
{
    if (snap == NULL) {
        return 0u;
    }

    return format_line(buf,
                       buf_size,
                       "[FREQ] state=%s mode=%s src=PB15 freq_mHz=%llu gate_us=%lu edges=%lu ref=%lu ovf=%lu age_ms=%lu over=%u\r\n",
                       freq_state_name(snap->state),
                       freq_mode_name(snap->mode),
                       (unsigned long long)snap->freq_millihz,
                       (unsigned long)snap->gate_us,
                       (unsigned long)snap->edge_count,
                       (unsigned long)snap->ref_ticks,
                       (unsigned long)snap->overflow_count,
                       (unsigned long)snap->last_edge_age_ms,
                       snap->overrange ? 1u : 0u);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_FormatFreqDebugLine
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
size_t DSO_SerialDebug_FormatFreqDebugLine(const DSO_SerialDebugFreqSummary *summary,
                                           char *buf,
                                           size_t buf_size)
{
    const DSO_FreqMeterSnapshot *snap;
    const DSO_FreqMeterHwTim12DebugSnapshot *hw;

    if (summary == NULL) {
        return 0u;
    }

    snap = &summary->meter;
    hw = &summary->hw;
    if (snap->mode == DSO_FREQ_METER_MODE_LOW_CAPTURE) {
        return format_line(buf,
                           buf_size,
                           "[FM] t=%lu st=%s md=%s v=%u src=PB15 f_mHz=%llu tick_hz=10000000 ticks=%llu periods=%lu cap_irq=%lu upd_irq=%lu age=%lu err=%u cnt=%u cap=%u cr1=0x%08lX smcr=0x%08lX dier=0x%08lX sr=0x%08lX ccmr1=0x%08lX ccer=0x%08lX\r\n",
                           (unsigned long)summary->now_ms,
                           freq_state_name(snap->state),
                           freq_mode_name(snap->mode),
                           snap->valid ? 1u : 0u,
                           (unsigned long long)snap->freq_millihz,
                           (unsigned long long)hw->capture_ticks,
                           (unsigned long)hw->capture_periods,
                           (unsigned long)hw->capture_irq_count,
                           (unsigned long)hw->update_irq_count,
                           (unsigned long)snap->last_edge_age_ms,
                           snap->overrange ? 1u : 0u,
                           (unsigned)hw->counter,
                           (unsigned)hw->capture,
                           (unsigned long)hw->cr1,
                           (unsigned long)hw->smcr,
                           (unsigned long)hw->dier,
                           (unsigned long)hw->sr,
                           (unsigned long)hw->ccmr1,
                           (unsigned long)hw->ccer);
    }

    return format_line(buf,
                       buf_size,
                       "[FM] t=%lu st=%s md=%s v=%u src=PB15 f_mHz=%llu gate=%lu edges=%lu ref=%lu ovf=%lu age=%lu err=%u cnt=%u cr1=0x%08lX smcr=0x%08lX dier=0x%08lX sr=0x%08lX ccmr1=0x%08lX ccer=0x%08lX\r\n",
                       (unsigned long)summary->now_ms,
                       freq_state_name(snap->state),
                       freq_mode_name(snap->mode),
                       snap->valid ? 1u : 0u,
                       (unsigned long long)snap->freq_millihz,
                       (unsigned long)snap->gate_us,
                       (unsigned long)snap->edge_count,
                       (unsigned long)snap->ref_ticks,
                       (unsigned long)snap->overflow_count,
                       (unsigned long)snap->last_edge_age_ms,
                       snap->overrange ? 1u : 0u,
                       (unsigned)hw->counter,
                       (unsigned long)hw->cr1,
                       (unsigned long)hw->smcr,
                       (unsigned long)hw->dier,
                       (unsigned long)hw->sr,
                       (unsigned long)hw->ccmr1,
                       (unsigned long)hw->ccer);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_DecideSnapshot
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_SerialDebug_DecideSnapshot(const DSO_SerialDebugSnapshotInput *in,
                                    DSO_SerialDebugSnapshotDecision *out)
{
    bool freq_jump = false;

    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    if (in == NULL) {
        return;
    }

    if (in->freq_valid && (in->prev_freq_hz > 1.0f) && (in->cur_freq_hz > 1.0f)) {
        float delta = in->cur_freq_hz - in->prev_freq_hz;
        if (delta < 0.0f) {
            delta = -delta;
        }
        freq_jump = (delta / in->prev_freq_hz) > 0.10f;
    }

    out->forced = in->force_snapshot;
    out->clipped = in->clipped;
    out->freq_jump = freq_jump;
    out->recon_changed = (in->prev_recon_mode != in->cur_recon_mode);
    out->ets_toggled = (in->prev_ets_active != in->cur_ets_active);
    out->limited_quality = (in->cur_display_quality != 0u);
    out->emit = out->forced ||
                out->clipped ||
                out->freq_jump ||
                out->recon_changed ||
                out->ets_toggled ||
                out->limited_quality;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_ShouldEmitSnapshot
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_SerialDebug_ShouldEmitSnapshot(const DSO_SerialDebugSnapshotDecision *decision,
                                        uint32_t now_ms,
                                        uint32_t last_snapshot_ms,
                                        uint32_t min_interval_ms)
{
    if ((decision == NULL) || !decision->emit) {
        return false;
    }

    if (decision->forced || (min_interval_ms == 0u)) {
        return true;
    }

    if (last_snapshot_ms == 0u) {
        return true;
    }

    return (uint32_t)(now_ms - last_snapshot_ms) >= min_interval_ms;
}
