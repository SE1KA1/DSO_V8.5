/**
 * @file dso_engine.c
 * @brief DSO_Engine_Task：采集服务、ingest、触发、时基渲染、UI 帧发布。
 */

#include "dso_engine.h"
#include "dso_acq.h"
#include "dso_acquire_control.h"
#include "dso_timebase.h"
#include "dso_8bit_vpp_cal.h"
#include "dso_10bit_display_sync.h"
#include "dso_10bit_lane_cal.h"
#include "dso_freq_meter.h"
#include "dso_serial_debug.h"
#include "dso_serial_debug_format.h"
#include "dso_record.h"
#include "dso_testgen.h"
#include "dso_vertical.h"
#include "dso_vofa_firewater.h"
#include "dso_sample_spec.h"
#include "dso_config.h"
#include "dso_engine_cal.h"
#include "dso_engine_roll.h"
#include "dso_eth_upload.h"
#include "stm32h7xx_hal.h"
#include "bsp_uart_fifo.h"
#include "debug_uart_lock.h"
#include "y_t_stress_ab.h"
#include "FreeRTOS.h"
#include "task.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* 宏定义: DSO_RAM_D1 - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_RAM_D1 __attribute__((section(".RAM_D1")))
/* 宏定义: DSO_RAM_D1_ALIGNED32 - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_RAM_D1_ALIGNED32 __attribute__((section(".RAM_D1"), aligned(32)))
/* 宏定义: DSO_ENGINE_SERVICE_INTERVAL_MS - 时间间隔、周期或时基相关配置常量。 */
#define DSO_ENGINE_SERVICE_INTERVAL_MS 1u
uint32_t s_roll_tb_hold_until_ms;

#if DSO_RUNTIME_LOG_ENABLE
/* 宏定义: DSO_LOG_PRINTF - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_LOG_PRINTF(...) \
    do { \
        DebugUartLock(); \
        printf(__VA_ARGS__); \
        DebugUartUnlock(); \
    } while (0)
#else
/* 宏定义: DSO_LOG_PRINTF - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_LOG_PRINTF(...) do { } while (0)
#endif

#if DSO_ROLL_SERIAL_DIAG_ENABLE || DSO_RUNTIME_LOG_ENABLE
/* 宏定义: DSO_ROLL_LOG - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ROLL_LOG(...) \
    do { \
        DebugUartLock(); \
        printf(__VA_ARGS__); \
        DebugUartUnlock(); \
    } while (0)
static uint32_t s_roll_dbg_copied;
static uint32_t s_roll_dbg_copied_ch2;
#else
/* 宏定义: DSO_ROLL_LOG - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ROLL_LOG(...) do { } while (0)
#endif

#if DSO_VPP_DIAG_ENABLE
static uint32_t s_vpp_diag_last_ms;
#endif

/*
*********************************************************************************************************
*   函 数 名: dso_vofa_send_bytes
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void dso_vofa_send_bytes(const uint8_t *data, uint16_t len)
{
    comSendBuf(COM1, (uint8_t *)data, len);
}

/*
*********************************************************************************************************
*   函 数 名: dso_vofa_tx_ready
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool dso_vofa_tx_ready(void)
{
    return UartTxEmpty(COM1) != 0u;
}

static uint16_t s_engine_recent_samples[DSO_DMA_BUF_SIZE * 2u] DSO_RAM_D1_ALIGNED32;
static uint16_t s_engine_display_samples[DSO_DISPLAY_WIDTH] DSO_RAM_D1;
static uint16_t s_engine_mode_samples[DSO_DMA_BUF_SIZE * 2u] DSO_RAM_D1_ALIGNED32;
static uint16_t s_engine_recent_samples_ch2[DSO_DMA_BUF_SIZE] DSO_RAM_D1_ALIGNED32;
static uint16_t s_engine_display_samples_ch2[DSO_DISPLAY_WIDTH] DSO_RAM_D1;
static uint16_t s_engine_display_samples_hi[DSO_DISPLAY_WIDTH] DSO_RAM_D1;
static uint16_t s_engine_display_samples_ch2_hi[DSO_DISPLAY_WIDTH] DSO_RAM_D1;
static uint16_t s_engine_mid_scratch[DSO_DISPLAY_WIDTH] DSO_RAM_D1;

/*
*********************************************************************************************************
*   函 数 名: engine_display_protect_high_bw_nonroll
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool engine_display_protect_high_bw_nonroll(void)
{
    DSO_TimebaseInfo tb_info;

    DSO_Timebase_GetInfo(&tb_info);
    /* LTDC 保护仅针对 FAST/Y-T：NORMAL 50 us..2 ms 需持续 record/DMA 刷新，不能按 sec/div 一刀切。 */
    return (tb_info.mode == TB_MODE_FAST) &&
           (tb_info.sec_div_ns <= DSO_ENGINE_DISPLAY_PROTECT_MAX_SEC_DIV_NS);
}

/*
*********************************************************************************************************
*   函 数 名: engine_apply_roll_y_mask
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_apply_roll_y_mask(DSO_Frame *frame, bool ch2_present,
                                     uint16_t roll_plot_valid_ch1,
                                     uint16_t roll_plot_valid_ch2)
{
    uint32_t i;
    uint32_t blank;

    if (frame == NULL) {
        return;
    }

    if ((roll_plot_valid_ch1 > 0u) && (roll_plot_valid_ch1 < DSO_DISPLAY_WIDTH)) {
        blank = (uint32_t)DSO_DISPLAY_WIDTH - (uint32_t)roll_plot_valid_ch1;
        for (i = 0u; i < blank; i++) {
            frame->y_coords[i] = INT16_MIN;
            frame->y_column_env_top_ch1[i] = INT16_MIN;
            frame->y_column_env_bot_ch1[i] = INT16_MIN;
        }
    }

    if (ch2_present == 0) {
        return;
    }

    if ((roll_plot_valid_ch2 > 0u) && (roll_plot_valid_ch2 < DSO_DISPLAY_WIDTH)) {
        blank = (uint32_t)DSO_DISPLAY_WIDTH - (uint32_t)roll_plot_valid_ch2;
        for (i = 0u; i < blank; i++) {
            frame->y_coords_ch2[i] = INT16_MIN;
            frame->y_column_env_top_ch2[i] = INT16_MIN;
            frame->y_column_env_bot_ch2[i] = INT16_MIN;
        }
    }
}

/*
*********************************************************************************************************
*   函 数 名: engine_analyze_and_calibrate_ch1
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool engine_analyze_and_calibrate_ch1(uint16_t *samples,
                                             uint32_t count,
                                             uint32_t sample_rate,
                                             const DSO_TriggerConfig *trig_cfg,
                                             DSO_ProcessAnalysis *analysis,
                                             bool service_cal)
{
    DSO_ProcessAnalysis merged_analysis = {0};

    if ((samples == NULL) || (analysis == NULL) || (trig_cfg == NULL)) {
        return false;
    }

    DSO_Process_AnalyzeSamplesEx(samples, count, sample_rate, trig_cfg, analysis);
    merged_analysis = *analysis;
    (void)DSO_EngineCal_RefineLowFreqFromLane0(samples, count, sample_rate, trig_cfg, analysis);

    if (service_cal) {
        DSO_EngineCal_ServiceFrame(samples, count, &merged_analysis, trig_cfg, sample_rate);
    }

    if (!DSO_EngineCal_ApplyCh1IfReady(samples, count)) {
        return false;
    }

    DSO_Process_AnalyzeSamplesEx(samples, count, sample_rate, trig_cfg, analysis);
    (void)DSO_EngineCal_RefineLowFreqFromLane0(samples, count, sample_rate, trig_cfg, analysis);
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: engine_effective_acquire_mode
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static DSO_AcquireMode engine_effective_acquire_mode(DSO_AcquireMode requested_mode,
                                                     DSO_TimebaseMode tb_mode)
{
    if (tb_mode == TB_MODE_ROLL) {
        return (requested_mode == DSO_ACQUIRE_MODE_PEAK_DETECT)
             ? DSO_ACQUIRE_MODE_PEAK_DETECT
             : DSO_ACQUIRE_MODE_SAMPLE;
    }

    return requested_mode;
}

/*
*********************************************************************************************************
*   函 数 名: engine_allow_ets
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool engine_allow_ets(void)
{
    return (DSO_DISABLE_ETS_FOR_SWEEP == 0u);
}

/*
*********************************************************************************************************
*   函 数 名: engine_channel_ac_coupled
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool engine_channel_ac_coupled(DSO_ChannelId channel)
{
    DSO_VerticalInfo vi;

    DSO_Vertical_GetInfoForChannel(channel, &vi);
    return vi.coupling == DSO_COUPLING_AC;
}

/*
*********************************************************************************************************
*   函 数 名: engine_measure_channel
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_measure_channel(DSO_ChannelId channel,
                                   bool roll_mode,
                                   const uint16_t *serial_samples,
                                   uint32_t serial_count,
                                   const uint16_t *display_lo,
                                   const uint16_t *display_hi,
                                   DSO_Frame *out)
{
    if (out == NULL) {
        return;
    }

    if (engine_channel_ac_coupled(channel)) {
        if (roll_mode && (display_lo != NULL)) {
            memcpy(s_engine_mid_scratch, display_lo, DSO_DISPLAY_WIDTH * sizeof(uint16_t));
            DSO_Vertical_ApplyACDisplayFilterForChannel(channel,
                                                        s_engine_mid_scratch,
                                                        DSO_DISPLAY_WIDTH);
            DSO_Process_MeasureSamples(s_engine_mid_scratch, DSO_DISPLAY_WIDTH, out);
        } else {
            DSO_Process_MeasureDisplay(display_lo, display_hi, DSO_DISPLAY_WIDTH, out);
        }
        return;
    }

    if (roll_mode) {
        DSO_Process_MeasureDisplay(display_lo, display_hi, DSO_DISPLAY_WIDTH, out);
        return;
    }

    if (DSO_EngineCal_SupportsRawPreprocess() &&
        (serial_samples != NULL) &&
        (serial_count > 0u)) {
        DSO_Process_MeasureSamples(serial_samples, serial_count, out);
    } else {
        DSO_Process_MeasureDisplay(display_lo, display_hi, DSO_DISPLAY_WIDTH, out);
    }
}

/*
*********************************************************************************************************
*   函 数 名: engine_measure_ch1_into_frame
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_measure_ch1_into_frame(DSO_Frame *frame,
                                          const uint16_t *serial_samples,
                                          uint32_t serial_count,
                                          const uint16_t *display_lo,
                                          DSO_AcquireMode acquire_mode)
{
    const uint16_t *meas_hi = NULL;

    if (frame == NULL) {
        return;
    }
    if (acquire_mode != DSO_ACQUIRE_MODE_AVERAGE) {
        meas_hi = DSO_Timebase_GetLastColumnHiForChannel(0u);
    }
    engine_measure_channel(DSO_CHANNEL_1,
                           false,
                           serial_samples,
                           serial_count,
                           display_lo,
                           meas_hi,
                           frame);
}

/*
*********************************************************************************************************
*   函 数 名: engine_measure_ch2_into_frame
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_measure_ch2_into_frame(DSO_Frame *frame,
                                          const uint16_t *serial_samples,
                                          uint32_t serial_count,
                                          const uint16_t *display_lo,
                                          DSO_AcquireMode acquire_mode)
{
    DSO_Frame ch2_frame = {0};
    const uint16_t *meas_hi = NULL;

    if (frame == NULL) {
        return;
    }
    if (acquire_mode != DSO_ACQUIRE_MODE_AVERAGE) {
        meas_hi = DSO_Timebase_GetLastColumnHiForChannel(1u);
    }
    engine_measure_channel(DSO_CHANNEL_2,
                           false,
                           serial_samples,
                           serial_count,
                           display_lo,
                           meas_hi,
                           &ch2_frame);
    frame->ch2_vmin = ch2_frame.vmin;
    frame->ch2_vmax = ch2_frame.vmax;
    frame->ch2_vpp = ch2_frame.vpp;
    frame->ch2_vrms = ch2_frame.vrms;
    frame->ch2_duty_cycle = ch2_frame.duty_cycle;
}

#if DSO_VPP_DIAG_ENABLE
/*
*********************************************************************************************************
*   函 数 名: engine_tb_mode_name
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static const char *engine_tb_mode_name(DSO_TimebaseMode mode)
{
    switch (mode) {
    case TB_MODE_FAST:
        return "FAST";
    case TB_MODE_NORMAL:
        return "NORMAL";
    case TB_MODE_ROLL:
        return "ROLL";
    default:
        return "?";
    }
}

/*
*********************************************************************************************************
*   函 数 名: engine_vpp_source_name
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static const char *engine_vpp_source_name(DSO_ChannelId channel,
                                          bool roll_mode,
                                          uint32_t serial_count)
{
    if (engine_channel_ac_coupled(channel)) {
        return roll_mode ? "ac_roll_display" : "ac_display";
    }
    if (roll_mode) {
        return "roll_display";
    }
    if (DSO_EngineCal_SupportsRawPreprocess() && (serial_count > 0u)) {
        return "raw";
    }
    return "display";
}

/*
*********************************************************************************************************
*   函 数 名: engine_vpp_source_count
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t engine_vpp_source_count(bool roll_mode,
                                        uint32_t serial_count)
{
    return roll_mode ? (uint32_t)DSO_DISPLAY_WIDTH : serial_count;
}

/*
*********************************************************************************************************
*   函 数 名: engine_median3_u16
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t engine_median3_u16(uint16_t a, uint16_t b, uint16_t c)
{
    if (a > b) {
        uint16_t t = a;
        a = b;
        b = t;
    }
    if (b > c) {
        uint16_t t = b;
        b = c;
        c = t;
    }
    if (a > b) {
        b = a;
    }
    return b;
}

/*
*********************************************************************************************************
*   函 数 名: engine_max3_u16
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t engine_max3_u16(uint16_t a, uint16_t b, uint16_t c)
{
    uint16_t m = (a > b) ? a : b;
    return (m > c) ? m : c;
}

/*
*********************************************************************************************************
*   函 数 名: engine_min3_u16
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t engine_min3_u16(uint16_t a, uint16_t b, uint16_t c)
{
    uint16_t m = (a < b) ? a : b;
    return (m < c) ? m : c;
}

/*
*********************************************************************************************************
*   函 数 名: engine_vpp_scaled_centered
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_vpp_scaled_centered(float min_v,
                                       float max_v,
                                       float pp_v,
                                       float scale,
                                       float *scaled_min_v,
                                       float *scaled_max_v,
                                       float *scaled_pp_v)
{
    float center_v = 0.5f * (min_v + max_v);
    float scaled_pp = pp_v * scale;
    float half_pp = 0.5f * scaled_pp;

    if (scaled_min_v != NULL) {
        *scaled_min_v = center_v - half_pp;
    }
    if (scaled_max_v != NULL) {
        *scaled_max_v = center_v + half_pp;
    }
    if (scaled_pp_v != NULL) {
        *scaled_pp_v = scaled_pp;
    }
}

/*
*********************************************************************************************************
*   函 数 名: engine_clamp_round_code
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t engine_clamp_round_code(float value, uint16_t max_code)
{
    if (value <= 0.0f) {
        return 0u;
    }
    if (value >= (float)max_code) {
        return max_code;
    }
    return (uint16_t)(value + 0.5f);
}

/*
*********************************************************************************************************
*   函 数 名: engine_apply_measure_amp_scale_codes
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_apply_measure_amp_scale_codes(uint16_t *vmin,
                                                 uint16_t *vmax,
                                                 uint16_t *vpp,
                                                 uint16_t max_code,
                                                 float scale)
{
    float min_code;
    float max_code_f;
    float center_code;
    float scaled_pp_code;
    float half_pp_code;
    uint16_t scaled_min;
    uint16_t scaled_max;

    if ((vmin == NULL) || (vmax == NULL) || (vpp == NULL)) {
        return;
    }
    if (*vmax < *vmin) {
        *vpp = 0u;
        return;
    }

    min_code = (float)*vmin;
    max_code_f = (float)*vmax;
    center_code = 0.5f * (min_code + max_code_f);
    scaled_pp_code = (max_code_f - min_code) * scale;
    half_pp_code = 0.5f * scaled_pp_code;
    scaled_min = engine_clamp_round_code(center_code - half_pp_code, max_code);
    scaled_max = engine_clamp_round_code(center_code + half_pp_code, max_code);

    if (scaled_max < scaled_min) {
        scaled_max = scaled_min;
    }

    *vmin = scaled_min;
    *vmax = scaled_max;
    *vpp = (uint16_t)(scaled_max - scaled_min);
}

/*
*********************************************************************************************************
*   函 数 名: engine_apply_measure_amp_scale_to_frame
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_apply_measure_amp_scale_to_frame(DSO_Frame *frame, float scale)
{
    const DSO_SampleSpec *spec;

    if (frame == NULL) {
        return;
    }
    spec = DSO_SampleSpec_GetActive();
    if (spec == NULL) {
        return;
    }

    engine_apply_measure_amp_scale_codes(&frame->vmin,
                                         &frame->vmax,
                                         &frame->vpp,
                                         spec->adc_max,
                                         scale);
    if (frame->ch2_present) {
        engine_apply_measure_amp_scale_codes(&frame->ch2_vmin,
                                             &frame->ch2_vmax,
                                             &frame->ch2_vpp,
                                             spec->adc_max,
                                             scale);
    }
}

/*
*********************************************************************************************************
*   函 数 名: engine_hf_measure_amp_scale
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static float engine_hf_measure_amp_scale(void)
{
    return (DSO_Timebase_GetMode() == TB_MODE_NORMAL) ? DSO_HF_NORMAL_MEASURE_AMP_SCALE
                                                      : DSO_HF_MEASURE_AMP_SCALE;
}

/*
*********************************************************************************************************
*   函 数 名: engine_vpp_cal_freq_hz
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static float engine_vpp_cal_freq_hz(const DSO_Frame *frame)
{
    if (frame == NULL) {
        return 0.0f;
    }
    if (frame->ext_freq_valid && (frame->ext_freq_millihz > 0ull)) {
        return (float)((double)frame->ext_freq_millihz / 1000.0);
    }
    if (frame->freq_valid && (frame->freq_hz > 0.0f)) {
        return frame->freq_hz;
    }
    return 0.0f;
}

/*
*********************************************************************************************************
*   函 数 名: engine_measure_amp_scale_for_frame
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static float engine_measure_amp_scale_for_frame(const DSO_Frame *frame,
                                                bool hf_lane_measurement)
{
    float legacy_scale = hf_lane_measurement ? engine_hf_measure_amp_scale()
                                             : DSO_MEASURE_AMP_SCALE;

    if (DSO_SampleSpec_GetActiveFormat() != DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED) {
        return legacy_scale;
    }

    return DSO_8bitVppCal_ScaleForFreq(engine_vpp_cal_freq_hz(frame),
                                       legacy_scale);
}

/*
*********************************************************************************************************
*   函 数 名: engine_measure_hs8_lane_median
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool engine_measure_hs8_lane_median(const uint16_t *samples,
                                           uint32_t count,
                                           uint8_t lane_count,
                                           uint16_t *out_vmin,
                                           uint16_t *out_vmax,
                                           uint16_t *out_vpp)
{
    uint16_t lane_min[3] = {255u, 255u, 255u};
    uint16_t lane_max[3] = {0u, 0u, 0u};
    uint16_t lane_pp[3];
    uint32_t lane_seen[3] = {0u, 0u, 0u};

    if ((samples == NULL) || (out_vmin == NULL) || (out_vmax == NULL) || (out_vpp == NULL)) {
        return false;
    }
    if ((lane_count < 3u) || (count < (uint32_t)lane_count * 8u)) {
        return false;
    }

    for (uint32_t i = 0u; i < count; i++) {
        uint8_t lane = (uint8_t)(i % lane_count);
        uint16_t sample = samples[i];

        if (lane >= 3u) {
            continue;
        }
        if (sample < lane_min[lane]) {
            lane_min[lane] = sample;
        }
        if (sample > lane_max[lane]) {
            lane_max[lane] = sample;
        }
        lane_seen[lane]++;
    }

    if ((lane_seen[0] == 0u) || (lane_seen[1] == 0u) || (lane_seen[2] == 0u)) {
        return false;
    }

    lane_pp[0] = (uint16_t)(lane_max[0] - lane_min[0]);
    lane_pp[1] = (uint16_t)(lane_max[1] - lane_min[1]);
    lane_pp[2] = (uint16_t)(lane_max[2] - lane_min[2]);
    *out_vmin = engine_median3_u16(lane_min[0], lane_min[1], lane_min[2]);
    *out_vmax = engine_median3_u16(lane_max[0], lane_max[1], lane_max[2]);
    *out_vpp = engine_median3_u16(lane_pp[0], lane_pp[1], lane_pp[2]);
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: engine_apply_hf_lane_measurement_to_frame
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool engine_apply_hf_lane_measurement_to_frame(DSO_Frame *frame,
                                                      const uint16_t *samples,
                                                      uint32_t count)
{
    uint8_t lane_count;
    uint16_t lane_vmin = 0u;
    uint16_t lane_vmax = 0u;
    uint16_t lane_vpp = 0u;

    if ((frame == NULL) || (samples == NULL) || (count == 0u)) {
        return false;
    }
    if (DSO_Timebase_GetMode() == TB_MODE_ROLL) {
        return false;
    }
    if (DSO_SampleSpec_GetActiveFormat() != DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED) {
        return false;
    }

    lane_count = DSO_Acq_GetLaneCount();
    if (lane_count == 0u) {
        const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();
        lane_count = (spec != NULL) ? spec->lane_count : 0u;
    }

    if (engine_measure_hs8_lane_median(samples,
                                       count,
                                       lane_count,
                                       &lane_vmin,
                                       &lane_vmax,
                                       &lane_vpp)) {
        frame->vmin = lane_vmin;
        frame->vmax = lane_vmax;
        frame->vpp = lane_vpp;
        return true;
    }
    return false;
}

/*
*********************************************************************************************************
*   函 数 名: engine_log_vpp_diag_channel
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_log_vpp_diag_channel(uint8_t channel,
                                        const char *source,
                                        uint32_t source_count,
                                        uint32_t sample_rate,
                                        float freq_hz,
                                        bool freq_valid,
                                        uint16_t vmin,
                                        uint16_t vmax,
                                        uint16_t vpp,
                                        float vrms,
                                        const DSO_Frame *frame)
{
    float vpp_v;
    uint32_t ext_hz;

    if (frame == NULL) {
        return;
    }

    vpp_v = (float)vpp * frame->adc_lsb_v;
    ext_hz = frame->ext_freq_valid
           ? (uint32_t)(frame->ext_freq_millihz / 1000ull)
           : 0u;
//    printf("[V] ch=%u src=%s tb=%s n=%lu fs=%lu ext=%lu ev=%u sw=%.0f sv=%u mn=%u mx=%u pp=%u pv=%.3f rms=%.1f div=%lu\r\n",
//           (unsigned)channel,
//           source,
//           engine_tb_mode_name((DSO_TimebaseMode)frame->tb_mode),
//           (unsigned long)source_count,
//           (unsigned long)sample_rate,
//           (unsigned long)ext_hz,
//           frame->ext_freq_valid ? 1u : 0u,
//           (double)freq_hz,
//           freq_valid ? 1u : 0u,
//           (unsigned)vmin,
//           (unsigned)vmax,
//           (unsigned)vpp,
//           (double)vpp_v,
//           (double)vrms,
//           (unsigned long)frame->tb_sec_div_ns);
}

/*
*********************************************************************************************************
*   函 数 名: engine_log_vpp_adc_diag
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_log_vpp_adc_diag(const DSO_Frame *frame)
{
    DSO_AcqDebugSnapshot snap = {0};
    uint16_t lane0_pp;
    uint16_t lane1_pp;
    uint16_t lane2_pp;
    uint16_t lane_min_med;
    uint16_t lane_max_med;
    uint16_t lane_pp_med;
    uint16_t lane_pp_spread;
    float raw_min_v;
    float raw_max_v;
    float raw_pp_v;
    float lane_min_v;
    float lane_max_v;
    float lane_pp_v;
    float scaled_min_v;
    float scaled_max_v;
    float scaled_pp_v;
    float scaled_lane_min_v;
    float scaled_lane_max_v;
    float scaled_lane_pp_v;
    float hf_lane_min_v;
    float hf_lane_max_v;
    float hf_lane_pp_v;

    if (frame == NULL) {
        return;
    }
    if (!DSO_Acq_GetDebugSnapshot(&snap)) {
        return;
    }

    lane0_pp = (snap.lane0_vmax >= snap.lane0_vmin)
             ? (uint16_t)(snap.lane0_vmax - snap.lane0_vmin)
             : 0u;
    lane1_pp = (snap.lane1_vmax >= snap.lane1_vmin)
             ? (uint16_t)(snap.lane1_vmax - snap.lane1_vmin)
             : 0u;
    lane2_pp = (snap.lane2_vmax >= snap.lane2_vmin)
             ? (uint16_t)(snap.lane2_vmax - snap.lane2_vmin)
             : 0u;
    lane_min_med = engine_median3_u16(snap.lane0_vmin, snap.lane1_vmin, snap.lane2_vmin);
    lane_max_med = engine_median3_u16(snap.lane0_vmax, snap.lane1_vmax, snap.lane2_vmax);
    lane_pp_med = engine_median3_u16(lane0_pp, lane1_pp, lane2_pp);
    lane_pp_spread = (uint16_t)(engine_max3_u16(lane0_pp, lane1_pp, lane2_pp) -
                                engine_min3_u16(lane0_pp, lane1_pp, lane2_pp));
    raw_min_v = (float)frame->vmin * frame->adc_lsb_v;
    raw_max_v = (float)frame->vmax * frame->adc_lsb_v;
    raw_pp_v = (float)frame->vpp * frame->adc_lsb_v;
    lane_min_v = (float)lane_min_med * frame->adc_lsb_v;
    lane_max_v = (float)lane_max_med * frame->adc_lsb_v;
    lane_pp_v = (float)lane_pp_med * frame->adc_lsb_v;
    engine_vpp_scaled_centered(raw_min_v,
                               raw_max_v,
                               raw_pp_v,
                               DSO_VPP_DIAG_AMP_SCALE,
                               &scaled_min_v,
                               &scaled_max_v,
                               &scaled_pp_v);
    engine_vpp_scaled_centered(lane_min_v,
                               lane_max_v,
                               lane_pp_v,
                               DSO_VPP_DIAG_AMP_SCALE,
                               &scaled_lane_min_v,
                               &scaled_lane_max_v,
                               &scaled_lane_pp_v);
    engine_vpp_scaled_centered(lane_min_v,
                               lane_max_v,
                               lane_pp_v,
                               DSO_HF_MEASURE_AMP_SCALE,
                               &hf_lane_min_v,
                               &hf_lane_max_v,
                               &hf_lane_pp_v);

//    printf("[VA] fmt=%u bits=%u lsb=%.3fmV cfgr=%08lX/%08lX/%08lX lpp=%u/%u/%u lmn=%u/%u/%u lmx=%u/%u/%u\r\n",
//           (unsigned)snap.format,
//           (unsigned)frame->adc_bits,
//           (double)(frame->adc_lsb_v * 1000.0f),
//           (unsigned long)snap.adc1_cfgr,
//           (unsigned long)snap.adc2_cfgr,
//           (unsigned long)snap.adc3_cfgr,
//           (unsigned)lane0_pp,
//           (unsigned)lane1_pp,
//           (unsigned)lane2_pp,
//           (unsigned)snap.lane0_vmin,
//           (unsigned)snap.lane1_vmin,
//           (unsigned)snap.lane2_vmin,
//           (unsigned)snap.lane0_vmax,
//           (unsigned)snap.lane1_vmax,
//           (unsigned)snap.lane2_vmax);
//    printf("[VM] ch=1 mmn=%u mmx=%u mpp=%u mv=%.3f/%.3f/%.3f lmn=%u lmx=%u lpp=%u lv=%.3f/%.3f/%.3f spr=%u\r\n",
//           (unsigned)frame->vmin,
//           (unsigned)frame->vmax,
//           (unsigned)frame->vpp,
//           (double)((float)frame->vmin * frame->adc_lsb_v),
//           (double)((float)frame->vmax * frame->adc_lsb_v),
//           (double)((float)frame->vpp * frame->adc_lsb_v),
//           (unsigned)lane_min_med,
//           (unsigned)lane_max_med,
//           (unsigned)lane_pp_med,
//           (double)((float)lane_min_med * frame->adc_lsb_v),
//           (double)((float)lane_max_med * frame->adc_lsb_v),
//           (double)((float)lane_pp_med * frame->adc_lsb_v),
//           (unsigned)lane_pp_spread);
//    printf("[VS] ch=1 sc=%.3f smv=%.3f/%.3f/%.3f slv=%.3f/%.3f/%.3f\r\n",
//           (double)DSO_VPP_DIAG_AMP_SCALE,
//           (double)scaled_min_v,
//           (double)scaled_max_v,
//           (double)scaled_pp_v,
//           (double)scaled_lane_min_v,
//           (double)scaled_lane_max_v,
//           (double)scaled_lane_pp_v);
//    printf("[VH] ch=1 sc=%.3f hlv=%.3f/%.3f/%.3f\r\n",
//           (double)DSO_HF_MEASURE_AMP_SCALE,
//           (double)hf_lane_min_v,
//           (double)hf_lane_max_v,
//           (double)hf_lane_pp_v);
}

/*
*********************************************************************************************************
*   函 数 名: engine_log_vpp_diag_frame
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_log_vpp_diag_frame(uint32_t now_ms,
                                      bool roll_mode,
                                      const DSO_Frame *frame,
                                      const DSO_ProcessAnalysis *analysis_ch1,
                                      uint32_t source_count_ch1,
                                      const DSO_ProcessAnalysis *analysis_ch2,
                                      uint32_t source_count_ch2)
{
    if ((now_ms - s_vpp_diag_last_ms) < DSO_VPP_DIAG_INTERVAL_MS) {
        return;
    }

    s_vpp_diag_last_ms = now_ms;

    DebugUartLock();
    engine_log_vpp_diag_channel(1u,
                                engine_vpp_source_name(DSO_CHANNEL_1,
                                                       roll_mode,
                                                       source_count_ch1),
                                engine_vpp_source_count(roll_mode, source_count_ch1),
                                frame->sample_rate,
                                analysis_ch1 != NULL ? analysis_ch1->freq_hz : 0.0f,
                                (analysis_ch1 != NULL) && analysis_ch1->freq_valid,
                                frame->vmin,
                                frame->vmax,
                                frame->vpp,
                                frame->vrms,
                                frame);
    engine_log_vpp_adc_diag(frame);
    if (frame->ch2_present) {
        engine_log_vpp_diag_channel(2u,
                                    engine_vpp_source_name(DSO_CHANNEL_2,
                                                           roll_mode,
                                                           source_count_ch2),
                                    engine_vpp_source_count(roll_mode, source_count_ch2),
                                    frame->ch2_sample_rate,
                                    analysis_ch2 != NULL ? analysis_ch2->freq_hz : 0.0f,
                                    (analysis_ch2 != NULL) && analysis_ch2->freq_valid,
                                    frame->ch2_vmin,
                                    frame->ch2_vmax,
                                    frame->ch2_vpp,
                                    frame->ch2_vrms,
                                    frame);
    }
    DebugUartUnlock();
}

/*
*********************************************************************************************************
*   函 数 名: engine_log_vpp_publish_frame
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_log_vpp_publish_frame(const DSO_Frame *frame,
                                         float measure_scale,
                                         bool hf_lane_measurement)
{
    if (frame == NULL) {
        return;
    }

    DebugUartLock();
//    printf("[VP] ch=1 sc=%.3f hf=%u pub=%.3f/%.3f/%.3f code=%u/%u/%u tb=%s\r\n",
//           (double)measure_scale,
//           hf_lane_measurement ? 1u : 0u,
//           (double)((float)frame->vmin * frame->adc_lsb_v),
//           (double)((float)frame->vmax * frame->adc_lsb_v),
//           (double)((float)frame->vpp * frame->adc_lsb_v),
//           (unsigned)frame->vmin,
//           (unsigned)frame->vmax,
//           (unsigned)frame->vpp,
//           engine_tb_mode_name((DSO_TimebaseMode)frame->tb_mode));
    DebugUartUnlock();
}
#endif

/*
*********************************************************************************************************
*   函 数 名: engine_ac_filter_display_source
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static const uint16_t *engine_ac_filter_display_source(DSO_ChannelId channel,
                                                       const uint16_t *samples,
                                                       uint32_t count,
                                                       uint16_t *scratch_primary,
                                                       uint16_t *scratch_alt,
                                                       uint32_t scratch_words)
{
    DSO_VerticalInfo vi;
    uint16_t *scratch;

    if ((samples == NULL) || (count == 0u)) {
        return samples;
    }

    DSO_Vertical_GetInfoForChannel(channel, &vi);
    if (vi.coupling != DSO_COUPLING_AC) {
        return samples;
    }

    scratch = (samples == scratch_primary) ? scratch_alt : scratch_primary;
    if ((scratch == NULL) || (count > scratch_words)) {
        return samples;
    }

    if (samples != scratch) {
        memcpy(scratch, samples, count * sizeof(uint16_t));
    }
    DSO_Vertical_ApplyACDisplayFilterForChannel(channel, scratch, count);
    return scratch;
}

/*
*********************************************************************************************************
*   函 数 名: engine_render_ch1_display
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static const uint16_t *engine_render_ch1_display(const uint16_t *render_source_samples,
                                                 uint32_t render_source_count,
                                                 uint32_t render_sample_rate,
                                                 DSO_ProcessAnalysis *render_analysis,
                                                 uint8_t trig_position_pct,
                                                 DSO_AcquireMode acquire_mode,
                                                 DSO_RenderMeta *render_meta)
{
    const uint16_t *filtered;

    filtered = engine_ac_filter_display_source(DSO_CHANNEL_1,
                                               render_source_samples,
                                               render_source_count,
                                               s_engine_mode_samples,
                                               s_engine_recent_samples,
                                               DSO_DMA_BUF_SIZE * 2u);
    return DSO_Timebase_RenderEx(filtered,
                                 render_source_count,
                                 render_sample_rate,
                                 render_analysis,
                                 trig_position_pct,
                                 acquire_mode,
                                 render_meta);
}

/*
*********************************************************************************************************
*   函 数 名: engine_render_ch2_display
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_render_ch2_display(const uint16_t *samples,
                                      uint32_t count,
                                      uint32_t sample_rate_hz,
                                      const DSO_ProcessAnalysis *analysis,
                                      uint8_t trig_position_pct,
                                      DSO_AcquireMode acquire_mode,
                                      uint16_t *out_lo,
                                      uint16_t *out_hi,
                                      DSO_RenderMeta *out_meta)
{
    const uint16_t *filtered;

    filtered = engine_ac_filter_display_source(DSO_CHANNEL_2,
                                               samples,
                                               count,
                                               s_engine_mode_samples,
                                               s_engine_recent_samples_ch2,
                                               DSO_DMA_BUF_SIZE * 2u);
    DSO_Timebase_RenderToBufferForChannel(1u,
                                          filtered,
                                          count,
                                          sample_rate_hz,
                                          analysis,
                                          trig_position_pct,
                                          acquire_mode,
                                          engine_allow_ets(),
                                          out_lo,
                                          out_hi,
                                          out_meta);
}

/*
*********************************************************************************************************
*   函 数 名: engine_preserve_processed_trigger
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_preserve_processed_trigger(const DSO_ProcessAnalysis *raw_analysis,
                                              uint32_t raw_count,
                                              uint32_t processed_count,
                                              DSO_ProcessAnalysis *processed_analysis)
{
    float raw_trigger_pos;
    float scale;
    float proc_trigger_pos;

    if ((raw_analysis == NULL) || (processed_analysis == NULL) ||
        !raw_analysis->triggered || processed_analysis->triggered ||
        (raw_count == 0u) || (processed_count == 0u)) {
        return;
    }

    raw_trigger_pos = raw_analysis->trigger_pos_samples;
    if (raw_trigger_pos < 0.0f) {
        if (raw_analysis->trigger_index < 0) {
            return;
        }
        raw_trigger_pos = (float)raw_analysis->trigger_index;
    }

    scale = (float)processed_count / (float)raw_count;
    proc_trigger_pos = raw_trigger_pos * scale;
    if (proc_trigger_pos < 0.0f) {
        proc_trigger_pos = 0.0f;
    }
    if (proc_trigger_pos > (float)(processed_count - 1u)) {
        proc_trigger_pos = (float)(processed_count - 1u);
    }

    processed_analysis->triggered = true;
    processed_analysis->trigger_pos_samples = proc_trigger_pos;
    processed_analysis->trigger_index = (int32_t)proc_trigger_pos;
}

/*
*********************************************************************************************************
*   函 数 名: engine_prepare_ch1_render_source
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static const uint16_t *engine_prepare_ch1_render_source(const uint16_t *raw_samples,
                                                        uint32_t raw_count,
                                                        uint32_t raw_sample_rate,
                                                        DSO_AcquireMode acquire_mode,
                                                        const DSO_TriggerConfig *trig_cfg,
                                                        const DSO_ProcessAnalysis *raw_analysis,
                                                        uint32_t *out_count,
                                                        uint32_t *out_sample_rate,
                                                        DSO_ProcessAnalysis *out_analysis)
{
    uint32_t processed_rate = raw_sample_rate;
    uint32_t processed_count = raw_count;
    const uint16_t *processed_samples = raw_samples;
    DSO_ProcessAnalysis processed_analysis = {0};

    if (out_count != NULL) {
        *out_count = 0u;
    }
    if (out_sample_rate != NULL) {
        *out_sample_rate = raw_sample_rate;
    }
    if (out_analysis != NULL) {
        memset(out_analysis, 0, sizeof(*out_analysis));
        out_analysis->trigger_index = -1;
        out_analysis->trigger_pos_samples = -1.0f;
    }

    if ((raw_samples == NULL) || (raw_count == 0u) || (trig_cfg == NULL) || (out_analysis == NULL)) {
        return raw_samples;
    }

    if (DSO_EngineCal_SupportsRawPreprocess()) {
        if (acquire_mode == DSO_ACQUIRE_MODE_AVERAGE) {
            processed_samples = DSO_AcquireCtrl_ApplyAverageForChannel(0u,
                                                                       raw_samples,
                                                                       raw_count);
        } else if (acquire_mode == DSO_ACQUIRE_MODE_HIRES) {
            processed_count = DSO_Timebase_HiResBoxcar(raw_samples,
                                                       raw_count,
                                                       raw_sample_rate,
                                                       s_engine_mode_samples,
                                                       DSO_DMA_BUF_SIZE * 2u,
                                                       &processed_rate);
            if (processed_count > 0u) {
                processed_samples = s_engine_mode_samples;
            } else {
                processed_samples = raw_samples;
                processed_count = raw_count;
                processed_rate = raw_sample_rate;
            }
        }
    }

    DSO_Process_AnalyzeSamplesEx(processed_samples,
                                 processed_count,
                                 processed_rate,
                                 trig_cfg,
                                 &processed_analysis);
    engine_preserve_processed_trigger(raw_analysis,
                                      raw_count,
                                      processed_count,
                                      &processed_analysis);

    *out_analysis = processed_analysis;
    if (out_count != NULL) {
        *out_count = processed_count;
    }
    if (out_sample_rate != NULL) {
        *out_sample_rate = processed_rate;
    }
    return processed_samples;
}

/*
*********************************************************************************************************
*   函 数 名: acq_ctrl_requires_record_reset
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool acq_ctrl_requires_record_reset(const DSO_AcquireCtrlInfo *prev,
                                           const DSO_AcquireCtrlInfo *cur)
{
    if ((prev == NULL) || (cur == NULL)) {
        return true;
    }

    return (prev->trigger_mode != cur->trigger_mode) ||
           (prev->trigger_source != cur->trigger_source) ||
           (prev->acquire_mode != cur->acquire_mode) ||
           (prev->record_depth_index != cur->record_depth_index) ||
           (prev->pretrigger_pct != cur->pretrigger_pct);
}

/*
*********************************************************************************************************
*   函 数 名: engine_effective_trigger_source
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static DSO_TriggerSource engine_effective_trigger_source(const DSO_AcquireCtrlInfo *acq_info,
                                                         bool ch2_available)
{
    if ((acq_info != NULL) &&
        (acq_info->trigger_source == DSO_TRIGGER_SOURCE_CH2) &&
        ch2_available) {
        return DSO_TRIGGER_SOURCE_CH2;
    }

    return DSO_TRIGGER_SOURCE_CH1;
}

/*
*********************************************************************************************************
*   函 数 名: engine_active_analysis_for_source
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static const DSO_ProcessAnalysis *engine_active_analysis_for_source(DSO_TriggerSource source,
                                                                    const DSO_ProcessAnalysis *analysis_ch1,
                                                                    const DSO_ProcessAnalysis *analysis_ch2)
{
    if ((source == DSO_TRIGGER_SOURCE_CH2) && (analysis_ch2 != NULL)) {
        return analysis_ch2;
    }

    return analysis_ch1;
}

/*
*********************************************************************************************************
*   函 数 名: engine_scale_total_index
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint64_t engine_scale_total_index(uint64_t total_index,
                                         uint32_t src_rate_hz,
                                         uint32_t dst_rate_hz)
{
    if ((src_rate_hz == 0u) || (dst_rate_hz == 0u)) {
        return 0u;
    }

    return (total_index * (uint64_t)dst_rate_hz) / (uint64_t)src_rate_hz;
}

/*
*********************************************************************************************************
*   函 数 名: engine_prepare_render_analyses
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_prepare_render_analyses(DSO_TriggerSource source,
                                           const DSO_ProcessAnalysis *analysis_ch1,
                                           uint32_t ch1_sample_rate_hz,
                                           uint32_t ch1_sample_count,
                                           const DSO_ProcessAnalysis *analysis_ch2,
                                           uint32_t ch2_sample_rate_hz,
                                           uint32_t ch2_sample_count,
                                           DSO_ProcessAnalysis *render_analysis_ch1,
                                           DSO_ProcessAnalysis *render_analysis_ch2)
{
    if ((render_analysis_ch1 == NULL) || (render_analysis_ch2 == NULL)) {
        return;
    }

    if (analysis_ch1 != NULL) {
        *render_analysis_ch1 = *analysis_ch1;
    } else {
        memset(render_analysis_ch1, 0, sizeof(*render_analysis_ch1));
        render_analysis_ch1->trigger_index = -1;
        render_analysis_ch1->trigger_pos_samples = -1.0f;
    }

    if (analysis_ch2 != NULL) {
        *render_analysis_ch2 = *analysis_ch2;
    } else {
        memset(render_analysis_ch2, 0, sizeof(*render_analysis_ch2));
        render_analysis_ch2->trigger_index = -1;
        render_analysis_ch2->trigger_pos_samples = -1.0f;
    }

    if ((source == DSO_TRIGGER_SOURCE_CH2) &&
        (analysis_ch2 != NULL) &&
        (ch1_sample_count > 0u)) {
        (void)DSO_TriggerDisplaySync_MapByRate(analysis_ch2,
                                               ch2_sample_rate_hz,
                                               ch1_sample_rate_hz,
                                               ch1_sample_count,
                                               render_analysis_ch1);
        return;
    }

    if ((analysis_ch1 != NULL) && (ch2_sample_count > 0u)) {
        (void)DSO_TriggerDisplaySync_MapByRate(analysis_ch1,
                                               ch1_sample_rate_hz,
                                               ch2_sample_rate_hz,
                                               ch2_sample_count,
                                               render_analysis_ch2);
    }
}

/*
*********************************************************************************************************
*   函 数 名: engine_log_acq_hold
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_log_acq_hold(uint32_t frame_id,
                                const char *reason,
                                uint32_t pos,
                                uint32_t pos_ch2)
{
#if DSO_SERIAL_DEBUG_ENABLE
    static char line[512];
    DSO_AcqDebugSnapshot snap;
    bool snap_valid;
    int written;

    memset(&snap, 0, sizeof(snap));
    snap_valid = DSO_Acq_GetDebugSnapshot(&snap);

    written = snprintf(line,
                       sizeof(line),
                       "[ACQREADY] f=%lu reason=%s fmt=%u ready=%u rlanes=%u pos=%lu ch2pos=%lu snap=%u spos=%lu top=%u lc=%u lpos=%lu/%lu/%lu d=%lu/%lu/%lu cd=%lu phase_err=%u ovr=%lu tim_arr=%lu\r\n",
                       (unsigned long)frame_id,
                       (reason != NULL) ? reason : "unknown",
                       (unsigned)DSO_Acq_GetSampleFormat(),
                       snap.data_ready ? 1u : 0u,
                       (unsigned)snap.data_ready_lane_count,
                       (unsigned long)pos,
                       (unsigned long)pos_ch2,
                       snap_valid ? 1u : 0u,
                       (unsigned long)snap.dma_pos,
                       (unsigned)snap.hs8_topology,
                       (unsigned)snap.hs8_lane_count,
                       (unsigned long)snap.lane0_pos,
                       (unsigned long)snap.lane1_pos,
                       (unsigned long)snap.lane2_pos,
                       (unsigned long)snap.lane0_delta,
                       (unsigned long)snap.lane1_delta,
                       (unsigned long)snap.lane2_delta,
                       (unsigned long)snap.common_delta,
                       snap.phase_sync_error ? 1u : 0u,
                       (unsigned long)snap.overrun_count,
                       (unsigned long)DSO_Acq_GetTimArr());
    if (written > 0) {
        DSO_SerialDebug_SendLine(line);
        DSO_ETH_Upload_SendLine(line);
    }
#else
    (void)frame_id;
    (void)reason;
    (void)pos;
    (void)pos_ch2;
#endif
}

/*
*********************************************************************************************************
*   函 数 名: engine_hold_until_acq_ready
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_hold_until_acq_ready(uint32_t frame_id,
                                        const char *reason,
                                        uint32_t pos,
                                        uint32_t pos_ch2,
                                        uint32_t *ingest_prev_pos,
                                        uint32_t *ingest_prev_pos_ch2,
                                        uint32_t *roll_prev_pos,
                                        uint32_t *roll_prev_pos_ch2)
{
    if (ingest_prev_pos != NULL) {
        *ingest_prev_pos = pos;
    }
    if (ingest_prev_pos_ch2 != NULL) {
        *ingest_prev_pos_ch2 = pos_ch2;
    }
    if (roll_prev_pos != NULL) {
        *roll_prev_pos = pos;
    }
    if (roll_prev_pos_ch2 != NULL) {
        *roll_prev_pos_ch2 = pos_ch2;
    }
    DSO_Timebase_ResetRollColumnState();
    engine_log_acq_hold(frame_id, reason, pos, pos_ch2);
}

/*
*********************************************************************************************************
*   函 数 名: engine_align_dma_cursors
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_align_dma_cursors(uint32_t *ingest_prev_pos,
                                     uint32_t *ingest_prev_pos_ch2,
                                     uint32_t *roll_prev_pos,
                                     uint32_t *roll_prev_pos_ch2,
                                     bool ch2_available)
{
    uint32_t pos = DSO_Acq_GetPos();
    uint32_t pos_ch2 = ch2_available ? DSO_Acq_GetCh2Pos() : 0u;

    if (ingest_prev_pos != NULL) {
        *ingest_prev_pos = pos;
    }
    if (ingest_prev_pos_ch2 != NULL) {
        *ingest_prev_pos_ch2 = pos_ch2;
    }
    if (roll_prev_pos != NULL) {
        *roll_prev_pos = pos;
    }
    if (roll_prev_pos_ch2 != NULL) {
        *roll_prev_pos_ch2 = pos_ch2;
    }
    DSO_EngineRoll_RingFeedReset();
}

/*
*********************************************************************************************************
*   函 数 名: engine_record_target_samples
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t engine_record_target_samples(uint32_t window_samples)
{
    uint32_t target = window_samples;

    if (target > (DSO_DMA_BUF_SIZE * 2u)) {
        target = DSO_DMA_BUF_SIZE * 2u;
    }
    if (target < 32u) {
        target = 32u;
    }
    return target;
}

/*
*********************************************************************************************************
*   函 数 名: engine_record_view_ready
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool engine_record_view_ready(uint32_t available, uint32_t window_samples)
{
    if (available == 0u) {
        return false;
    }

    /* RUN: wait until record holds the full displayable window; otherwise
     * copied_view ramps up frame-by-frame and the renderer stretches a
     * growing slice across 608 px — the trace looks progressively wider,
     * then slowly settles when valid_count reaches the target. */
    return available >= engine_record_target_samples(window_samples);
}

/*
*********************************************************************************************************
*   函 数 名: fill_frame_metadata
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fill_frame_metadata(DSO_Frame *frame)
{
    const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();
    DSO_TimebaseInfo tb_info;
    DSO_VerticalInfo v_info_ch1;
    DSO_VerticalInfo v_info_ch2;
    DSO_AcquireCtrlInfo acq_info;
    DSO_RecordInfo record_info;
    DSO_10bitLaneCalInfo cal_info = {0};

    DSO_Timebase_GetInfo(&tb_info);
    DSO_Vertical_GetInfoForChannel(DSO_CHANNEL_1, &v_info_ch1);
    DSO_Vertical_GetInfoForChannel(DSO_CHANNEL_2, &v_info_ch2);
    DSO_AcquireCtrl_GetInfo(&acq_info);
    DSO_Record_GetInfo(&record_info);
    if (DSO_EngineCal_Is10bitActive()) {
        DSO_10bitLaneCal_GetInfo(&cal_info);
    }

    frame->adc_bits = spec->adc_bits;
    frame->adc_lsb_v = spec->vref / (float)spec->adc_max;

    frame->tb_sec_div_ns = tb_info.sec_div_ns;
    frame->tb_mode = (uint8_t)tb_info.mode;
    frame->tb_time_per_pixel_ns = tb_info.time_per_pixel_ns;

    frame->vert_mv_per_div = v_info_ch1.mv_per_div;
    frame->vert_scale_min_v = v_info_ch1.scale_min_v;
    frame->vert_scale_max_v = v_info_ch1.scale_max_v;
    frame->ch2_vert_mv_per_div = v_info_ch2.mv_per_div;
    frame->ch2_vert_scale_min_v = v_info_ch2.scale_min_v;
    frame->ch2_vert_scale_max_v = v_info_ch2.scale_max_v;
    frame->trig_mode = (uint8_t)acq_info.trigger_mode;
    frame->trig_source = (uint8_t)acq_info.trigger_source;
    frame->trig_level_v = acq_info.trigger_level_v;
    frame->trig_level_enabled = acq_info.trigger_level_enabled;
    if (DSO_SampleSpec_GetActiveFormat() == DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED) {
        frame->ch1_interleave_cal_valid = DSO_EngineCal_IsCh1Ready();
        frame->cal_state = 0u;
        frame->cal_progress_pct = 0u;
    } else {
        frame->ch1_interleave_cal_valid = cal_info.valid;
        frame->cal_state = (uint8_t)cal_info.state;
        frame->cal_progress_pct = cal_info.progress_pct;
    }
}

#if DSO_DEBUG_PIPELINE
typedef struct {
    uint16_t vmin;
    uint16_t vmax;
    float vavg;
    uint32_t crossing_count;
    uint32_t first_cross;
    uint32_t last_cross;
    uint32_t clip_low_count;
    uint32_t clip_high_count;
    float clip_low_ratio;
    float clip_high_ratio;
    bool valid;
} ZC_Stats;

static uint16_t s_boot_seq[DSO_DEBUG_ZC_WORDS];

/*
*********************************************************************************************************
*   函 数 名: freq_from_zc_stats
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t freq_from_zc_stats(const ZC_Stats *stats, uint32_t sample_rate_hz);

/*
*********************************************************************************************************
*   函 数 名: measure_dma_transfer_rate_hz
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t measure_dma_transfer_rate_hz(void)
{
    uint32_t prev_pos;
    uint32_t total_delta = 0;

    if ((DSO_DEBUG_DMA_RATE_INTERVAL_MS == 0u) || (DSO_DEBUG_DMA_RATE_SAMPLES == 0u)) {
        return 0u;
    }

    prev_pos = DSO_Acq_GetPos();
    for (uint32_t i = 0; i < DSO_DEBUG_DMA_RATE_SAMPLES; i++) {
        uint32_t pos;
        uint32_t delta;

        vTaskDelay(pdMS_TO_TICKS(DSO_DEBUG_DMA_RATE_INTERVAL_MS));
        pos = DSO_Acq_GetPos();
        delta = (pos >= prev_pos)
              ? (pos - prev_pos)
              : ((DSO_DMA_BUF_SIZE - prev_pos) + pos);
        total_delta += delta;
        prev_pos = pos;
    }

    return (total_delta * 1000u) /
           (DSO_DEBUG_DMA_RATE_SAMPLES * DSO_DEBUG_DMA_RATE_INTERVAL_MS);
}

/*
*********************************************************************************************************
*   函 数 名: fill_dma_lane_sequence_packed_pair
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fill_dma_lane_sequence_packed_pair(const volatile uint32_t *buf, bool use_upper_half)
{
    for (uint32_t i = 0; i < DSO_DEBUG_ZC_WORDS; i++) {
        uint32_t word = buf[i];
        s_boot_seq[i] = use_upper_half
                      ? (uint16_t)((word >> 16) & 0xFFFFu)
                      : (uint16_t)(word & 0xFFFFu);
    }
}

/*
*********************************************************************************************************
*   函 数 名: fill_unpack_sequence_packed_pair
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fill_unpack_sequence_packed_pair(const volatile uint32_t *buf)
{
    for (uint32_t i = 0; i < DSO_DEBUG_ZC_WORDS; i++) {
        uint32_t word = buf[i / 2u];
        s_boot_seq[i] = ((i & 1u) != 0u)
                      ? (uint16_t)((word >> 16) & 0xFFFFu)
                      : (uint16_t)(word & 0xFFFFu);
    }
}

/*
*********************************************************************************************************
*   函 数 名: fill_dma_sequence_single
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fill_dma_sequence_single(const volatile uint32_t *buf)
{
    const volatile uint16_t *buf16 = (const volatile uint16_t *)buf;

    for (uint32_t i = 0; i < DSO_DEBUG_ZC_WORDS; i++) {
        s_boot_seq[i] = buf16[i];
    }
}

/*
*********************************************************************************************************
*   函 数 名: analyze_zc_u16
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static ZC_Stats analyze_zc_u16(const uint16_t *seq, uint32_t count)
{
    ZC_Stats out = {0};
    uint32_t sum = 0;
    const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();
    const uint16_t clip_margin = (spec->adc_max >= 64u) ? (uint16_t)(spec->adc_max / 64u) : 1u;
    const uint16_t clip_low_thresh = clip_margin;
    const uint16_t clip_high_thresh = (spec->adc_max > clip_margin)
                                    ? (uint16_t)(spec->adc_max - clip_margin)
                                    : spec->adc_max;
    const uint16_t vpp_min_runtime = (uint16_t)((spec->adc_max * 3u) / 100u);

    out.vmin = spec->adc_max;
    for (uint32_t i = 0; i < count; i++) {
        uint16_t v = seq[i];
        if (v < out.vmin) out.vmin = v;
        if (v > out.vmax) out.vmax = v;
        if (v <= clip_low_thresh) out.clip_low_count++;
        if (v >= clip_high_thresh) out.clip_high_count++;
        sum += v;
    }
    out.clip_low_ratio = (float)out.clip_low_count / (float)count;
    out.clip_high_ratio = (float)out.clip_high_count / (float)count;

    out.vavg = (float)sum / (float)count;
    if ((uint16_t)(out.vmax - out.vmin) <= vpp_min_runtime) {
        return out;
    }

    {
        float hyst = (float)(out.vmax - out.vmin) * 0.05f;
        if (hyst < 1.0f) hyst = 1.0f;
        float lo = out.vavg - hyst;
        float hi = out.vavg + hyst;
        bool armed = false;

        for (uint32_t i = 0; i < count; i++) {
            float v = (float)seq[i];
            if (!armed) {
                if (v < lo) armed = true;
            } else {
                if (v > hi) {
                    if (out.crossing_count == 0u) out.first_cross = i;
                    out.last_cross = i;
                    out.crossing_count++;
                    armed = false;
                }
            }
        }
    }

    out.valid = (out.crossing_count >= 3u) && (out.last_cross > out.first_cross);
    return out;
}

/*
*********************************************************************************************************
*   函 数 名: stats_input_clipped
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool stats_input_clipped(const ZC_Stats *stats)
{
    const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();
    const uint16_t clip_margin = (spec->adc_max >= 64u) ? (uint16_t)(spec->adc_max / 64u) : 1u;
    uint16_t vpp = (uint16_t)(stats->vmax - stats->vmin);
    uint16_t threshold = (spec->adc_max > (uint16_t)(2u * clip_margin))
                       ? (uint16_t)(spec->adc_max - (2u * clip_margin))
                       : spec->adc_max;
    return (vpp >= threshold);
}

/*
*********************************************************************************************************
*   函 数 名: print_seq_stats
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void print_seq_stats(const char *label,
                            const ZC_Stats *stats,
                            const char *freq_label,
                            uint32_t derived_rate_hz,
                            uint32_t dma_measured_rate_hz)
{
    printf("[BOOT] %s: min=%u max=%u vpp=%u avg=%u xing=%u first=%u last=%u clip_lo=%u(%.2f%%) clip_hi=%u(%.2f%%) input_clipped=%u\r\n",
           label,
           (unsigned)stats->vmin,
           (unsigned)stats->vmax,
           (unsigned)(stats->vmax - stats->vmin),
           (unsigned)stats->vavg,
           (unsigned)stats->crossing_count,
           (unsigned)stats->first_cross,
           (unsigned)stats->last_cross,
           (unsigned)stats->clip_low_count,
           (double)(stats->clip_low_ratio * 100.0f),
           (unsigned)stats->clip_high_count,
           (double)(stats->clip_high_ratio * 100.0f),
           stats_input_clipped(stats) ? 1u : 0u);
    printf("[BOOT] %s: derived=%u Hz dma_measured=%u Hz\r\n",
           freq_label,
           (unsigned)freq_from_zc_stats(stats, derived_rate_hz),
           (unsigned)freq_from_zc_stats(stats, dma_measured_rate_hz));
}

/*
*********************************************************************************************************
*   函 数 名: freq_from_zc_stats
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t freq_from_zc_stats(const ZC_Stats *stats, uint32_t sample_rate_hz)
{
    if (!stats->valid || sample_rate_hz == 0u) {
        return 0u;
    }

    {
        float span_samples = (float)(stats->last_cross - stats->first_cross);
        float span_seconds = span_samples / (float)sample_rate_hz;
        if (span_seconds <= 0.0f) {
            return 0u;
        }
        return (uint32_t)(((float)(stats->crossing_count - 1u) / span_seconds) + 0.5f);
    }
}

/*
*********************************************************************************************************
*   函 数 名: print_u16_sequence
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void print_u16_sequence(const char *label, const uint16_t *seq, uint32_t count)
{
    printf("%s\r\n", label);
    for (uint32_t i = 0; i < count; i++) {
        printf("%u ", (unsigned)seq[i]);
        if ((i + 1u) % 20u == 0u) printf("\r\n");
    }
    if ((count % 20u) != 0u) printf("\r\n");
}

/*
*********************************************************************************************************
*   函 数 名: print_boot_diagnostics
*   功能说明: 在 DSO 主引擎中完成采集服务、测量分析、时基渲染、垂直映射或 UI 帧填充。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void print_boot_diagnostics(const DSO_AcqRuntimeInfo *rt,
                                   uint32_t measured_transfer_rate_hz,
                                   uint32_t measured_sample_rate_hz)
{
    const volatile uint32_t *buf = DSO_Acq_GetBuf();
    int16_t cal = DSO_Acq_GetCalOffset();
    uint32_t pos = DSO_Acq_GetPos();
    uint32_t measured_lane_rate_hz;
    uint32_t total_nyquist_hz;
    uint32_t lane_nyquist_hz;
    ZC_Stats adc1_stats;
    ZC_Stats full_stats;

#if DSO_SINGLE_ADC_TEST
    measured_lane_rate_hz = measured_sample_rate_hz;
#else
    measured_lane_rate_hz = measured_sample_rate_hz / 2u;
#endif
    total_nyquist_hz = measured_sample_rate_hz / 2u;
    lane_nyquist_hz = measured_lane_rate_hz / 2u;

    DebugUartLock();
    printf("[BOOT] DMA pos=%u\r\n", (unsigned)pos);
    printf("[BOOT] CLK: REVID=0x%04lX REV=%s ADCIN=%u ADCCLK=%u trigger_rate=%u derived_transfer_rate=%u dma_measured_transfer_rate=%u derived_sample_rate=%u dma_measured_sample_rate=%u derived_lane_rate=%u dma_measured_lane_rate=%u nyquist_total=%u nyquist_lane=%u\r\n",
           (unsigned long)rt->revid,
           DSO_AcqModel_RevisionName(rt->revision),
           (unsigned)rt->adc_input_clk_hz,
           (unsigned)rt->adc_core_clk_hz,
           (unsigned)rt->trigger_rate_hz,
           (unsigned)rt->lane_rate_hz,
           (unsigned)measured_transfer_rate_hz,
           (unsigned)rt->sample_rate_hz,
           (unsigned)measured_sample_rate_hz,
           (unsigned)rt->lane_rate_hz,
           (unsigned)measured_lane_rate_hz,
           (unsigned)total_nyquist_hz,
           (unsigned)lane_nyquist_hz);
    printf("[BOOT] ACQ: requested_profile=%s active_profile=%s actual_sample_rate=%u lane_rate=%u cal_offset=%d mode=timer-triggered single=%u sweep_direct=%u adc_core_target=%u adc_core_div=%u required_period=%u active_period=%u dmngt=%s profile_valid=%u\r\n",
           rt->requested_profile_name,
           rt->active_profile_name,
           (unsigned)rt->sample_rate_hz,
           (unsigned)rt->lane_rate_hz,
           (int)cal,
           (unsigned)DSO_SINGLE_ADC_TEST,
           (unsigned)DSO_DISABLE_ETS_FOR_SWEEP,
           (unsigned)DSO_ADC_CORE_TARGET_HZ,
           (unsigned)rt->adc_core_divider,
           (unsigned)rt->required_period_clocks,
           (unsigned)rt->period_clocks,
           (DSO_ADC_MASTER_DMNGT_MODE == DSO_ADC_MASTER_DMNGT_DMA_ONESHOT) ? "oneshot" : "circular",
           rt->profile_valid ? 1u : 0u);

#if DSO_SINGLE_ADC_TEST
    printf("[BOOT] Raw ADC1 samples [0..%u]:\r\n",
           (unsigned)(DSO_DEBUG_BOOT_WORDS - 1u));
    {
        const volatile uint16_t *buf16 = (const volatile uint16_t *)buf;
        for (uint32_t i = 0; i < DSO_DEBUG_BOOT_WORDS; i++) {
            printf("  [%2u] %4u\r\n",
                   (unsigned)i,
                   (unsigned)buf16[i]);
        }
    }

    fill_dma_sequence_single(buf);
    print_u16_sequence("[BOOT] ADC1 only (first 40 DMA samples):", s_boot_seq, DSO_DEBUG_BOOT_SEQ_WORDS);
    adc1_stats = analyze_zc_u16(s_boot_seq, DSO_DEBUG_ZC_WORDS);

    print_seq_stats("ADC1 stats", &adc1_stats, "ADC1-only ZC",
                    rt->sample_rate_hz, measured_sample_rate_hz);
#else
    printf("[BOOT] Raw DMA words [0..%u] (hex: ADC2|ADC1):\r\n",
           (unsigned)(DSO_DEBUG_BOOT_WORDS - 1u));
    for (uint32_t i = 0; i < DSO_DEBUG_BOOT_WORDS; i++) {
        uint32_t w = buf[i];
        uint16_t adc1 = (uint16_t)(w & 0xFFFFu);
        uint16_t adc2 = (uint16_t)((w >> 16) & 0xFFFFu);
        printf("  [%2u] 0x%08X  ADC1=%4u  ADC2=%4u  d=%4d  d_cal=%4d\r\n",
               (unsigned)i,
               (unsigned)w,
               (unsigned)adc1,
               (unsigned)adc2,
               (int)adc2 - (int)adc1,
               (int)adc2 - (int)adc1 - (int)cal);
    }

    fill_dma_lane_sequence_packed_pair(buf, false);
    print_u16_sequence("[BOOT] ADC1 only (first 40 DMA words):", s_boot_seq, DSO_DEBUG_BOOT_SEQ_WORDS);
    adc1_stats = analyze_zc_u16(s_boot_seq, DSO_DEBUG_ZC_WORDS);

    fill_dma_lane_sequence_packed_pair(buf, true);
    print_u16_sequence("[BOOT] ADC2 only (first 40 DMA words):", s_boot_seq, DSO_DEBUG_BOOT_SEQ_WORDS);
    {
        ZC_Stats adc2_stats = analyze_zc_u16(s_boot_seq, DSO_DEBUG_ZC_WORDS);
        fill_unpack_sequence_packed_pair(buf);
        full_stats = analyze_zc_u16(s_boot_seq, DSO_DEBUG_ZC_WORDS);

        print_seq_stats("ADC1 stats", &adc1_stats, "ADC1-only ZC",
                        rt->lane_rate_hz, measured_lane_rate_hz);
        print_seq_stats("ADC2 stats", &adc2_stats, "ADC2-only ZC",
                        rt->lane_rate_hz, measured_lane_rate_hz);
        print_seq_stats("FULL stats", &full_stats, "FULL-path ZC",
                        rt->sample_rate_hz, measured_sample_rate_hz);
    }
#endif
    printf("[BOOT] === DIAGNOSTIC DONE ===\r\n");
    DebugUartUnlock();
}
#endif

/*
*********************************************************************************************************
*   函 数 名: DSO_Engine_Task
*   功能说明: 执行 DSO 主引擎任务的采集服务、处理、渲染和 UI 帧发布流程。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Engine_Task(void *param)
{
#if DSO_DEBUG_PIPELINE
    const DSO_AcqRuntimeInfo *rt;
#endif
    DSO_AcquireCtrlInfo prev_acq_info;
    uint32_t ingest_prev_pos;
    uint32_t ingest_prev_pos_ch2;
    uint32_t roll_prev_pos;
    uint32_t roll_prev_pos_ch2;
    uint32_t last_publish_tick;
    bool prev_run_requested;
    uint8_t prev_hs8_topology;
    uint8_t prev_hs8_lane_count;
    bool sample_restart_hold_pending;

    (void)param;

    DSO_Acq_Init();
    DSO_Process_Init();
    DSO_Timebase_Init();
    DSO_AcquireCtrl_Init();
    DSO_Record_Init();
    DSO_Vertical_Init();
    DSO_10bitLaneCal_Init();
    DSO_EngineCal_Init();
    if (DSO_EngineCal_IsHs8Active()) {
        DSO_Timebase_SetHs8CalReadyForChannel(0u, false);
    }
    DSO_FreqMeter_Init();
    DSO_FreqMeter_Start();
    DSO_Acq_SetTimArr(DSO_Timebase_GetTimArr());
    {
        DSO_TimebaseInfo tb_info;
        DSO_Timebase_GetInfo(&tb_info);
        DSO_Vertical_UpdateViewTime(tb_info.time_per_pixel_ns);
    }
    DSO_AcquireCtrl_GetInfo(&prev_acq_info);
    DSO_Record_Reset((DSO_RecordDepth)prev_acq_info.record_depth_index,
                     prev_acq_info.pretrigger_pct,
                     DSO_Acq_GetSampleRate());
#if DSO_TESTGEN_ENABLE
    DSO_TestGen_Start();
#endif

    /* Wait for ADC to stabilize */
    vTaskDelay(pdMS_TO_TICKS(200));

    /* DMA may run during the settle window while ingest cursors are stale. */
    if (DSO_AcquireCtrl_IsRunRequested()) {
        if (!DSO_Acq_IsRunning()) {
            DSO_Acq_Start();
        }
        engine_align_dma_cursors(&ingest_prev_pos,
                                 &ingest_prev_pos_ch2,
                                 &roll_prev_pos,
                                 &roll_prev_pos_ch2,
                                 DSO_Acq_IsCh2Available());
        DSO_Timebase_ResetFastState();
    }

#if DSO_VOFA_FIREWATER_ENABLE
    comSetBaud(COM1, DSO_SERIAL_DEBUG_ENABLE ? DSO_SERIAL_DEBUG_UART_BAUD
                                             : DSO_VOFA_FIREWATER_UART_BAUD);
#endif
    DSO_VofaFirewater_Init(dso_vofa_send_bytes, dso_vofa_tx_ready);
    DSO_SerialDebug_Init();

#if DSO_DEBUG_PIPELINE
    {
        uint32_t measured_transfer_rate_hz = measure_dma_transfer_rate_hz();
        uint32_t measured_sample_rate_hz = DSO_SINGLE_ADC_TEST
                                         ? measured_transfer_rate_hz
                                         : (measured_transfer_rate_hz * 2u);

        rt = DSO_Acq_GetRuntimeInfo();
        print_boot_diagnostics(rt, measured_transfer_rate_hz, measured_sample_rate_hz);
    }
#endif

    static DSO_Frame frame;
    uint32_t count = 0;
    ingest_prev_pos = DSO_Acq_GetPos();
    ingest_prev_pos_ch2 = DSO_Acq_GetCh2Pos();
    roll_prev_pos = DSO_Acq_GetPos();
    roll_prev_pos_ch2 = DSO_Acq_GetCh2Pos();
    last_publish_tick = HAL_GetTick();
    prev_run_requested = DSO_AcquireCtrl_IsRunRequested();
    prev_hs8_topology = DSO_Acq_GetHs8Topology();
    prev_hs8_lane_count = DSO_Acq_GetLaneCount();
    sample_restart_hold_pending = false;

    for (;;) {
        bool format_changed;
        bool tb_changed;
        bool ctrl_changed;
        bool hs8_topology_changed;
        DSO_AcquireCtrlInfo acq_info;
        DSO_RecordInfo record_info;
        DSO_10bitLaneCalInfo cal_info = {0};
        uint32_t now;
        uint32_t pos;
        uint32_t pos_ch2;
        const volatile uint32_t *buf;
        const volatile uint16_t *buf_ch2;
        int16_t cal;
        uint32_t sample_rate;
        uint32_t ch2_sample_rate;
        const uint16_t *display_samples = NULL;
        const uint16_t *display_samples_ch2 = NULL;
        DSO_ProcessAnalysis analysis = {0};
        DSO_ProcessAnalysis analysis_ch2 = {0};
        DSO_ProcessAnalysis render_analysis = {0};
        DSO_ProcessAnalysis render_analysis_ch2 = {0};
        DSO_TriggerConfig trig_cfg = {0};
        DSO_RenderMeta render_meta;
        DSO_RenderMeta render_meta_ch2;
        const uint16_t *serial_source_samples = NULL;
        const uint16_t *serial_source_samples_ch2 = NULL;
        uint32_t serial_source_count = 0u;
        uint32_t serial_source_count_ch2 = 0u;
        const uint16_t *render_source_samples = NULL;
        uint32_t render_source_count = 0u;
        uint32_t render_sample_rate = 0u;
        DSO_AcquireMode render_acquire_mode;
        bool serial_cal_active = false;
        bool publish_frame = true;
        bool force_publish = false;
        bool ch2_available;
        bool ch2_present = false;
        bool ch1_measured_early = false;
        bool ch2_measured_early = false;
        bool run_requested;
        DSO_TriggerSource effective_trigger_source = DSO_TRIGGER_SOURCE_CH1;
        const DSO_ProcessAnalysis *active_analysis = &analysis;
        DSO_TimebaseMode tb_mode_before_apply;
        bool gentle_tb;
        bool roll_ring_coherent_engine;
        bool display_protect_high_bw;
        uint8_t hs8_topology;
        uint8_t hs8_lane_count;

        roll_ring_coherent_engine = false;
        DSO_Acq_Service();
#if (Y_T_AB_DSO_ENGINE_ACQ_SERVICE_ONLY != 0)
        vTaskDelay(pdMS_TO_TICKS(DSO_ENGINE_SERVICE_INTERVAL_MS));
        continue;
#endif
        format_changed = DSO_Acq_ApplyPendingSampleFormat();
        tb_mode_before_apply = DSO_Timebase_GetMode();
        tb_changed = DSO_Timebase_ApplyPending();
        ctrl_changed = DSO_AcquireCtrl_ApplyPending();
        now = HAL_GetTick();
        DSO_FreqMeter_Service(now);
        pos = DSO_Acq_GetPos();
        pos_ch2 = DSO_Acq_GetCh2Pos();
        buf = DSO_Acq_GetBuf();
        buf_ch2 = DSO_Acq_GetCh2Buf();
        cal = DSO_Acq_GetCalOffset();
        sample_rate = DSO_Acq_GetSampleRate();
        ch2_sample_rate = DSO_Acq_GetCh2SampleRate();
        ch2_available = DSO_Acq_IsCh2Available();
        hs8_topology = DSO_Acq_GetHs8Topology();
        hs8_lane_count = DSO_Acq_GetLaneCount();
        hs8_topology_changed = DSO_EngineCal_IsHs8Active() &&
                               ((hs8_topology != prev_hs8_topology) ||
                                (hs8_lane_count != prev_hs8_lane_count));
        if (!ch2_available) {
            pos_ch2 = 0u;
            buf_ch2 = NULL;
            ch2_sample_rate = 0u;
        }

        DSO_AcquireCtrl_GetInfo(&acq_info);
        if ((acq_info.trigger_source == DSO_TRIGGER_SOURCE_CH2) &&
            !ch2_available) {
            if (DSO_AcquireCtrl_NormalizeTriggerSource(DSO_TRIGGER_SOURCE_CH1)) {
                ctrl_changed = true;
                DSO_AcquireCtrl_GetInfo(&acq_info);
            } else {
                acq_info.trigger_source = DSO_TRIGGER_SOURCE_CH1;
            }
        }
        effective_trigger_source = engine_effective_trigger_source(&acq_info, ch2_available);
        active_analysis = engine_active_analysis_for_source(effective_trigger_source,
                                                            &analysis,
                                                            &analysis_ch2);
        DSO_Record_GetInfo(&record_info);
        run_requested = DSO_AcquireCtrl_IsRunRequested();
        if (run_requested && !DSO_Acq_IsRunning()) {
            DSO_Acq_Start();
            pos = DSO_Acq_GetPos();
            pos_ch2 = DSO_Acq_GetCh2Pos();
            engine_align_dma_cursors(&ingest_prev_pos,
                                     &ingest_prev_pos_ch2,
                                     &roll_prev_pos,
                                     &roll_prev_pos_ch2,
                                     ch2_available);
            force_publish = true;
        }
        if (run_requested && !prev_run_requested) {
            pos = DSO_Acq_GetPos();
            pos_ch2 = DSO_Acq_GetCh2Pos();
            if (DSO_Record_IsFrozen()) {
                DSO_Record_Reset((DSO_RecordDepth)acq_info.record_depth_index,
                                 acq_info.pretrigger_pct,
                                 sample_rate);
            } else {
                DSO_Record_ClearTrigger();
            }
            DSO_Record_GetInfo(&record_info);
            DSO_AcquireCtrl_ResetAveraging();
            ingest_prev_pos = pos;
            ingest_prev_pos_ch2 = pos_ch2;
            roll_prev_pos = pos;
            roll_prev_pos_ch2 = pos_ch2;
            force_publish = true;
        }
        if (!run_requested) {
            prev_run_requested = run_requested;
            vTaskDelay(pdMS_TO_TICKS(DSO_ENGINE_SERVICE_INTERVAL_MS));
            continue;
        }
        prev_run_requested = run_requested;
        if (!ch2_available) {
            pos_ch2 = 0u;
        }
        if (DSO_EngineCal_Is10bitActive()) {
            DSO_10bitLaneCal_GetInfo(&cal_info);
        }

        /* Gentle 仅用于 Roll↔Roll 同模式步进；NORMAL/FAST 步进必须复位 record/ingest，否则 50 us..2 ms 会卡帧。 */
        gentle_tb = tb_changed &&
                    !format_changed &&
                    !hs8_topology_changed &&
                    !ctrl_changed &&
                    !acq_ctrl_requires_record_reset(&prev_acq_info, &acq_info) &&
                    (tb_mode_before_apply == TB_MODE_ROLL) &&
                    (DSO_Timebase_GetMode() == TB_MODE_ROLL);

        if (format_changed || hs8_topology_changed || ctrl_changed || (tb_changed && !gentle_tb)) {
            DSO_EngineRoll_RingFeedReset();
        }

        memset(&render_meta, 0, sizeof(render_meta));
        memset(&render_meta_ch2, 0, sizeof(render_meta_ch2));
#if !DSO_SERIAL_DEBUG_CH2_ENABLE
        (void)serial_source_samples_ch2;
        (void)serial_source_count_ch2;
#endif
        analysis.trigger_index = -1;
        analysis.trigger_pos_samples = -1.0f;
        analysis_ch2.trigger_index = -1;
        analysis_ch2.trigger_pos_samples = -1.0f;
        trig_cfg.level_volts = acq_info.trigger_level_v;
        trig_cfg.level_enabled = acq_info.trigger_level_enabled;
        trig_cfg.slope = acq_info.trigger_slope;
        render_acquire_mode = engine_effective_acquire_mode(acq_info.acquire_mode,
                                                            DSO_Timebase_GetMode());
        display_protect_high_bw = engine_display_protect_high_bw_nonroll();

        if (format_changed || hs8_topology_changed || tb_changed || ctrl_changed) {
            DSO_TimebaseInfo tb_info;
            bool hold_captured_record = record_info.frozen && !DSO_Acq_IsRunning();
            bool reset_record = format_changed ||
                                hs8_topology_changed ||
                                acq_ctrl_requires_record_reset(&prev_acq_info, &acq_info) ||
                                (tb_changed && !hold_captured_record && !gentle_tb);

            if (format_changed || hs8_topology_changed) {
                DSO_EngineCal_Reset();
                DSO_Timebase_SetHs8CalReadyForChannel(0u, false);
                sample_restart_hold_pending = true;
            }
            if (format_changed || (tb_changed && !gentle_tb)) {
                sample_restart_hold_pending = true;
            }
            if (!gentle_tb) {
                DSO_Timebase_ResetFastState();
            }
            if (!gentle_tb) {
                DSO_AcquireCtrl_ResetAveraging();
            }
            DSO_Acq_SetTimArr(DSO_Timebase_GetTimArr());
            DSO_Timebase_GetInfo(&tb_info);
            DSO_Vertical_UpdateViewTime(tb_info.time_per_pixel_ns);
            if (gentle_tb && (DSO_Timebase_GetMode() == TB_MODE_ROLL)) {
                uint32_t sr_roll;
                uint32_t sr2_roll;

                /* sample_rate at loop head can be stale vs TIM8 after SetTimArr above. */
                sr_roll = DSO_Acq_GetSampleRate();
                sr2_roll = ch2_available ? DSO_Acq_GetCh2SampleRate() : 0u;
                if (DSO_EngineRoll_ResyncBegin(sr_roll,
                                                sr2_roll,
                                                ch2_available,
                                                &tb_info)) {
                    roll_ring_coherent_engine = true;
                } else {
                    /* Ring seed failed (empty ring, pull buffers, or need cap). Old roll
                     * accumulators target the previous sec/div; keep them and the trace can
                     * appear frozen across Roll-only gentle steps (e.g. 12-bit 20 ms/div → next). */
                    DSO_ROLL_LOG("[DSO] gentle roll step: resync_begin failed; roll column reset\r\n");
                    DSO_Timebase_ResetRollColumnState();
                    /* ResyncBegin already aborts on failure internally */
                }
                /* Align DMA cursor so this frame's RollRender does not re-decimate a huge tail
                 * with reset accumulators (would phase-mismatch the seeded roll_buf). */
                roll_prev_pos = pos;
                roll_prev_pos_ch2 = pos_ch2;
            }
            if (reset_record) {
                DSO_Record_Reset((DSO_RecordDepth)acq_info.record_depth_index,
                                 acq_info.pretrigger_pct,
                                 DSO_Acq_GetSampleRate());
            }
            if (run_requested && !DSO_Acq_IsRunning()) {
                if ((acq_info.trigger_mode != DSO_TRIGGER_MODE_SINGLE) || acq_info.single_armed) {
                    DSO_Acq_Start();
                }
            }
            force_publish = true;
            pos = DSO_Acq_GetPos();
            pos_ch2 = DSO_Acq_GetCh2Pos();
            buf = DSO_Acq_GetBuf();
            buf_ch2 = DSO_Acq_GetCh2Buf();
            cal = DSO_Acq_GetCalOffset();
            sample_rate = DSO_Acq_GetSampleRate();
            ch2_sample_rate = ch2_available ? DSO_Acq_GetCh2SampleRate() : 0u;
            if (!ch2_available) {
                pos_ch2 = 0u;
                buf_ch2 = NULL;
                ch2_sample_rate = 0u;
            }
            if (!gentle_tb) {
                ingest_prev_pos = pos;
                ingest_prev_pos_ch2 = pos_ch2;
                roll_prev_pos = pos;
                roll_prev_pos_ch2 = pos_ch2;
            }
            prev_acq_info = acq_info;
            prev_hs8_topology = hs8_topology;
            prev_hs8_lane_count = hs8_lane_count;
            if (tb_changed &&
                ((tb_mode_before_apply == TB_MODE_ROLL) || (DSO_Timebase_GetMode() == TB_MODE_ROLL))) {
                DSO_EngineRoll_SetTbHold(now + DSO_ROLL_TB_HOLD_MS);
            }
        } else {
            prev_hs8_topology = hs8_topology;
            prev_hs8_lane_count = hs8_lane_count;
        }

        if (DSO_EngineRoll_ResyncService()) {
            roll_ring_coherent_engine = true;
        }

        DSO_EngineRoll_UpdateTbHoldIfResyncing(now);

        memset(&frame, 0, sizeof(frame));
        pos = DSO_Acq_GetPos();
        pos_ch2 = DSO_Acq_GetCh2Pos();
        if (!ch2_available) {
            pos_ch2 = 0u;
        }
        if (sample_restart_hold_pending) {
            if (!DSO_Acq_IsDataReady()) {
                engine_hold_until_acq_ready(count,
                                            "sample_restart",
                                            pos,
                                            pos_ch2,
                                            &ingest_prev_pos,
                                            &ingest_prev_pos_ch2,
                                            &roll_prev_pos,
                                            &roll_prev_pos_ch2);
                vTaskDelay(pdMS_TO_TICKS(DSO_ENGINE_SERVICE_INTERVAL_MS));
                continue;
            }
            sample_restart_hold_pending = false;
            engine_hold_until_acq_ready(count,
                                        "sample_restart",
                                        pos,
                                        pos_ch2,
                                        &ingest_prev_pos,
                                        &ingest_prev_pos_ch2,
                                        &roll_prev_pos,
                                        &roll_prev_pos_ch2);
        }

        if ((DSO_Timebase_GetMode() == TB_MODE_ROLL) ||
            !display_protect_high_bw ||
            (DSO_ENGINE_HIGH_BW_ROLL_PREFEED_ENABLE != 0u)) {
            DSO_EngineRoll_RingFeed(pos,
                                     pos_ch2,
                                     buf,
                                     buf_ch2,
                                     cal,
                                     ch2_available);
        } else {
            DSO_EngineRoll_RingFeedReset();
            roll_prev_pos = pos;
            roll_prev_pos_ch2 = pos_ch2;
        }
        if (DSO_EngineRoll_ResyncInProgress()) {
            force_publish = true;
        }

        if ((DSO_Timebase_GetMode() != TB_MODE_ROLL) && DSO_Acq_IsRunning()) {
            uint32_t copied_new = 0u;
            uint32_t copied_new_ch2 = 0u;

            if (!display_protect_high_bw ||
                (DSO_ENGINE_HIGH_BW_LIVE_RECORD_ENABLE != 0u) ||
                (acq_info.trigger_mode == DSO_TRIGGER_MODE_SINGLE)) {
                copied_new = DSO_Acq_CopyNewSamples(ingest_prev_pos,
                                                    pos,
                                                    buf,
                                                    cal,
                                                    s_engine_recent_samples,
                                                    DSO_DMA_BUF_SIZE * 2u);

                if (ch2_available) {
                    copied_new_ch2 = DSO_Acq_CopyNewCh2Samples(ingest_prev_pos_ch2,
                                                               pos_ch2,
                                                               buf_ch2,
                                                               s_engine_recent_samples_ch2,
                                                               DSO_DMA_BUF_SIZE);
                }
                if (copied_new > 0u && DSO_EngineCal_ApplyCh1IfReady(s_engine_recent_samples, copied_new)) {
                    serial_cal_active = true;
                }
                if (copied_new > 0u) {
                    DSO_Record_Ingest(s_engine_recent_samples, copied_new, sample_rate);
                }
                if (ch2_available && (copied_new_ch2 > 0u)) {
                    DSO_Record_IngestCh2(s_engine_recent_samples_ch2, copied_new_ch2, ch2_sample_rate);
                }
            }
            ingest_prev_pos = pos;
            ingest_prev_pos_ch2 = pos_ch2;
        }

        if (!force_publish && ((now - last_publish_tick) < DSO_ENGINE_FRAME_INTERVAL_MS)) {
            vTaskDelay(pdMS_TO_TICKS(DSO_ENGINE_SERVICE_INTERVAL_MS));
            continue;
        }

        if (DSO_Timebase_GetMode() == TB_MODE_ROLL) {
            /* TIM8/TIM1 period updates run when tb/format/ctrl changes; refresh before RollRender. */
            sample_rate = DSO_Acq_GetSampleRate();
            ch2_sample_rate = ch2_available ? DSO_Acq_GetCh2SampleRate() : 0u;
#if DSO_ROLL_SERIAL_DIAG_ENABLE || DSO_RUNTIME_LOG_ENABLE
            if (format_changed || tb_changed || ctrl_changed) {
                DSO_ROLL_LOG("[DSO] roll: acq_hz ch1=%lu ch2=%lu\r\n",
                             (unsigned long)sample_rate,
                             (unsigned long)ch2_sample_rate);
            }
#endif
            uint32_t copied = DSO_Acq_CopyNewSamples(roll_prev_pos,
                                                     pos,
                                                     buf,
                                                     cal,
                                                     s_engine_recent_samples,
                                                     DSO_DMA_BUF_SIZE * 2u);
            uint32_t copied_ch2 = ch2_available
                                ? DSO_Acq_CopyNewCh2Samples(roll_prev_pos_ch2,
                                                            pos_ch2,
                                                            buf_ch2,
                                                            s_engine_recent_samples_ch2,
                                                            DSO_DMA_BUF_SIZE)
                                : 0u;
            DSO_TimebaseInfo tb_info;
            uint32_t display_rate_hz;

            roll_prev_pos = pos;
            roll_prev_pos_ch2 = pos_ch2;
#if DSO_ROLL_SERIAL_DIAG_ENABLE || DSO_RUNTIME_LOG_ENABLE
            s_roll_dbg_copied = copied;
            s_roll_dbg_copied_ch2 = copied_ch2;
#endif
            if (copied > 0u) {
                DSO_SerialDebug_CaptureRawStage(1u,
                                                count,
                                                s_engine_recent_samples,
                                                copied,
                                                sample_rate,
                                                DSO_EngineCal_IsCh1Ready(),
                                                (uint8_t)cal_info.state);
            }
            if (copied_ch2 > 0u) {
                DSO_SerialDebug_CaptureRawStage(2u,
                                                count,
                                                s_engine_recent_samples_ch2,
                                                copied_ch2,
                                                ch2_sample_rate,
                                                false,
                                                0u);
            }
            if (copied > 0u && DSO_EngineCal_ApplyCh1IfReady(s_engine_recent_samples, copied)) {
                serial_cal_active = true;
            }
            if (copied > 0u) {
                if (DSO_EngineRoll_TbHoldActive(now)) {
                    memset(&analysis, 0, sizeof(analysis));
                    analysis.trigger_index = -1;
                    analysis.trigger_pos_samples = -1.0f;
                } else {
                    DSO_Process_AnalyzeSamplesEx(s_engine_recent_samples,
                                             copied,
                                             sample_rate,
                                             &trig_cfg,
                                             &analysis);
                    DSO_SerialDebug_RefineRawStageFocus(1u,
                                                        s_engine_recent_samples,
                                                        copied,
                                                        analysis.triggered
                                                      ? analysis.trigger_index
                                                      : -1);
                }
            } else {
                memset(&analysis, 0, sizeof(analysis));
                analysis.trigger_index = -1;
                analysis.trigger_pos_samples = -1.0f;
            }
            serial_source_samples = s_engine_recent_samples;
            serial_source_count = copied;
            serial_source_samples_ch2 = s_engine_recent_samples_ch2;
            serial_source_count_ch2 = copied_ch2;
            display_samples = DSO_Timebase_RollRender(s_engine_recent_samples, copied, sample_rate);
            if (ch2_available) {
                if (copied_ch2 > 0u) {
                    if (DSO_EngineRoll_TbHoldActive(now)) {
                        memset(&analysis_ch2, 0, sizeof(analysis_ch2));
                        analysis_ch2.trigger_index = -1;
                        analysis_ch2.trigger_pos_samples = -1.0f;
                    } else {
                        DSO_Process_AnalyzeSamplesEx(s_engine_recent_samples_ch2,
                                                     copied_ch2,
                                                     ch2_sample_rate,
                                                     &trig_cfg,
                                                     &analysis_ch2);
                        DSO_SerialDebug_RefineRawStageFocus(2u,
                                                            s_engine_recent_samples_ch2,
                                                            copied_ch2,
                                                            analysis_ch2.triggered
                                                          ? analysis_ch2.trigger_index
                                                          : -1);
                    }
                } else {
                    memset(&analysis_ch2, 0, sizeof(analysis_ch2));
                    analysis_ch2.trigger_index = -1;
                    analysis_ch2.trigger_pos_samples = -1.0f;
                }
                display_samples_ch2 = DSO_Timebase_RollRenderForChannel(1u,
                                                                        s_engine_recent_samples_ch2,
                                                                        copied_ch2,
                                                                        ch2_sample_rate);
                ch2_present = (copied_ch2 > 0u);
            } else {
                memset(s_engine_display_samples_ch2, 0, sizeof(s_engine_display_samples_ch2));
                display_samples_ch2 = s_engine_display_samples_ch2;
                memset(&render_meta_ch2, 0, sizeof(render_meta_ch2));
            }
            DSO_Timebase_GetInfo(&tb_info);
            display_rate_hz = (tb_info.time_per_pixel_ns > 0.0f)
                            ? (uint32_t)((1.0e9f / tb_info.time_per_pixel_ns) + 0.5f)
                            : 0u;
            frame.triggered = false;
            render_meta.recon_mode = DSO_RECON_MODE_RAW_LINEAR;
            render_meta.display_quality = DSO_DISPLAY_QUALITY_GOOD;
            render_meta.samples_per_cycle = analysis.samples_per_cycle;
            render_meta.effective_sample_rate_hz = (float)display_rate_hz;
            if (ch2_available) {
                memset(&render_meta_ch2, 0, sizeof(render_meta_ch2));
                render_meta_ch2.recon_mode = DSO_RECON_MODE_RAW_LINEAR;
                render_meta_ch2.display_quality = DSO_DISPLAY_QUALITY_GOOD;
                render_meta_ch2.samples_per_cycle = analysis_ch2.samples_per_cycle;
                render_meta_ch2.effective_sample_rate_hz = (float)display_rate_hz;
            }
        } else {
            uint32_t copied_recent = 0u;
            uint32_t copied_recent_ch2 = 0u;
            uint32_t window_samples = DSO_Timebase_GetWindowSampleCount(sample_rate);
            uint32_t ch2_window_samples = DSO_Timebase_GetWindowSampleCount(ch2_sample_rate);
            bool use_record_window = false;
            bool record_anchor_on_trigger = false;
            int32_t record_trigger_index = -1;
            int32_t ch2_trigger_index = -1;

            if (DSO_Acq_IsRunning()) {
                copied_recent = DSO_Acq_CopyRecentSamples(pos,
                                                          buf,
                                                          cal,
                                                          s_engine_recent_samples,
                                                          DSO_DMA_BUF_SIZE * 2u);
                if (copied_recent > 0u) {
                    DSO_SerialDebug_CaptureRawStage(1u,
                                                    count,
                                                    s_engine_recent_samples,
                                                    copied_recent,
                                                    sample_rate,
                                                    DSO_EngineCal_IsCh1Ready(),
                                                    (uint8_t)cal_info.state);
                    if (engine_analyze_and_calibrate_ch1(s_engine_recent_samples,
                                                         copied_recent,
                                                         sample_rate,
                                                         &trig_cfg,
                                                         &analysis,
                                                         true)) {
                        serial_cal_active = true;
                    }
                    DSO_SerialDebug_RefineRawStageFocus(1u,
                                                        s_engine_recent_samples,
                                                        copied_recent,
                                                        analysis.triggered
                                                      ? analysis.trigger_index
                                                      : -1);
                }
            }
            if (ch2_available && DSO_Acq_IsRunning()) {
                copied_recent_ch2 = DSO_Acq_CopyRecentCh2Samples(pos_ch2,
                                                                 buf_ch2,
                                                                 s_engine_recent_samples_ch2,
                                                                 DSO_DMA_BUF_SIZE);
            }
            if (copied_recent_ch2 > 0u) {
                DSO_SerialDebug_CaptureRawStage(2u,
                                                count,
                                                s_engine_recent_samples_ch2,
                                                copied_recent_ch2,
                                                ch2_sample_rate,
                                                false,
                                                0u);
                DSO_Process_AnalyzeSamplesEx(s_engine_recent_samples_ch2,
                                             copied_recent_ch2,
                                             ch2_sample_rate,
                                             &trig_cfg,
                                             &analysis_ch2);
                DSO_SerialDebug_RefineRawStageFocus(2u,
                                                    s_engine_recent_samples_ch2,
                                                    copied_recent_ch2,
                                                    analysis_ch2.triggered
                                                  ? analysis_ch2.trigger_index
                                                  : -1);
                ch2_present = true;
            }
            if (effective_trigger_source == DSO_TRIGGER_SOURCE_CH2) {
                if (analysis_ch2.triggered &&
                    (DSO_Record_GetTotalWrittenCh2() >= copied_recent_ch2)) {
                    uint64_t abs_trigger_ch2 = DSO_Record_GetTotalWrittenCh2()
                                             - (uint64_t)copied_recent_ch2
                                             + (uint64_t)analysis_ch2.trigger_index;
                    uint64_t abs_trigger = engine_scale_total_index(abs_trigger_ch2,
                                                                    ch2_sample_rate,
                                                                    sample_rate);

                    DSO_Record_SetTrigger(abs_trigger);
                    if ((acq_info.trigger_mode == DSO_TRIGGER_MODE_SINGLE) &&
                        !DSO_Record_IsFrozen()) {
                        DSO_Record_BeginSingleCapture(abs_trigger, acq_info.pretrigger_pct);
                    }
                }
            } else if (analysis.triggered && (DSO_Record_GetTotalWritten() >= copied_recent)) {
                uint64_t abs_trigger = DSO_Record_GetTotalWritten()
                                     - (uint64_t)copied_recent
                                     + (uint64_t)analysis.trigger_index;

                DSO_Record_SetTrigger(abs_trigger);
                if ((acq_info.trigger_mode == DSO_TRIGGER_MODE_SINGLE) &&
                    !DSO_Record_IsFrozen()) {
                    DSO_Record_BeginSingleCapture(abs_trigger, acq_info.pretrigger_pct);
                }
            }
            serial_source_samples = s_engine_recent_samples;
            serial_source_count = copied_recent;
            serial_source_samples_ch2 = s_engine_recent_samples_ch2;
            serial_source_count_ch2 = copied_recent_ch2;
            DSO_EngineCal_SyncTimebase();

            use_record_window = !DSO_Acq_IsRunning() ||
                                (acq_info.record_view_offset != 0) ||
                                (window_samples > copied_recent) ||
                                (ch2_available &&
                                 (ch2_window_samples > copied_recent_ch2));
            record_anchor_on_trigger = active_analysis->triggered ||
                                       (!DSO_Acq_IsRunning() && record_info.trigger_valid);

            if (use_record_window) {
                bool try_record_copy;
                uint32_t copied_view = 0u;
                uint32_t copied_view_ch2 = 0u;

                DSO_Record_GetInfo(&record_info);
                try_record_copy = !DSO_Acq_IsRunning() ||
                                  engine_record_view_ready(record_info.total_samples,
                                                           window_samples);

                if (try_record_copy) {
                    copied_view = DSO_Record_CopyViewWindow(window_samples,
                                                            record_anchor_on_trigger,
                                                            acq_info.trigger_position_pct,
                                                            acq_info.record_view_offset,
                                                            s_engine_recent_samples,
                                                            DSO_DMA_BUF_SIZE * 2u,
                                                            &record_trigger_index);
                    if (ch2_available) {
                        copied_view_ch2 = DSO_Record_CopyViewWindowCh2(ch2_window_samples,
                                                                       record_anchor_on_trigger,
                                                                       acq_info.trigger_position_pct,
                                                                       acq_info.record_view_offset,
                                                                       s_engine_recent_samples_ch2,
                                                                       DSO_DMA_BUF_SIZE,
                                                                       &ch2_trigger_index);
                    }
                }
                {
                    bool record_render_ok = (copied_view > 0u) &&
                                            (engine_record_view_ready(copied_view, window_samples) ||
                                             !DSO_Acq_IsRunning());

                    if (record_render_ok) {
                    bool ch2_view_ready = (copied_view_ch2 > 0u) && ch2_available;

                    DSO_SerialDebug_CaptureRawStage(1u,
                                                    count,
                                                    s_engine_recent_samples,
                                                    copied_view,
                                                    sample_rate,
                                                    DSO_EngineCal_IsCh1Ready(),
                                                    (uint8_t)cal_info.state);
                    if (engine_analyze_and_calibrate_ch1(s_engine_recent_samples,
                                                         copied_view,
                                                         sample_rate,
                                                         &trig_cfg,
                                                         &analysis,
                                                         false)) {
                        serial_cal_active = true;
                    }
                    if (record_trigger_index >= 0) {
                        float frac = (analysis.trigger_pos_samples >= 0.0f)
                                   ? (analysis.trigger_pos_samples -
                                      floorf(analysis.trigger_pos_samples))
                                   : 0.0f;
                        analysis.trigger_index = record_trigger_index;
                        analysis.trigger_pos_samples = (float)record_trigger_index + frac;
                        analysis.triggered = true;
                    }

                    if (ch2_view_ready) {
                        DSO_Process_AnalyzeSamplesEx(s_engine_recent_samples_ch2,
                                                     copied_view_ch2,
                                                     ch2_sample_rate,
                                                     &trig_cfg,
                                                     &analysis_ch2);
                        if (ch2_trigger_index >= 0) {
                            float frac_ch2 = (analysis_ch2.trigger_pos_samples >= 0.0f)
                                           ? (analysis_ch2.trigger_pos_samples -
                                              floorf(analysis_ch2.trigger_pos_samples))
                                           : 0.0f;
                            analysis_ch2.trigger_index = ch2_trigger_index;
                            analysis_ch2.trigger_pos_samples = (float)ch2_trigger_index + frac_ch2;
                            analysis_ch2.triggered = true;
                        }
                        DSO_SerialDebug_RefineRawStageFocus(2u,
                                                            s_engine_recent_samples_ch2,
                                                            copied_view_ch2,
                                                            analysis_ch2.triggered
                                                          ? analysis_ch2.trigger_index
                                                          : -1);
                    }

                    engine_prepare_render_analyses(effective_trigger_source,
                                                   &analysis,
                                                   sample_rate,
                                                   copied_view,
                                                   &analysis_ch2,
                                                   ch2_sample_rate,
                                                   copied_view_ch2,
                                                   &render_analysis,
                                                   &render_analysis_ch2);

                    DSO_SerialDebug_RefineRawStageFocus(1u,
                                                        s_engine_recent_samples,
                                                        copied_view,
                                                        analysis.triggered
                                                      ? analysis.trigger_index
                                                      : -1);
                    render_source_samples = engine_prepare_ch1_render_source(s_engine_recent_samples,
                                                                             copied_view,
                                                                             sample_rate,
                                                                             render_acquire_mode,
                                                                             &trig_cfg,
                                                                             &render_analysis,
                                                                             &render_source_count,
                                                                             &render_sample_rate,
                                                                             &render_analysis);
                    serial_source_samples = render_source_samples;
                    serial_source_count = render_source_count;
                    serial_source_samples_ch2 = s_engine_recent_samples_ch2;
                    serial_source_count_ch2 = copied_view_ch2;
                    display_samples = engine_render_ch1_display(render_source_samples,
                                                                render_source_count,
                                                                render_sample_rate,
                                                                &render_analysis,
                                                                acq_info.trigger_position_pct,
                                                                render_acquire_mode,
                                                                &render_meta);
                    engine_measure_ch1_into_frame(&frame,
                                                  serial_source_samples,
                                                  serial_source_count,
                                                  display_samples,
                                                  render_acquire_mode);
                    ch1_measured_early = true;
                    if (copied_view_ch2 > 0u) {
                        engine_render_ch2_display(s_engine_recent_samples_ch2,
                                                  copied_view_ch2,
                                                  ch2_sample_rate,
                                                  &render_analysis_ch2,
                                                  acq_info.trigger_position_pct,
                                                  render_acquire_mode,
                                                  s_engine_display_samples_ch2,
                                                  s_engine_display_samples_ch2_hi,
                                                  &render_meta_ch2);
                        display_samples_ch2 = s_engine_display_samples_ch2;
                        ch2_present = true;
                        engine_measure_ch2_into_frame(&frame,
                                                      serial_source_samples_ch2,
                                                      serial_source_count_ch2,
                                                      display_samples_ch2,
                                                      render_acquire_mode);
                        ch2_measured_early = true;
                    } else {
                        memset(s_engine_display_samples_ch2, 0, sizeof(s_engine_display_samples_ch2));
                        display_samples_ch2 = s_engine_display_samples_ch2;
                        memset(&render_meta_ch2, 0, sizeof(render_meta_ch2));
                    }
                } else if (DSO_Acq_IsRunning()) {
                    if (copied_recent > 0u) {
                        /* Record window may be temporarily empty just after a NORMAL timebase step.
                         * Keep the live trace visible from the recent DMA window instead of publishing nothing. */
                        engine_prepare_render_analyses(effective_trigger_source,
                                                       &analysis,
                                                       sample_rate,
                                                       copied_recent,
                                                       &analysis_ch2,
                                                       ch2_sample_rate,
                                                       copied_recent_ch2,
                                                       &render_analysis,
                                                       &render_analysis_ch2);
                        render_source_samples = engine_prepare_ch1_render_source(s_engine_recent_samples,
                                                                                 copied_recent,
                                                                                 sample_rate,
                                                                                 render_acquire_mode,
                                                                                 &trig_cfg,
                                                                                 &render_analysis,
                                                                                 &render_source_count,
                                                                                 &render_sample_rate,
                                                                                 &render_analysis);
                        serial_source_samples = render_source_samples;
                        serial_source_count = render_source_count;
                        serial_source_samples_ch2 = s_engine_recent_samples_ch2;
                        serial_source_count_ch2 = copied_recent_ch2;
                        display_samples = engine_render_ch1_display(render_source_samples,
                                                                    render_source_count,
                                                                    render_sample_rate,
                                                                    &render_analysis,
                                                                    acq_info.trigger_position_pct,
                                                                    render_acquire_mode,
                                                                    &render_meta);
                        engine_measure_ch1_into_frame(&frame,
                                                      serial_source_samples,
                                                      serial_source_count,
                                                      display_samples,
                                                      render_acquire_mode);
                        ch1_measured_early = true;
                        if (copied_recent_ch2 > 0u) {
                            engine_render_ch2_display(s_engine_recent_samples_ch2,
                                                      copied_recent_ch2,
                                                      ch2_sample_rate,
                                                      &render_analysis_ch2,
                                                      acq_info.trigger_position_pct,
                                                      render_acquire_mode,
                                                      s_engine_display_samples_ch2,
                                                      s_engine_display_samples_ch2_hi,
                                                      &render_meta_ch2);
                            display_samples_ch2 = s_engine_display_samples_ch2;
                            ch2_present = true;
                            engine_measure_ch2_into_frame(&frame,
                                                          serial_source_samples_ch2,
                                                          serial_source_count_ch2,
                                                          display_samples_ch2,
                                                          render_acquire_mode);
                            ch2_measured_early = true;
                        } else {
                            memset(s_engine_display_samples_ch2, 0, sizeof(s_engine_display_samples_ch2));
                            display_samples_ch2 = s_engine_display_samples_ch2;
                            memset(&render_meta_ch2, 0, sizeof(render_meta_ch2));
                        }
                    } else {
                        publish_frame = false;
                    }
                } else {
                    memset(s_engine_recent_samples, 0, sizeof(s_engine_recent_samples));

                    if ((copied_view_ch2 > 0u) && ch2_available) {
                        DSO_Process_AnalyzeSamplesEx(s_engine_recent_samples_ch2,
                                                     copied_view_ch2,
                                                     ch2_sample_rate,
                                                     &trig_cfg,
                                                     &analysis_ch2);
                        if (ch2_trigger_index >= 0) {
                            float frac_ch2 = (analysis_ch2.trigger_pos_samples >= 0.0f)
                                           ? (analysis_ch2.trigger_pos_samples -
                                              floorf(analysis_ch2.trigger_pos_samples))
                                           : 0.0f;
                            analysis_ch2.trigger_index = ch2_trigger_index;
                            analysis_ch2.trigger_pos_samples = (float)ch2_trigger_index + frac_ch2;
                            analysis_ch2.triggered = true;
                        }
                    }

                    engine_prepare_render_analyses(effective_trigger_source,
                                                   &analysis,
                                                   sample_rate,
                                                   1u,
                                                   &analysis_ch2,
                                                   ch2_sample_rate,
                                                   copied_view_ch2,
                                                   &render_analysis,
                                                   &render_analysis_ch2);

                    render_source_samples = engine_prepare_ch1_render_source(s_engine_recent_samples,
                                                                             1u,
                                                                             sample_rate,
                                                                             render_acquire_mode,
                                                                             &trig_cfg,
                                                                             &render_analysis,
                                                                             &render_source_count,
                                                                             &render_sample_rate,
                                                                             &render_analysis);
                    serial_source_samples = render_source_samples;
                    serial_source_count = render_source_count;
                    display_samples = engine_render_ch1_display(render_source_samples,
                                                                render_source_count,
                                                                render_sample_rate,
                                                                &render_analysis,
                                                                acq_info.trigger_position_pct,
                                                                render_acquire_mode,
                                                                &render_meta);
                    engine_measure_ch1_into_frame(&frame,
                                                  serial_source_samples,
                                                  serial_source_count,
                                                  display_samples,
                                                  render_acquire_mode);
                    ch1_measured_early = true;
                    serial_source_samples_ch2 = s_engine_recent_samples_ch2;
                    serial_source_count_ch2 = copied_view_ch2;
                    if (copied_view_ch2 > 0u) {
                        engine_render_ch2_display(s_engine_recent_samples_ch2,
                                                  copied_view_ch2,
                                                  ch2_sample_rate,
                                                  &render_analysis_ch2,
                                                  acq_info.trigger_position_pct,
                                                  render_acquire_mode,
                                                  s_engine_display_samples_ch2,
                                                  s_engine_display_samples_ch2_hi,
                                                  &render_meta_ch2);
                        display_samples_ch2 = s_engine_display_samples_ch2;
                        ch2_present = true;
                        engine_measure_ch2_into_frame(&frame,
                                                      serial_source_samples_ch2,
                                                      serial_source_count_ch2,
                                                      display_samples_ch2,
                                                      render_acquire_mode);
                        ch2_measured_early = true;
                    } else {
                        memset(s_engine_display_samples_ch2, 0, sizeof(s_engine_display_samples_ch2));
                        display_samples_ch2 = s_engine_display_samples_ch2;
                        memset(&render_meta_ch2, 0, sizeof(render_meta_ch2));
                    }
                }
                }
            } else {
                engine_prepare_render_analyses(effective_trigger_source,
                                               &analysis,
                                               sample_rate,
                                               copied_recent,
                                               &analysis_ch2,
                                               ch2_sample_rate,
                                               copied_recent_ch2,
                                               &render_analysis,
                                               &render_analysis_ch2);
                render_source_samples = engine_prepare_ch1_render_source(s_engine_recent_samples,
                                                                         copied_recent,
                                                                         sample_rate,
                                                                         render_acquire_mode,
                                                                         &trig_cfg,
                                                                         &render_analysis,
                                                                         &render_source_count,
                                                                         &render_sample_rate,
                                                                         &render_analysis);
                serial_source_samples = render_source_samples;
                serial_source_count = render_source_count;
                serial_source_samples_ch2 = s_engine_recent_samples_ch2;
                serial_source_count_ch2 = copied_recent_ch2;
                display_samples = engine_render_ch1_display(render_source_samples,
                                                            render_source_count,
                                                            render_sample_rate,
                                                            &render_analysis,
                                                            acq_info.trigger_position_pct,
                                                            render_acquire_mode,
                                                            &render_meta);
                engine_measure_ch1_into_frame(&frame,
                                              serial_source_samples,
                                              serial_source_count,
                                              display_samples,
                                              render_acquire_mode);
                ch1_measured_early = true;
                if (copied_recent_ch2 > 0u) {
                    engine_render_ch2_display(s_engine_recent_samples_ch2,
                                              copied_recent_ch2,
                                              ch2_sample_rate,
                                              &render_analysis_ch2,
                                              acq_info.trigger_position_pct,
                                              render_acquire_mode,
                                              s_engine_display_samples_ch2,
                                              s_engine_display_samples_ch2_hi,
                                              &render_meta_ch2);
                    display_samples_ch2 = s_engine_display_samples_ch2;
                    ch2_present = true;
                    engine_measure_ch2_into_frame(&frame,
                                                  serial_source_samples_ch2,
                                                  serial_source_count_ch2,
                                                  display_samples_ch2,
                                                  render_acquire_mode);
                    ch2_measured_early = true;
                } else {
                    memset(s_engine_display_samples_ch2, 0, sizeof(s_engine_display_samples_ch2));
                    display_samples_ch2 = s_engine_display_samples_ch2;
                    memset(&render_meta_ch2, 0, sizeof(render_meta_ch2));
                }
            }

            if (DSO_Record_IsFrozen() && DSO_Acq_IsRunning()) {
                DSO_Acq_Stop();
                DSO_AcquireCtrl_SetRunRequested(false);
                force_publish = true;
            }
            frame.triggered = active_analysis->triggered;
            roll_prev_pos = pos;
            roll_prev_pos_ch2 = pos_ch2;
        }

        if (DSO_Timebase_GetMode() != TB_MODE_ROLL) {
            if (DSO_Acq_IsRunning() &&
                (acq_info.trigger_mode == DSO_TRIGGER_MODE_NORMAL) &&
                !active_analysis->triggered) {
                publish_frame = false;
            }
            if (DSO_Acq_IsRunning() &&
                (acq_info.trigger_mode == DSO_TRIGGER_MODE_SINGLE) &&
                !active_analysis->triggered &&
                !DSO_Record_IsFrozen()) {
                publish_frame = false;
            }
        }

        if (analysis.freq_valid &&
            !DSO_Process_IsHs8DualFreqReliable(serial_source_samples,
                                               serial_source_count,
                                               &analysis)) {
            analysis.freq_valid = false;
            analysis.freq_hz = 0.0f;
            analysis.samples_per_cycle = 0.0f;
        }

        if (!publish_frame) {
            vTaskDelay(pdMS_TO_TICKS(DSO_ENGINE_SERVICE_INTERVAL_MS));
            continue;
        }

        if (!DSO_EngineCal_SupportsRawPreprocess() &&
            (DSO_Timebase_GetMode() != TB_MODE_ROLL) &&
            (acq_info.acquire_mode == DSO_ACQUIRE_MODE_AVERAGE)) {
            display_samples = DSO_AcquireCtrl_ApplyAverageForChannel(0u,
                                                                     display_samples,
                                                                     DSO_DISPLAY_WIDTH);
            if (ch2_available) {
                display_samples_ch2 = DSO_AcquireCtrl_ApplyAverageForChannel(1u,
                                                                             display_samples_ch2,
                                                                             DSO_DISPLAY_WIDTH);
            }
        }

        frame.freq_hz = analysis.freq_hz;
        frame.freq_valid = analysis.freq_valid;
        {
            DSO_FreqMeterSnapshot meter = {0};

            (void)DSO_FreqMeter_GetSnapshot(&meter);
            frame.ext_freq_valid = meter.valid;
            frame.ext_freq_state = (uint8_t)meter.state;
            frame.ext_freq_mode = (uint8_t)meter.mode;
            frame.ext_freq_millihz = meter.freq_millihz;
        }
        frame.sample_rate = sample_rate;
        frame.ch2_freq_hz = analysis_ch2.freq_hz;
        frame.ch2_freq_valid = analysis_ch2.freq_valid;
        frame.ch2_sample_rate = ch2_sample_rate;
        frame.ch2_present = ch2_present;
        frame.ets_active = render_meta.ets_active;
        frame.ets_building = render_meta.ets_building;
        frame.recon_mode = (uint8_t)render_meta.recon_mode;
        frame.display_quality = (uint8_t)render_meta.display_quality;
        frame.effective_sample_rate_hz = (render_meta.effective_sample_rate_hz > 0.0f)
                                       ? render_meta.effective_sample_rate_hz
                                       : (float)sample_rate;
        {
            const uint16_t *meas_hi_ch1 = NULL;
            const uint16_t *meas_hi_ch2 = NULL;
            uint32_t xi;

            {
                bool roll_mode = (DSO_Timebase_GetMode() == TB_MODE_ROLL);

                if (!roll_mode) {
                    if (render_acquire_mode != DSO_ACQUIRE_MODE_AVERAGE) {
                        meas_hi_ch1 = DSO_Timebase_GetLastColumnHiForChannel(0u);
                        if ((ch2_present != 0) && ch2_available) {
                            meas_hi_ch2 = DSO_Timebase_GetLastColumnHiForChannel(1u);
                        }
                    }
                }

                if (!ch1_measured_early) {
                    engine_measure_channel(DSO_CHANNEL_1,
                                           roll_mode,
                                           serial_source_samples,
                                           serial_source_count,
                                           display_samples,
                                           meas_hi_ch1,
                                           &frame);
                }
            }
            {
                DSO_Frame frame_ch2 = {0};
                bool roll_mode = (DSO_Timebase_GetMode() == TB_MODE_ROLL);

                if (ch2_present && ch2_available && !ch2_measured_early) {
                    engine_measure_channel(DSO_CHANNEL_2,
                                           roll_mode,
                                           serial_source_samples_ch2,
                                           serial_source_count_ch2,
                                           display_samples_ch2,
                                           meas_hi_ch2,
                                           &frame_ch2);
                    frame.ch2_vmin = frame_ch2.vmin;
                    frame.ch2_vmax = frame_ch2.vmax;
                    frame.ch2_vpp = frame_ch2.vpp;
                    frame.ch2_vrms = frame_ch2.vrms;
                    frame.ch2_duty_cycle = frame_ch2.duty_cycle;
                }
            }

            memcpy(s_engine_display_samples, display_samples, sizeof(s_engine_display_samples));
            if (meas_hi_ch1 != NULL) {
                memcpy(s_engine_display_samples_hi, meas_hi_ch1, sizeof(s_engine_display_samples_hi));
            } else {
                memcpy(s_engine_display_samples_hi, display_samples, sizeof(s_engine_display_samples_hi));
            }
            memcpy(s_engine_display_samples_ch2, display_samples_ch2, sizeof(s_engine_display_samples_ch2));
            if (meas_hi_ch2 != NULL) {
                memcpy(s_engine_display_samples_ch2_hi, meas_hi_ch2, sizeof(s_engine_display_samples_ch2_hi));
            } else {
                memcpy(s_engine_display_samples_ch2_hi,
                       display_samples_ch2,
                       sizeof(s_engine_display_samples_ch2_hi));
            }

            for (xi = 0u; xi < DSO_DISPLAY_WIDTH; xi++) {
                uint32_t a = (uint32_t)s_engine_display_samples[xi] +
                             (uint32_t)s_engine_display_samples_hi[xi] + 1u;

                s_engine_mid_scratch[xi] = (uint16_t)(a >> 1u);
            }
            if (DSO_Timebase_GetMode() == TB_MODE_ROLL) {
                DSO_Vertical_ApplyACDisplayFilter(s_engine_mid_scratch, DSO_DISPLAY_WIDTH);
            }
            DSO_Vertical_MapY(s_engine_mid_scratch, DSO_DISPLAY_WIDTH, frame.y_coords);

            for (xi = 0u; xi < DSO_DISPLAY_WIDTH; xi++) {
                uint32_t a2 = (uint32_t)s_engine_display_samples_ch2[xi] +
                              (uint32_t)s_engine_display_samples_ch2_hi[xi] + 1u;

                s_engine_mid_scratch[xi] = (uint16_t)(a2 >> 1u);
            }
            if (DSO_Timebase_GetMode() == TB_MODE_ROLL) {
                DSO_Vertical_ApplyACDisplayFilterForChannel(DSO_CHANNEL_2,
                                                            s_engine_mid_scratch,
                                                            DSO_DISPLAY_WIDTH);
            }
            DSO_Vertical_MapYForChannel(DSO_CHANNEL_2,
                                        s_engine_mid_scratch,
                                        DSO_DISPLAY_WIDTH,
                                        frame.y_coords_ch2);

            frame.waveform_column_envelope_ch1 = (meas_hi_ch1 != NULL) &&
                                                 render_meta.column_envelope &&
                                                 (render_acquire_mode != DSO_ACQUIRE_MODE_AVERAGE);
            frame.waveform_column_envelope_ch2 = (meas_hi_ch2 != NULL) &&
                                                 render_meta_ch2.column_envelope &&
                                                 (render_acquire_mode != DSO_ACQUIRE_MODE_AVERAGE) &&
                                                 (ch2_present != 0) &&
                                                 ch2_available;

            if (frame.waveform_column_envelope_ch1) {
                for (xi = 0u; xi < DSO_DISPLAY_WIDTH; xi++) {
                    frame.y_column_env_top_ch1[xi] =
                        DSO_Vertical_MapSampleToPlotYForChannel(DSO_CHANNEL_1,
                                                                 s_engine_display_samples_hi[xi]);
                    frame.y_column_env_bot_ch1[xi] =
                        DSO_Vertical_MapSampleToPlotYForChannel(DSO_CHANNEL_1,
                                                                 s_engine_display_samples[xi]);
                }
            }

            if (frame.waveform_column_envelope_ch2) {
                for (xi = 0u; xi < DSO_DISPLAY_WIDTH; xi++) {
                    frame.y_column_env_top_ch2[xi] =
                        DSO_Vertical_MapSampleToPlotYForChannel(DSO_CHANNEL_2,
                                                                 s_engine_display_samples_ch2_hi[xi]);
                    frame.y_column_env_bot_ch2[xi] =
                        DSO_Vertical_MapSampleToPlotYForChannel(DSO_CHANNEL_2,
                                                                 s_engine_display_samples_ch2[xi]);
                }
            }
        }
        if (DSO_Timebase_GetMode() == TB_MODE_ROLL) {
            uint16_t rpv_ch1 = DSO_Timebase_GetRollPlotValidPx(0u);
            uint16_t rpv_ch2 = DSO_Timebase_GetRollPlotValidPx(1u);

            engine_apply_roll_y_mask(&frame, ch2_present ? 1 : 0, rpv_ch1, rpv_ch2);
            frame.roll_fill_ch1 = (uint16_t)DSO_Timebase_GetRollFillPx(0u);
            frame.roll_fill_ch2 = (uint16_t)DSO_Timebase_GetRollFillPx(1u);
        } else {
            engine_apply_roll_y_mask(&frame, ch2_present ? 1 : 0,
                                     (uint16_t)DSO_DISPLAY_WIDTH,
                                     (uint16_t)DSO_DISPLAY_WIDTH);
            frame.roll_fill_ch1 = 0u;
            frame.roll_fill_ch2 = 0u;
        }
        if (DSO_Timebase_GetMode() != TB_MODE_ROLL) {
            DSO_AcquireCtrl_OnFrameResult(active_analysis->triggered);
        }
        fill_frame_metadata(&frame);
        frame.live_record_paused = display_protect_high_bw &&
                                   (DSO_ENGINE_HIGH_BW_LIVE_RECORD_ENABLE == 0u) &&
                                   (DSO_Timebase_GetMode() != TB_MODE_ROLL);
        frame.roll_ring_coherent_engine = roll_ring_coherent_engine;
        frame.roll_resync_in_progress = DSO_EngineRoll_ResyncInProgress();
#if DSO_VPP_DIAG_ENABLE
        (void)serial_cal_active;
        engine_log_vpp_diag_frame(now,
                                  DSO_Timebase_GetMode() == TB_MODE_ROLL,
                                  &frame,
                                  &analysis,
                                  serial_source_count,
                                  &analysis_ch2,
                                  serial_source_count_ch2);
#endif
#if !DSO_VPP_DIAG_ENABLE
        if (!DSO_SerialDebug_ShouldBypassVofa()) {
            DSO_VofaFirewater_TrySendFrame(&frame, HAL_GetTick());
        }
#endif
#if !DSO_VPP_DIAG_ENABLE
        DSO_SerialDebug_LogFrame(1u,
                                 count,
                                 serial_cal_active,
                                 frame.cal_state,
                                 serial_source_samples,
                                 serial_source_count,
                                 sample_rate,
                                 &analysis,
                                 &render_meta,
                                 display_samples,
                                 DSO_DISPLAY_WIDTH,
                                 &frame,
                                 frame.y_coords,
                                 now,
                                 acq_info.trigger_position_pct);
#if DSO_SERIAL_DEBUG_CH2_ENABLE
        DSO_SerialDebug_LogFrame(2u,
                                 count,
                                 false,
                                 0u,
                                 serial_source_samples_ch2,
                                 serial_source_count_ch2,
                                 ch2_sample_rate,
                                 &analysis_ch2,
                                 &render_meta_ch2,
                                 display_samples_ch2,
                                 DSO_DISPLAY_WIDTH,
                                 &frame,
                                 frame.y_coords_ch2,
                                 now,
                                 acq_info.trigger_position_pct);
#endif
#endif
#if DSO_ROLL_SERIAL_DIAG_ENABLE || DSO_RUNTIME_LOG_ENABLE
        if (DSO_Timebase_GetMode() == TB_MODE_ROLL) {
            static uint32_t s_roll_log_ctr;
            bool heartbeat = ((s_roll_log_ctr++ % 30u) == 0u);

            if (tb_changed || heartbeat) {
                DSO_ROLL_LOG("[DSO] roll dbg: coh=%u gentle=%u tbchg=%u fill1=%u fill2=%u cpy1=%lu cpy2=%lu\r\n",
                             (unsigned)frame.roll_ring_coherent_engine,
                             (unsigned)((gentle_tb && tb_changed) ? 1u : 0u),
                             (unsigned)(tb_changed ? 1u : 0u),
                             (unsigned)frame.roll_fill_ch1,
                             (unsigned)frame.roll_fill_ch2,
                             (unsigned long)s_roll_dbg_copied,
                             (unsigned long)s_roll_dbg_copied_ch2);
            }
        }
#endif
        {
            float measure_scale;
            bool hf_lane_measurement;

            hf_lane_measurement = engine_apply_hf_lane_measurement_to_frame(&frame,
                                                                            serial_source_samples,
                                                                            serial_source_count);
            measure_scale = engine_measure_amp_scale_for_frame(&frame,
                                                               hf_lane_measurement);
            engine_apply_measure_amp_scale_to_frame(&frame, measure_scale);
#if DSO_VPP_DIAG_ENABLE
            engine_log_vpp_publish_frame(&frame, measure_scale, hf_lane_measurement);
#endif
        }
        DSO_UI_SendFrame(&frame);
        last_publish_tick = now;

        /* HS8 lane calibration diagnostics:
         * emit on transitions plus a slow heartbeat, and route through the
         * shared serial debug formatting/sending path. */
        if (DSO_EngineCal_IsHs8Active()) {
            static uint8_t last_stage = 0xFFu;
            static uint8_t last_lane2_fail = 0xFFu;
            static uint8_t last_ready = 0xFFu;
            static uint32_t last_emit_frame = 0u;
            static char hs8cal_line[256];
            const DSO_HS8LaneCalState *cs = DSO_EngineCal_GetHs8State();
            bool changed = (last_stage != cs->last_stage) ||
                           (last_lane2_fail != cs->last_lane2_fail) ||
                           (last_ready != (cs->ready ? 1u : 0u));
            bool heartbeat_due = (count == 0u) ||
                                 ((uint32_t)(count - last_emit_frame) >= 100u);

            if (changed || heartbeat_due) {
                DSO_SerialDebugHs8CalSummary hs8cal = {
                    .frame_id = count,
                    .hs8_topology = DSO_Acq_GetHs8Topology(),
                    .hs8_lane_count = DSO_Acq_GetLaneCount(),
                    .service_calls = cs->service_calls,
                    .stage = cs->last_stage,
                    .good_frames = cs->good_frames,
                    .ready = cs->ready,
                    .lane1_used = cs->last_lane1_used,
                    .lane1_fail = cs->last_lane1_fail,
                    .lane2_used = cs->last_lane2_used,
                    .lane2_fail = cs->last_lane2_fail,
                    .gain1 = cs->phase[1].gain,
                    .offset1 = cs->phase[1].offset_lsb,
                    .skew1 = cs->phase[1].skew,
                    .gain2 = cs->phase[2].gain,
                    .offset2 = cs->phase[2].offset_lsb,
                    .skew2 = cs->phase[2].skew,
                    .repetitive_confidence = analysis.repetitive_confidence,
                    .locked_freq_hz = cs->locked_freq_hz,
                };

                if (!(DSO_SerialDebug_IsPhaseOnlyModeEnabled() &&
                      ((DSO_Acq_GetHs8Topology() == DSO_ACQ8_TOPOLOGY_SINGLE_SAFE) ||
                       (DSO_Acq_GetHs8Topology() == DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED)))) {
                    if (DSO_SerialDebug_FormatHs8CalLine(&hs8cal,
                                                         hs8cal_line,
                                                         sizeof(hs8cal_line)) > 0u) {
                        DSO_SerialDebug_SendLine(hs8cal_line);
                        DSO_ETH_Upload_SendLine(hs8cal_line);
                    }
                }
                last_stage = cs->last_stage;
                last_lane2_fail = cs->last_lane2_fail;
                last_ready = cs->ready ? 1u : 0u;
                last_emit_frame = count;
            }
        }

        count++;
        vTaskDelay(pdMS_TO_TICKS(DSO_ENGINE_SERVICE_INTERVAL_MS));
    }
}
