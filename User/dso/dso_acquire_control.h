/**
 * @file dso_acquire_control.h
 * @brief 采集控制：RUN/STOP、触发模式/源/边沿/电平、记录深度。
 */

#ifndef DSO_ACQUIRE_CONTROL_H
/* 宏定义: DSO_ACQUIRE_CONTROL_H - 头文件保护宏。 */
#define DSO_ACQUIRE_CONTROL_H

#include "dso_record.h"
#include "dso_config.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    /* 自动触发：未找到触发点时仍允许刷新。 */
    DSO_TRIGGER_MODE_AUTO = 0,
    /* 普通触发：未满足触发条件时等待。 */
    DSO_TRIGGER_MODE_NORMAL = 1,
    /* 单次触发：触发一次后进入捕获完成状态。 */
    DSO_TRIGGER_MODE_SINGLE = 2,
} DSO_TriggerMode;

typedef enum {
    /* 上升沿触发。 */
    DSO_TRIGGER_SLOPE_RISING = 0,
    /* 下降沿触发。 */
    DSO_TRIGGER_SLOPE_FALLING = 1,
    /* 双边沿触发。 */
    DSO_TRIGGER_SLOPE_BOTH = 2,
} DSO_TriggerSlope;

typedef enum {
    /* 以 CH1 作为触发源。 */
    DSO_TRIGGER_SOURCE_CH1 = 0,
    /* 以 CH2 作为触发源；CH2 不可用时会规整回 CH1。 */
    DSO_TRIGGER_SOURCE_CH2 = 1,
} DSO_TriggerSource;

typedef enum {
    /* 普通采样显示。 */
    DSO_ACQUIRE_MODE_SAMPLE = 0,
    /* 峰值检测显示，适合稠密窗口保留尖峰。 */
    DSO_ACQUIRE_MODE_PEAK_DETECT = 1,
    /* 高分辨率采样显示，当前支持格式下参与模式轮转。 */
    DSO_ACQUIRE_MODE_HIRES = 2,
    /* 多帧平均显示。 */
    DSO_ACQUIRE_MODE_AVERAGE = 3,
} DSO_AcquireMode;

typedef struct {
    /* 触发模式。 */
    DSO_TriggerMode trigger_mode;
    /* 触发边沿。 */
    DSO_TriggerSlope trigger_slope;
    /* 触发源通道。 */
    DSO_TriggerSource trigger_source;
    /* 触发电平，单位 V。 */
    float trigger_level_v;
    /* 是否启用触发电平线和电平判定。 */
    bool trigger_level_enabled;
    /* 触发点在显示窗口中的百分比位置。 */
    uint8_t trigger_position_pct;
    /* 采集显示模式。 */
    DSO_AcquireMode acquire_mode;
    /* 平均深度。 */
    uint8_t average_depth;
    /* 记录深度索引。 */
    uint8_t record_depth_index;
    /* 预触发比例。 */
    uint8_t pretrigger_pct;
    /* 深存储视图偏移，单位样本。 */
    int32_t record_view_offset;
    /* Single 模式是否已经捕获完成。 */
    bool single_captured;
    /* Single 模式是否处于 armed 状态。 */
    bool single_armed;
    /* UI 请求的 RUN/STOP 逻辑状态；手动 STOP 不直接物理停止 ADC/DMA。 */
    bool run_requested;
} DSO_AcquireCtrlInfo;

/* 初始化采集控制状态和默认触发参数。 */
void DSO_AcquireCtrl_Init(void);

/* 设置 pending 触发模式。 */
void DSO_AcquireCtrl_SetTriggerMode(DSO_TriggerMode mode);

/* 设置 pending 触发电平，单位 V。 */
void DSO_AcquireCtrl_SetTriggerLevelVolts(float volts);

/* 设置 pending 触发电平使能状态。 */
void DSO_AcquireCtrl_SetTriggerLevelEnabled(bool enabled);

/* 设置 pending 触发边沿。 */
void DSO_AcquireCtrl_SetTriggerSlope(DSO_TriggerSlope slope);

/* 设置 pending 触发源。 */
void DSO_AcquireCtrl_SetTriggerSource(DSO_TriggerSource source);

/* 立即规整并同步触发源，常用于采样格式切换后 CH2 可用性变化。 */
bool DSO_AcquireCtrl_NormalizeTriggerSource(DSO_TriggerSource source);

/* 设置 pending 触发点百分比位置。 */
void DSO_AcquireCtrl_SetTriggerPositionPct(uint8_t pct);

/* 设置 pending 采集显示模式。 */
void DSO_AcquireCtrl_SetAcquireMode(DSO_AcquireMode mode);

/* 按当前采样格式返回下一个可用采集显示模式。 */
DSO_AcquireMode DSO_AcquireCtrl_GetNextAcquireMode(DSO_AcquireMode current);

/* 设置 pending 记录深度索引。 */
void DSO_AcquireCtrl_SetRecordDepthIndex(uint8_t idx);

/* 设置 pending 预触发比例。 */
void DSO_AcquireCtrl_SetPretriggerPct(uint8_t pct);

/* 设置 pending 深存储视图偏移。 */
void DSO_AcquireCtrl_SetRecordViewOffset(int32_t samples);

/* 重新 arm Single 触发。 */
void DSO_AcquireCtrl_RearmSingle(void);

/* 设置 RUN/STOP 请求状态。 */
void DSO_AcquireCtrl_SetRunRequested(bool running);

/* 查询当前生效的 RUN/STOP 请求状态。 */
bool DSO_AcquireCtrl_IsRunRequested(void);

/* 将 pending 控制参数应用到 active 状态。 */
bool DSO_AcquireCtrl_ApplyPending(void);

/* 读取当前 active 控制状态。 */
void DSO_AcquireCtrl_GetInfo(DSO_AcquireCtrlInfo *out);

/* 引擎每帧处理结果回传，用于 Single 模式完成锁存。 */
void DSO_AcquireCtrl_OnFrameResult(bool triggered);

/* 清空平均采集累积状态。 */
void DSO_AcquireCtrl_ResetAveraging(void);

/* 对指定通道应用平均采集处理；非平均模式直接返回原始输入指针。 */
const uint16_t *DSO_AcquireCtrl_ApplyAverageForChannel(uint8_t channel,
                                                       const uint16_t *samples,
                                                       uint32_t count);

/* 对 CH1 应用平均采集处理。 */
const uint16_t *DSO_AcquireCtrl_ApplyAverage(const uint16_t *samples, uint32_t count);

#endif /* DSO_ACQUIRE_CONTROL_H */
