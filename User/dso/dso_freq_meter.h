/**
 * @file dso_freq_meter.h
 * @brief PB15/TIM12 外部频率计状态机 API。
 */

#ifndef DSO_FREQ_METER_H
/* 宏定义: DSO_FREQ_METER_H - 头文件保护宏。 */
#define DSO_FREQ_METER_H

#include "dso_freq_meter_math.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DSO_FREQ_METER_STATE_STOPPED = 0,
    DSO_FREQ_METER_STATE_WAIT_EDGE,
    DSO_FREQ_METER_STATE_MEASURING,
    DSO_FREQ_METER_STATE_VALID,
    DSO_FREQ_METER_STATE_NO_SIGNAL,
    DSO_FREQ_METER_STATE_OVERRANGE,
    DSO_FREQ_METER_STATE_FAULT,
} DSO_FreqMeterState;

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

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeter_Init
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeter_Init(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeter_Start
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeter_Start(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeter_Stop
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeter_Stop(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeter_Service
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeter_Service(uint32_t now_ms);
/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeter_GetSnapshot
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_FreqMeter_GetSnapshot(DSO_FreqMeterSnapshot *out);
/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeter_TIM12_IRQHandler
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeter_TIM12_IRQHandler(void);

#endif /* DSO_FREQ_METER_H */
