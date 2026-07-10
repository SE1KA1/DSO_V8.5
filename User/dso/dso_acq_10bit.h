/**
 * @file dso_acq_10bit.h
 * @brief 10-bit 采集后端 API：ADC1/2/3 + TIM1 + DMA 配置。
 */

#ifndef DSO_ACQ_10BIT_H
/* 宏定义: DSO_ACQ_10BIT_H - 头文件保护宏。 */
#define DSO_ACQ_10BIT_H

#include "dso_acq_model.h"
#include "stm32h7xx.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct DSO_AcqDebugSnapshot DSO_AcqDebugSnapshot;

/* 初始化 10-bit 采集后端，包括 ADC1/ADC2/ADC3、TIM1 和 DMA。 */
void DSO_Acq10_Init(void);

/* 释放或关闭 10-bit 采集后端资源。 */
void DSO_Acq10_Deinit(void);

/* 周期服务 10-bit 后端，当前主要保留统一后端接口。 */
void DSO_Acq10_Service(void);

/* 启动 10-bit 采集。 */
void DSO_Acq10_Start(void);

/* 停止 10-bit 采集。 */
void DSO_Acq10_Stop(void);

/* 查询 10-bit 后端是否正在运行。 */
bool DSO_Acq10_IsRunning(void);

/* 查询 10-bit 后端是否已有可处理数据。 */
bool DSO_Acq10_IsDataReady(void);

/* 读取 CH1 DMA 当前位置。 */
uint32_t DSO_Acq10_GetPos(void);

/* 读取 CH1 DMA 原始缓冲地址。 */
const volatile uint32_t *DSO_Acq10_GetBuf(void);

/* 读取 CH2 DMA 当前位置。 */
uint32_t DSO_Acq10_GetCh2Pos(void);

/* 读取 CH2 DMA 原始缓冲地址。 */
const volatile uint16_t *DSO_Acq10_GetCh2Buf(void);

/* 读取 10-bit 校准偏移。 */
int16_t DSO_Acq10_GetCalOffset(void);

/* 读取 CH1 双 lane 交织后的等效采样率。 */
uint32_t DSO_Acq10_GetSampleRate(void);

/* 读取 CH2 实时采样率。 */
uint32_t DSO_Acq10_GetCh2SampleRate(void);

/* 读取 ADC kernel clock。 */
uint32_t DSO_Acq10_GetAdcKernelClockHz(void);

/* 读取 10-bit 采集运行时模型。 */
const DSO_AcqRuntimeInfo *DSO_Acq10_GetRuntimeInfo(void);

/* 填充 10-bit 后端调试快照。 */
bool DSO_Acq10_GetDebugSnapshot(DSO_AcqDebugSnapshot *out);

/* 读取触发定时器时钟。 */
uint32_t DSO_Acq10_GetTimClkHz(void);

/* 读取触发定时器 ARR。 */
uint32_t DSO_Acq10_GetTimArr(void);

/* 设置触发定时器 ARR。 */
void DSO_Acq10_SetTimArr(uint32_t new_arr);

/* 拷贝最近一段 CH1 样本，并应用校准偏移。 */
uint32_t DSO_Acq10_CopyRecentSamples(uint32_t dma_pos,
                                     int16_t cal_offset,
                                     uint16_t *out_samples,
                                     uint32_t max_samples);

/* 拷贝 CH1 新增样本，并应用校准偏移。 */
uint32_t DSO_Acq10_CopyNewSamples(uint32_t prev_dma_pos,
                                  uint32_t dma_pos,
                                  int16_t cal_offset,
                                  uint16_t *out_samples,
                                  uint32_t max_samples);

/* 拷贝最近一段 CH2 样本。 */
uint32_t DSO_Acq10_CopyRecentCh2Samples(uint32_t dma_pos,
                                        uint16_t *out_samples,
                                        uint32_t max_samples);

/* 拷贝 CH2 新增样本。 */
uint32_t DSO_Acq10_CopyNewCh2Samples(uint32_t prev_dma_pos,
                                     uint32_t dma_pos,
                                     uint16_t *out_samples,
                                     uint32_t max_samples);

/* 10-bit 后端 DMA 中断处理入口。 */
void DSO_Acq10_DMA_IRQHandler(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_DMA2_IRQHandler
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq10_DMA2_IRQHandler(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_CH2_DMA_IRQHandler
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq10_CH2_DMA_IRQHandler(void);

/* 10-bit 后端 ADC 半传输/全传输回调入口。 */
void DSO_Acq10_HandleAdcHalfTransfer(ADC_TypeDef *instance);
/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_HandleAdcTransferComplete
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq10_HandleAdcTransferComplete(ADC_TypeDef *instance);

/* 检查并累计 ADC overrun。 */
uint32_t DSO_Acq10_CheckOverrun(void);

/* 读取累计 overrun 数量。 */
uint32_t DSO_Acq10_GetOverrunCount(void);

/* 读取 lane 相位同步错误累计数量。 */
uint32_t DSO_Acq10_GetPhaseSyncErrorCount(void);

#endif /* DSO_ACQ_10BIT_H */
