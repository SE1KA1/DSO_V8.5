/**
 * @file dso_serial_debug_format.h
 * @brief 串口调试输出格式化声明。
 */

#ifndef DSO_SERIAL_DEBUG_FORMAT_H
/* 宏定义: DSO_SERIAL_DEBUG_FORMAT_H - 头文件保护宏。 */
#define DSO_SERIAL_DEBUG_FORMAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dso_freq_meter.h"
#include "dso_freq_meter_hw_tim12.h"

typedef struct {
    uint32_t frame_id;
    uint8_t channel;
    uint32_t sample_count;
    uint32_t sample_rate_hz;
    uint16_t vmin;
    uint16_t vmax;
    uint16_t mean;
    uint32_t zero_crossings;
    bool clip_low;
    bool clip_high;
    bool cal_applied;
    uint8_t cal_state;
} DSO_SerialDebugAcqSummary;

typedef struct {
    uint32_t frame_id;
    uint8_t channel;
    bool freq_valid;
    float freq_hz;
    bool triggered;
    int32_t trigger_index;
    float trigger_pos_samples;
    float samples_per_cycle;
    float period_jitter_pct;
    float repetitive_confidence;
    uint8_t recon_mode;
    uint8_t display_quality;
    bool ets_active;
    bool ets_building;
} DSO_SerialDebugProcSummary;

typedef struct {
    uint32_t frame_id;
    uint8_t channel;
    float ui_freq_hz;
    bool ui_freq_valid;
    uint16_t display_vpp;
    float display_vrms;
    float effective_sample_rate_hz;
    uint32_t tb_sec_div_ns;
    uint32_t vert_mv_per_div;
    int16_t y_min;
    int16_t y_max;
} DSO_SerialDebugUiSummary;

typedef struct {
    uint32_t frame_id;
    uint8_t channel;
    uint8_t adc_bits;
    uint8_t lane_count;
    uint32_t sample_rate_hz;
    uint32_t total_count;
    int32_t trigger_index;
    uint32_t window_start;
    uint32_t window_count;
    const uint16_t *samples;
} DSO_SerialDebugRawDumpSummary;

typedef struct {
    uint32_t frame_id;
    bool running;
    uint32_t sample_rate_hz;
    uint32_t ch2_sample_rate_hz;
    uint32_t adc_kernel_clock_hz;
    uint32_t dma_pos;
    uint32_t lane0_pos;
    uint32_t lane1_pos;
    uint32_t lane2_pos;
    bool ch2_valid;
    bool ch2_capture_active;
    uint32_t ch2_count;
    uint32_t phase_sync_errors;
    uint32_t overrun_count;
    uint16_t lane0_vmin;
    uint16_t lane0_vmax;
    uint16_t lane0_mean;
    uint16_t lane1_vmin;
    uint16_t lane1_vmax;
    uint16_t lane1_mean;
    uint16_t lane2_vmin;
    uint16_t lane2_vmax;
    uint16_t lane2_mean;
    uint16_t ch2_vmin;
    uint16_t ch2_vmax;
    uint16_t ch2_mean;
} DSO_SerialDebugHs8Summary;

typedef struct {
    uint32_t frame_id;
    bool tim_enabled;
    uint32_t tim_counter;
    uint32_t tim_arr;
    uint32_t tim_ccr1;
    uint32_t tim_ccr2;
    uint32_t tim_ccr3;
    uint32_t tim_cr1;
    uint32_t tim_ccmr1;
    uint32_t tim_ccmr2;
    uint32_t tim_ccer;
    uint32_t tim_bdtr;
    uint32_t tim_sr;
    uint32_t dma_ndtr0;
    uint32_t dma_ndtr1;
    uint32_t dma_ndtr2;
    uint32_t adc1_cr;
    uint32_t adc2_cr;
    uint32_t adc3_cr;
    uint32_t adc1_cfgr;
    uint32_t adc2_cfgr;
    uint32_t adc3_cfgr;
    uint32_t adc1_isr;
    uint32_t adc2_isr;
    uint32_t adc3_isr;
} DSO_SerialDebugHs8RegsSummary;

typedef struct {
    uint32_t frame_id;
    uint8_t hs8_topology;
    uint8_t hs8_lane_count;
    bool data_ready;
    uint8_t data_ready_lane_count;
    uint32_t sample_rate_hz;
    uint32_t dma_sample_rate_hz;
    uint32_t dma_sample_count;
    uint32_t lane0_dma_sample_rate_hz;
    uint32_t lane1_dma_sample_rate_hz;
    uint32_t lane2_dma_sample_rate_hz;
    uint32_t lane0_dma_pos_sample_rate_hz;
    uint32_t lane1_dma_pos_sample_rate_hz;
    uint32_t lane2_dma_pos_sample_rate_hz;
    uint32_t lane0_dma_accept_per_mille;
    uint32_t lane1_dma_accept_per_mille;
    uint32_t lane2_dma_accept_per_mille;
    uint32_t lane0_dma_pos_accept_per_mille;
    uint32_t lane1_dma_pos_accept_per_mille;
    uint32_t lane2_dma_pos_accept_per_mille;
    uint32_t dma_pos_rate_max_elapsed_ms;
    uint32_t lane0_dma_sample_count;
    uint32_t lane1_dma_sample_count;
    uint32_t lane2_dma_sample_count;
    uint32_t lane0_dma_half_count;
    uint32_t lane1_dma_half_count;
    uint32_t lane2_dma_half_count;
    uint32_t lane0_dma_full_count;
    uint32_t lane1_dma_full_count;
    uint32_t lane2_dma_full_count;
    uint32_t trigger_rate_hz;
    uint32_t accepted_trigger_div;
    uint32_t common_pos;
    uint32_t lane0_pos;
    uint32_t lane1_pos;
    uint32_t lane2_pos;
    uint32_t lane0_delta;
    uint32_t lane1_delta;
    uint32_t lane2_delta;
    uint32_t common_delta;
    bool phase_sync_error;
    uint32_t overrun_count;
    uint32_t lane0_overrun_count;
    uint32_t lane1_overrun_count;
    uint32_t lane2_overrun_count;
    bool freq_valid;
    float freq_hz;
    float samples_per_cycle;
    float trig_pos_pct;
    float trigger_pos_samples;
    float trigger_grid_phase;
    bool allow_ets_requested;
    bool allow_ets_effective;
    bool fast_mode;
    bool recon_fast_mode;
    bool sample_mode;
    bool rep_gate_ok;
    float repetitive_confidence_min;
    bool hs8_spc_gate_ok;
    bool ets_spc_gate_ok;
    bool hs8_candidate;
    bool hs8_ready;
    bool ets_candidate;
    bool hs8_frame_local;
    uint32_t hs8_accum_start;
    uint32_t hs8_accum_count;
} DSO_SerialDebugHs8DiagSummary;

typedef struct {
    uint32_t frame_id;
    uint8_t hs8_topology;
    uint8_t hs8_lane_count;
    uint32_t sample_rate_hz;
    uint32_t dma_pos;
    uint32_t lane0_pos;
    uint32_t common_delta;
    bool phase_sync_error;
    bool freq_valid;
    bool triggered;
    int32_t trigger_index;
    float trigger_pos_samples;
    float trigger_grid_phase;
    float samples_per_cycle;
    float freq_hz;
} DSO_SerialDebugPhaseSummary;

typedef struct {
    uint32_t frame_id;
    uint8_t hs8_topology;
    uint8_t hs8_lane_count;
    uint32_t sample_rate_hz;
    float freq_hz;
    uint32_t valid_count;
    float grid_min;
    float grid_max;
    float grid_span;
    float grid_mean;
} DSO_SerialDebugPhaseStatsSummary;

typedef struct {
    uint32_t frame_id;
    uint8_t hs8_topology;
    uint8_t hs8_lane_count;
    uint32_t sample_rate_hz;
    uint32_t lane0_pos;
    uint32_t lane1_pos;
    uint32_t common_delta;
    bool phase_sync_error;
    bool freq_valid;
    bool triggered;
    int32_t trigger_index;
    float trigger_pos_samples;
    float samples_per_cycle;
    float freq_hz;
    float lane_phase_sep;
    float common_phase;
    float rel_lane0_phase;
    float rel_lane1_phase;
} DSO_SerialDebugInterleavePhaseSummary;

typedef struct {
    uint32_t frame_id;
    uint8_t hs8_topology;
    uint8_t hs8_lane_count;
    uint32_t sample_rate_hz;
    float freq_hz;
    uint32_t valid_count;
    float sep_min;
    float sep_max;
    float sep_span;
    float sep_mean;
    float common_min;
    float common_max;
    float common_span;
    float common_mean;
    float rel_lane0_min;
    float rel_lane0_max;
    float rel_lane0_span;
    float rel_lane0_mean;
    float rel_lane1_min;
    float rel_lane1_max;
    float rel_lane1_span;
    float rel_lane1_mean;
} DSO_SerialDebugInterleaveStatsSummary;

typedef struct {
    uint32_t frame_id;
    uint8_t hs8_topology;
    uint8_t hs8_lane_count;
    uint32_t lane0_start;
    uint32_t lane1_start;
    uint32_t lane2_start;
    uint32_t sample_count;
    const uint16_t *lane0;
    const uint16_t *lane1;
    const uint16_t *lane2;
} DSO_SerialDebugHs8Window;

typedef struct {
    const char *line_tag;
    uint32_t frame_id;
    uint8_t hs8_topology;
    uint8_t hs8_lane_count;
    uint32_t sample_rate_hz;
    bool current_freq_valid;
    float current_freq_hz;
    uint32_t lane0_start;
    uint32_t lane1_start;
    uint32_t lane2_start;
    uint32_t sample_count;
    const uint16_t *lane0;
    const uint16_t *lane1;
    const uint16_t *lane2;
} DSO_SerialDebugHs8OrderSummary;

typedef struct {
    uint8_t hs8_stage;
    uint8_t hs8_topology;
    uint8_t hs8_lane_count;
    bool hs8_ch2_realtime;
    uint32_t acq8_lane2_adc_channel;
} DSO_SerialDebugInitSummary;

typedef struct {
    uint32_t frame_id;
    uint8_t hs8_topology;
    uint8_t hs8_lane_count;
    uint32_t service_calls;
    uint8_t stage;
    uint8_t good_frames;
    bool ready;
    uint32_t lane1_used;
    uint8_t lane1_fail;
    uint32_t lane2_used;
    uint8_t lane2_fail;
    float gain1;
    float offset1;
    float skew1;
    float gain2;
    float offset2;
    float skew2;
    float repetitive_confidence;
    float locked_freq_hz;
} DSO_SerialDebugHs8CalSummary;

typedef struct {
    uint32_t now_ms;
    DSO_FreqMeterSnapshot meter;
    DSO_FreqMeterHwTim12DebugSnapshot hw;
} DSO_SerialDebugFreqSummary;

typedef struct {
    bool force_snapshot;
    bool clipped;
    bool freq_valid;
    float prev_freq_hz;
    float cur_freq_hz;
    uint8_t prev_recon_mode;
    uint8_t cur_recon_mode;
    bool prev_ets_active;
    bool cur_ets_active;
    uint8_t cur_display_quality;
} DSO_SerialDebugSnapshotInput;

typedef struct {
    bool emit;
    bool freq_jump;
    bool recon_changed;
    bool ets_toggled;
    bool limited_quality;
    bool clipped;
    bool forced;
} DSO_SerialDebugSnapshotDecision;

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
                                        uint32_t min_interval_ms);

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
                                      size_t buf_size);
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
                                     size_t buf_size);
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
                                      size_t buf_size);
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
                                    size_t buf_size);
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
                                         size_t buf_size);
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
                                     size_t buf_size);
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
                                         size_t buf_size);
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
                                         size_t buf_size);
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
                                       size_t buf_size);
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
                                            size_t buf_size);
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
                                                 size_t buf_size);
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
                                                 size_t buf_size);
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
                                           size_t buf_size);
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
                                          size_t buf_size);
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
                                        size_t buf_size);
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
                                      size_t buf_size);
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
                                           size_t buf_size);
/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_DecideSnapshot
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_SerialDebug_DecideSnapshot(const DSO_SerialDebugSnapshotInput *in,
                                    DSO_SerialDebugSnapshotDecision *out);

#endif /* DSO_SERIAL_DEBUG_FORMAT_H */
