/**
 * @file dso_process.h
 * @brief 原始样本分析：触发点、频率、Vpp/Vrms/Duty、重复性置信度。
 */

#ifndef DSO_PROCESS_H
/* 宏定义: DSO_PROCESS_H - 头文件保护宏。 */
#define DSO_PROCESS_H

#include "dso_acquire_control.h"
#include "dso_config.h"
#include "dso_render_policy.h"

typedef enum {
    /* 同一 32-bit word 中按 master/slave 顺序解释。 */
    DSO_DIAG_CANDIDATE_SAME_WORD_MASTER_SLAVE = 0,
    /* 同一 32-bit word 中按 slave/master 顺序解释。 */
    DSO_DIAG_CANDIDATE_SAME_WORD_SLAVE_MASTER,
    /* 跨 word：当前 slave 与下一 word master 组合。 */
    DSO_DIAG_CANDIDATE_CROSS_WORD_SLAVE_MASTER_NEXT,
    /* 跨 word：上一 word master 与当前 slave 组合。 */
    DSO_DIAG_CANDIDATE_CROSS_WORD_MASTER_PREV_SLAVE,
    /* 仅使用 master lane。 */
    DSO_DIAG_CANDIDATE_LANE_MASTER_ONLY,
    /* 仅使用 slave lane。 */
    DSO_DIAG_CANDIDATE_LANE_SLAVE_ONLY,
    /* 诊断候选数量。 */
    DSO_DIAG_CANDIDATE_COUNT
} DSO_DiagCandidateId;

typedef struct {
    /* 候选样本流对应采样率。 */
    uint32_t sample_rate_hz;
    /* 候选样本数量。 */
    uint32_t sample_count;
    /* 该候选分析是否有效。 */
    bool     valid;
    /* 估计频率，单位 Hz。 */
    uint32_t freq_hz;
    /* 最小/最大/Vpp/均值 code。 */
    uint16_t vmin;
    uint16_t vmax;
    uint16_t vpp;
    uint16_t avg;
    /* 低/高阈值 code。 */
    uint16_t lo_level;
    uint16_t hi_level;
    /* 低端/高端削顶样本计数。 */
    uint32_t clip_low_count;
    uint32_t clip_high_count;
    /* crossing 统计与首末 crossing 位置。 */
    uint32_t crossing_count;
    uint32_t first_cross;
    uint32_t last_cross;
    /* 样本预览窗口。 */
    uint16_t sample_preview[DSO_DEBUG_DIAG_SAMPLE_PREVIEW];
    uint32_t sample_preview_count;
    /* crossing 位置预览。 */
    uint32_t cross_preview[DSO_DEBUG_DIAG_CROSS_PREVIEW];
    uint32_t cross_preview_count;
    /* crossing 间隔预览。 */
    uint32_t cross_gap_preview[DSO_DEBUG_DIAG_CROSS_PREVIEW];
    uint32_t cross_gap_preview_count;
} DSO_DiagCandidateStats;

typedef struct {
    /* 输入 word 数量。 */
    uint32_t word_count;
    /* 应用到候选样本的校准偏移。 */
    int16_t  cal_offset;
    /* 输入总采样率。 */
    uint32_t total_sample_rate_hz;
    /* 各候选解释方式对应的分析结果。 */
    DSO_DiagCandidateStats candidate[DSO_DIAG_CANDIDATE_COUNT];
} DSO_DiagReport;

typedef struct {
    /* 频率估计结果，单位 Hz。 */
    float    freq_hz;
    /* 频率估计是否有效。 */
    bool     freq_valid;
    /* 最小/最大 code。 */
    uint16_t vmin;
    uint16_t vmax;
    /* 平均 code。 */
    float    vavg;
    /* 触发点整数索引。 */
    int32_t  trigger_index;
    /* 触发点小数位置。 */
    float    trigger_pos_samples;
    /* 当前帧是否满足触发条件。 */
    bool     triggered;
    /* 每周期样本数。 */
    float    samples_per_cycle;
    /* crossing 数量。 */
    uint32_t crossing_count;
    /* 周期间隔抖动百分比。 */
    float    period_jitter_pct;
    /* 重复性置信度。 */
    float    repetitive_confidence;
    /* 是否疑似削顶。 */
    bool     clipped;
} DSO_ProcessAnalysis;

typedef struct {
    /* 触发电平，单位 V。 */
    float level_volts;
    /* 是否启用触发电平。 */
    bool level_enabled;
    /* 触发边沿。 */
    DSO_TriggerSlope slope;
} DSO_TriggerConfig;

/* Frame sent from DSO engine to UI task. */
typedef struct {
    int16_t  y_coords[DSO_DISPLAY_WIDTH];
    int16_t  y_coords_ch2[DSO_DISPLAY_WIDTH];
    /** Dense min/max decimation: per-column envelope in plot Y (top <= bot pixel index). */
    bool     waveform_column_envelope_ch1;
    bool     waveform_column_envelope_ch2;
    int16_t  y_column_env_top_ch1[DSO_DISPLAY_WIDTH];
    int16_t  y_column_env_bot_ch1[DSO_DISPLAY_WIDTH];
    int16_t  y_column_env_top_ch2[DSO_DISPLAY_WIDTH];
    int16_t  y_column_env_bot_ch2[DSO_DISPLAY_WIDTH];
    float    freq_hz;
    bool     freq_valid;
    uint16_t vpp, vmin, vmax;
    float    vrms;
    float    duty_cycle;    /* 0.0-1.0, or -1 if not measurable */
    float    ch2_freq_hz;
    bool     ch2_freq_valid;
    bool     ext_freq_valid;
    uint8_t  ext_freq_state;
    uint8_t  ext_freq_mode;
    uint64_t ext_freq_millihz;
    uint16_t ch2_vpp, ch2_vmin, ch2_vmax;
    float    ch2_vrms;
    float    ch2_duty_cycle;
    uint8_t  adc_bits;
    float    adc_lsb_v;
    uint32_t sample_rate;
    uint32_t ch2_sample_rate;
    bool     triggered;
    bool     ch2_present;
    bool     ets_active;
    bool     ets_building;
    uint8_t  recon_mode;
    uint8_t  display_quality;
    float    effective_sample_rate_hz;
    bool     ch1_interleave_cal_valid;
    uint8_t  cal_state;
    uint8_t  cal_progress_pct;
    uint32_t tb_sec_div_ns;
    uint8_t  tb_mode;
    float    tb_time_per_pixel_ns;
    uint32_t vert_mv_per_div;
    float    vert_scale_min_v;
    float    vert_scale_max_v;
    uint32_t ch2_vert_mv_per_div;
    float    ch2_vert_scale_min_v;
    float    ch2_vert_scale_max_v;
    uint8_t  trig_mode;
    uint8_t  trig_source;
    float    trig_level_v;
    bool     trig_level_enabled;
    /* Gentle Roll timebase step: engine rebuilt roll line from ring; UI may skip duplicate prefill. */
    bool     roll_ring_coherent_engine;
    /* Multi-frame ring resync in progress (freeze display path). */
    bool     roll_resync_in_progress;
    /** Roll column fill progress (for UI prefill skip / debug). */
    uint16_t roll_fill_ch1;
    uint16_t roll_fill_ch2;
    /** FAST high-BW display protect: live record/roll feed paused. */
    bool     live_record_paused;
} DSO_Frame;

/*
*********************************************************************************************************
*   函 数 名: DSO_Process_Init
*   功能说明: 分析采样数据并计算触发、测量、频率、占空比或诊断统计结果。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Process_Init(void);

/* 返回诊断候选名称。 */
const char *DSO_Process_DebugCandidateName(DSO_DiagCandidateId id);

/* 对 packed word 做多种候选解释并生成诊断报告。 */
void DSO_Process_DebugAnalyzeWords(const uint32_t *words,
                                   uint32_t word_count,
                                   int16_t cal_offset,
                                   uint32_t sample_rate_hz,
                                   DSO_DiagReport *out);

/* 使用默认触发配置分析样本。 */
void DSO_Process_AnalyzeSamples(const uint16_t *samples,
                                uint32_t count,
                                uint32_t sample_rate,
                                DSO_ProcessAnalysis *out);

/* 使用指定触发配置分析样本。 */
void DSO_Process_AnalyzeSamplesEx(const uint16_t *samples,
                                  uint32_t count,
                                  uint32_t sample_rate,
                                  const DSO_TriggerConfig *trigger,
                                  DSO_ProcessAnalysis *out);

/* 判断 HS8 双 lane 频率分析是否可靠。 */
bool DSO_Process_IsHs8DualFreqReliable(const uint16_t *samples,
                                       uint32_t count,
                                       const DSO_ProcessAnalysis *analysis);

/* 基于显示列低/高包络测量 Vpp、Vrms 等显示结果。 */
void DSO_Process_MeasureDisplay(const uint16_t *column_lo,
                                const uint16_t *column_hi,
                                uint32_t count,
                                DSO_Frame *out);

/* 基于原始样本测量 Vpp、Vrms 等结果。 */
void DSO_Process_MeasureSamples(const uint16_t *samples,
                                uint32_t count,
                                DSO_Frame *out);

#endif /* DSO_PROCESS_H */
