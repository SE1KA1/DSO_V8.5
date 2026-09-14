/**
 * @file module_5ch_stub.h
 * @brief 精简五模块交付工程的符号桩声明。
 *
 * 主固件 dso_module_upload.c 里由 dso_switch_cmd / dso_freq_meter / dso_acq
 * 提供的符号，在本精简工程中不编译那些子系统，改为桩实现：
 *  - DSO_Switch_UsbActive / DSO_Switch_BuzzerMute → 恒 0（无串口命令通道/无静音）
 *  - bgm_adc_dac 相关 → GetSnapshot 恒 false + 采样态恒 0（发占位无效帧）
 *
 * DSO_FreqMeterSnapshot 结构/枚举逐字段照抄主固件 dso_freq_meter.h，
 * 保证 dso_module_upload.c 的 JSON 模板编译一致。
 */
#ifndef MODULE_5CH_STUB_H
#define MODULE_5CH_STUB_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 与 dso_freq_meter_math.h 一致 */
typedef enum {
    DSO_FREQ_METER_MODE_LOW_CAPTURE = 0,
    DSO_FREQ_METER_MODE_GATED_COUNT,
} DSO_FreqMeterMode;

/* 与 dso_freq_meter.h 一致 */
typedef enum {
    DSO_FREQ_METER_STATE_STOPPED = 0,
    DSO_FREQ_METER_STATE_WAIT_EDGE,
    DSO_FREQ_METER_STATE_MEASURING,
    DSO_FREQ_METER_STATE_VALID,
    DSO_FREQ_METER_STATE_NO_SIGNAL,
    DSO_FREQ_METER_STATE_OVERRANGE,
    DSO_FREQ_METER_STATE_FAULT,
} DSO_FreqMeterState;

/* 与 dso_freq_meter.h 逐字段一致 */
typedef struct {
    bool               valid;
    DSO_FreqMeterState state;
    DSO_FreqMeterMode  mode;
    uint64_t freq_millihz;
    uint32_t gate_us;
    uint32_t edge_count;
    uint32_t ref_ticks;
    uint32_t overflow_count;
    uint32_t last_edge_age_ms;
    bool               overrange;
} DSO_FreqMeterSnapshot;

/** bgm 占位：恒返回 false（上报走 freq_valid:0 分支）。 */
bool DSO_FreqMeter_GetSnapshot(DSO_FreqMeterSnapshot *out);

/** bgm 采样态占位（主固件 DSO_Acq_Get* 签名，返回类型以 uint32_t 兼容）。 */
uint32_t DSO_Acq_GetSampleFormat(void);
uint32_t DSO_Acq_GetSampleRate(void);
uint32_t DSO_Acq_GetOverrunCount(void);

/** 精简工程无 USB 串口命令通道 → YDS5/USB 活动恒 0。 */
uint8_t DSO_Switch_UsbActive(void);

/** 精简工程无静音命令 → 蜂鸣器默认不静音（声光联动照常）。 */
uint8_t DSO_Switch_BuzzerMute(void);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_5CH_STUB_H */
