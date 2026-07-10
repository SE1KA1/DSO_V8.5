/**
 * @file dso_freq_meter_hw_tim12.h
 * @brief TIM12_CH2/PB15 硬件捕获与 gated count。
 */

#ifndef DSO_FREQ_METER_HW_TIM12_H
/* 宏定义: DSO_FREQ_METER_HW_TIM12_H - 头文件保护宏。 */
#define DSO_FREQ_METER_HW_TIM12_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t overflow_count;
    uint16_t counter;
    bool     pending_update;
} DSO_FreqMeterHwCounterSnapshot;

typedef struct {
    bool     ready;
    bool     overcapture;
    uint64_t ticks;
    uint32_t periods;
    uint32_t overflow_count;
    uint32_t capture_irq_count;
    uint32_t update_irq_count;
} DSO_FreqMeterHwCaptureResult;

typedef struct {
    uint32_t cr1;
    uint32_t smcr;
    uint32_t dier;
    uint32_t sr;
    uint32_t ccmr1;
    uint32_t ccer;
    uint16_t counter;
    uint16_t capture;
    uint32_t overflow_count;
    bool     pending_update;
    bool     capture_ready;
    bool     capture_overrun;
    uint64_t capture_ticks;
    uint32_t capture_periods;
    uint32_t capture_irq_count;
    uint32_t update_irq_count;
} DSO_FreqMeterHwTim12DebugSnapshot;

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterHwTim12_InitGpio
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeterHwTim12_InitGpio(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterHwTim12_ConfigureCaptureMode
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeterHwTim12_ConfigureCaptureMode(uint16_t prescaler);
/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterHwTim12_ConfigureCounterMode
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeterHwTim12_ConfigureCounterMode(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterHwTim12_Start
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeterHwTim12_Start(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterHwTim12_Stop
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeterHwTim12_Stop(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterHwTim12_LatchAndResetCounter
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_FreqMeterHwCounterSnapshot DSO_FreqMeterHwTim12_LatchAndResetCounter(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterHwTim12_TakeCaptureResult
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_FreqMeterHwCaptureResult DSO_FreqMeterHwTim12_TakeCaptureResult(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterHwTim12_ReadDebug
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeterHwTim12_ReadDebug(DSO_FreqMeterHwTim12DebugSnapshot *out);
/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterHwTim12_IRQHandler
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeterHwTim12_IRQHandler(void);

#endif /* DSO_FREQ_METER_HW_TIM12_H */
