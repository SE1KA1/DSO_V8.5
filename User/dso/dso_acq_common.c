/**
 * @file dso_acq_common.c
 * @brief 采集公共逻辑：环形缓冲辅助、运行时信息。
 */

/*
 * dso_acq_common.c — Shared acquisition utilities used by both 8-bit and 10-bit backends.
 *
 * Extracted from identical static functions in dso_acq_8bit.c and dso_acq_10bit.c.
 * Also includes timer_clock_from_pclk() previously duplicated in dso_testgen.c
 * and dso_wavegen_dac_pa4.c.
 */

#include "dso_acq_common.h"
#include "dso_cache_range.h"

#include <string.h>

/* ====== Ring buffer utilities ====== */

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_RingDelta
*   功能说明: 计算 DMA 环形缓冲两个位置之间的正向样本增量。
*   形    参: prev_pos - 上一次记录的 DMA 位置。
*             pos      - 当前 DMA 位置。
*   返 回 值: 从 prev_pos 前进到 pos 所经过的样本数量。
*********************************************************************************************************
*/
uint32_t DSO_AcqCommon_RingDelta(uint32_t prev_pos, uint32_t pos)
{
    return (pos >= prev_pos)
         ? (pos - prev_pos)
         : ((DSO_DMA_BUF_SIZE - prev_pos) + pos);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_RingStart
*   功能说明: 在环形缓冲中计算“以 end_pos 结尾、长度为 count”的窗口起点。
*   形    参: end_pos - 窗口结束位置。
*             count   - 窗口长度。
*   返 回 值: 环形窗口起点。
*********************************************************************************************************
*/
uint32_t DSO_AcqCommon_RingStart(uint32_t end_pos, uint32_t count)
{
    return (end_pos >= count)
         ? (end_pos - count)
         : (DSO_DMA_BUF_SIZE - (count - end_pos));
}

/* ====== Timer clock utilities ====== */

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_TimerClockFromPclk
*   功能说明: 按 STM32H7 定时器时钟规则，由 APB 时钟和分频计算定时器输入时钟。
*   形    参: pclk_hz     - APB 时钟频率，单位 Hz。
*             apb_divider - APB 分频配置。
*   返 回 值: 定时器输入时钟，单位 Hz。
*********************************************************************************************************
*/
uint32_t DSO_AcqCommon_TimerClockFromPclk(uint32_t pclk_hz, uint32_t apb_divider)
{
    return (apb_divider == RCC_HCLK_DIV1) ? pclk_hz : (pclk_hz * 2u);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_CurrentTim1ClockHz
*   功能说明: 读取当前 RCC 配置并计算 TIM1 输入时钟。
*   形    参: 无。
*   返 回 值: TIM1 输入时钟，单位 Hz。
*********************************************************************************************************
*/
uint32_t DSO_AcqCommon_CurrentTim1ClockHz(void)
{
    RCC_ClkInitTypeDef clk_cfg = {0};
    uint32_t latency = 0u;

    HAL_RCC_GetClockConfig(&clk_cfg, &latency);
    return DSO_AcqCommon_TimerClockFromPclk(HAL_RCC_GetPCLK2Freq(),
                                              clk_cfg.APB2CLKDivider);
}

/* ====== Cache management ====== */

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_InvalidateDCacheWindow
*   功能说明: 对指定内存范围执行 D-Cache invalidate，通常用于 DMA 写入后 CPU 读取前。
*   形    参: address    - 需要维护的起始地址。
*             byte_count - 需要维护的长度，单位字节。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcqCommon_InvalidateDCacheWindow(const void *address, uint32_t byte_count)
{
    DSO_CacheRange range = {0};

    if (!DSO_CacheRange_Align(address, byte_count, &range)) {
        return;
    }

    SCB_InvalidateDCache_by_Addr((uint32_t *)range.start, (int32_t)range.length);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_CleanInvalidateDCacheWindow
*   功能说明: 对指定内存范围执行 D-Cache clean + invalidate，确保 DMA 前后内存与缓存一致。
*   形    参: address    - 需要维护的起始地址。
*             byte_count - 需要维护的长度，单位字节。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcqCommon_CleanInvalidateDCacheWindow(const void *address, uint32_t byte_count)
{
    DSO_CacheRange range = {0};

    if (!DSO_CacheRange_Align(address, byte_count, &range)) {
        return;
    }

    SCB_CleanInvalidateDCache_by_Addr((uint32_t *)range.start, (int32_t)range.length);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_InvalidateDMAU16Window
*   功能说明: 对 uint16_t DMA 环形缓冲中的窗口执行 cache invalidate；窗口回绕时拆成两段处理。
*   形    参: samples - DMA 缓冲起始地址。
*             start   - 环形窗口起点。
*             count   - 窗口样本数量。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcqCommon_InvalidateDMAU16Window(const uint16_t *samples,
                                           uint32_t start,
                                           uint32_t count)
{
    uint32_t first_count;

    /* Half/full DMA callbacks do not cover the active half that recent-window
     * reads consume, so invalidate the exact ring span before any CPU read. */
    if ((samples == NULL) || (count == 0u)) {
        return;
    }

    if (count > DSO_DMA_BUF_SIZE) {
        count = DSO_DMA_BUF_SIZE;
    }
    start %= DSO_DMA_BUF_SIZE;

    first_count = DSO_DMA_BUF_SIZE - start;
    if (first_count > count) {
        first_count = count;
    }

    DSO_AcqCommon_InvalidateDCacheWindow(&samples[start],
                                          first_count * (uint32_t)sizeof(samples[0]));
    if (count > first_count) {
        DSO_AcqCommon_InvalidateDCacheWindow(samples,
                                              (count - first_count) * (uint32_t)sizeof(samples[0]));
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_InvalidateDMAU16Recent
*   功能说明: 对以 end_pos 结尾的最近 count 个 uint16_t DMA 样本执行 cache invalidate。
*   形    参: samples - DMA 缓冲起始地址。
*             end_pos - 当前 DMA 结束位置。
*             count   - 需要覆盖的最近样本数量。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcqCommon_InvalidateDMAU16Recent(const uint16_t *samples,
                                           uint32_t end_pos,
                                           uint32_t count)
{
    uint32_t start;

    if (count > DSO_DMA_BUF_SIZE) {
        count = DSO_DMA_BUF_SIZE;
    }

    start = DSO_AcqCommon_RingStart(end_pos, count);
    DSO_AcqCommon_InvalidateDMAU16Window(samples, start, count);
}

/* ====== DMA helpers ====== */

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_PrepareDMARxBuffer
*   功能说明: 启动 DMA 接收前，对完整 DMA 缓冲执行 clean + invalidate。
*   形    参: samples - DMA 接收缓冲起始地址。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcqCommon_PrepareDMARxBuffer(const uint16_t *samples)
{
    DSO_AcqCommon_CleanInvalidateDCacheWindow(samples,
                                               DSO_DMA_BUF_SIZE * (uint32_t)sizeof(samples[0]));
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_DmaPosOrZero
*   功能说明: 读取 DMA 剩余计数并换算成当前写入位置；句柄无效时返回 0。
*   形    参: hdma - DMA 句柄。
*   返 回 值: DMA 当前写入位置。
*********************************************************************************************************
*/
uint32_t DSO_AcqCommon_DmaPosOrZero(const DMA_HandleTypeDef *hdma)
{
    if ((hdma == NULL) || (hdma->Instance == NULL)) {
        return 0u;
    }
    return DSO_DMA_BUF_SIZE - __HAL_DMA_GET_COUNTER(hdma);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_DmaCounterOrZero
*   功能说明: 读取 DMA 当前剩余计数；句柄无效时返回 0。
*   形    参: hdma - DMA 句柄。
*   返 回 值: DMA 剩余计数。
*********************************************************************************************************
*/
uint32_t DSO_AcqCommon_DmaCounterOrZero(const DMA_HandleTypeDef *hdma)
{
    if ((hdma == NULL) || (hdma->Instance == NULL)) {
        return 0u;
    }
    return __HAL_DMA_GET_COUNTER(hdma);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_ResetDmaIRQ
*   功能说明: 关闭指定 DMA 中断并清除挂起标志，常用于后端重新初始化前。
*   形    参: irqn - DMA 中断号。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcqCommon_ResetDmaIRQ(IRQn_Type irqn)
{
    HAL_NVIC_DisableIRQ(irqn);
    HAL_NVIC_ClearPendingIRQ(irqn);
}

/* ====== ADC configuration helpers ====== */

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_AdcSampleTimeHalValue
*   功能说明: 将项目配置的 ADC 采样周期转换为 HAL 可识别的采样时间宏。
*   形    参: 无。
*   返 回 值: ADC_SAMPLETIME_x 宏值；配置非法时停在 while(1) 便于暴露问题。
*********************************************************************************************************
*/
uint32_t DSO_AcqCommon_AdcSampleTimeHalValue(void)
{
    switch (DSO_ADC_SAMPLE_CYCLES_X10) {
    case 15u:   return ADC_SAMPLETIME_1CYCLE_5;
    case 25u:   return ADC_SAMPLETIME_2CYCLES_5;
    case 85u:   return ADC_SAMPLETIME_8CYCLES_5;
    case 165u:  return ADC_SAMPLETIME_16CYCLES_5;
    case 325u:  return ADC_SAMPLETIME_32CYCLES_5;
    case 645u:  return ADC_SAMPLETIME_64CYCLES_5;
    case 3875u: return ADC_SAMPLETIME_387CYCLES_5;
    case 8105u: return ADC_SAMPLETIME_810CYCLES_5;
    default:    while (1) {}
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_ConfigAdcChannel
*   功能说明: 按当前采样时间配置指定 ADC 常规通道。
*   形    参: hadc    - ADC 句柄。
*             channel - ADC 通道号。
*   返 回 值: 无；HAL 配置失败时停在 while(1)。
*********************************************************************************************************
*/
void DSO_AcqCommon_ConfigAdcChannel(ADC_HandleTypeDef *hadc, uint32_t channel)
{
    ADC_ChannelConfTypeDef ch_cfg = {0};

    ch_cfg.Channel = channel;
    ch_cfg.Rank = ADC_REGULAR_RANK_1;
    ch_cfg.SamplingTime = DSO_AcqCommon_AdcSampleTimeHalValue();
    ch_cfg.SingleDiff = ADC_SINGLE_ENDED;
    ch_cfg.OffsetNumber = ADC_OFFSET_NONE;
    ch_cfg.Offset = 0u;

    if (HAL_ADC_ConfigChannel(hadc, &ch_cfg) != HAL_OK) {
        while (1) {}
    }
}

/* ====== Timer PWM sync ====== */

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_StartTim1PwmChannelsSynchronized
*   功能说明: 以同步方式启动 TIM1 CH1~CH3 PWM 输出，作为多 ADC lane 的触发源。
*   形    参: htim - TIM1 句柄。
*   返 回 值: 无。
*********************************************************************************************************
*/
void DSO_AcqCommon_StartTim1PwmChannelsSynchronized(TIM_HandleTypeDef *htim)
{
    TIM_TypeDef *tim = htim->Instance;

    tim->CR1 &= ~TIM_CR1_CEN;
    tim->CNT = 0u;
    tim->EGR = TIM_EGR_UG;
    TIM_CCxChannelCmd(tim, TIM_CHANNEL_1, TIM_CCx_ENABLE);
    TIM_CCxChannelCmd(tim, TIM_CHANNEL_2, TIM_CCx_ENABLE);
    TIM_CCxChannelCmd(tim, TIM_CHANNEL_3, TIM_CCx_ENABLE);
    __HAL_TIM_MOE_ENABLE(htim);
    __HAL_TIM_ENABLE(htim);
}
