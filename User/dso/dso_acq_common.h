/**
 * @file dso_acq_common.h
 * @brief 8-bit/10-bit 后端公共辅助声明。
 */

/*
 * dso_acq_common.h — Shared acquisition utilities used by both 8-bit and 10-bit backends.
 *
 * This module extracts identical static helper functions that were duplicated across
 * dso_acq_8bit.c and dso_acq_10bit.c into a single shared implementation.
 * Also includes timer_clock_from_pclk() previously duplicated in dso_testgen.c
 * and dso_wavegen_dac_pa4.c.
 *
 * All functions are prefixed with DSO_AcqCommon_ to avoid namespace collisions.
 */

#ifndef DSO_ACQ_COMMON_H
/* 宏定义: DSO_ACQ_COMMON_H - 头文件保护宏。 */
#define DSO_ACQ_COMMON_H

#include "dso_config.h"
#include "stm32h7xx_hal.h"

#include <stdint.h>
#include <stdbool.h>

/* ====== Ring buffer utilities ====== */

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_RingDelta
*   功能说明: 计算 DMA 环形缓冲中 prev_pos 到 pos 的正向距离，并处理回绕。
*   形    参: prev_pos - 上一次 DMA 位置。
*             pos      - 当前 DMA 位置。
*   返 回 值: 两个位置之间的新样本数量。
*********************************************************************************************************
*/
uint32_t DSO_AcqCommon_RingDelta(uint32_t prev_pos, uint32_t pos);

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_RingStart
*   功能说明: 计算以 end_pos 结尾、长度为 count 的环形窗口起点。
*   形    参: end_pos - 窗口结束位置。
*             count   - 需要回溯的样本数量。
*   返 回 值: 环形缓冲中的窗口起始位置。
*********************************************************************************************************
*/
uint32_t DSO_AcqCommon_RingStart(uint32_t end_pos, uint32_t count);

/* ====== Timer clock utilities ====== */

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_TimerClockFromPclk
*   功能说明: 根据 APB 时钟和分频计算 STM32H7 定时器输入时钟。
*   形    参: pclk_hz     - APB 总线时钟，单位 Hz。
*             apb_divider - APB 分频系数。
*   返 回 值: 定时器输入时钟，单位 Hz。
*********************************************************************************************************
*/
uint32_t DSO_AcqCommon_TimerClockFromPclk(uint32_t pclk_hz, uint32_t apb_divider);

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_CurrentTim1ClockHz
*   功能说明: 运行时读取 APB2 配置并计算 TIM1 输入时钟。
*   形    参: 无。
*   返 回 值: TIM1 输入时钟，单位 Hz。
*********************************************************************************************************
*/
uint32_t DSO_AcqCommon_CurrentTim1ClockHz(void);

/* ====== Cache management ====== */

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_InvalidateDCacheWindow
*   功能说明: 对指定内存窗口执行 D-Cache invalidate，供 DMA 写入后 CPU 读取前使用。
*   形    参: address    - 起始地址。
*             byte_count - 字节数。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcqCommon_InvalidateDCacheWindow(const void *address, uint32_t byte_count);

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_CleanInvalidateDCacheWindow
*   功能说明: 对指定内存窗口执行 D-Cache clean + invalidate。
*   形    参: address    - 起始地址。
*             byte_count - 字节数。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcqCommon_CleanInvalidateDCacheWindow(const void *address, uint32_t byte_count);

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_InvalidateDMAU16Window
*   功能说明: 对 uint16_t DMA 环形缓冲窗口执行 D-Cache invalidate，支持窗口回绕拆分。
*   形    参: samples - DMA 缓冲起始地址。
*             start   - 窗口起点。
*             count   - 窗口样本数。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcqCommon_InvalidateDMAU16Window(const uint16_t *samples,
                                           uint32_t start,
                                           uint32_t count);

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_InvalidateDMAU16Recent
*   功能说明: 对以 end_pos 结尾的最近 count 个 DMA 样本执行 D-Cache invalidate。
*   形    参: samples - DMA 缓冲起始地址。
*             end_pos - 窗口结束位置。
*             count   - 样本数量。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcqCommon_InvalidateDMAU16Recent(const uint16_t *samples,
                                           uint32_t end_pos,
                                           uint32_t count);

/* ====== DMA helpers ====== */

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_PrepareDMARxBuffer
*   功能说明: 对完整 DMA 接收缓冲执行 clean + invalidate，启动 DMA 前保证缓存状态干净。
*   形    参: samples - DMA 接收缓冲起始地址。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcqCommon_PrepareDMARxBuffer(const uint16_t *samples);

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_DmaPosOrZero
*   功能说明: 从 DMA NDTR 换算当前已传输位置，DMA 无效时返回 0。
*   形    参: hdma - DMA 句柄。
*   返 回 值: 当前 DMA 写入位置。
*********************************************************************************************************
*/
uint32_t DSO_AcqCommon_DmaPosOrZero(const DMA_HandleTypeDef *hdma);

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_DmaCounterOrZero
*   功能说明: 读取 DMA 剩余传输计数，DMA 无效时返回 0。
*   形    参: hdma - DMA 句柄。
*   返 回 值: DMA 剩余计数。
*********************************************************************************************************
*/
uint32_t DSO_AcqCommon_DmaCounterOrZero(const DMA_HandleTypeDef *hdma);

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_ResetDmaIRQ
*   功能说明: 禁止并清除指定 DMA 中断挂起状态。
*   形    参: irqn - DMA 中断号。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcqCommon_ResetDmaIRQ(IRQn_Type irqn);

/* ====== ADC configuration helpers ====== */

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_AdcSampleTimeHalValue
*   功能说明: 将 DSO_ADC_SAMPLE_CYCLES_X10 映射为 HAL ADC 采样时间枚举。
*   形    参: 无。
*   返 回 值: HAL ADC_SAMPLETIME_x 宏值。
*********************************************************************************************************
*/
uint32_t DSO_AcqCommon_AdcSampleTimeHalValue(void);

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_ConfigAdcChannel
*   功能说明: 使用当前采样时间配置指定 ADC 通道。
*   形    参: hadc    - ADC 句柄。
*             channel - ADC 通道号。
*   返 回 值: 无；HAL 配置失败时进入错误停机路径。
*********************************************************************************************************
*/
void DSO_AcqCommon_ConfigAdcChannel(ADC_HandleTypeDef *hadc, uint32_t channel);

/* ====== Timer PWM sync ====== */

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_StartTim1PwmChannelsSynchronized
*   功能说明: 同步启动 TIM1 PWM CH1~CH3，用于多 ADC lane 对齐触发。
*   形    参: htim - TIM1 句柄。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcqCommon_StartTim1PwmChannelsSynchronized(TIM_HandleTypeDef *htim);

#endif /* DSO_ACQ_COMMON_H */
