/**
 * @file dso_timebase.h
 * @brief 时基表、Normal/Fast/Roll/ETS 渲染、窗口样本数。
 */

#ifndef DSO_TIMEBASE_H
/* 宏定义: DSO_TIMEBASE_H - 头文件保护宏。 */
#define DSO_TIMEBASE_H

#include "dso_acquire_control.h"
#include "dso_config.h"
#include "dso_process.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    /* 高速时基，优先使用 recent buffer 或高速重建路径。 */
    TB_MODE_FAST = 0,
    /* 普通 Y-T 时基，主要从 record/window 中取样渲染。 */
    TB_MODE_NORMAL = 1,
    /* 慢扫 Roll 时基，按列滚动累积显示。 */
    TB_MODE_ROLL = 2,
} DSO_TimebaseMode;

typedef struct {
    /* 时基表索引。 */
    uint8_t index;
    /* 每格时间，单位 ns/div。 */
    uint32_t sec_div_ns;
    /* 当前时基所属模式。 */
    DSO_TimebaseMode mode;
    /* 对应触发定时器 ARR。 */
    uint32_t tim8_arr;
    /* 该时基期望硬件采样率。 */
    uint32_t hw_sample_rate;
} DSO_TimebaseEntry;

typedef struct {
    /* 当前时基索引。 */
    uint8_t index;
    /* 每格时间，单位 ns/div。 */
    uint32_t sec_div_ns;
    /* 当前时基模式。 */
    DSO_TimebaseMode mode;
    /* 当前硬件采样率。 */
    uint32_t hw_sample_rate;
    /* 每个显示像素对应的时间，单位 ns。 */
    float time_per_pixel_ns;
    /* 是否处于 Roll 模式。 */
    bool roll_active;
} DSO_TimebaseInfo;

typedef struct {
    /* 当前选择的重建模式。 */
    DSO_ReconMode recon_mode;
    /* 当前显示质量等级。 */
    DSO_DisplayQuality display_quality;
    /* ETS 是否已经启用显示。 */
    bool ets_active;
    /* ETS 是否正在构建。 */
    bool ets_building;
    /* 上层是否请求允许 ETS。 */
    bool allow_ets_requested;
    /* 策略判定后是否实际允许 ETS。 */
    bool allow_ets_effective;
    /* 是否为 fast 时基。 */
    bool fast_mode;
    /* 重建策略是否按 fast 模式处理。 */
    bool recon_fast_mode;
    /* 是否为 sample 采集模式。 */
    bool sample_mode;
    /* 重复性 gate 是否通过。 */
    bool rep_gate_ok;
    /* HS8 每周期样本数 gate 是否通过。 */
    bool hs8_spc_gate_ok;
    /* ETS 每周期样本数 gate 是否通过。 */
    bool ets_spc_gate_ok;
    /* 是否满足 HS8 增强候选条件。 */
    bool hs8_candidate;
    /* HS8 增强重建是否准备完成。 */
    bool hs8_ready;
    /* 是否满足 ETS 候选条件。 */
    bool ets_candidate;
    /* 当前帧是否使用本地 HS8 重建结果。 */
    bool hs8_frame_local;
    /* HS8 累积窗口起点。 */
    uint32_t hs8_accum_start;
    /* HS8 累积窗口样本数量。 */
    uint32_t hs8_accum_count;
    /* 重复性置信度门限。 */
    float repetitive_confidence_min;
    /* 每周期样本数。 */
    float samples_per_cycle;
    /* 实际用于显示的等效采样率。 */
    float effective_sample_rate_hz;
    bool column_envelope; /* min/max per display column (dense downsample) */
} DSO_RenderMeta;

/* 初始化时基表状态。 */
void DSO_Timebase_Init(void);

/* 获取时基表项数量。 */
uint8_t DSO_Timebase_GetCount(void);

/* 获取完整时基表指针。 */
const DSO_TimebaseEntry *DSO_Timebase_GetTable(void);

/* 按索引获取时基表项。 */
const DSO_TimebaseEntry *DSO_Timebase_GetEntryAt(uint8_t idx);

/* 设置 pending 时基索引。 */
void DSO_Timebase_SetIndex(uint8_t idx);

/* 应用 pending 时基索引。 */
bool DSO_Timebase_ApplyPending(void);

/* 获取当前时基模式。 */
DSO_TimebaseMode DSO_Timebase_GetMode(void);

/* 获取当前时基信息。 */
void DSO_Timebase_GetInfo(DSO_TimebaseInfo *out);

/* 获取当前时基索引。 */
uint8_t DSO_Timebase_GetIndex(void);

/* 获取当前时基对应的定时器 ARR。 */
uint32_t DSO_Timebase_GetTimArr(void);

/* 获取当前时基对应的采样率。 */
uint32_t DSO_Timebase_GetSampleRateHz(void);

/* 根据采样率计算当前显示窗口需要的样本数。 */
uint32_t DSO_Timebase_GetWindowSampleCount(uint32_t sample_rate_hz);

/* 对输入样本做 HiRes boxcar 降噪，并输出等效采样率。 */
uint32_t DSO_Timebase_HiResBoxcar(const uint16_t *samples,
                                  uint32_t sample_count,
                                  uint32_t sample_rate_hz,
                                  uint16_t *out_samples,
                                  uint32_t out_capacity,
                                  uint32_t *out_effective_rate_hz);

/* 设置指定通道 HS8 校准是否准备完成。 */
void DSO_Timebase_SetHs8CalReadyForChannel(uint8_t channel, bool ready);

/* 重置指定通道 HS8 重建状态。 */
void DSO_Timebase_ResetHs8ReconstructionForChannel(uint8_t channel);

/* 重置 fast/ETS/HS 重建状态。 */
void DSO_Timebase_ResetFastState(void);
/** Roll column buffer + decimation accumulators only (no ETS/HS8). */
void DSO_Timebase_ResetRollColumnState(void);
/** Same as above for one logical channel (0=CH1, 1=CH2). */
void DSO_Timebase_ResetRollColumnStateForChannel(uint8_t channel);
/*
*********************************************************************************************************
*   函 数 名: DSO_Timebase_SetRollAccumRemainder
*   功能说明: 维护时基状态并执行 Normal/Fast/Roll 渲染、重建或显示窗口管理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Timebase_SetRollAccumRemainder(uint8_t channel, float acc_sum, float acc_count);
/**
 * Seed roll_buf from a decimated line. right_align: place valid_px samples at the right edge
 * (left filled with midscale for stable mapping; mask y in UI). remainder_* is applied to
 * roll accumulators for continuity with RollRender.
 */
void DSO_Timebase_SeedRollDecimatedLineEx(uint8_t channel,
                                          const uint16_t *decimated_line,
                                          uint32_t valid_px,
                                          int right_align,
                                          float remainder_sum,
                                          float remainder_count);
/** Valid horizontal pixels for Roll plot (DSO_DISPLAY_WIDTH = full). */
uint16_t DSO_Timebase_GetRollPlotValidPx(uint8_t channel);
/** Roll columns filled so far (0..DSO_DISPLAY_WIDTH); remainder padded to midscale for MapY. */
uint32_t DSO_Timebase_GetRollFillPx(uint8_t channel);
/** Last RenderToBuffer/RenderEx per-column max sample (paired with display_buf min); valid until next render on that channel. */
const uint16_t *DSO_Timebase_GetLastColumnHiForChannel(uint8_t channel);
/** Replace roll column buffer with a full-width offline-decimated line; pads short with last value. */
void DSO_Timebase_SeedRollDecimatedLine(uint8_t channel,
                                        const uint16_t *decimated_line,
                                        uint32_t width);
/** Copy current CH1 roll column buffer (decimated display line) for freeze during multi-frame resync. */
void DSO_Timebase_SnapshotRollDisplayCh1(uint16_t *dst);
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
                                           DSO_RenderMeta *out_meta);

/* 渲染 CH1 样本到指定显示缓冲。 */
void DSO_Timebase_RenderToBuffer(const uint16_t *samples,
                                 uint32_t sample_count,
                                 uint32_t sample_rate_hz,
                                 const DSO_ProcessAnalysis *analysis,
                                 uint8_t trig_position_pct,
                                 DSO_AcquireMode acquire_mode,
                                 bool allow_ets,
                                 uint16_t *out_display,
                                 uint16_t *out_column_hi,
                                 DSO_RenderMeta *out_meta);

/* 对指定通道执行时基渲染并返回内部显示缓冲。 */
const uint16_t *DSO_Timebase_RenderExForChannel(uint8_t channel,
                                                const uint16_t *samples,
                                                uint32_t sample_count,
                                                uint32_t sample_rate_hz,
                                                const DSO_ProcessAnalysis *analysis,
                                                uint8_t trig_position_pct,
                                                DSO_AcquireMode acquire_mode,
                                                DSO_RenderMeta *out_meta);

/* 使用默认触发位置和采集模式渲染 CH1。 */
const uint16_t *DSO_Timebase_Render(const uint16_t *samples,
                                    uint32_t sample_count,
                                    uint32_t sample_rate_hz,
                                    const DSO_ProcessAnalysis *analysis,
                                    DSO_RenderMeta *out_meta);

/* 使用指定触发位置和采集模式渲染 CH1。 */
const uint16_t *DSO_Timebase_RenderEx(const uint16_t *samples,
                                      uint32_t sample_count,
                                      uint32_t sample_rate_hz,
                                      const DSO_ProcessAnalysis *analysis,
                                      uint8_t trig_position_pct,
                                      DSO_AcquireMode acquire_mode,
                                      DSO_RenderMeta *out_meta);

/* 对 CH1 执行 Roll 渲染并返回内部 roll 显示缓冲。 */
const uint16_t *DSO_Timebase_RollRender(const uint16_t *samples,
                                        uint32_t sample_count,
                                        uint32_t sample_rate_hz);

/* 对指定通道执行 Roll 渲染并返回内部 roll 显示缓冲。 */
const uint16_t *DSO_Timebase_RollRenderForChannel(uint8_t channel,
                                                  const uint16_t *samples,
                                                  uint32_t sample_count,
                                                  uint32_t sample_rate_hz);

#endif /* DSO_TIMEBASE_H */
