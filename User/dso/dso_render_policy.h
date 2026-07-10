/**
 * @file dso_render_policy.h
 * @brief 渲染重建策略决策（envelope/line/HS 增强）。
 */

#ifndef DSO_RENDER_POLICY_H
/* 宏定义: DSO_RENDER_POLICY_H - 头文件保护宏。 */
#define DSO_RENDER_POLICY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    /* 原始线性连接显示。 */
    DSO_RECON_MODE_RAW_LINEAR = 0,
    /* 原始受限显示，用于采样点不足或频率信息不可靠时。 */
    DSO_RECON_MODE_RAW_LIMITED = 1,
    /* ETS 等效时间采样重建显示。 */
    DSO_RECON_MODE_ETS = 2,
    /* 8-bit 高速增强重建显示。 */
    DSO_RECON_MODE_HS8_ENHANCED = 3,
    /* 10-bit 高速增强重建显示。 */
    DSO_RECON_MODE_HS10_ENHANCED = 4,
} DSO_ReconMode;

typedef enum {
    /* 显示质量良好。 */
    DSO_DISPLAY_QUALITY_GOOD = 0,
    /* 显示质量受限，但仍可显示。 */
    DSO_DISPLAY_QUALITY_LIMITED = 1,
    /* 需要重复信号支持，典型为 ETS 构建阶段。 */
    DSO_DISPLAY_QUALITY_REPEAT_REQUIRED = 2,
} DSO_DisplayQuality;

typedef struct {
    /* 是否处于 fast 时基。 */
    bool fast_mode;
    /* 是否为 sample 采集模式。 */
    bool sample_mode;
    /* 上层是否允许 ETS。 */
    bool allow_ets;
    /* 频率分析是否有效。 */
    bool freq_valid;
    /* 信号是否疑似削顶。 */
    bool clipped;
    /* ETS 缓冲是否已经构建完成。 */
    bool ets_ready;
    /* 重复性置信度，范围通常为 0..1。 */
    float repetitive_confidence;
    /* 进入重建策略所需的最低重复性置信度。 */
    float repetitive_confidence_min;
    /* 每周期样本数。 */
    float samples_per_cycle;
} DSO_RenderPolicyInput;

typedef struct {
    /* 选择的重建模式。 */
    DSO_ReconMode recon_mode;
    /* 输出给 UI/诊断的显示质量等级。 */
    DSO_DisplayQuality display_quality;
    /* ETS 是否仍处于构建阶段。 */
    bool ets_building;
} DSO_RenderPolicyDecision;

/*
*********************************************************************************************************
*   函 数 名: DSO_RenderPolicy_Decide
*   功能说明: 根据频率、重复性、每周期样本数和 ETS 状态选择渲染重建策略。
*   形    参: in  - 策略输入。
*             out - 策略输出。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_RenderPolicy_Decide(const DSO_RenderPolicyInput *in,
                             DSO_RenderPolicyDecision *out);

#endif /* DSO_RENDER_POLICY_H */
