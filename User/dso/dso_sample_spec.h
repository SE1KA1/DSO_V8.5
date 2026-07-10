/**
 * @file dso_sample_spec.h
 * @brief 产品采样格式规格：8-bit 三路交替与 10-bit 双 lane 定义。
 */

#ifndef DSO_SAMPLE_SPEC_H
/* 宏定义: DSO_SAMPLE_SPEC_H - 头文件保护宏。 */
#define DSO_SAMPLE_SPEC_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    /* 10-bit 采样格式：CH1 双 lane 交织，CH2 实时采样。 */
    DSO_SAMPLE_FORMAT_10BIT = 0,
    /* 8-bit 高速采样格式：CH1 三路交替采样，当前产品默认格式。 */
    DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED = 1,
} DSO_SampleFormat;

/* 上电默认采样格式；由 DSO_Acq_Init() 读取并初始化对应采集后端。 */
#define DSO_SAMPLE_FORMAT_DEFAULT DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED

typedef enum {
    /* CH2 独立实时采样，典型用于 10-bit 模式。 */
    DSO_CH2_MODE_REALTIME = 0,
    /* CH2 预览模式预留值，当前主链路未作为产品路径使用。 */
    DSO_CH2_MODE_PREVIEW = 1,
    /* CH2 不可用，8-bit 三路交替模式下 ADC3 作为 CH1 第三 lane。 */
    DSO_CH2_MODE_DISABLED = 2,
} DSO_Ch2Mode;

typedef struct {
    /* ADC 有效位宽，用于测量换算、UI 显示和削顶判断。 */
    uint8_t adc_bits;
    /* 当前格式的 ADC 最大 code 值，例如 10-bit 为 1023、8-bit 为 255。 */
    uint16_t adc_max;
    /* 当前格式的中点 code，用于触发默认值和垂直映射中心。 */
    uint16_t midscale;
    /* 当前格式使用的参考电压，单位 V。 */
    float vref;
    /* CH1 采样 lane 数量，用于交织合并、校准和采样率计算。 */
    uint8_t lane_count;
    /* CH2 在当前采样格式下的工作方式。 */
    DSO_Ch2Mode ch2_mode;
    /* 是否为交织采样格式；影响渲染和校准策略。 */
    bool interleaved;
    /* 初始化后是否立即启动采集后端。 */
    bool start_immediately;
} DSO_SampleSpec;

/*
*********************************************************************************************************
*   函 数 名: DSO_SampleSpec_Get
*   功能说明: 按采样格式取得只读规格表项。
*   形    参: format - 采样格式枚举值。
*   返 回 值: 指向 DSO_SampleSpec 的只读指针；非法 format 返回安全默认表项。
*********************************************************************************************************
*/
const DSO_SampleSpec *DSO_SampleSpec_Get(DSO_SampleFormat format);

/*
*********************************************************************************************************
*   函 数 名: DSO_SampleSpec_SetActive
*   功能说明: 设置当前活动采样格式，供采集 facade、处理和 UI 查询。
*   形    参: format - 需要设为活动状态的采样格式。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_SampleSpec_SetActive(DSO_SampleFormat format);

/*
*********************************************************************************************************
*   函 数 名: DSO_SampleSpec_GetActiveFormat
*   功能说明: 读取当前活动采样格式枚举值。
*   形    参: 无。
*   返 回 值: 当前活动的 DSO_SampleFormat。
*********************************************************************************************************
*/
DSO_SampleFormat DSO_SampleSpec_GetActiveFormat(void);

/*
*********************************************************************************************************
*   函 数 名: DSO_SampleSpec_GetActive
*   功能说明: 读取当前活动采样格式对应的规格表项。
*   形    参: 无。
*   返 回 值: 当前活动 DSO_SampleSpec 的只读指针。
*********************************************************************************************************
*/
const DSO_SampleSpec *DSO_SampleSpec_GetActive(void);

/*
*********************************************************************************************************
*   函 数 名: DSO_SampleSpec_ClampFromI32
*   功能说明: 将有符号计算结果钳位到当前采样格式允许的 ADC code 范围。
*   形    参: spec  - 采样格式规格指针。
*             value - 待钳位的有符号数值。
*   返 回 值: 钳位后的无符号 ADC code。
*********************************************************************************************************
*/
uint16_t DSO_SampleSpec_ClampFromI32(const DSO_SampleSpec *spec, int32_t value);

#endif /* DSO_SAMPLE_SPEC_H */
