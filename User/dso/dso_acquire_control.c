/**
 * @file dso_acquire_control.c
 * @brief AcquireCtrl pending 状态与 run_requested 逻辑暂停。
 */

#include "dso_acquire_control.h"
#include "dso_sample_spec.h"

#include <string.h>

/* 将控制状态和平均缓冲放入 D1 RAM，降低高速处理路径访问延迟。 */
#define DSO_RAM_D1 __attribute__((section(".RAM_D1")))

/* 平均采集固定深度。 */
#define DSO_AVERAGE_DEPTH 8u

typedef struct {
    /* 已生效的控制状态。 */
    DSO_AcquireCtrlInfo active;
    /* UI 或其他模块提交但尚未应用的控制状态。 */
    DSO_AcquireCtrlInfo pending;
    /* 普通参数是否存在 pending 变化。 */
    bool pending_change;
    /* Single 是否请求重新 arm。 */
    bool pending_rearm;
    /* RUN/STOP 是否存在 pending 变化。 */
    bool pending_run_change;
    /* 双通道平均采集缓冲。 */
    uint16_t avg_buf[2][DSO_DMA_BUF_SIZE * 2u];
    /* 每个通道当前平均缓冲对应的样本数量。 */
    uint32_t avg_sample_count[2];
    /* 每个通道已经参与平均的帧数。 */
    uint8_t avg_frame_count[2];
} DSO_AcquireCtrlState;

/* 采集控制全局状态。 */
static DSO_AcquireCtrlState s_ctrl DSO_RAM_D1;

/*
*********************************************************************************************************
*   函 数 名: default_info
*   功能说明: 构造采集控制默认参数。
*   形    参: 无。
*   返 回 值: 默认 DSO_AcquireCtrlInfo。
*********************************************************************************************************
*/
static DSO_AcquireCtrlInfo default_info(void)
{
    DSO_AcquireCtrlInfo info;

    memset(&info, 0, sizeof(info));
    info.trigger_mode = DSO_TRIGGER_MODE_AUTO;
    info.trigger_slope = DSO_TRIGGER_SLOPE_RISING;
    info.trigger_source = DSO_TRIGGER_SOURCE_CH1;
    info.trigger_level_v = DSO_VREF * 0.5f;
    info.trigger_level_enabled = true;
    info.trigger_position_pct = 50u;
    info.acquire_mode = DSO_ACQUIRE_MODE_SAMPLE;
    info.average_depth = DSO_AVERAGE_DEPTH;
    info.record_depth_index = (uint8_t)DSO_RECORD_DEPTH_MEDIUM;
    info.pretrigger_pct = 50u;
    info.record_view_offset = 0;
    info.single_captured = false;
    info.single_armed = false;
    info.run_requested = true;
    return info;
}

/*
*********************************************************************************************************
*   函 数 名: clamp_trigger_mode
*   功能说明: 将触发模式规整到有效枚举范围。
*   形    参: mode - 输入触发模式。
*   返 回 值: 有效触发模式。
*********************************************************************************************************
*/
static DSO_TriggerMode clamp_trigger_mode(DSO_TriggerMode mode)
{
    switch (mode) {
    case DSO_TRIGGER_MODE_AUTO:
    case DSO_TRIGGER_MODE_NORMAL:
    case DSO_TRIGGER_MODE_SINGLE:
        return mode;
    default:
        return DSO_TRIGGER_MODE_AUTO;
    }
}

/*
*********************************************************************************************************
*   函 数 名: clamp_trigger_slope
*   功能说明: 将触发边沿规整到有效枚举范围。
*   形    参: slope - 输入触发边沿。
*   返 回 值: 有效触发边沿。
*********************************************************************************************************
*/
static DSO_TriggerSlope clamp_trigger_slope(DSO_TriggerSlope slope)
{
    switch (slope) {
    case DSO_TRIGGER_SLOPE_RISING:
    case DSO_TRIGGER_SLOPE_FALLING:
    case DSO_TRIGGER_SLOPE_BOTH:
        return slope;
    default:
        return DSO_TRIGGER_SLOPE_RISING;
    }
}

/*
*********************************************************************************************************
*   函 数 名: clamp_trigger_source
*   功能说明: 将触发源规整到有效枚举范围。
*   形    参: source - 输入触发源。
*   返 回 值: 有效触发源。
*********************************************************************************************************
*/
static DSO_TriggerSource clamp_trigger_source(DSO_TriggerSource source)
{
    switch (source) {
    case DSO_TRIGGER_SOURCE_CH1:
    case DSO_TRIGGER_SOURCE_CH2:
        return source;
    default:
        return DSO_TRIGGER_SOURCE_CH1;
    }
}

/*
*********************************************************************************************************
*   函 数 名: format_supports_hires
*   功能说明: 判断当前活动采样格式是否支持 HiRes 模式参与轮转。
*   形    参: 无。
*   返 回 值: true 表示支持 HiRes。
*********************************************************************************************************
*/
static bool format_supports_hires(void)
{
    DSO_SampleFormat fmt = DSO_SampleSpec_GetActiveFormat();

    return (fmt == DSO_SAMPLE_FORMAT_10BIT) ||
           (fmt == DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
}

/*
*********************************************************************************************************
*   函 数 名: clamp_acquire_mode
*   功能说明: 根据当前采样格式将采集显示模式规整到可用范围。
*   形    参: mode - 输入采集显示模式。
*   返 回 值: 有效采集显示模式。
*********************************************************************************************************
*/
static DSO_AcquireMode clamp_acquire_mode(DSO_AcquireMode mode)
{
    if (format_supports_hires()) {
        switch (mode) {
        case DSO_ACQUIRE_MODE_SAMPLE:
        case DSO_ACQUIRE_MODE_PEAK_DETECT:
        case DSO_ACQUIRE_MODE_HIRES:
        case DSO_ACQUIRE_MODE_AVERAGE:
            return mode;
        default:
            return DSO_ACQUIRE_MODE_SAMPLE;
        }
    }

    switch (mode) {
    case DSO_ACQUIRE_MODE_SAMPLE:
    case DSO_ACQUIRE_MODE_PEAK_DETECT:
    case DSO_ACQUIRE_MODE_AVERAGE:
        return mode;
    default:
        return DSO_ACQUIRE_MODE_SAMPLE;
    }
}

/*
*********************************************************************************************************
*   函 数 名: clamp_info
*   功能说明: 对采集控制状态做集中边界规整，限制电平、百分比、记录深度等范围。
*   形    参: info - 待规整的控制状态。
*   返 回 值: 无。
*********************************************************************************************************
*/
static void clamp_info(DSO_AcquireCtrlInfo *info)
{
    if (info == NULL) {
        return;
    }

    info->trigger_mode = clamp_trigger_mode(info->trigger_mode);
    info->trigger_slope = clamp_trigger_slope(info->trigger_slope);
    info->trigger_source = clamp_trigger_source(info->trigger_source);
    info->acquire_mode = clamp_acquire_mode(info->acquire_mode);
    if (info->trigger_level_v < 0.0f) {
        info->trigger_level_v = 0.0f;
    }
    if (info->trigger_level_v > DSO_VREF) {
        info->trigger_level_v = DSO_VREF;
    }
    if (info->trigger_position_pct < 10u) {
        info->trigger_position_pct = 10u;
    }
    if (info->trigger_position_pct > 90u) {
        info->trigger_position_pct = 90u;
    }
    if (info->record_depth_index > (uint8_t)DSO_RECORD_DEPTH_DEEP) {
        info->record_depth_index = (uint8_t)DSO_RECORD_DEPTH_DEEP;
    }
    if (info->pretrigger_pct < 10u) {
        info->pretrigger_pct = 10u;
    }
    if (info->pretrigger_pct > 90u) {
        info->pretrigger_pct = 90u;
    }
    info->average_depth = DSO_AVERAGE_DEPTH;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_ResetAveraging
*   功能说明: 清空所有通道的平均采集缓冲和帧计数。
*   形    参: 无。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcquireCtrl_ResetAveraging(void)
{
    memset(s_ctrl.avg_buf, 0, sizeof(s_ctrl.avg_buf));
    memset(s_ctrl.avg_sample_count, 0, sizeof(s_ctrl.avg_sample_count));
    memset(s_ctrl.avg_frame_count, 0, sizeof(s_ctrl.avg_frame_count));
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_Init
*   功能说明: 初始化 active/pending 控制状态，建立默认触发和 RUN 状态。
*   形    参: 无。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcquireCtrl_Init(void)
{
    memset(&s_ctrl, 0, sizeof(s_ctrl));
    s_ctrl.active = default_info();
    s_ctrl.pending = s_ctrl.active;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_SetTriggerMode
*   功能说明: 设置 pending 触发模式，等待引擎循环 apply。
*   形    参: mode - 触发模式。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcquireCtrl_SetTriggerMode(DSO_TriggerMode mode)
{
    s_ctrl.pending.trigger_mode = mode;
    s_ctrl.pending_change = true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_SetTriggerLevelVolts
*   功能说明: 设置 pending 触发电平。
*   形    参: volts - 触发电平，单位 V。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcquireCtrl_SetTriggerLevelVolts(float volts)
{
    s_ctrl.pending.trigger_level_v = volts;
    s_ctrl.pending_change = true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_SetTriggerLevelEnabled
*   功能说明: 设置 pending 触发电平使能状态。
*   形    参: enabled - true 启用触发电平。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcquireCtrl_SetTriggerLevelEnabled(bool enabled)
{
    s_ctrl.pending.trigger_level_enabled = enabled ? true : false;
    s_ctrl.pending_change = true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_SetTriggerSlope
*   功能说明: 设置 pending 触发边沿。
*   形    参: slope - 触发边沿。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcquireCtrl_SetTriggerSlope(DSO_TriggerSlope slope)
{
    s_ctrl.pending.trigger_slope = slope;
    s_ctrl.pending_change = true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_SetTriggerSource
*   功能说明: 设置 pending 触发源。
*   形    参: source - 触发源通道。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcquireCtrl_SetTriggerSource(DSO_TriggerSource source)
{
    s_ctrl.pending.trigger_source = source;
    s_ctrl.pending_change = true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_NormalizeTriggerSource
*   功能说明: 立即规整 active/pending 触发源，常用于 CH2 不可用时强制回到 CH1。
*   形    参: source - 期望触发源。
*   返 回 值: true 表示 active 触发源发生变化。
*********************************************************************************************************
*/
bool DSO_AcquireCtrl_NormalizeTriggerSource(DSO_TriggerSource source)
{
    DSO_TriggerSource clamped = clamp_trigger_source(source);

    if (s_ctrl.active.trigger_source == clamped) {
        s_ctrl.pending.trigger_source = clamped;
        return false;
    }

    s_ctrl.active.trigger_source = clamped;
    s_ctrl.pending.trigger_source = clamped;

    if (s_ctrl.active.trigger_mode == DSO_TRIGGER_MODE_SINGLE) {
        s_ctrl.active.single_armed = true;
        s_ctrl.active.single_captured = false;
        s_ctrl.pending.single_armed = true;
        s_ctrl.pending.single_captured = false;
    }

    return true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_SetTriggerPositionPct
*   功能说明: 设置 pending 触发位置百分比。
*   形    参: pct - 触发位置百分比。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcquireCtrl_SetTriggerPositionPct(uint8_t pct)
{
    s_ctrl.pending.trigger_position_pct = pct;
    s_ctrl.pending_change = true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_SetAcquireMode
*   功能说明: 设置 pending 采集显示模式。
*   形    参: mode - 采集显示模式。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcquireCtrl_SetAcquireMode(DSO_AcquireMode mode)
{
    s_ctrl.pending.acquire_mode = mode;
    s_ctrl.pending_change = true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_GetNextAcquireMode
*   功能说明: 根据当前采样格式返回下一个可轮转的采集显示模式。
*   形    参: current - 当前采集显示模式。
*   返 回 值: 下一个采集显示模式。
*********************************************************************************************************
*/
DSO_AcquireMode DSO_AcquireCtrl_GetNextAcquireMode(DSO_AcquireMode current)
{
    if (format_supports_hires()) {
        switch (current) {
        case DSO_ACQUIRE_MODE_SAMPLE:
            return DSO_ACQUIRE_MODE_PEAK_DETECT;
        case DSO_ACQUIRE_MODE_PEAK_DETECT:
            return DSO_ACQUIRE_MODE_HIRES;
        case DSO_ACQUIRE_MODE_HIRES:
            return DSO_ACQUIRE_MODE_AVERAGE;
        default:
            return DSO_ACQUIRE_MODE_SAMPLE;
        }
    }

    switch (current) {
    case DSO_ACQUIRE_MODE_SAMPLE:
        return DSO_ACQUIRE_MODE_PEAK_DETECT;
    case DSO_ACQUIRE_MODE_PEAK_DETECT:
        return DSO_ACQUIRE_MODE_AVERAGE;
    default:
        return DSO_ACQUIRE_MODE_SAMPLE;
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_SetRecordDepthIndex
*   功能说明: 设置 pending 记录深度索引。
*   形    参: idx - 记录深度索引。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcquireCtrl_SetRecordDepthIndex(uint8_t idx)
{
    s_ctrl.pending.record_depth_index = idx;
    s_ctrl.pending_change = true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_SetPretriggerPct
*   功能说明: 设置 pending 预触发比例。
*   形    参: pct - 预触发比例。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcquireCtrl_SetPretriggerPct(uint8_t pct)
{
    s_ctrl.pending.pretrigger_pct = pct;
    s_ctrl.pending_change = true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_SetRecordViewOffset
*   功能说明: 设置 pending 深存储显示窗口偏移。
*   形    参: samples - 偏移量，单位样本。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcquireCtrl_SetRecordViewOffset(int32_t samples)
{
    s_ctrl.pending.record_view_offset = samples;
    s_ctrl.pending_change = true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_RearmSingle
*   功能说明: 请求重新 arm Single 模式，实际状态在 ApplyPending 中生效。
*   形    参: 无。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcquireCtrl_RearmSingle(void)
{
    s_ctrl.pending_rearm = true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_SetRunRequested
*   功能说明: 设置 RUN/STOP 逻辑请求，不直接操作 ADC/DMA 物理启停。
*   形    参: running - true 表示 RUN，false 表示 STOP。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcquireCtrl_SetRunRequested(bool running)
{
    s_ctrl.pending.run_requested = running ? true : false;
    s_ctrl.pending_run_change = true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_IsRunRequested
*   功能说明: 查询当前 active RUN/STOP 逻辑状态。
*   形    参: 无。
*   返 回 值: true 表示 RUN 请求有效。
*********************************************************************************************************
*/
bool DSO_AcquireCtrl_IsRunRequested(void)
{
    return s_ctrl.active.run_requested;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_ApplyPending
*   功能说明: 将 pending 控制状态应用到 active，处理 Single rearm、RUN/STOP 和平均缓冲复位。
*   形    参: 无。
*   返 回 值: true 表示普通控制参数或 Single rearm 引起 active 状态变化。
*********************************************************************************************************
*/
bool DSO_AcquireCtrl_ApplyPending(void)
{
    bool changed = false;

    if (s_ctrl.pending_change) {
        s_ctrl.active = s_ctrl.pending;
        clamp_info(&s_ctrl.active);
        changed = true;
    }

    if (s_ctrl.pending_rearm && (s_ctrl.active.trigger_mode == DSO_TRIGGER_MODE_SINGLE)) {
        changed = true;
    }

    if (s_ctrl.pending_run_change) {
        s_ctrl.active.run_requested = s_ctrl.pending.run_requested;
    }

    if (changed) {
        if (s_ctrl.active.trigger_mode == DSO_TRIGGER_MODE_SINGLE) {
            s_ctrl.active.single_armed = true;
            s_ctrl.active.single_captured = false;
        } else {
            s_ctrl.active.single_armed = false;
            s_ctrl.active.single_captured = false;
        }
        DSO_AcquireCtrl_ResetAveraging();
    }

    s_ctrl.pending = s_ctrl.active;
    s_ctrl.pending.average_depth = DSO_AVERAGE_DEPTH;
    s_ctrl.pending.single_armed = s_ctrl.active.single_armed;
    s_ctrl.pending.single_captured = s_ctrl.active.single_captured;
    s_ctrl.pending.run_requested = s_ctrl.active.run_requested;
    s_ctrl.pending_change = false;
    s_ctrl.pending_rearm = false;
    s_ctrl.pending_run_change = false;
    return changed;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_GetInfo
*   功能说明: 读取当前 active 采集控制状态。
*   形    参: out - 输出控制状态指针。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcquireCtrl_GetInfo(DSO_AcquireCtrlInfo *out)
{
    if (out == NULL) {
        return;
    }
    *out = s_ctrl.active;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_OnFrameResult
*   功能说明: 引擎完成一帧后回传触发结果，用于 Single 模式捕获完成锁存。
*   形    参: triggered - 当前帧是否触发成功。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcquireCtrl_OnFrameResult(bool triggered)
{
    if ((s_ctrl.active.trigger_mode == DSO_TRIGGER_MODE_SINGLE) &&
        s_ctrl.active.single_armed &&
        triggered) {
        s_ctrl.active.single_armed = false;
        s_ctrl.active.single_captured = true;
        s_ctrl.pending.single_armed = false;
        s_ctrl.pending.single_captured = true;
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_ApplyAverageForChannel
*   功能说明: 对指定通道执行多帧滑动平均；非平均模式或参数无效时返回原始样本指针。
*   形    参: channel - 通道索引，0 为 CH1，非 0 作为 CH2。
*             samples - 输入样本。
*             count   - 样本数量。
*   返 回 值: 平均缓冲指针或原始 samples 指针。
*********************************************************************************************************
*/
const uint16_t *DSO_AcquireCtrl_ApplyAverageForChannel(uint8_t channel,
                                                       const uint16_t *samples,
                                                       uint32_t count)
{
    uint8_t depth = DSO_AVERAGE_DEPTH;
    uint8_t idx = (channel != 0u) ? 1u : 0u;
    uint8_t frames;

    if ((samples == NULL) || (count == 0u) || (count > (DSO_DMA_BUF_SIZE * 2u))) {
        return samples;
    }

    if (s_ctrl.active.acquire_mode != DSO_ACQUIRE_MODE_AVERAGE) {
        return samples;
    }

    if (s_ctrl.avg_sample_count[idx] != count) {
        s_ctrl.avg_sample_count[idx] = count;
        s_ctrl.avg_frame_count[idx] = 0u;
    }

    frames = s_ctrl.avg_frame_count[idx];
    if (frames == 0u) {
        memcpy(s_ctrl.avg_buf[idx], samples, sizeof(uint16_t) * count);
        s_ctrl.avg_frame_count[idx] = 1u;
        return s_ctrl.avg_buf[idx];
    }

    if (frames < depth) {
        frames++;
        s_ctrl.avg_frame_count[idx] = frames;
    }

    for (uint32_t i = 0u; i < count; ++i) {
        int32_t acc = (int32_t)s_ctrl.avg_buf[idx][i];
        int32_t sample = (int32_t)samples[i];
        int32_t next = acc + ((sample - acc) / (int32_t)frames);

        if (next < 0) {
            next = 0;
        } else if (next > 0xFFFF) {
            next = 0xFFFF;
        }
        s_ctrl.avg_buf[idx][i] = (uint16_t)next;
    }

    return s_ctrl.avg_buf[idx];
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcquireCtrl_ApplyAverage
*   功能说明: 对 CH1 执行平均采集处理。
*   形    参: samples - 输入样本。
*             count   - 样本数量。
*   返 回 值: 平均缓冲指针或原始 samples 指针。
*********************************************************************************************************
*/
const uint16_t *DSO_AcquireCtrl_ApplyAverage(const uint16_t *samples, uint32_t count)
{
    return DSO_AcquireCtrl_ApplyAverageForChannel(0u, samples, count);
}
