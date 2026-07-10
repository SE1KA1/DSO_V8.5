/**
 * @file dso_hs8_render.h
 * @brief 8-bit 高频重复信号增强重建。
 */

#ifndef DSO_HS8_RENDER_H
/* 宏定义: DSO_HS8_RENDER_H - 头文件保护宏。 */
#define DSO_HS8_RENDER_H

#include <stdbool.h>
#include <stdint.h>

/* 宏定义: DSO_HS8_RENDER_PHASE_BINS - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_HS8_RENDER_PHASE_BINS 128u

typedef struct {
    float locked_freq_hz;
    bool ready;
    float bin_sum[DSO_HS8_RENDER_PHASE_BINS];
    uint16_t bin_count[DSO_HS8_RENDER_PHASE_BINS];
} DSO_HS8RenderState;

/*
*********************************************************************************************************
*   函 数 名: DSO_HS8Render_Reset
*   功能说明: 处理 8-bit 高速重复信号增强重建的累积、状态判断和渲染输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_HS8Render_Reset(DSO_HS8RenderState *state);
/*
*********************************************************************************************************
*   函 数 名: DSO_HS8Render_ShouldBuild
*   功能说明: 处理 8-bit 高速重复信号增强重建的累积、状态判断和渲染输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_HS8Render_ShouldBuild(bool fast_mode,
                               bool sample_mode,
                               bool freq_valid,
                               bool clipped,
                               float repetitive_confidence,
                               float samples_per_cycle);
/*
*********************************************************************************************************
*   函 数 名: DSO_HS8Render_Accumulate
*   功能说明: 处理 8-bit 高速重复信号增强重建的累积、状态判断和渲染输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_HS8Render_Accumulate(DSO_HS8RenderState *state,
                              const uint16_t *samples,
                              uint32_t sample_count,
                              uint32_t sample_rate_hz,
                              float freq_hz,
                              float trig_pos_samples);
/*
*********************************************************************************************************
*   函 数 名: DSO_HS8Render_IsReady
*   功能说明: 处理 8-bit 高速重复信号增强重建的累积、状态判断和渲染输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_HS8Render_IsReady(const DSO_HS8RenderState *state);
/*
*********************************************************************************************************
*   函 数 名: DSO_HS8Render_Render
*   功能说明: 处理 8-bit 高速重复信号增强重建的累积、状态判断和渲染输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_HS8Render_Render(const DSO_HS8RenderState *state,
                          float sec_div_ns,
                          uint16_t adc_max,
                          uint16_t midscale,
                          uint16_t *out_display,
                          uint32_t display_width);
/*
*********************************************************************************************************
*   函 数 名: DSO_HS8Render_EffectiveRateHz
*   功能说明: 处理 8-bit 高速重复信号增强重建的累积、状态判断和渲染输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
float DSO_HS8Render_EffectiveRateHz(const DSO_HS8RenderState *state);

#endif /* DSO_HS8_RENDER_H */
