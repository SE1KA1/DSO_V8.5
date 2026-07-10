/**
 * @file dso_ui_bridge.h
 * @brief DSO_UI_Frame 类型与引擎→UI 帧字段、ADC LSB 换算 inline。
 */

/*
 * dso_ui_bridge.h — Narrow interface between UI and DSO core.
 *
 * UI code should include this header instead of individual dso/ headers.
 * This decouples the UI layer from DSO internals and allows the DSO_Frame
 * layout to change without recompiling UI files.
 *
 * Phase 5: Define DSO_UI_Frame, UI-facing constants, and helper functions.
 * Frame double-buffer and transfer live in UI_WaveformCtrl.c.
 */

#ifndef DSO_UI_BRIDGE_H
/* 宏定义: DSO_UI_BRIDGE_H - 头文件保护宏。 */
#define DSO_UI_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "dso/dso_config.h"   /* DSO_DISPLAY_WIDTH */

/* ====== Display mode (mirrors DSO_TimebaseMode numeric values) ====== */

#define DSO_UI_TB_MODE_FAST    0u
/* 宏定义: DSO_UI_TB_MODE_NORMAL - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_TB_MODE_NORMAL  1u
/* 宏定义: DSO_UI_TB_MODE_ROLL - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_TB_MODE_ROLL    2u

/* ====== Trigger mode (mirrors DSO_TriggerMode) ====== */

#define DSO_UI_TRIG_AUTO    0u
/* 宏定义: DSO_UI_TRIG_NORMAL - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_TRIG_NORMAL  1u
/* 宏定义: DSO_UI_TRIG_SINGLE - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_TRIG_SINGLE  2u

/* ====== Trigger slope (mirrors DSO_TriggerSlope) ====== */

#define DSO_UI_TRIG_SLOPE_RISING   0u
/* 宏定义: DSO_UI_TRIG_SLOPE_FALLING - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_TRIG_SLOPE_FALLING  1u
/* 宏定义: DSO_UI_TRIG_SLOPE_BOTH - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_TRIG_SLOPE_BOTH     2u

/* ====== Trigger source (mirrors DSO_TriggerSource) ====== */

#define DSO_UI_TRIG_SRC_CH1  0u
/* 宏定义: DSO_UI_TRIG_SRC_CH2 - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_TRIG_SRC_CH2  1u

/* ====== Channel ID (mirrors DSO_ChannelId) ====== */

#define DSO_UI_CH1  0
/* 宏定义: DSO_UI_CH2 - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_CH2  1

/* ====== Sample format ====== */

#define DSO_UI_FORMAT_8BIT_HS  0u
/* 宏定义: DSO_UI_FORMAT_10BIT - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_FORMAT_10BIT    1u

/* ====== Calibration state (mirrors DSO_10bitLaneCalState) ====== */

#define DSO_UI_CAL_IDLE        0u
/* 宏定义: DSO_UI_CAL_LOADED - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_CAL_LOADED      1u
/* 宏定义: DSO_UI_CAL_COLLECTING - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_CAL_COLLECTING  2u
/* 宏定义: DSO_UI_CAL_SUCCESS - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_CAL_SUCCESS     3u
/* 宏定义: DSO_UI_CAL_FAILED - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_CAL_FAILED      4u

/* ====== Render recon mode (mirrors DSO_ReconMode) ====== */

#define DSO_UI_RECON_RAW_LINEAR       0u
/* 宏定义: DSO_UI_RECON_RAW_LIMITED - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_RECON_RAW_LIMITED      1u
/* 宏定义: DSO_UI_RECON_ETS - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_RECON_ETS              2u
/* 宏定义: DSO_UI_RECON_HS8_ENHANCED - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_RECON_HS8_ENHANCED     3u
/* 宏定义: DSO_UI_RECON_HS10_ENHANCED - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_RECON_HS10_ENHANCED    4u

/* ====== Display quality (mirrors DSO_DisplayQuality) ====== */

#define DSO_UI_QUALITY_GOOD                0u
/* 宏定义: DSO_UI_QUALITY_LIMITED - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_QUALITY_LIMITED             1u
/* 宏定义: DSO_UI_QUALITY_REPEAT_REQUIRED - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_QUALITY_REPEAT_REQUIRED     2u

/* ====== External frequency meter state (mirrors DSO_FreqMeterState) ====== */

#define DSO_UI_FREQ_STATE_STOPPED     0u
/* 宏定义: DSO_UI_FREQ_STATE_WAIT_EDGE - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_FREQ_STATE_WAIT_EDGE   1u
/* 宏定义: DSO_UI_FREQ_STATE_MEASURING - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_FREQ_STATE_MEASURING   2u
/* 宏定义: DSO_UI_FREQ_STATE_VALID - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_FREQ_STATE_VALID       3u
/* 宏定义: DSO_UI_FREQ_STATE_NO_SIGNAL - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_FREQ_STATE_NO_SIGNAL  4u
/* 宏定义: DSO_UI_FREQ_STATE_OVERRANGE - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_FREQ_STATE_OVERRANGE  5u
/* 宏定义: DSO_UI_FREQ_STATE_FAULT - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_UI_FREQ_STATE_FAULT      6u

/* ====== Frame data (UI read-only snapshot) ====== */

typedef struct {
    /* Waveform display */
    int16_t y_coords[DSO_DISPLAY_WIDTH];
    int16_t y_coords_ch2[DSO_DISPLAY_WIDTH];
    bool  waveform_column_envelope_ch1;
    bool  waveform_column_envelope_ch2;
    int16_t y_column_env_top_ch1[DSO_DISPLAY_WIDTH];
    int16_t y_column_env_bot_ch1[DSO_DISPLAY_WIDTH];
    int16_t y_column_env_top_ch2[DSO_DISPLAY_WIDTH];
    int16_t y_column_env_bot_ch2[DSO_DISPLAY_WIDTH];

    /* Measurement CH1 */
    float  freq_hz;
    bool   freq_valid;
    uint16_t vpp;
    uint16_t vmin;
    uint16_t vmax;
    float  vrms;
    float  duty_cycle;
    bool   triggered;

    /* Measurement CH2 */
    float  ch2_freq_hz;
    bool   ch2_freq_valid;
    uint16_t ch2_vpp;
    uint16_t ch2_vmin;
    uint16_t ch2_vmax;
    float  ch2_vrms;
    float  ch2_duty_cycle;
    bool   ch2_present;

    /* External frequency counter */
    uint64_t ext_freq_millihz;
    bool   ext_freq_valid;
    uint8_t ext_freq_state;
    uint8_t ext_freq_mode;

    /* Timebase */
    uint32_t tb_sec_div_ns;
    uint8_t  tb_mode;
    float    tb_time_per_pixel_ns;
    float    effective_sample_rate_hz;
    uint32_t sample_rate;
    uint32_t ch2_sample_rate;

    /* Vertical */
    uint32_t vert_mv_per_div;
    uint32_t ch2_vert_mv_per_div;
    float    vert_scale_min_v;
    float    vert_scale_max_v;
    float    ch2_vert_scale_min_v;
    float    ch2_vert_scale_max_v;

    /* Trigger */
    uint8_t  trig_mode;
    uint8_t  trig_source;
    float    trig_level_v;
    bool     trig_level_enabled;

    /* ADC metadata */
    uint8_t  adc_bits;
    float    adc_lsb_v;

    /* Render quality */
    uint8_t  recon_mode;
    uint8_t  display_quality;

    /* Calibration */
    uint8_t  cal_state;
    uint8_t  cal_progress_pct;
    bool     ch1_interleave_cal_valid;

    /* Roll */
    bool     roll_ring_coherent_engine;
    bool     roll_resync_in_progress;
    uint16_t roll_fill_ch1;
    uint16_t roll_fill_ch2;

    /* ETS */
    bool ets_active;
    bool ets_building;

    /** FAST high-BW display protect: live record/roll feed paused. */
    bool live_record_paused;
} DSO_UI_Frame;

/* ====== Vertical info ====== */

typedef struct {
    uint32_t mv_per_div;
    float    offset_v;
    uint8_t  probe;
    uint8_t  coupling;
    float    scale_min_v;
    float    scale_max_v;
} DSO_UI_VerticalInfo;

/*
*********************************************************************************************************
*   函 数 名: DSO_UI_GetVerticalInfo
*   功能说明: 在 DSO 引擎与 UI 之间转换或发布波形帧数据。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_UI_VerticalInfo DSO_UI_GetVerticalInfo(int channel);

/* ====== Measurement helpers ====== */

float DSO_UI_FrameAdcLsbV(const DSO_UI_Frame *frame);
/*
*********************************************************************************************************
*   函 数 名: DSO_UI_CodeDeltaToVolts
*   功能说明: 在 DSO 引擎与 UI 之间转换或发布波形帧数据。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
float DSO_UI_CodeDeltaToVolts(float code_delta, float lsb_v);

#endif /* DSO_UI_BRIDGE_H */