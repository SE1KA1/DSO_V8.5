/**
 * @file dso_wavegen_dac_pa4.c
 * @brief DAC PA4 HAL 驱动与启动/停止。
 */

#include "dso_wavegen_dac_pa4.h"

#include "dso_acq_common.h"
#include "dso_cache_range.h"
#include "dso_wavegen_timer_math.h"
#include "stm32h7xx_hal.h"

#include <stdbool.h>
#include <string.h>

#if !defined(DMA_REQUEST_DAC1) && defined(DMA_REQUEST_DAC1_CH1)
/* 宏定义: DMA_REQUEST_DAC1 - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DMA_REQUEST_DAC1 DMA_REQUEST_DAC1_CH1
#endif

__attribute__((section(".RAM_D2"), aligned(32)))
static uint16_t s_wavegen_lut[DSO_WAVEGEN_MAX_LUT_SAMPLES];

static DAC_HandleTypeDef s_dac;
static DMA_HandleTypeDef s_dma;
static bool s_dac_init_done;
static DSO_WavegenSnapshot s_snapshot;

/* timer_clock_from_pclk moved to dso_acq_common.c */

static uint32_t tim5_clock_hz(void)
{
    RCC_ClkInitTypeDef clk_cfg = {0};
    uint32_t latency = 0u;

    HAL_RCC_GetClockConfig(&clk_cfg, &latency);
    return DSO_AcqCommon_TimerClockFromPclk(HAL_RCC_GetPCLK1Freq(), clk_cfg.APB1CLKDivider);
}

/*
*********************************************************************************************************
*   函 数 名: clean_dcache_window
*   功能说明: 在波形发生器模块中完成 DAC/定时器配置、波形查表、参数换算或运行状态控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void clean_dcache_window(const void *address, uint32_t byte_count)
{
    DSO_CacheRange range = {0};

    if (!DSO_CacheRange_Align(address, byte_count, &range)) {
        return;
    }
    SCB_CleanDCache_by_Addr((uint32_t *)range.start, (int32_t)range.length);
}

/*
*********************************************************************************************************
*   函 数 名: stop_tim5
*   功能说明: 在波形发生器模块中完成 DAC/定时器配置、波形查表、参数换算或运行状态控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void stop_tim5(void)
{
    TIM5->CR1 &= ~TIM_CR1_CEN;
    TIM5->SR = 0u;
}

/*
*********************************************************************************************************
*   函 数 名: configure_tim5
*   功能说明: 在波形发生器模块中完成 DAC/定时器配置、波形查表、参数换算或运行状态控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static DSO_WavegenStatus configure_tim5(uint32_t update_hz, DSO_WavegenTimerPlan *plan)
{
    DSO_WavegenStatus status;

    status = DSO_WavegenTimer_CalcPlan(tim5_clock_hz(), update_hz, plan);
    if (status != DSO_WAVEGEN_OK) {
        return status;
    }

    __HAL_RCC_TIM5_CLK_ENABLE();
    TIM5->CR1 = 0u;
    TIM5->PSC = plan->psc;
    TIM5->ARR = plan->arr;
    TIM5->CR2 = TIM_CR2_MMS_1;
    TIM5->EGR = TIM_EGR_UG;
    TIM5->SR = 0u;
    return DSO_WAVEGEN_OK;
}

/*
*********************************************************************************************************
*   函 数 名: configure_dma
*   功能说明: 在波形发生器模块中完成 DAC/定时器配置、波形查表、参数换算或运行状态控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static DSO_WavegenStatus configure_dma(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    s_dma.Instance = DMA1_Stream5;
    (void)HAL_DMA_DeInit(&s_dma);

    s_dma.Init.Request = DMA_REQUEST_DAC1;
    s_dma.Init.Direction = DMA_MEMORY_TO_PERIPH;
    s_dma.Init.PeriphInc = DMA_PINC_DISABLE;
    s_dma.Init.MemInc = DMA_MINC_ENABLE;
    s_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    s_dma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    s_dma.Init.Mode = DMA_CIRCULAR;
    s_dma.Init.Priority = DMA_PRIORITY_VERY_HIGH;
    s_dma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    s_dma.Init.MemBurst = DMA_MBURST_SINGLE;
    s_dma.Init.PeriphBurst = DMA_PBURST_SINGLE;

    if (HAL_DMA_Init(&s_dma) != HAL_OK) {
        return DSO_WAVEGEN_HAL_ERROR;
    }
    return DSO_WAVEGEN_OK;
}

/*
*********************************************************************************************************
*   函 数 名: configure_dac
*   功能说明: 在波形发生器模块中完成 DAC/定时器配置、波形查表、参数换算或运行状态控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static DSO_WavegenStatus configure_dac(const uint16_t *lut, uint16_t lut_count)
{
    GPIO_InitTypeDef gpio = {0};
    DAC_ChannelConfTypeDef ch = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_DAC12_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_4;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    if (!s_dac_init_done) {
        s_dac.Instance = DAC1;
        if (HAL_DAC_Init(&s_dac) != HAL_OK) {
            return DSO_WAVEGEN_HAL_ERROR;
        }
        s_dac_init_done = true;
    }

    __HAL_LINKDMA(&s_dac, DMA_Handle1, s_dma);

    ch.DAC_Trigger = DAC_TRIGGER_T5_TRGO;
    ch.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
    if (HAL_DAC_ConfigChannel(&s_dac, &ch, DAC_CHANNEL_1) != HAL_OK) {
        return DSO_WAVEGEN_HAL_ERROR;
    }

    if (HAL_DAC_SetValue(&s_dac,
                         DAC_CHANNEL_1,
                         DAC_ALIGN_12B_R,
                         lut[lut_count - 1u]) != HAL_OK) {
        return DSO_WAVEGEN_HAL_ERROR;
    }

    __HAL_DAC_CLEAR_FLAG(&s_dac, DAC_FLAG_DMAUDR1);
    return DSO_WAVEGEN_OK;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_WavegenDacPa4_Start
*   功能说明: 处理 PA4/DAC 波形发生器的配置、启动停止、查表或定时器参数计算。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_WavegenStatus DSO_WavegenDacPa4_Start(const uint16_t *lut,
                                          uint16_t lut_count,
                                          uint32_t update_hz)
{
    DSO_WavegenTimerPlan plan = {0};
    DSO_WavegenStatus status;

    if ((lut == NULL) ||
        (lut_count < 2u) ||
        (lut_count > DSO_WAVEGEN_MAX_LUT_SAMPLES) ||
        (update_hz == 0u)) {
        return DSO_WAVEGEN_INVALID;
    }

    DSO_WavegenDacPa4_Stop();

    memcpy(s_wavegen_lut, lut, (uint32_t)lut_count * sizeof(s_wavegen_lut[0]));
    clean_dcache_window(s_wavegen_lut, (uint32_t)lut_count * sizeof(s_wavegen_lut[0]));

    status = configure_tim5(update_hz, &plan);
    if (status != DSO_WAVEGEN_OK) {
        return status;
    }

    status = configure_dma();
    if (status != DSO_WAVEGEN_OK) {
        return status;
    }

    status = configure_dac(s_wavegen_lut, lut_count);
    if (status != DSO_WAVEGEN_OK) {
        return status;
    }

    if (HAL_DAC_Start_DMA(&s_dac,
                          DAC_CHANNEL_1,
                          (uint32_t *)s_wavegen_lut,
                          lut_count,
                          DAC_ALIGN_12B_R) != HAL_OK) {
        return DSO_WAVEGEN_HAL_ERROR;
    }

    __HAL_DAC_DISABLE_IT(&s_dac, DAC_IT_DMAUDR1);
    __HAL_DMA_DISABLE_IT(&s_dma, DMA_IT_TC | DMA_IT_HT);

    s_snapshot.running = true;
    s_snapshot.dma_underrun = false;
    s_snapshot.requested_update_hz = update_hz;
    s_snapshot.actual_update_hz = plan.actual_update_hz;
    s_snapshot.lut_count = lut_count;
    s_snapshot.tim_psc = plan.psc;
    s_snapshot.tim_arr = plan.arr;

    TIM5->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
    return DSO_WAVEGEN_OK;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_WavegenDacPa4_Stop
*   功能说明: 处理 PA4/DAC 波形发生器的配置、启动停止、查表或定时器参数计算。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_WavegenDacPa4_Stop(void)
{
    stop_tim5();
    if (s_dac_init_done) {
        (void)HAL_DAC_Stop_DMA(&s_dac, DAC_CHANNEL_1);
    }
    if (s_dma.Instance != NULL) {
        (void)HAL_DMA_DeInit(&s_dma);
    }
    s_snapshot.running = false;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_WavegenDacPa4_Poll
*   功能说明: 处理 PA4/DAC 波形发生器的配置、启动停止、查表或定时器参数计算。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_WavegenStatus DSO_WavegenDacPa4_Poll(DSO_WavegenSnapshot *out)
{
    if (s_dac_init_done && __HAL_DAC_GET_FLAG(&s_dac, DAC_FLAG_DMAUDR1)) {
        stop_tim5();
        s_snapshot.running = false;
        s_snapshot.dma_underrun = true;
        __HAL_DAC_CLEAR_FLAG(&s_dac, DAC_FLAG_DMAUDR1);
        if (out != NULL) {
            *out = s_snapshot;
        }
        return DSO_WAVEGEN_DMA_UNDERRUN;
    }

    if (out != NULL) {
        *out = s_snapshot;
    }
    return DSO_WAVEGEN_OK;
}
