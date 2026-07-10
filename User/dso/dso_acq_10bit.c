/**
 * @file dso_acq_10bit.c
 * @brief 10-bit HAL 实现：CH1 双 lane 交织、CH2 实时 ADC3。
 */

#include "dso_acq_10bit.h"

#include "dso_acq.h"
#include "dso_acq_common.h"
#include "dso_acq10_tim_trigger.h"
#include "dso_cache_range.h"
#include "dso_lane_merge.h"
#include "dso_sample_spec.h"
#include "stm32h7xx_hal.h"
#include "y_t_stress_ab.h"
#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

typedef struct {
    ADC_HandleTypeDef hadc1;
    ADC_HandleTypeDef hadc2;
    ADC_HandleTypeDef hadc3;
    DMA_HandleTypeDef hdma_adc1;
    DMA_HandleTypeDef hdma_adc2;
    DMA_HandleTypeDef hdma_adc3;
    TIM_HandleTypeDef htim1;
    DSO_AcqRuntimeInfo runtime;
    int16_t cal_offset;
    uint32_t ch2_sample_rate_hz;
    uint32_t ovr_count;
    uint32_t phase_sync_error_count;
    uint32_t common_pos;
    uint32_t snapshot_pos0;
    uint32_t snapshot_pos1;
    uint32_t snapshot_common_pos;
    uint32_t snapshot_lane0_delta;
    uint32_t snapshot_lane1_delta;
    uint32_t snapshot_common_delta;
    bool snapshot_phase_sync_error;
    bool data_ready;
    uint8_t data_ready_lane_count;
    bool running;
} DSO_Acq10State;

static DSO_Acq10State s_acq10;
static const char s_custom_profile_name[] = "custom_arr";

static uint16_t s_dma_lane0[DSO_DMA_BUF_SIZE]
    __attribute__((section(".RAM_D2"), aligned(32)));
static uint16_t s_dma_lane1[DSO_DMA_BUF_SIZE]
    __attribute__((section(".RAM_D2"), aligned(32)));
static uint16_t s_dma_ch2[DSO_DMA_BUF_SIZE]
    __attribute__((section(".RAM_D2"), aligned(32)));

/*
*********************************************************************************************************
*   函 数 名: clamp_u16_from_i32
*   功能说明: 在 10-bit 采集后端中完成 ADC1/ADC2/ADC3、TIM1、DMA、CH2 实时采样或样本拷贝处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t clamp_u16_from_i32(int32_t v)
{
    return DSO_SampleSpec_ClampFromI32(DSO_SampleSpec_Get(DSO_SAMPLE_FORMAT_10BIT), v);
}

/* DSO_AcqCommon_RingDelta moved to dso_acq_common.c */

/* DSO_AcqCommon_RingStart moved to dso_acq_common.c */

/* timer_clock_from_pclk moved to dso_acq_common.c */

static uint32_t current_tim1_clock_hz(void)
{
    return DSO_AcqCommon_CurrentTim1ClockHz();
}

/*
*********************************************************************************************************
*   函 数 名: refresh_runtime_info
*   功能说明: 在 10-bit 采集后端中完成 ADC1/ADC2/ADC3、TIM1、DMA、CH2 实时采样或样本拷贝处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void refresh_runtime_info(uint32_t adc_input_clk_hz)
{
    s_acq10.runtime = DSO_AcqModel_Build(HAL_GetREVID(),
                                         DSO_DUAL_SWEEP_PROFILE,
                                         adc_input_clk_hz,
                                         current_tim1_clock_hz(),
                                         DSO_ADC_SAMPLE_CYCLES_X10,
                                         DSO_ADC_SAR_CYCLES_X10_10BIT,
                                         DSO_ADC_DELAY_CYCLES,
                                         false);
    s_acq10.ch2_sample_rate_hz = s_acq10.runtime.trigger_rate_hz;
}

/*
*********************************************************************************************************
*   函 数 名: refresh_runtime_rates_from_period
*   功能说明: 在 10-bit 采集后端中完成 ADC1/ADC2/ADC3、TIM1、DMA、CH2 实时采样或样本拷贝处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void refresh_runtime_rates_from_period(void)
{
    s_acq10.runtime.period_clocks = s_acq10.runtime.tim8_arr + 1u;
    s_acq10.runtime.trigger_rate_hz = (s_acq10.runtime.period_clocks != 0u)
                                    ? (s_acq10.runtime.tim8_clk_hz / s_acq10.runtime.period_clocks)
                                    : 0u;

    if ((s_acq10.runtime.required_period_clocks != 0u) &&
        (s_acq10.runtime.period_clocks >= s_acq10.runtime.required_period_clocks)) {
        s_acq10.runtime.lane_rate_hz = s_acq10.runtime.trigger_rate_hz;
        s_acq10.runtime.sample_rate_hz = 2u * s_acq10.runtime.trigger_rate_hz;
    } else {
        s_acq10.runtime.lane_rate_hz = s_acq10.runtime.tim8_clk_hz /
                                       (s_acq10.runtime.period_clocks * 2u);
        s_acq10.runtime.sample_rate_hz = s_acq10.runtime.trigger_rate_hz;
    }

    s_acq10.runtime.profile_valid = (s_acq10.runtime.required_period_clocks == 0u) ||
                                    (s_acq10.runtime.period_clocks >= s_acq10.runtime.required_period_clocks);
    s_acq10.ch2_sample_rate_hz = s_acq10.runtime.trigger_rate_hz;
}

/* adc_sample_time_hal_value moved to dso_acq_common.c */

/* DSO_AcqCommon_InvalidateDCacheWindow moved to dso_acq_common.c */

/* clean_DSO_AcqCommon_InvalidateDCacheWindow moved to dso_acq_common.c */

/* DSO_AcqCommon_InvalidateDMAU16Window moved to dso_acq_common.c */

/* DSO_AcqCommon_InvalidateDMAU16Recent moved to dso_acq_common.c */

static void prepare_dma_rx_buffer(const uint16_t *samples)
{
    DSO_AcqCommon_CleanInvalidateDCacheWindow(samples,
                                    DSO_DMA_BUF_SIZE * (uint32_t)sizeof(samples[0]));
}

/*
*********************************************************************************************************
*   函 数 名: prepare_all_dma_rx_buffers
*   功能说明: 在 10-bit 采集后端中完成 ADC1/ADC2/ADC3、TIM1、DMA、CH2 实时采样或样本拷贝处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void prepare_all_dma_rx_buffers(void)
{
    prepare_dma_rx_buffer(s_dma_lane0);
    prepare_dma_rx_buffer(s_dma_lane1);
    prepare_dma_rx_buffer(s_dma_ch2);
}

/* DSO_AcqCommon_DmaPosOrZero moved to dso_acq_common.c */
/* DSO_AcqCommon_DmaCounterOrZero moved to dso_acq_common.c */

static void config_pll2(uint32_t adc_input_clk_hz)
{
    RCC_PeriphCLKInitTypeDef periph_clk = {0};
    uint32_t actual_input_clk_hz;

    periph_clk.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    periph_clk.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;
    periph_clk.PLL2.PLL2M = 5;
    periph_clk.PLL2.PLL2Q = 10;
    periph_clk.PLL2.PLL2R = 10;
    periph_clk.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_2;
    periph_clk.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
    periph_clk.PLL2.PLL2FRACN = 0;

    switch (adc_input_clk_hz) {
    case 75000000u:
        periph_clk.PLL2.PLL2N = 150;
        periph_clk.PLL2.PLL2P = 10;
        break;
    case 150000000u:
        periph_clk.PLL2.PLL2N = 150;
        periph_clk.PLL2.PLL2P = 5;
        break;
    default:
        while (1) {}
    }

    if (HAL_RCCEx_PeriphCLKConfig(&periph_clk) != HAL_OK) {
        while (1) {}
    }

    actual_input_clk_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_ADC);
    if (actual_input_clk_hz == 0u) {
        actual_input_clk_hz = adc_input_clk_hz;
    }
    refresh_runtime_info(actual_input_clk_hz);
}

/*
*********************************************************************************************************
*   函 数 名: config_gpio
*   功能说明: 在 10-bit 采集后端中完成 ADC1/ADC2/ADC3、TIM1、DMA、CH2 实时采样或样本拷贝处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void config_gpio(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_6;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_2;
    HAL_GPIO_Init(GPIOC, &gpio);

    HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PC2, SYSCFG_SWITCH_PC2_CLOSE);
}

/*
*********************************************************************************************************
*   函 数 名: init_dma_handle
*   功能说明: 在 10-bit 采集后端中完成 ADC1/ADC2/ADC3、TIM1、DMA、CH2 实时采样或样本拷贝处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void init_dma_handle(DMA_HandleTypeDef *hdma,
                            DMA_Stream_TypeDef *instance,
                            uint32_t request,
                            IRQn_Type irqn)
{
    memset(hdma, 0, sizeof(*hdma));
    hdma->Instance = instance;
    hdma->Init.Request = request;
    hdma->Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma->Init.PeriphInc = DMA_PINC_DISABLE;
    hdma->Init.MemInc = DMA_MINC_ENABLE;
    hdma->Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma->Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma->Init.Mode = DMA_CIRCULAR;
    hdma->Init.Priority = DMA_PRIORITY_HIGH;
    hdma->Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    hdma->Init.MemBurst = DMA_MBURST_SINGLE;
    hdma->Init.PeriphBurst = DMA_PBURST_SINGLE;
    if (HAL_DMA_Init(hdma) != HAL_OK) {
        while (1) {}
    }

    HAL_NVIC_SetPriority(irqn, 5, 0);
    HAL_NVIC_EnableIRQ(irqn);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_AcqCommon_ResetDmaIRQ
*   功能说明: 提供采集后端共用的环形缓冲、定时器时钟、DMA 或 Cache 辅助处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void DSO_AcqCommon_ResetDmaIRQ(IRQn_Type irqn)
{
    HAL_NVIC_DisableIRQ(irqn);
    HAL_NVIC_ClearPendingIRQ(irqn);
}

/*
*********************************************************************************************************
*   函 数 名: config_dma
*   功能说明: 在 10-bit 采集后端中完成 ADC1/ADC2/ADC3、TIM1、DMA、CH2 实时采样或样本拷贝处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void config_dma(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();

    init_dma_handle(&s_acq10.hdma_adc1,
                    DMA1_Stream1,
                    DMA_REQUEST_ADC1,
                    DMA1_Stream1_IRQn);
    __HAL_LINKDMA(&s_acq10.hadc1, DMA_Handle, s_acq10.hdma_adc1);

    init_dma_handle(&s_acq10.hdma_adc2,
                    DMA1_Stream2,
                    DMA_REQUEST_ADC2,
                    DMA1_Stream2_IRQn);
    __HAL_LINKDMA(&s_acq10.hadc2, DMA_Handle, s_acq10.hdma_adc2);

    init_dma_handle(&s_acq10.hdma_adc3,
                    DMA2_Stream1,
                    DMA_REQUEST_ADC3,
                    DMA2_Stream1_IRQn);
    __HAL_LINKDMA(&s_acq10.hadc3, DMA_Handle, s_acq10.hdma_adc3);
}

/*
*********************************************************************************************************
*   函 数 名: fill_adc_init
*   功能说明: 在 10-bit 采集后端中完成 ADC1/ADC2/ADC3、TIM1、DMA、CH2 实时采样或样本拷贝处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fill_adc_init(ADC_HandleTypeDef *hadc,
                          ADC_TypeDef *instance,
                          uint32_t trigger)
{
    hadc->Instance = instance;
    hadc->Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
    hadc->Init.Resolution = ADC_RESOLUTION_10B;
    hadc->Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc->Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc->Init.LowPowerAutoWait = DISABLE;
    hadc->Init.ContinuousConvMode = DISABLE;
    hadc->Init.NbrOfConversion = 1;
    hadc->Init.DiscontinuousConvMode = DISABLE;
    hadc->Init.ExternalTrigConv = trigger;
    hadc->Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc->Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR;
    hadc->Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    hadc->Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
    hadc->Init.BoostMode = ENABLE;
    hadc->Init.OversamplingMode = DISABLE;
}

/* config_adc_channel moved to dso_acq_common.c — use DSO_AcqCommon_ConfigAdcChannel() directly */

static uint32_t tim1_cc_pulse(uint32_t period_clocks, uint32_t which)
{
    switch (which) {
    case 0u:
        return period_clocks / 4u;
    case 1u:
        return (period_clocks * 3u) / 4u;
    default:
        return period_clocks / 2u;
    }
}

/*
*********************************************************************************************************
*   函 数 名: config_tim1_channel
*   功能说明: 在 10-bit 采集后端中完成 ADC1/ADC2/ADC3、TIM1、DMA、CH2 实时采样或样本拷贝处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void config_tim1_channel(uint32_t channel, uint32_t pulse)
{
    TIM_OC_InitTypeDef oc = {0};

    oc.OCMode = (DSO_Acq10_GetTimTriggerMode() == DSO_ACQ10_TIM_TRIGGER_MODE_PWM1)
              ? TIM_OCMODE_PWM1
              : TIM_OCMODE_TIMING;
    oc.Pulse = pulse;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    oc.OCIdleState = TIM_OCIDLESTATE_RESET;
    oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;

    if (DSO_Acq10_GetTimTriggerMode() == DSO_ACQ10_TIM_TRIGGER_MODE_PWM1) {
        if (HAL_TIM_PWM_ConfigChannel(&s_acq10.htim1, &oc, channel) != HAL_OK) {
            while (1) {}
        }
    } else {
        if (HAL_TIM_OC_ConfigChannel(&s_acq10.htim1, &oc, channel) != HAL_OK) {
            while (1) {}
        }
    }
}

/*
*********************************************************************************************************
*   函 数 名: config_tim1_phase
*   功能说明: 在 10-bit 采集后端中完成 ADC1/ADC2/ADC3、TIM1、DMA、CH2 实时采样或样本拷贝处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void config_tim1_phase(void)
{
    __HAL_RCC_TIM1_CLK_ENABLE();

    memset(&s_acq10.htim1, 0, sizeof(s_acq10.htim1));
    s_acq10.htim1.Instance = TIM1;
    s_acq10.htim1.Init.Prescaler = 0u;
    s_acq10.htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    s_acq10.htim1.Init.Period = s_acq10.runtime.tim8_arr;
    s_acq10.htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    s_acq10.htim1.Init.RepetitionCounter = 0u;
    s_acq10.htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (DSO_Acq10_GetTimTriggerMode() == DSO_ACQ10_TIM_TRIGGER_MODE_PWM1) {
        if (HAL_TIM_PWM_Init(&s_acq10.htim1) != HAL_OK) {
            while (1) {}
        }
    } else {
        if (HAL_TIM_Base_Init(&s_acq10.htim1) != HAL_OK) {
            while (1) {}
        }
        if (HAL_TIM_OC_Init(&s_acq10.htim1) != HAL_OK) {
            while (1) {}
        }
    }

    config_tim1_channel(TIM_CHANNEL_1, tim1_cc_pulse(s_acq10.runtime.period_clocks, 0u));
    config_tim1_channel(TIM_CHANNEL_2, tim1_cc_pulse(s_acq10.runtime.period_clocks, 1u));
    config_tim1_channel(TIM_CHANNEL_3, tim1_cc_pulse(s_acq10.runtime.period_clocks, 2u));
}

/*
*********************************************************************************************************
*   函 数 名: config_adc
*   功能说明: 在 10-bit 采集后端中完成 ADC1/ADC2/ADC3、TIM1、DMA、CH2 实时采样或样本拷贝处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void config_adc(void)
{
    ADC_MultiModeTypeDef multi = {0};

    __HAL_RCC_ADC12_CLK_ENABLE();
    __HAL_RCC_ADC3_CLK_ENABLE();

    fill_adc_init(&s_acq10.hadc1, ADC1, ADC_EXTERNALTRIG_T1_CC1);
    fill_adc_init(&s_acq10.hadc2, ADC2, ADC_EXTERNALTRIG_T1_CC2);
    fill_adc_init(&s_acq10.hadc3, ADC3, ADC_EXTERNALTRIG_T1_CC3);

    if (HAL_ADC_Init(&s_acq10.hadc1) != HAL_OK) {
        while (1) {}
    }
    if (HAL_ADC_Init(&s_acq10.hadc2) != HAL_OK) {
        while (1) {}
    }
    if (HAL_ADC_Init(&s_acq10.hadc3) != HAL_OK) {
        while (1) {}
    }

    DSO_AcqCommon_ConfigAdcChannel(&s_acq10.hadc1, ADC_CHANNEL_3);
    DSO_AcqCommon_ConfigAdcChannel(&s_acq10.hadc2, ADC_CHANNEL_3);
    DSO_AcqCommon_ConfigAdcChannel(&s_acq10.hadc3, ADC_CHANNEL_0);

    multi.Mode = ADC_MODE_INDEPENDENT;
    if (HAL_ADCEx_MultiModeConfigChannel(&s_acq10.hadc1, &multi) != HAL_OK) {
        while (1) {}
    }

    if (HAL_ADCEx_Calibration_Start(&s_acq10.hadc3, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK) {
        while (1) {}
    }
    if (HAL_ADCEx_Calibration_Start(&s_acq10.hadc2, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK) {
        while (1) {}
    }
    if (HAL_ADCEx_Calibration_Start(&s_acq10.hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK) {
        while (1) {}
    }
}

/* start_tim1_pwm_channels_synchronized moved to dso_acq_common.c */
/* Use DSO_AcqCommon_StartTim1PwmChannelsSynchronized(&s_acq10.htim1) directly */

static void start_phase_timer(void)
{
    if (DSO_Acq10_GetTimTriggerMode() == DSO_ACQ10_TIM_TRIGGER_MODE_PWM1) {
        DSO_AcqCommon_StartTim1PwmChannelsSynchronized(&s_acq10.htim1);
    } else {
        TIM1->CNT = 0u;
        TIM1->EGR = TIM_EGR_UG;

        if (HAL_TIM_OC_Start(&s_acq10.htim1, TIM_CHANNEL_1) != HAL_OK) {
            while (1) {}
        }
        if (HAL_TIM_OC_Start(&s_acq10.htim1, TIM_CHANNEL_2) != HAL_OK) {
            while (1) {}
        }
        if (HAL_TIM_OC_Start(&s_acq10.htim1, TIM_CHANNEL_3) != HAL_OK) {
            while (1) {}
        }
        if (HAL_TIM_Base_Start(&s_acq10.htim1) != HAL_OK) {
            while (1) {}
        }
    }
}

/*
*********************************************************************************************************
*   函 数 名: stop_phase_timer
*   功能说明: 在 10-bit 采集后端中完成 ADC1/ADC2/ADC3、TIM1、DMA、CH2 实时采样或样本拷贝处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void stop_phase_timer(void)
{
    if (DSO_Acq10_GetTimTriggerMode() == DSO_ACQ10_TIM_TRIGGER_MODE_PWM1) {
        (void)HAL_TIM_PWM_Stop(&s_acq10.htim1, TIM_CHANNEL_3);
        (void)HAL_TIM_PWM_Stop(&s_acq10.htim1, TIM_CHANNEL_2);
        (void)HAL_TIM_PWM_Stop(&s_acq10.htim1, TIM_CHANNEL_1);
    } else {
        (void)HAL_TIM_OC_Stop(&s_acq10.htim1, TIM_CHANNEL_3);
        (void)HAL_TIM_OC_Stop(&s_acq10.htim1, TIM_CHANNEL_2);
        (void)HAL_TIM_OC_Stop(&s_acq10.htim1, TIM_CHANNEL_1);
        (void)HAL_TIM_Base_Stop(&s_acq10.htim1);
    }
}

/*
*********************************************************************************************************
*   函 数 名: acq10_mark_data_not_ready
*   功能说明: 在 10-bit 采集后端中完成 ADC1/ADC2/ADC3、TIM1、DMA、CH2 实时采样或样本拷贝处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void acq10_mark_data_not_ready(void)
{
    s_acq10.data_ready = false;
    s_acq10.data_ready_lane_count = 2u;
}

/*
*********************************************************************************************************
*   函 数 名: reset_tracking
*   功能说明: 在 10-bit 采集后端中完成 ADC1/ADC2/ADC3、TIM1、DMA、CH2 实时采样或样本拷贝处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void reset_tracking(void)
{
    s_acq10.common_pos = 0u;
    s_acq10.snapshot_pos0 = 0u;
    s_acq10.snapshot_pos1 = 0u;
    s_acq10.snapshot_common_pos = 0u;
    s_acq10.snapshot_lane0_delta = 0u;
    s_acq10.snapshot_lane1_delta = 0u;
    s_acq10.snapshot_common_delta = 0u;
    s_acq10.snapshot_phase_sync_error = false;
    acq10_mark_data_not_ready();
}

/*
*********************************************************************************************************
*   函 数 名: acq10_all_ch1_lanes_have_dma_progress
*   功能说明: 在 10-bit 采集后端中完成 ADC1/ADC2/ADC3、TIM1、DMA、CH2 实时采样或样本拷贝处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool acq10_all_ch1_lanes_have_dma_progress(void)
{
    if (!s_acq10.running || (s_acq10.snapshot_common_delta == 0u)) {
        return false;
    }
    if (s_acq10.snapshot_common_pos == 0u) {
        return false;
    }
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: acq10_update_data_ready_from_snapshot
*   功能说明: 在 10-bit 采集后端中完成 ADC1/ADC2/ADC3、TIM1、DMA、CH2 实时采样或样本拷贝处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void acq10_update_data_ready_from_snapshot(void)
{
    if (acq10_all_ch1_lanes_have_dma_progress()) {
        s_acq10.data_ready = true;
    }
    s_acq10.data_ready_lane_count = 2u;
}

/*
*********************************************************************************************************
*   函 数 名: compute_initial_lane_offset
*   功能说明: 在 10-bit 采集后端中完成 ADC1/ADC2/ADC3、TIM1、DMA、CH2 实时采样或样本拷贝处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void compute_initial_lane_offset(void)
{
    uint32_t count = (DSO_DMA_BUF_SIZE > 2048u) ? 2048u : DSO_DMA_BUF_SIZE;
    int32_t sum0 = 0;
    int32_t sum1 = 0;

    if (count == 0u) {
        s_acq10.cal_offset = 0;
        return;
    }

    DSO_AcqCommon_InvalidateDMAU16Window(s_dma_lane0, 0u, count);
    DSO_AcqCommon_InvalidateDMAU16Window(s_dma_lane1, 0u, count);

    for (uint32_t i = 0u; i < count; ++i) {
        sum0 += (int32_t)s_dma_lane0[i];
        sum1 += (int32_t)s_dma_lane1[i];
    }

    s_acq10.cal_offset = (int16_t)((sum1 / (int32_t)count) - (sum0 / (int32_t)count));
}

/*
*********************************************************************************************************
*   函 数 名: snapshot_common_position
*   功能说明: 在 10-bit 采集后端中完成 ADC1/ADC2/ADC3、TIM1、DMA、CH2 实时采样或样本拷贝处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void snapshot_common_position(void)
{
    DSO_LaneDeltaInfo delta;
    uint32_t pos0 = DSO_AcqCommon_DmaPosOrZero(&s_acq10.hdma_adc1);
    uint32_t pos1 = DSO_AcqCommon_DmaPosOrZero(&s_acq10.hdma_adc2);
    uint32_t common;

    delta = DSO_LaneMerge_CommonDelta2(s_acq10.common_pos,
                                       pos0,
                                       pos1,
                                       DSO_DMA_BUF_SIZE);
    common = (s_acq10.common_pos + delta.common_delta) % DSO_DMA_BUF_SIZE;

    s_acq10.common_pos = common;
    s_acq10.snapshot_pos0 = pos0;
    s_acq10.snapshot_pos1 = pos1;
    s_acq10.snapshot_common_pos = s_acq10.common_pos;
    s_acq10.snapshot_lane0_delta = DSO_AcqCommon_RingDelta(s_acq10.snapshot_common_pos, pos0);
    s_acq10.snapshot_lane1_delta = DSO_AcqCommon_RingDelta(s_acq10.snapshot_common_pos, pos1);
    s_acq10.snapshot_common_delta = delta.common_delta;
    s_acq10.snapshot_phase_sync_error = delta.phase_sync_error;
    if (s_acq10.snapshot_phase_sync_error) {
        s_acq10.phase_sync_error_count++;
    }
    acq10_update_data_ready_from_snapshot();
}

/*
*********************************************************************************************************
*   函 数 名: summarize_ring_window_u16
*   功能说明: 在 10-bit 采集后端中完成 ADC1/ADC2/ADC3、TIM1、DMA、CH2 实时采样或样本拷贝处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void summarize_ring_window_u16(const uint16_t *samples,
                                      uint32_t ring_size,
                                      uint32_t end_pos,
                                      uint32_t window_count,
                                      uint16_t *out_vmin,
                                      uint16_t *out_vmax,
                                      uint16_t *out_mean)
{
    uint32_t count = (window_count > ring_size) ? ring_size : window_count;
    uint32_t start;
    uint32_t sum = 0u;
    uint16_t vmin = 0u;
    uint16_t vmax = 0u;

    if ((samples == NULL) || (count == 0u)) {
        if (out_vmin != NULL) {
            *out_vmin = 0u;
        }
        if (out_vmax != NULL) {
            *out_vmax = 0u;
        }
        if (out_mean != NULL) {
            *out_mean = 0u;
        }
        return;
    }

    start = (end_pos >= count) ? (end_pos - count) : (ring_size - (count - end_pos));
    vmin = samples[start % ring_size];
    vmax = vmin;

    for (uint32_t i = 0u; i < count; ++i) {
        uint16_t v = samples[(start + i) % ring_size];
        if (v < vmin) {
            vmin = v;
        }
        if (v > vmax) {
            vmax = v;
        }
        sum += v;
    }

    if (out_vmin != NULL) {
        *out_vmin = vmin;
    }
    if (out_vmax != NULL) {
        *out_vmax = vmax;
    }
    if (out_mean != NULL) {
        *out_mean = (uint16_t)(sum / count);
    }
}

/*
*********************************************************************************************************
*   函 数 名: copy_ring_window_u16
*   功能说明: 在 10-bit 采集后端中完成 ADC1/ADC2/ADC3、TIM1、DMA、CH2 实时采样或样本拷贝处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t copy_ring_window_u16(const uint16_t *samples,
                                     uint32_t ring_size,
                                     uint32_t end_pos,
                                     uint16_t *out_samples,
                                     uint32_t max_samples,
                                     uint32_t *start_out)
{
    uint32_t count = (max_samples > ring_size) ? ring_size : max_samples;
    uint32_t start;

    if ((samples == NULL) || (out_samples == NULL) || (count == 0u)) {
        if (start_out != NULL) {
            *start_out = 0u;
        }
        return 0u;
    }

    start = (end_pos >= count) ? (end_pos - count) : (ring_size - (count - end_pos));
    for (uint32_t i = 0u; i < count; ++i) {
        out_samples[i] = samples[(start + i) % ring_size];
    }

    if (start_out != NULL) {
        *start_out = start;
    }
    return count;
}

/*
*********************************************************************************************************
*   函 数 名: apply_lane1_offset
*   功能说明: 在 10-bit 采集后端中完成 ADC1/ADC2/ADC3、TIM1、DMA、CH2 实时采样或样本拷贝处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void apply_lane1_offset(uint16_t *samples, uint32_t count, int16_t cal_offset)
{
    if (samples == NULL) {
        return;
    }

    for (uint32_t i = 1u; i < count; i += 2u) {
        samples[i] = clamp_u16_from_i32((int32_t)samples[i] - (int32_t)cal_offset);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_Init
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq10_Init(void)
{
    uint32_t target_input_clk_hz;

    memset(&s_acq10, 0, sizeof(s_acq10));
    memset(s_dma_lane0, 0, sizeof(s_dma_lane0));
    memset(s_dma_lane1, 0, sizeof(s_dma_lane1));
    memset(s_dma_ch2, 0, sizeof(s_dma_ch2));
    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_10BIT);

    target_input_clk_hz = DSO_AcqModel_TargetInputClockHz(
        DSO_AcqModel_RevisionFromRevid(HAL_GetREVID()));
    refresh_runtime_info(target_input_clk_hz);

    config_pll2(s_acq10.runtime.adc_input_clk_hz);
    config_gpio();
    config_dma();
    config_adc();
    config_tim1_phase();
    reset_tracking();
    DSO_Acq10_Start();
    vTaskDelay(pdMS_TO_TICKS(2));
    compute_initial_lane_offset();
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_Deinit
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq10_Deinit(void)
{
    DSO_Acq10_Stop();

    if (s_acq10.htim1.Instance != NULL) {
        if (DSO_Acq10_GetTimTriggerMode() == DSO_ACQ10_TIM_TRIGGER_MODE_PWM1) {
            (void)HAL_TIM_PWM_DeInit(&s_acq10.htim1);
        } else {
            (void)HAL_TIM_OC_DeInit(&s_acq10.htim1);
            (void)HAL_TIM_Base_DeInit(&s_acq10.htim1);
        }
    }
    if (s_acq10.hadc1.Instance != NULL) {
        (void)HAL_ADC_DeInit(&s_acq10.hadc1);
    }
    if (s_acq10.hadc2.Instance != NULL) {
        (void)HAL_ADC_DeInit(&s_acq10.hadc2);
    }
    if (s_acq10.hadc3.Instance != NULL) {
        (void)HAL_ADC_DeInit(&s_acq10.hadc3);
    }
    if (s_acq10.hdma_adc1.Instance != NULL) {
        (void)HAL_DMA_DeInit(&s_acq10.hdma_adc1);
    }
    if (s_acq10.hdma_adc2.Instance != NULL) {
        (void)HAL_DMA_DeInit(&s_acq10.hdma_adc2);
    }
    if (s_acq10.hdma_adc3.Instance != NULL) {
        (void)HAL_DMA_DeInit(&s_acq10.hdma_adc3);
    }

    DSO_AcqCommon_ResetDmaIRQ(DMA1_Stream1_IRQn);
    DSO_AcqCommon_ResetDmaIRQ(DMA1_Stream2_IRQn);
    DSO_AcqCommon_ResetDmaIRQ(DMA2_Stream1_IRQn);

    memset(&s_acq10, 0, sizeof(s_acq10));
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_Service
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq10_Service(void)
{
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_Start
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq10_Start(void)
{
    if (s_acq10.running) {
        return;
    }

    prepare_all_dma_rx_buffers();

    if (HAL_ADC_Start_DMA(&s_acq10.hadc1, (uint32_t *)s_dma_lane0, DSO_DMA_BUF_SIZE) != HAL_OK) {
        while (1) {}
    }
    if (HAL_ADC_Start_DMA(&s_acq10.hadc2, (uint32_t *)s_dma_lane1, DSO_DMA_BUF_SIZE) != HAL_OK) {
        while (1) {}
    }
    if (HAL_ADC_Start_DMA(&s_acq10.hadc3, (uint32_t *)s_dma_ch2, DSO_DMA_BUF_SIZE) != HAL_OK) {
        while (1) {}
    }

    start_phase_timer();
#if (Y_T_AB_DISABLE_ACQ_DMA_IRQ_AFTER_START != 0)
    HAL_NVIC_DisableIRQ(DMA1_Stream1_IRQn);
    HAL_NVIC_DisableIRQ(DMA1_Stream2_IRQn);
    HAL_NVIC_DisableIRQ(DMA2_Stream1_IRQn);
#endif
#if (Y_T_AB_DISABLE_ACQ_DMA_HT_TC_IRQ_SOURCE != 0)
    __HAL_DMA_DISABLE_IT(&s_acq10.hdma_adc1, DMA_IT_HT | DMA_IT_TC);
    __HAL_DMA_DISABLE_IT(&s_acq10.hdma_adc2, DMA_IT_HT | DMA_IT_TC);
    __HAL_DMA_DISABLE_IT(&s_acq10.hdma_adc3, DMA_IT_HT | DMA_IT_TC);
#endif
    reset_tracking();
    s_acq10.running = true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_Stop
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq10_Stop(void)
{
    if (!s_acq10.running) {
        return;
    }

    stop_phase_timer();
    (void)HAL_ADC_Stop_DMA(&s_acq10.hadc3);
    (void)HAL_ADC_Stop_DMA(&s_acq10.hadc2);
    (void)HAL_ADC_Stop_DMA(&s_acq10.hadc1);
    s_acq10.running = false;
    acq10_mark_data_not_ready();
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_IsRunning
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_Acq10_IsRunning(void)
{
    return s_acq10.running;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_GetPos
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq10_GetPos(void)
{
    if (s_acq10.running) {
        snapshot_common_position();
    }
    return s_acq10.snapshot_common_pos;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_GetBuf
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const volatile uint32_t *DSO_Acq10_GetBuf(void)
{
    return (const volatile uint32_t *)s_dma_lane0;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_GetCh2Pos
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq10_GetCh2Pos(void)
{
    return DSO_AcqCommon_DmaPosOrZero(&s_acq10.hdma_adc3);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_GetCh2Buf
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const volatile uint16_t *DSO_Acq10_GetCh2Buf(void)
{
    return s_dma_ch2;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_GetCalOffset
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
int16_t DSO_Acq10_GetCalOffset(void)
{
    return s_acq10.cal_offset;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_GetSampleRate
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq10_GetSampleRate(void)
{
    return s_acq10.runtime.sample_rate_hz;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_GetCh2SampleRate
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq10_GetCh2SampleRate(void)
{
    return s_acq10.ch2_sample_rate_hz;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_GetAdcKernelClockHz
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq10_GetAdcKernelClockHz(void)
{
    return s_acq10.runtime.adc_core_clk_hz;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_GetRuntimeInfo
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const DSO_AcqRuntimeInfo *DSO_Acq10_GetRuntimeInfo(void)
{
    return &s_acq10.runtime;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_GetDebugSnapshot
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_Acq10_GetDebugSnapshot(DSO_AcqDebugSnapshot *out)
{
    const uint32_t window = 64u;

    if (out == NULL) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    if (s_acq10.running) {
        snapshot_common_position();
    }

    out->format = DSO_SAMPLE_FORMAT_10BIT;
    out->running = s_acq10.running;
    out->hs8_lane_count = 2u;
    out->sample_rate_hz = s_acq10.runtime.sample_rate_hz;
    out->ch2_sample_rate_hz = s_acq10.ch2_sample_rate_hz;
    out->trigger_rate_hz = s_acq10.runtime.trigger_rate_hz;
    out->accepted_trigger_div = 1u;
    out->adc_kernel_clock_hz = s_acq10.runtime.adc_core_clk_hz;
    out->dma_pos = s_acq10.snapshot_common_pos;
    out->ch2_dma_pos = DSO_Acq10_GetCh2Pos();
    out->lane0_pos = s_acq10.snapshot_pos0;
    out->lane1_pos = s_acq10.snapshot_pos1;
    out->lane0_delta = s_acq10.snapshot_lane0_delta;
    out->lane1_delta = s_acq10.snapshot_lane1_delta;
    out->common_delta = s_acq10.snapshot_common_delta;
    out->phase_sync_error = s_acq10.snapshot_phase_sync_error;
    out->data_ready = s_acq10.data_ready;
    out->data_ready_lane_count = s_acq10.data_ready_lane_count;
    out->phase_sync_errors = s_acq10.phase_sync_error_count;
    out->overrun_count = s_acq10.ovr_count;
    out->tim_enabled = (TIM1->CR1 & TIM_CR1_CEN) != 0u;
    out->tim_counter = TIM1->CNT;
    out->tim_arr = TIM1->ARR;
    out->tim_ccr1 = TIM1->CCR1;
    out->tim_ccr2 = TIM1->CCR2;
    out->tim_ccr3 = TIM1->CCR3;
    out->tim_cr1 = TIM1->CR1;
    out->tim_ccmr1 = TIM1->CCMR1;
    out->tim_ccmr2 = TIM1->CCMR2;
    out->tim_ccer = TIM1->CCER;
    out->tim_bdtr = TIM1->BDTR;
    out->tim_sr = TIM1->SR;
    out->dma_ndtr0 = DSO_AcqCommon_DmaCounterOrZero(&s_acq10.hdma_adc1);
    out->dma_ndtr1 = DSO_AcqCommon_DmaCounterOrZero(&s_acq10.hdma_adc2);
    out->dma_ndtr2 = DSO_AcqCommon_DmaCounterOrZero(&s_acq10.hdma_adc3);
    out->adc1_cr = ADC1->CR;
    out->adc2_cr = ADC2->CR;
    out->adc3_cr = ADC3->CR;
    out->adc1_cfgr = ADC1->CFGR;
    out->adc2_cfgr = ADC2->CFGR;
    out->adc3_cfgr = ADC3->CFGR;
    out->adc1_isr = ADC1->ISR;
    out->adc2_isr = ADC2->ISR;
    out->adc3_isr = ADC3->ISR;

    summarize_ring_window_u16(s_dma_lane0,
                              DSO_DMA_BUF_SIZE,
                              s_acq10.snapshot_common_pos,
                              window,
                              &out->lane0_vmin,
                              &out->lane0_vmax,
                              &out->lane0_mean);
    summarize_ring_window_u16(s_dma_lane1,
                              DSO_DMA_BUF_SIZE,
                              s_acq10.snapshot_common_pos,
                              window,
                              &out->lane1_vmin,
                              &out->lane1_vmax,
                              &out->lane1_mean);
    out->lane_window_count = copy_ring_window_u16(s_dma_lane0,
                                                  DSO_DMA_BUF_SIZE,
                                                  s_acq10.snapshot_common_pos,
                                                  out->lane0_window,
                                                  DSO_SERIAL_DEBUG_HS8_WINDOW_POINTS,
                                                  &out->lane0_window_start);
    (void)copy_ring_window_u16(s_dma_lane1,
                               DSO_DMA_BUF_SIZE,
                               s_acq10.snapshot_common_pos,
                               out->lane1_window,
                               out->lane_window_count,
                               &out->lane1_window_start);

    summarize_ring_window_u16(s_dma_ch2,
                              DSO_DMA_BUF_SIZE,
                              out->ch2_dma_pos,
                              window,
                              &out->ch2_vmin,
                              &out->ch2_vmax,
                              &out->ch2_mean);
    out->ch2_valid = true;
    out->ch2_capture_active = s_acq10.running;
    out->ch2_count = DSO_DMA_BUF_SIZE;
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_GetTimClkHz
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq10_GetTimClkHz(void)
{
    return s_acq10.runtime.tim8_clk_hz;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_GetTimArr
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq10_GetTimArr(void)
{
    return s_acq10.runtime.tim8_arr;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_SetTimArr
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq10_SetTimArr(uint32_t new_arr)
{
    s_acq10.runtime.tim8_arr = new_arr;
    s_acq10.runtime.active_profile = 0u;
    s_acq10.runtime.active_profile_name = s_custom_profile_name;
    refresh_runtime_rates_from_period();

    TIM1->CR1 |= TIM_CR1_ARPE;
    TIM1->ARR = new_arr;
    TIM1->CCR1 = tim1_cc_pulse(s_acq10.runtime.period_clocks, 0u);
    TIM1->CCR2 = tim1_cc_pulse(s_acq10.runtime.period_clocks, 1u);
    TIM1->CCR3 = tim1_cc_pulse(s_acq10.runtime.period_clocks, 2u);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_CopyRecentSamples
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq10_CopyRecentSamples(uint32_t dma_pos,
                                     int16_t cal_offset,
                                     uint16_t *out_samples,
                                     uint32_t max_samples)
{
    DSO_LaneMergeWindow window;
    uint32_t copied;

    if ((out_samples == NULL) || (max_samples == 0u)) {
        return 0u;
    }

    if (dma_pos != s_acq10.snapshot_common_pos) {
        DSO_Acq10_GetPos();
    }

    window = DSO_LaneMerge_ResolveCommonRecentWindow2(s_acq10.snapshot_common_pos,
                                                      max_samples / 2u,
                                                      DSO_DMA_BUF_SIZE);
    if (window.lane_count == 0u) {
        return 0u;
    }

    DSO_AcqCommon_InvalidateDMAU16Window(s_dma_lane0, window.start0, window.lane_count);
    DSO_AcqCommon_InvalidateDMAU16Window(s_dma_lane1, window.start1, window.lane_count);

    copied = DSO_LaneMerge_CopyAligned2(window.start0,
                                        window.start1,
                                        window.lane_count,
                                        DSO_DMA_BUF_SIZE,
                                        s_dma_lane0,
                                        s_dma_lane1,
                                        out_samples,
                                        max_samples);
    apply_lane1_offset(out_samples, copied, cal_offset);
    return copied;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_CopyNewSamples
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq10_CopyNewSamples(uint32_t prev_dma_pos,
                                  uint32_t dma_pos,
                                  int16_t cal_offset,
                                  uint16_t *out_samples,
                                  uint32_t max_samples)
{
    DSO_LaneMergeWindow window;
    uint32_t copied;

    if ((out_samples == NULL) || (max_samples == 0u)) {
        return 0u;
    }

    if (dma_pos != s_acq10.snapshot_common_pos) {
        DSO_Acq10_GetPos();
    }

    window = DSO_LaneMerge_ResolveCommonNewWindow2(prev_dma_pos,
                                                   s_acq10.snapshot_common_pos,
                                                   max_samples / 2u,
                                                   DSO_DMA_BUF_SIZE);
    if (window.lane_count == 0u) {
        return 0u;
    }

    DSO_AcqCommon_InvalidateDMAU16Window(s_dma_lane0, window.start0, window.lane_count);
    DSO_AcqCommon_InvalidateDMAU16Window(s_dma_lane1, window.start1, window.lane_count);

    copied = DSO_LaneMerge_CopyAligned2(window.start0,
                                        window.start1,
                                        window.lane_count,
                                        DSO_DMA_BUF_SIZE,
                                        s_dma_lane0,
                                        s_dma_lane1,
                                        out_samples,
                                        max_samples);
    apply_lane1_offset(out_samples, copied, cal_offset);
    return copied;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_CopyRecentCh2Samples
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq10_CopyRecentCh2Samples(uint32_t dma_pos,
                                        uint16_t *out_samples,
                                        uint32_t max_samples)
{
    uint32_t count = (max_samples > DSO_DMA_BUF_SIZE) ? DSO_DMA_BUF_SIZE : max_samples;
    uint32_t start;

    if ((out_samples == NULL) || (count == 0u)) {
        return 0u;
    }

    start = DSO_AcqCommon_RingStart(dma_pos, count);
    DSO_AcqCommon_InvalidateDMAU16Recent(s_dma_ch2, dma_pos, count);

    for (uint32_t i = 0u; i < count; ++i) {
        out_samples[i] = s_dma_ch2[(start + i) % DSO_DMA_BUF_SIZE];
    }
    return count;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_CopyNewCh2Samples
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq10_CopyNewCh2Samples(uint32_t prev_dma_pos,
                                     uint32_t dma_pos,
                                     uint16_t *out_samples,
                                     uint32_t max_samples)
{
    uint32_t count = DSO_AcqCommon_RingDelta(prev_dma_pos, dma_pos);

    if ((out_samples == NULL) || (max_samples == 0u)) {
        return 0u;
    }
    if (count > max_samples) {
        count = max_samples;
    }
    if (count == 0u) {
        return 0u;
    }

    {
        uint32_t start = DSO_AcqCommon_RingStart(dma_pos, count);

        DSO_AcqCommon_InvalidateDMAU16Window(s_dma_ch2, start, count);

        for (uint32_t i = 0u; i < count; ++i) {
            out_samples[i] = s_dma_ch2[(start + i) % DSO_DMA_BUF_SIZE];
        }
    }

    return count;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_DMA_IRQHandler
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq10_DMA_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&s_acq10.hdma_adc1);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_DMA2_IRQHandler
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq10_DMA2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&s_acq10.hdma_adc2);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_CH2_DMA_IRQHandler
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq10_CH2_DMA_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&s_acq10.hdma_adc3);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_HandleAdcHalfTransfer
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq10_HandleAdcHalfTransfer(ADC_TypeDef *instance)
{
    if (instance == ADC1) {
#if (Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE == 0)
        DSO_AcqCommon_InvalidateDCacheWindow(s_dma_lane0, sizeof(s_dma_lane0) / 2u);
#endif
    } else if (instance == ADC2) {
#if (Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE == 0)
        DSO_AcqCommon_InvalidateDCacheWindow(s_dma_lane1, sizeof(s_dma_lane1) / 2u);
#endif
    } else if (instance == ADC3) {
#if (Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE == 0)
        DSO_AcqCommon_InvalidateDCacheWindow(s_dma_ch2, sizeof(s_dma_ch2) / 2u);
#endif
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_HandleAdcTransferComplete
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq10_HandleAdcTransferComplete(ADC_TypeDef *instance)
{
    if (instance == ADC1) {
#if (Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE == 0)
        DSO_AcqCommon_InvalidateDCacheWindow(&s_dma_lane0[DSO_DMA_BUF_SIZE / 2u], sizeof(s_dma_lane0) / 2u);
#endif
    } else if (instance == ADC2) {
#if (Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE == 0)
        DSO_AcqCommon_InvalidateDCacheWindow(&s_dma_lane1[DSO_DMA_BUF_SIZE / 2u], sizeof(s_dma_lane1) / 2u);
#endif
    } else if (instance == ADC3) {
#if (Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE == 0)
        DSO_AcqCommon_InvalidateDCacheWindow(&s_dma_ch2[DSO_DMA_BUF_SIZE / 2u], sizeof(s_dma_ch2) / 2u);
#endif
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_CheckOverrun
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq10_CheckOverrun(void)
{
    if (ADC1->ISR & ADC_FLAG_OVR) {
        ADC1->ISR = ADC_FLAG_OVR;
        s_acq10.ovr_count++;
    }
    if (ADC2->ISR & ADC_FLAG_OVR) {
        ADC2->ISR = ADC_FLAG_OVR;
        s_acq10.ovr_count++;
    }
    if (ADC3->ISR & ADC_FLAG_OVR) {
        ADC3->ISR = ADC_FLAG_OVR;
        s_acq10.ovr_count++;
    }
    return s_acq10.ovr_count;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_GetOverrunCount
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq10_GetOverrunCount(void)
{
    return s_acq10.ovr_count;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_GetPhaseSyncErrorCount
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq10_GetPhaseSyncErrorCount(void)
{
    return s_acq10.phase_sync_error_count;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_IsDataReady
*   功能说明: 处理 10-bit 采集后端的 ADC/TIM/DMA 控制、状态查询或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_Acq10_IsDataReady(void)
{
    if (!s_acq10.running) {
        return false;
    }
    (void)DSO_Acq10_GetPos();
    return s_acq10.data_ready;
}
