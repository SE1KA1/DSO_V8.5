/**
 * @file dso_acq_8bit.h
 * @brief 8-bit 高速采集后端 API：三路 ADC 交替采样。
 */

#ifndef DSO_ACQ_8BIT_H
/* 宏定义: DSO_ACQ_8BIT_H - 头文件保护宏。 */
#define DSO_ACQ_8BIT_H

#include "dso_acq_model.h"
#include "stm32h7xx.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct DSO_AcqDebugSnapshot DSO_AcqDebugSnapshot;

/* 8-bit 后端拓扑类型；当前产品路径固定为三路交替采样。 */
typedef uint8_t DSO_Acq8Topology;

/* 初始化 8-bit 高速采集后端，包括 ADC/TIM/DMA 资源和运行时模型。 */
void DSO_Acq8_Init(void);

/* 释放或关闭 8-bit 高速采集后端资源。 */
void DSO_Acq8_Deinit(void);

/* 周期服务 8-bit 后端，用于故障策略、刷新策略等后台维护。 */
void DSO_Acq8_Service(void);

/* 启动 8-bit 高速采集。 */
void DSO_Acq8_Start(void);

/* 停止 8-bit 高速采集。 */
void DSO_Acq8_Stop(void);

/* 查询 8-bit 后端当前是否处于运行状态。 */
bool DSO_Acq8_IsRunning(void);

/* 查询 8-bit 后端是否已有可供处理的数据。 */
bool DSO_Acq8_IsDataReady(void);

/* 读取 CH1 合并 DMA 位置。 */
uint32_t DSO_Acq8_GetPos(void);

/* 读取 CH1 原始 DMA 缓冲地址。 */
const volatile uint32_t *DSO_Acq8_GetBuf(void);

/* 判断 8-bit 模式下 CH2 是否可用；三路交替模式下通常不可用。 */
bool DSO_Acq8_IsCh2Available(void);

/* 读取 CH2 DMA 位置；CH2 不可用时由后端返回安全值。 */
uint32_t DSO_Acq8_GetCh2Pos(void);

/* 读取 CH2 DMA 缓冲地址；CH2 不可用时由后端返回安全值。 */
const volatile uint16_t *DSO_Acq8_GetCh2Buf(void);

/* 读取 8-bit 后端校准偏移。 */
int16_t DSO_Acq8_GetCalOffset(void);

/* 读取 CH1 三路交替后的等效采样率。 */
uint32_t DSO_Acq8_GetSampleRate(void);

/* 读取 CH2 采样率；CH2 不可用时返回 0。 */
uint32_t DSO_Acq8_GetCh2SampleRate(void);

/* 读取 ADC kernel clock。 */
uint32_t DSO_Acq8_GetAdcKernelClockHz(void);

/* 读取当前 8-bit 拓扑。 */
DSO_Acq8Topology DSO_Acq8_GetTopology(void);

/* 读取当前 8-bit lane 数。 */
uint8_t DSO_Acq8_GetLaneCount(void);

/* 请求 8-bit 拓扑；当前主线会规整到三路交替。 */
bool DSO_Acq8_RequestTopology(DSO_Acq8Topology topology);

/* 读取 8-bit 采集运行时模型。 */
const DSO_AcqRuntimeInfo *DSO_Acq8_GetRuntimeInfo(void);

/* 8-bit 后端 DMA 中断处理入口。 */
void DSO_Acq8_DMA_IRQHandler(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_DMA2_IRQHandler
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq8_DMA2_IRQHandler(void);
/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_CH2_DMA_IRQHandler
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq8_CH2_DMA_IRQHandler(void);

/* 8-bit 后端 ADC 半传输/全传输回调入口。 */
void DSO_Acq8_HandleAdcHalfTransfer(ADC_TypeDef *instance);
/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_HandleAdcTransferComplete
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq8_HandleAdcTransferComplete(ADC_TypeDef *instance);

/* 检查并累计 ADC overrun。 */
uint32_t DSO_Acq8_CheckOverrun(void);

/* 读取累计 overrun 数量。 */
uint32_t DSO_Acq8_GetOverrunCount(void);

/* 读取 lane 相位同步错误累计数量。 */
uint32_t DSO_Acq8_GetPhaseSyncErrorCount(void);

/* 填充 8-bit 后端调试快照。 */
bool DSO_Acq8_GetDebugSnapshot(DSO_AcqDebugSnapshot *out);

/* 读取触发定时器时钟。 */
uint32_t DSO_Acq8_GetTimClkHz(void);

/* 读取触发定时器 ARR。 */
uint32_t DSO_Acq8_GetTimArr(void);

/* 设置触发定时器 ARR。 */
void DSO_Acq8_SetTimArr(uint32_t new_arr);

/* 拷贝最近一段 CH1 样本。 */
uint32_t DSO_Acq8_CopyRecentSamples(uint32_t dma_pos,
                                    uint16_t *out_samples,
                                    uint32_t max_samples);

/* 拷贝 CH1 新增样本。 */
uint32_t DSO_Acq8_CopyNewSamples(uint32_t prev_dma_pos,
                                 uint32_t dma_pos,
                                 uint16_t *out_samples,
                                 uint32_t max_samples);

/* 拷贝最近一段 CH2 样本。 */
uint32_t DSO_Acq8_CopyRecentCh2Samples(uint32_t dma_pos,
                                       uint16_t *out_samples,
                                       uint32_t max_samples);

/* 拷贝 CH2 新增样本。 */
uint32_t DSO_Acq8_CopyNewCh2Samples(uint32_t prev_dma_pos,
                                    uint32_t dma_pos,
                                    uint16_t *out_samples,
                                    uint32_t max_samples);

#endif /* DSO_ACQ_8BIT_H */
