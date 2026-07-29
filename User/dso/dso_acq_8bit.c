/**
 * @file dso_acq_8bit.c
 * @brief 8-bit HAL 实现：tri-interleaved，CH2 不可用。
 */

#include "dso_acq_8bit.h"

#include "dso_config.h"
#include "dso_acq.h"
#include "dso_acq_common.h"
#include "dso_acq8_fault_policy.h"
#include "dso_acq8_sample_unpack.h"
#include "dso_acq8_tim_trigger.h"
#include "dso_cache_range.h"
#include "dso_lane_merge.h"
#include "stm32h7xx_hal.h"
#include "y_t_stress_ab.h"

#include <stdio.h>
#include <string.h>

/* 宏定义: DSO_ACQ8_ADC_INPUT_CLK_HZ - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DSO_ACQ8_ADC_INPUT_CLK_HZ       75000000u
/* 宏定义: DSO_ACQ8_TIMER_PERIOD_CLKS - 时间间隔、周期或时基相关配置常量。 */
#define DSO_ACQ8_TIMER_PERIOD_CLKS      DSO_ACQ8_TRI_INTERLEAVED_TIM_PERIOD_CLKS
/* 宏定义: DSO_ACQ8_DEFAULT_ARR - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ACQ8_DEFAULT_ARR            DSO_ACQ8_TRI_INTERLEAVED_TIM_ARR
typedef struct {
    ADC_HandleTypeDef hadc1;
    ADC_HandleTypeDef hadc2;
    ADC_HandleTypeDef hadc3;
    DMA_HandleTypeDef hdma_adc1;
    DMA_HandleTypeDef hdma_adc2;
    DMA_HandleTypeDef hdma_adc3;
    TIM_HandleTypeDef htim1;
    TIM_HandleTypeDef htim8;
    DSO_AcqRuntimeInfo runtime;
    bool running;
    DSO_Acq8Topology requested_topology;
    DSO_Acq8Topology active_topology;
    uint32_t accepted_trigger_div;
    uint32_t phase_sync_error_count;
    uint32_t ovr_count;
    uint32_t ovr_count_lane[3];
    uint32_t common_pos;
    volatile uint32_t dma_sample_count[3];
    volatile uint32_t dma_half_count[3];
    volatile uint32_t dma_full_count[3];
    volatile uint32_t dma_sample_rate_hz[3];
    uint32_t dma_pos_sample_rate_hz[3];
    uint32_t dma_rate_tick[3];
    uint32_t dma_rate_count[3];
    uint32_t dma_pos_rate_tick;
    uint32_t dma_pos_rate_window_tick;
    uint32_t dma_pos_rate_last[3];
    uint32_t dma_pos_rate_accum[3];
    uint32_t dma_pos_rate_max_elapsed_ms;
    uint32_t dma_pos_rate_report_max_elapsed_ms;
    bool dma_pos_rate_ready;
    uint32_t snapshot_pos0;
    uint32_t snapshot_pos1;
    uint32_t snapshot_pos2;
    uint32_t snapshot_common_pos;
    uint32_t snapshot_lane0_delta;
    uint32_t snapshot_lane1_delta;
    uint32_t snapshot_lane2_delta;
    uint32_t snapshot_common_delta;
    bool snapshot_phase_sync_error;
    bool data_ready;
    uint8_t data_ready_lane_count;
} DSO_Acq8State;

static DSO_Acq8State s_acq8;
static uint16_t s_dma_single0[DSO_DMA_BUF_SIZE]
    __attribute__((section(".RAM_D2"), aligned(32)));
static uint16_t s_dma_dual1[DSO_DMA_BUF_SIZE]
    __attribute__((section(".RAM_D2"), aligned(32)));
static uint16_t s_dma_lane2[DSO_DMA_BUF_SIZE]
    __attribute__((section(".RAM_D2"), aligned(32)));

/*
*********************************************************************************************************
*   函 数 名: sanitize_topology
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static DSO_Acq8Topology sanitize_topology(uint32_t topology)
{
    (void)topology;
    return (DSO_Acq8Topology)DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED;
}

/*
*********************************************************************************************************
*   函 数 名: topology_lane_count
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t topology_lane_count(DSO_Acq8Topology topology)
{
    (void)topology;
    return 3u;
}

/*
*********************************************************************************************************
*   函 数 名: topology_profile_name
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static const char *topology_profile_name(DSO_Acq8Topology topology)
{
    (void)topology;
    return "8bit_tri_interleaved";
}

/* timer_clock_from_pclk and DSO_AcqCommon_CurrentTim1ClockHz moved to dso_acq_common.c */

/* DSO_AcqCommon_InvalidateDCacheWindow moved to dso_acq_common.c */

/* clean_DSO_AcqCommon_InvalidateDCacheWindow moved to dso_acq_common.c */

static void prepare_dma_rx_buffer(const uint16_t *samples)
{
    DSO_AcqCommon_CleanInvalidateDCacheWindow(samples,
                                    DSO_DMA_BUF_SIZE * (uint32_t)sizeof(samples[0]));
}

/*
*********************************************************************************************************
*   函 数 名: prepare_all_dma_rx_buffers
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void prepare_all_dma_rx_buffers(void)
{
    prepare_dma_rx_buffer(s_dma_single0);
    prepare_dma_rx_buffer(s_dma_dual1);
    prepare_dma_rx_buffer(s_dma_lane2);
}

/* DSO_AcqCommon_InvalidateDMAU16Window moved to dso_acq_common.c */

/* DSO_AcqCommon_InvalidateDMAU16Recent moved to dso_acq_common.c — use DSO_AcqCommon_InvalidateDMAU16Recent() directly */

/* DSO_AcqCommon_DmaCounterOrZero moved to dso_acq_common.c */

/* DSO_AcqCommon_DmaPosOrZero moved to dso_acq_common.c — use DSO_AcqCommon_DmaPosOrZero() directly */

/* DSO_AcqCommon_RingDelta moved to dso_acq_common.c */

static uint32_t dma_pos_for_lane(uint32_t lane)
{
    switch (lane) {
    case 0u:
        return DSO_AcqCommon_DmaPosOrZero(&s_acq8.hdma_adc1);
    case 1u:
        return DSO_AcqCommon_DmaPosOrZero(&s_acq8.hdma_adc2);
    case 2u:
        return DSO_AcqCommon_DmaPosOrZero(&s_acq8.hdma_adc3);
    default:
        return 0u;
    }
}

/* DSO_AcqCommon_RingStart moved to dso_acq_common.c */

/* adc_sample_time_hal_value moved to dso_acq_common.c */

static void force_adc_8bit_no_shift(ADC_TypeDef *instance)
{
    if (instance == NULL) {
        return;
    }

    MODIFY_REG(instance->CFGR, ADC_CFGR_RES, ADC_RESOLUTION_8B);
    MODIFY_REG(instance->CFGR2, ADC_CFGR2_LSHIFT, ADC_LEFTBITSHIFT_NONE);
}

/*
*********************************************************************************************************
*   函 数 名: log_adc_8bit_config
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void log_adc_8bit_config(const char *tag, ADC_TypeDef *instance)
{
    if ((tag == NULL) || (instance == NULL)) {
        return;
    }

#if DSO_DEBUG_PIPELINE
    printf("[ADC8CFG] %s CFGR=0x%08lX CFGR2=0x%08lX RES=0x%08lX LSHIFT=0x%08lX\r\n",
           tag,
           (unsigned long)instance->CFGR,
           (unsigned long)instance->CFGR2,
           (unsigned long)(instance->CFGR & ADC_CFGR_RES),
           (unsigned long)(instance->CFGR2 & ADC_CFGR2_LSHIFT));
#else
    (void)tag;
    (void)instance;
#endif
}

/*
*********************************************************************************************************
*   函 数 名: refresh_runtime_info
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void refresh_runtime_info(void)
{
    uint32_t tim_clk_hz = DSO_AcqCommon_CurrentTim1ClockHz();
    uint32_t period_clocks;
    uint32_t accepted_trigger_div = 1u;
    uint32_t accepted_rate_hz;

    s_acq8.runtime.revid = HAL_GetREVID();
    s_acq8.runtime.revision = DSO_AcqModel_RevisionFromRevid(s_acq8.runtime.revid);
    s_acq8.runtime.requested_profile = (uint32_t)s_acq8.requested_topology;
    s_acq8.runtime.active_profile = (uint32_t)s_acq8.active_topology;
    s_acq8.runtime.requested_profile_name = topology_profile_name(s_acq8.requested_topology);
    s_acq8.runtime.active_profile_name = topology_profile_name(s_acq8.active_topology);
    s_acq8.runtime.adc_input_clk_hz = DSO_ACQ8_ADC_INPUT_CLK_HZ;
    s_acq8.runtime.adc_core_divider = 1u;
    s_acq8.runtime.adc_core_clk_hz = DSO_ACQ8_ADC_INPUT_CLK_HZ;
    s_acq8.runtime.tim8_clk_hz = tim_clk_hz;
    if (s_acq8.runtime.tim8_arr == 0u) {
        s_acq8.runtime.tim8_arr = DSO_ACQ8_DEFAULT_ARR;
    }
    s_acq8.runtime.period_clocks = s_acq8.runtime.tim8_arr + 1u;
    s_acq8.runtime.sample_cycles_x10 = DSO_ADC_SAMPLE_CYCLES_X10;
    s_acq8.runtime.sar_cycles_x10 = DSO_ADC_SAR_CYCLES_X10_8BIT;
    s_acq8.runtime.delay_cycles = DSO_ADC_DELAY_CYCLES;

    period_clocks = s_acq8.runtime.period_clocks;
    s_acq8.runtime.trigger_rate_hz = (period_clocks != 0u)
                                   ? (tim_clk_hz / period_clocks)
                                   : 0u;
    accepted_rate_hz =
        DSO_AcqModel_AcceptedTriggerRateHz(s_acq8.runtime.trigger_rate_hz,
                                           DSO_ACQ8_TRI_INTERLEAVED_LANE_RATE_HZ,
                                           &accepted_trigger_div);

    s_acq8.runtime.single_adc = false;
    s_acq8.runtime.required_period_clocks =
        (tim_clk_hz != 0u) ? (tim_clk_hz / DSO_ACQ8_TRI_INTERLEAVED_LANE_RATE_HZ) : 0u;
    s_acq8.runtime.lane_rate_hz = accepted_rate_hz;
    s_acq8.runtime.sample_rate_hz = 3u * accepted_rate_hz;
    s_acq8.runtime.profile_valid = (accepted_trigger_div == 1u);
    s_acq8.accepted_trigger_div = accepted_trigger_div;
}

/*
*********************************************************************************************************
*   函 数 名: config_pll2
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void config_pll2(void)
{
    RCC_PeriphCLKInitTypeDef periph_clk = {0};

    periph_clk.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    periph_clk.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;
    periph_clk.PLL2.PLL2M = 5;
    periph_clk.PLL2.PLL2N = 150;
    periph_clk.PLL2.PLL2P = 10;
    periph_clk.PLL2.PLL2Q = 10;
    periph_clk.PLL2.PLL2R = 10;
    periph_clk.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_2;
    periph_clk.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
    periph_clk.PLL2.PLL2FRACN = 0;

    if (HAL_RCCEx_PeriphCLKConfig(&periph_clk) != HAL_OK) {
        while (1) {}
    }
}

/*
*********************************************************************************************************
*   函 数 名: config_gpio
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void config_gpio(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_6;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    HAL_GPIO_Init(GPIOC, &gpio);

    HAL_SYSCFG_EnableBOOST();
    HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PC2, SYSCFG_SWITCH_PC2_CLOSE);
    HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PC3, SYSCFG_SWITCH_PC3_CLOSE);
}

/*
*********************************************************************************************************
*   函 数 名: init_dma_handle
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void init_dma_handle(DMA_HandleTypeDef *hdma,
                            DMA_Stream_TypeDef *instance,
                            uint32_t request,
                            uint32_t periph_align,
                            uint32_t mem_align,
                            IRQn_Type irqn)
{
    memset(hdma, 0, sizeof(*hdma));
    hdma->Instance = instance;
    hdma->Init.Request = request;
    hdma->Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma->Init.PeriphInc = DMA_PINC_DISABLE;
    hdma->Init.MemInc = DMA_MINC_ENABLE;
    hdma->Init.PeriphDataAlignment = periph_align;
    hdma->Init.MemDataAlignment = mem_align;
    hdma->Init.Mode = DMA_CIRCULAR;
    hdma->Init.Priority = DMA_PRIORITY_VERY_HIGH;
    hdma->Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    hdma->Init.MemBurst = DMA_MBURST_SINGLE;
    hdma->Init.PeriphBurst = DMA_PBURST_SINGLE;
    if (HAL_DMA_Init(hdma) != HAL_OK) {
        while (1) {}
    }

    HAL_NVIC_SetPriority(irqn, 5, 0);
    HAL_NVIC_EnableIRQ(irqn);
}

/* DSO_AcqCommon_ResetDmaIRQ moved to dso_acq_common.c */

static void fill_adc_init(ADC_HandleTypeDef *hadc,
                          ADC_TypeDef *instance,
                          uint32_t trigger,
                          uint32_t trigger_edge,
                          uint32_t dmngt)
{
    hadc->Instance = instance;
    hadc->Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
    hadc->Init.Resolution = ADC_RESOLUTION_8B;
    hadc->Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc->Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc->Init.LowPowerAutoWait = DISABLE;
    hadc->Init.ContinuousConvMode = DISABLE;
    hadc->Init.NbrOfConversion = 1;
    hadc->Init.DiscontinuousConvMode = DISABLE;
    hadc->Init.ExternalTrigConv = trigger;
    hadc->Init.ExternalTrigConvEdge = trigger_edge;
    hadc->Init.ConversionDataManagement = dmngt;
    hadc->Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    hadc->Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
    hadc->Init.BoostMode = ENABLE;
    hadc->Init.OversamplingMode = DISABLE;
}

/* DSO_AcqCommon_ConfigAdcChannel moved to dso_acq_common.c */

static uint32_t tim1_cc_pulse(uint32_t period_clocks, uint32_t which)
{
    switch (which) {
    case 0u:
        return period_clocks / 6u;
    case 1u:
        return period_clocks / 2u;
    default:
        return (period_clocks * 5u) / 6u;
    }
}

/*
*********************************************************************************************************
*   函 数 名: config_tim1_channel
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void config_tim1_channel(uint32_t channel, uint32_t pulse)
{
    TIM_OC_InitTypeDef oc = {0};

    oc.OCMode = (DSO_Acq8_GetTimTriggerMode() == DSO_ACQ8_TIM_TRIGGER_MODE_PWM1)
              ? TIM_OCMODE_PWM1
              : TIM_OCMODE_TIMING;
    oc.Pulse = pulse;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    oc.OCIdleState = TIM_OCIDLESTATE_RESET;
    oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;

    if (DSO_Acq8_GetTimTriggerMode() == DSO_ACQ8_TIM_TRIGGER_MODE_PWM1) {
        if (HAL_TIM_PWM_ConfigChannel(&s_acq8.htim1, &oc, channel) != HAL_OK) {
            while (1) {}
        }
    } else {
        if (HAL_TIM_OC_ConfigChannel(&s_acq8.htim1, &oc, channel) != HAL_OK) {
            while (1) {}
        }
    }
}

/*
*********************************************************************************************************
*   函 数 名: config_tim1_phase
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void config_tim1_phase(void)
{
    __HAL_RCC_TIM1_CLK_ENABLE();

    memset(&s_acq8.htim1, 0, sizeof(s_acq8.htim1));
    s_acq8.htim1.Instance = TIM1;
    s_acq8.htim1.Init.Prescaler = 0u;
    s_acq8.htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    s_acq8.htim1.Init.Period = s_acq8.runtime.tim8_arr;
    s_acq8.htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    s_acq8.htim1.Init.RepetitionCounter = 0u;
    s_acq8.htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (DSO_Acq8_GetTimTriggerMode() == DSO_ACQ8_TIM_TRIGGER_MODE_PWM1) {
        if (HAL_TIM_PWM_Init(&s_acq8.htim1) != HAL_OK) {
            while (1) {}
        }
    } else {
        if (HAL_TIM_Base_Init(&s_acq8.htim1) != HAL_OK) {
            while (1) {}
        }
        if (HAL_TIM_OC_Init(&s_acq8.htim1) != HAL_OK) {
            while (1) {}
        }
    }

    config_tim1_channel(TIM_CHANNEL_1, tim1_cc_pulse(s_acq8.runtime.period_clocks, 0u));
    config_tim1_channel(TIM_CHANNEL_2, tim1_cc_pulse(s_acq8.runtime.period_clocks, 1u));
    config_tim1_channel(TIM_CHANNEL_3, tim1_cc_pulse(s_acq8.runtime.period_clocks, 2u));
}

/*
*********************************************************************************************************
*   函 数 名: reset_common_tracking
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void reset_common_tracking(void)
{
    uint32_t now = HAL_GetTick();

    s_acq8.common_pos = 0u;
    for (uint32_t lane = 0u; lane < 3u; ++lane) {
        s_acq8.dma_sample_count[lane] = 0u;
        s_acq8.dma_half_count[lane] = 0u;
        s_acq8.dma_full_count[lane] = 0u;
        s_acq8.dma_sample_rate_hz[lane] = 0u;
        s_acq8.dma_pos_sample_rate_hz[lane] = 0u;
        s_acq8.dma_rate_tick[lane] = now;
        s_acq8.dma_rate_count[lane] = 0u;
        s_acq8.dma_pos_rate_last[lane] = 0u;
        s_acq8.dma_pos_rate_accum[lane] = 0u;
    }
    s_acq8.dma_pos_rate_tick = now;
    s_acq8.dma_pos_rate_window_tick = now;
    s_acq8.dma_pos_rate_max_elapsed_ms = 0u;
    s_acq8.dma_pos_rate_report_max_elapsed_ms = 0u;
    s_acq8.dma_pos_rate_ready = false;
    s_acq8.snapshot_pos0 = 0u;
    s_acq8.snapshot_pos1 = 0u;
    s_acq8.snapshot_pos2 = 0u;
    s_acq8.snapshot_common_pos = 0u;
    s_acq8.snapshot_lane0_delta = 0u;
    s_acq8.snapshot_lane1_delta = 0u;
    s_acq8.snapshot_lane2_delta = 0u;
    s_acq8.snapshot_common_delta = 0u;
    s_acq8.snapshot_phase_sync_error = false;
    s_acq8.data_ready = false;
    s_acq8.data_ready_lane_count = topology_lane_count(s_acq8.active_topology);
}

/*
*********************************************************************************************************
*   函 数 名: acq8_mark_data_not_ready
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void acq8_mark_data_not_ready(void)
{
    s_acq8.data_ready = false;
    s_acq8.data_ready_lane_count = topology_lane_count(s_acq8.active_topology);
}

/*
*********************************************************************************************************
*   函 数 名: acq8_all_active_lanes_have_dma_progress
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool acq8_all_active_lanes_have_dma_progress(void)
{
    uint8_t lanes = topology_lane_count(s_acq8.active_topology);

    if (!s_acq8.running || (lanes == 0u) || (s_acq8.snapshot_common_delta == 0u)) {
        return false;
    }
    if (s_acq8.snapshot_common_pos == 0u) {
        return false;
    }
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: acq8_update_data_ready_from_snapshot
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void acq8_update_data_ready_from_snapshot(void)
{
    if (acq8_all_active_lanes_have_dma_progress()) {
        s_acq8.data_ready = true;
    }
    s_acq8.data_ready_lane_count = topology_lane_count(s_acq8.active_topology);
}

/*
*********************************************************************************************************
*   函 数 名: note_dma_samples
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void note_dma_samples(uint8_t lane, uint32_t sample_count)
{
    uint32_t total_count;

    if (lane >= 3u) {
        return;
    }

    total_count = s_acq8.dma_sample_count[lane] + sample_count;
    s_acq8.dma_sample_count[lane] = total_count;
}

/*
*********************************************************************************************************
*   函 数 名: note_dma_half_transfer
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void note_dma_half_transfer(uint8_t lane)
{
    if (lane < 3u) {
        s_acq8.dma_half_count[lane]++;
    }
}

/*
*********************************************************************************************************
*   函 数 名: note_dma_full_transfer
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void note_dma_full_transfer(uint8_t lane)
{
    if (lane < 3u) {
        s_acq8.dma_full_count[lane]++;
    }
}

/*
*********************************************************************************************************
*   函 数 名: acq8_dma_half_callback
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void acq8_dma_half_callback(DMA_HandleTypeDef *hdma)
{
    ADC_HandleTypeDef *hadc;

    if ((hdma == NULL) || (hdma->Parent == NULL)) {
        return;
    }

    hadc = (ADC_HandleTypeDef *)hdma->Parent;
    DSO_Acq8_HandleAdcHalfTransfer(hadc->Instance);
}

/*
*********************************************************************************************************
*   函 数 名: acq8_dma_full_callback
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void acq8_dma_full_callback(DMA_HandleTypeDef *hdma)
{
    ADC_HandleTypeDef *hadc;

    if ((hdma == NULL) || (hdma->Parent == NULL)) {
        return;
    }

    hadc = (ADC_HandleTypeDef *)hdma->Parent;
    DSO_Acq8_HandleAdcTransferComplete(hadc->Instance);
}

/*
*********************************************************************************************************
*   函 数 名: install_acq8_dma_callbacks
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void install_acq8_dma_callbacks(ADC_HandleTypeDef *hadc)
{
    if ((hadc == NULL) || (hadc->DMA_Handle == NULL)) {
        return;
    }

    /*
     * HAL ADC suppresses conversion-complete callbacks once the ADC error
     * state is set. High-speed circular DMA still needs both halves refreshed.
     */
    hadc->DMA_Handle->XferHalfCpltCallback = acq8_dma_half_callback;
    hadc->DMA_Handle->XferCpltCallback = acq8_dma_full_callback;
}

/*
*********************************************************************************************************
*   函 数 名: clear_adc_overrun_flag
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void clear_adc_overrun_flag(ADC_HandleTypeDef *hadc)
{
    if ((hadc == NULL) || (hadc->Instance == NULL)) {
        return;
    }

    __HAL_ADC_CLEAR_FLAG(hadc, ADC_FLAG_OVR);
}

/*
*********************************************************************************************************
*   函 数 名: clear_active_adc_overrun_flags
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void clear_active_adc_overrun_flags(void)
{
    clear_adc_overrun_flag(&s_acq8.hadc1);
    clear_adc_overrun_flag(&s_acq8.hadc2);
    clear_adc_overrun_flag(&s_acq8.hadc3);
}

/*
*********************************************************************************************************
*   函 数 名: refresh_dma_sample_rates
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void refresh_dma_sample_rates(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t elapsed_ms = now - s_acq8.dma_rate_tick[0];

    if (elapsed_ms < 250u) {
        return;
    }

    for (uint32_t lane = 0u; lane < 3u; ++lane) {
        uint32_t count = s_acq8.dma_sample_count[lane];
        uint32_t delta_count = count - s_acq8.dma_rate_count[lane];

        s_acq8.dma_sample_rate_hz[lane] =
            (uint32_t)(((uint64_t)delta_count * 1000u) / elapsed_ms);
        s_acq8.dma_rate_count[lane] = count;
        s_acq8.dma_rate_tick[lane] = now;
    }
}

/*
*********************************************************************************************************
*   函 数 名: refresh_dma_pos_sample_rates
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void refresh_dma_pos_sample_rates(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t lane_count = topology_lane_count(s_acq8.active_topology);
    uint32_t elapsed_ms;
    uint32_t window_ms;

    if (!s_acq8.dma_pos_rate_ready) {
        for (uint32_t lane = 0u; lane < 3u; ++lane) {
            s_acq8.dma_pos_rate_last[lane] =
                (lane < lane_count) ? dma_pos_for_lane(lane) : 0u;
            s_acq8.dma_pos_rate_accum[lane] = 0u;
            s_acq8.dma_pos_sample_rate_hz[lane] = 0u;
        }
        s_acq8.dma_pos_rate_tick = now;
        s_acq8.dma_pos_rate_window_tick = now;
        s_acq8.dma_pos_rate_max_elapsed_ms = 0u;
        s_acq8.dma_pos_rate_report_max_elapsed_ms = 0u;
        s_acq8.dma_pos_rate_ready = true;
        return;
    }

    elapsed_ms = now - s_acq8.dma_pos_rate_tick;
    if (elapsed_ms == 0u) {
        return;
    }

    if (elapsed_ms > s_acq8.dma_pos_rate_max_elapsed_ms) {
        s_acq8.dma_pos_rate_max_elapsed_ms = elapsed_ms;
    }

    for (uint32_t lane = 0u; lane < 3u; ++lane) {
        if (lane < lane_count) {
            uint32_t pos = dma_pos_for_lane(lane);

            s_acq8.dma_pos_rate_accum[lane] +=
                DSO_AcqCommon_RingDelta(s_acq8.dma_pos_rate_last[lane], pos);
            s_acq8.dma_pos_rate_last[lane] = pos;
        } else {
            s_acq8.dma_pos_rate_accum[lane] = 0u;
            s_acq8.dma_pos_rate_last[lane] = 0u;
            s_acq8.dma_pos_sample_rate_hz[lane] = 0u;
        }
    }
    s_acq8.dma_pos_rate_tick = now;

    window_ms = now - s_acq8.dma_pos_rate_window_tick;
    if (window_ms < 250u) {
        return;
    }

    for (uint32_t lane = 0u; lane < 3u; ++lane) {
        if (lane < lane_count) {
            s_acq8.dma_pos_sample_rate_hz[lane] =
                (uint32_t)(((uint64_t)s_acq8.dma_pos_rate_accum[lane] * 1000u) / window_ms);
        } else {
            s_acq8.dma_pos_sample_rate_hz[lane] = 0u;
        }
        s_acq8.dma_pos_rate_accum[lane] = 0u;
    }
    s_acq8.dma_pos_rate_window_tick = now;
    s_acq8.dma_pos_rate_report_max_elapsed_ms = s_acq8.dma_pos_rate_max_elapsed_ms;
    s_acq8.dma_pos_rate_max_elapsed_ms = 0u;
}

/*
*********************************************************************************************************
*   函 数 名: backend_reset_handles
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void backend_reset_handles(void)
{
    memset(&s_acq8.hadc1, 0, sizeof(s_acq8.hadc1));
    memset(&s_acq8.hadc2, 0, sizeof(s_acq8.hadc2));
    memset(&s_acq8.hadc3, 0, sizeof(s_acq8.hadc3));
    memset(&s_acq8.hdma_adc1, 0, sizeof(s_acq8.hdma_adc1));
    memset(&s_acq8.hdma_adc2, 0, sizeof(s_acq8.hdma_adc2));
    memset(&s_acq8.hdma_adc3, 0, sizeof(s_acq8.hdma_adc3));
    memset(&s_acq8.htim1, 0, sizeof(s_acq8.htim1));
    memset(&s_acq8.htim8, 0, sizeof(s_acq8.htim8));
}

/*
*********************************************************************************************************
*   函 数 名: backend_deinit_peripherals
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void backend_deinit_peripherals(void);
/*
*********************************************************************************************************
*   函 数 名: configure_active_backend
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void configure_active_backend(void);
/*
*********************************************************************************************************
*   函 数 名: start_active_streaming
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void start_active_streaming(void);
/*
*********************************************************************************************************
*   函 数 名: stop_active_streaming
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void stop_active_streaming(void);
/*
*********************************************************************************************************
*   函 数 名: snapshot_common_position
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void snapshot_common_position(void);

/*
*********************************************************************************************************
*   函 数 名: backend_deinit_peripherals
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void backend_deinit_peripherals(void)
{
    if (s_acq8.htim1.Instance != NULL) {
        if (DSO_Acq8_GetTimTriggerMode() == DSO_ACQ8_TIM_TRIGGER_MODE_PWM1) {
            (void)HAL_TIM_PWM_DeInit(&s_acq8.htim1);
        } else {
            (void)HAL_TIM_OC_DeInit(&s_acq8.htim1);
            (void)HAL_TIM_Base_DeInit(&s_acq8.htim1);
        }
    }
    if (s_acq8.htim8.Instance != NULL) {
        (void)HAL_TIM_Base_DeInit(&s_acq8.htim8);
    }

    if (s_acq8.hadc1.Instance != NULL) {
        (void)HAL_ADC_DeInit(&s_acq8.hadc1);
    }
    if (s_acq8.hadc2.Instance != NULL) {
        (void)HAL_ADC_DeInit(&s_acq8.hadc2);
    }
    if (s_acq8.hadc3.Instance != NULL) {
        (void)HAL_ADC_DeInit(&s_acq8.hadc3);
    }

    if (s_acq8.hdma_adc1.Instance != NULL) {
        (void)HAL_DMA_DeInit(&s_acq8.hdma_adc1);
    }
    if (s_acq8.hdma_adc2.Instance != NULL) {
        (void)HAL_DMA_DeInit(&s_acq8.hdma_adc2);
    }
    if (s_acq8.hdma_adc3.Instance != NULL) {
        (void)HAL_DMA_DeInit(&s_acq8.hdma_adc3);
    }

    DSO_AcqCommon_ResetDmaIRQ(DMA1_Stream0_IRQn);
    DSO_AcqCommon_ResetDmaIRQ(DMA1_Stream1_IRQn);
    DSO_AcqCommon_ResetDmaIRQ(DMA1_Stream2_IRQn);
    DSO_AcqCommon_ResetDmaIRQ(DMA2_Stream0_IRQn);
    DSO_AcqCommon_ResetDmaIRQ(DMA2_Stream1_IRQn);

    backend_reset_handles();
}

/*
*********************************************************************************************************
*   函 数 名: configure_tri_interleaved_backend
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void configure_tri_interleaved_backend(void)
{
    ADC_MultiModeTypeDef multi = {0};

    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    __HAL_RCC_ADC12_CLK_ENABLE();
    __HAL_RCC_ADC3_CLK_ENABLE();

    fill_adc_init(&s_acq8.hadc1,
                  ADC1,
                  ADC_EXTERNALTRIG_T1_CC1,
                  ADC_EXTERNALTRIGCONVEDGE_RISING,
                  ADC_CONVERSIONDATA_DMA_CIRCULAR);
    fill_adc_init(&s_acq8.hadc2,
                  ADC2,
                  ADC_EXTERNALTRIG_T1_CC2,
                  ADC_EXTERNALTRIGCONVEDGE_RISING,
                  ADC_CONVERSIONDATA_DMA_CIRCULAR);
    fill_adc_init(&s_acq8.hadc3,
                  ADC3,
                  ADC_EXTERNALTRIG_T1_CC3,
                  ADC_EXTERNALTRIGCONVEDGE_RISING,
                  ADC_CONVERSIONDATA_DMA_CIRCULAR);

    if (HAL_ADC_Init(&s_acq8.hadc1) != HAL_OK) {
        while (1) {}
    }
    if (HAL_ADC_Init(&s_acq8.hadc2) != HAL_OK) {
        while (1) {}
    }
    if (HAL_ADC_Init(&s_acq8.hadc3) != HAL_OK) {
        while (1) {}
    }

    force_adc_8bit_no_shift(s_acq8.hadc1.Instance);
    force_adc_8bit_no_shift(s_acq8.hadc2.Instance);
    force_adc_8bit_no_shift(s_acq8.hadc3.Instance);

    DSO_AcqCommon_ConfigAdcChannel(&s_acq8.hadc1, ADC_CHANNEL_3);
    DSO_AcqCommon_ConfigAdcChannel(&s_acq8.hadc2, ADC_CHANNEL_3);
    DSO_AcqCommon_ConfigAdcChannel(&s_acq8.hadc3, ADC_CHANNEL_12);

    multi.Mode = ADC_MODE_INDEPENDENT;
    if (HAL_ADCEx_MultiModeConfigChannel(&s_acq8.hadc1, &multi) != HAL_OK) {
        while (1) {}
    }

    if (HAL_ADCEx_Calibration_Start(&s_acq8.hadc3, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK) {
        while (1) {}
    }
    if (HAL_ADCEx_Calibration_Start(&s_acq8.hadc2, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK) {
        while (1) {}
    }
    if (HAL_ADCEx_Calibration_Start(&s_acq8.hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK) {
        while (1) {}
    }
    log_adc_8bit_config("ADC1_TRI8", s_acq8.hadc1.Instance);
    log_adc_8bit_config("ADC2_TRI8", s_acq8.hadc2.Instance);
    log_adc_8bit_config("ADC3_TRI8", s_acq8.hadc3.Instance);

    /*
     * In 3L mode ADC1 and ADC2 each run at the lane rate. Keeping both on
     * DMA1 makes one ADC12 lane lose arbitration under sustained 10 MS/s load.
     */
    init_dma_handle(&s_acq8.hdma_adc1,
                    DMA2_Stream0,
                    DMA_REQUEST_ADC1,
                    DMA_PDATAALIGN_HALFWORD,
                    DMA_MDATAALIGN_HALFWORD,
                    DMA2_Stream0_IRQn);
    __HAL_LINKDMA(&s_acq8.hadc1, DMA_Handle, s_acq8.hdma_adc1);

    init_dma_handle(&s_acq8.hdma_adc2,
                    DMA1_Stream0,
                    DMA_REQUEST_ADC2,
                    DMA_PDATAALIGN_HALFWORD,
                    DMA_MDATAALIGN_HALFWORD,
                    DMA1_Stream0_IRQn);
    __HAL_LINKDMA(&s_acq8.hadc2, DMA_Handle, s_acq8.hdma_adc2);

    init_dma_handle(&s_acq8.hdma_adc3,
                    DMA2_Stream1,
                    DMA_REQUEST_ADC3,
                    DMA_PDATAALIGN_HALFWORD,
                    DMA_MDATAALIGN_HALFWORD,
                    DMA2_Stream1_IRQn);
    __HAL_LINKDMA(&s_acq8.hadc3, DMA_Handle, s_acq8.hdma_adc3);

    config_tim1_phase();
}

/*
*********************************************************************************************************
*   函 数 名: configure_active_backend
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void configure_active_backend(void)
{
    configure_tri_interleaved_backend();
}

/*
*********************************************************************************************************
*   函 数 名: active_timer_instance
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static TIM_TypeDef *active_timer_instance(void)
{
    return TIM1;
}

/* start_tim1_pwm_channels_synchronized moved to dso_acq_common.c */
/* Use DSO_AcqCommon_StartTim1PwmChannelsSynchronized(&s_acq8.htim1) directly */

static void start_triphasic_timer(void)
{
    if (DSO_Acq8_GetTimTriggerMode() == DSO_ACQ8_TIM_TRIGGER_MODE_PWM1) {
        DSO_AcqCommon_StartTim1PwmChannelsSynchronized(&s_acq8.htim1);
    } else {
        TIM1->CNT = 0u;
        TIM1->EGR = TIM_EGR_UG;

        if (HAL_TIM_OC_Start(&s_acq8.htim1, TIM_CHANNEL_1) != HAL_OK) {
            while (1) {}
        }
        if (HAL_TIM_OC_Start(&s_acq8.htim1, TIM_CHANNEL_2) != HAL_OK) {
            while (1) {}
        }
        if (HAL_TIM_OC_Start(&s_acq8.htim1, TIM_CHANNEL_3) != HAL_OK) {
            while (1) {}
        }
        if (HAL_TIM_Base_Start(&s_acq8.htim1) != HAL_OK) {
            while (1) {}
        }
    }
}

/*
*********************************************************************************************************
*   函 数 名: stop_triphasic_timer
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void stop_triphasic_timer(void)
{
    if (DSO_Acq8_GetTimTriggerMode() == DSO_ACQ8_TIM_TRIGGER_MODE_PWM1) {
        (void)HAL_TIM_PWM_Stop(&s_acq8.htim1, TIM_CHANNEL_1);
        (void)HAL_TIM_PWM_Stop(&s_acq8.htim1, TIM_CHANNEL_2);
        (void)HAL_TIM_PWM_Stop(&s_acq8.htim1, TIM_CHANNEL_3);
    } else {
        (void)HAL_TIM_OC_Stop(&s_acq8.htim1, TIM_CHANNEL_1);
        (void)HAL_TIM_OC_Stop(&s_acq8.htim1, TIM_CHANNEL_2);
        (void)HAL_TIM_OC_Stop(&s_acq8.htim1, TIM_CHANNEL_3);
        (void)HAL_TIM_Base_Stop(&s_acq8.htim1);
    }
}

/*
*********************************************************************************************************
*   函 数 名: start_tri_interleaved_streaming
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void start_tri_interleaved_streaming(void)
{
    prepare_all_dma_rx_buffers();

    if (HAL_ADC_Start_DMA(&s_acq8.hadc1, (uint32_t *)s_dma_single0, DSO_DMA_BUF_SIZE) != HAL_OK) {
        while (1) {}
    }
    install_acq8_dma_callbacks(&s_acq8.hadc1);
    if (HAL_ADC_Start_DMA(&s_acq8.hadc2, (uint32_t *)s_dma_dual1, DSO_DMA_BUF_SIZE) != HAL_OK) {
        while (1) {}
    }
    install_acq8_dma_callbacks(&s_acq8.hadc2);
    if (HAL_ADC_Start_DMA(&s_acq8.hadc3, (uint32_t *)s_dma_lane2, DSO_DMA_BUF_SIZE) != HAL_OK) {
        while (1) {}
    }
    install_acq8_dma_callbacks(&s_acq8.hadc3);
    clear_active_adc_overrun_flags();
    start_triphasic_timer();
    reset_common_tracking();
    acq8_mark_data_not_ready();
}

/*
*********************************************************************************************************
*   函 数 名: stop_tri_interleaved_streaming
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void stop_tri_interleaved_streaming(void)
{
    stop_triphasic_timer();
    (void)HAL_ADC_Stop_DMA(&s_acq8.hadc3);
    (void)HAL_ADC_Stop_DMA(&s_acq8.hadc2);
    (void)HAL_ADC_Stop_DMA(&s_acq8.hadc1);
}

/*
*********************************************************************************************************
*   函 数 名: start_active_streaming
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void start_active_streaming(void)
{
    start_tri_interleaved_streaming();
}

/*
*********************************************************************************************************
*   函 数 名: stop_active_streaming
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void stop_active_streaming(void)
{
    stop_tri_interleaved_streaming();
}

/*
*********************************************************************************************************
*   函 数 名: snapshot_tri_interleaved_position
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void snapshot_tri_interleaved_position(void)
{
    DSO_LaneDeltaInfo delta;
    uint32_t pos0 = DSO_AcqCommon_DmaPosOrZero(&s_acq8.hdma_adc1);
    uint32_t pos1 = DSO_AcqCommon_DmaPosOrZero(&s_acq8.hdma_adc2);
    uint32_t pos2 = DSO_AcqCommon_DmaPosOrZero(&s_acq8.hdma_adc3);
    uint32_t common;

    delta = DSO_LaneMerge_CommonDelta3(s_acq8.common_pos,
                                       pos0,
                                       pos1,
                                       pos2,
                                       DSO_DMA_BUF_SIZE);
    common = (s_acq8.common_pos + delta.common_delta) % DSO_DMA_BUF_SIZE;

    s_acq8.common_pos = common;
    s_acq8.snapshot_pos0 = pos0;
    s_acq8.snapshot_pos1 = pos1;
    s_acq8.snapshot_pos2 = pos2;
    s_acq8.snapshot_common_pos = s_acq8.common_pos;
    s_acq8.snapshot_lane0_delta = DSO_AcqCommon_RingDelta(s_acq8.snapshot_common_pos, pos0);
    s_acq8.snapshot_lane1_delta = DSO_AcqCommon_RingDelta(s_acq8.snapshot_common_pos, pos1);
    s_acq8.snapshot_lane2_delta = DSO_AcqCommon_RingDelta(s_acq8.snapshot_common_pos, pos2);
    s_acq8.snapshot_common_delta = delta.common_delta;
    s_acq8.snapshot_phase_sync_error = delta.phase_sync_error;
    if (s_acq8.snapshot_phase_sync_error) {
        s_acq8.phase_sync_error_count++;
    }
    acq8_update_data_ready_from_snapshot();
}

/*
*********************************************************************************************************
*   函 数 名: snapshot_common_position
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void snapshot_common_position(void)
{
    snapshot_tri_interleaved_position();
}

/*
*********************************************************************************************************
*   函 数 名: copy_ring_window_low_byte16
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t copy_ring_window_low_byte16(const uint16_t *samples,
                                            uint32_t ring_size,
                                            uint32_t end_pos,
                                            uint16_t *out_samples,
                                            uint32_t max_samples,
                                            uint32_t *start_out)
{
    uint32_t count = max_samples;
    uint32_t start;
    DSO_Acq8U16DecodeMode mode;

    if ((samples == NULL) || (out_samples == NULL) || (ring_size == 0u) || (max_samples == 0u)) {
        if (start_out != NULL) {
            *start_out = 0u;
        }
        return 0u;
    }

    if (count > ring_size) {
        count = ring_size;
    }

    start = DSO_AcqCommon_RingStart(end_pos, count);
    DSO_AcqCommon_InvalidateDMAU16Window(samples, start, count);
    mode = DSO_Acq8_DetectU16DecodeModeRing(samples, ring_size, end_pos, count);
    for (uint32_t i = 0u; i < count; ++i) {
        out_samples[i] = DSO_Acq8_DecodeU16Sample(samples[(start + i) % ring_size], mode);
    }

    if (start_out != NULL) {
        *start_out = start;
    }
    return count;
}

/*
*********************************************************************************************************
*   函 数 名: summarize_interleaved_lane_window
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void summarize_interleaved_lane_window(uint8_t lane_index,
                                              uint32_t end_word_pos,
                                              uint32_t lane_sample_count,
                                              uint16_t *out_vmin,
                                              uint16_t *out_vmax,
                                              uint16_t *out_mean)
{
    if ((lane_index > 2u) || (lane_sample_count == 0u)) {
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

    if (lane_index == 0u) {
        DSO_AcqCommon_InvalidateDMAU16Recent(s_dma_single0, end_word_pos, lane_sample_count);
        DSO_Acq8_SummarizeRingLowByte16(s_dma_single0,
                                        DSO_DMA_BUF_SIZE,
                                        end_word_pos,
                                        lane_sample_count,
                                        out_vmin,
                                        out_vmax,
                                        out_mean);
    } else if (lane_index == 1u) {
        DSO_AcqCommon_InvalidateDMAU16Recent(s_dma_dual1, end_word_pos, lane_sample_count);
        DSO_Acq8_SummarizeRingLowByte16(s_dma_dual1,
                                        DSO_DMA_BUF_SIZE,
                                        end_word_pos,
                                        lane_sample_count,
                                        out_vmin,
                                        out_vmax,
                                        out_mean);
    } else {
        DSO_AcqCommon_InvalidateDMAU16Recent(s_dma_lane2, end_word_pos, lane_sample_count);
        DSO_Acq8_SummarizeRingLowByte16(s_dma_lane2,
                                        DSO_DMA_BUF_SIZE,
                                        end_word_pos,
                                        lane_sample_count,
                                        out_vmin,
                                        out_vmax,
                                        out_mean);
    }
}

/*
*********************************************************************************************************
*   函 数 名: copy_interleaved_lane_window
*   功能说明: 在 8-bit 高速采集后端中完成 ADC/TIM/DMA 初始化、lane 同步、状态统计或样本拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t copy_interleaved_lane_window(uint8_t lane_index,
                                             uint32_t end_word_pos,
                                             uint16_t *out_samples,
                                             uint32_t max_samples,
                                             uint32_t *start_out)
{
    if ((lane_index > 2u) || (out_samples == NULL) || (max_samples == 0u)) {
        if (start_out != NULL) {
            *start_out = 0u;
        }
        return 0u;
    }

    if (lane_index == 0u) {
        return copy_ring_window_low_byte16(s_dma_single0,
                                           DSO_DMA_BUF_SIZE,
                                           end_word_pos,
                                           out_samples,
                                           max_samples,
                                           start_out);
    } else if (lane_index == 1u) {
        return copy_ring_window_low_byte16(s_dma_dual1,
                                           DSO_DMA_BUF_SIZE,
                                           end_word_pos,
                                           out_samples,
                                           max_samples,
                                           start_out);
    }

    return copy_ring_window_low_byte16(s_dma_lane2,
                                       DSO_DMA_BUF_SIZE,
                                       end_word_pos,
                                       out_samples,
                                       max_samples,
                                       start_out);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_Init
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq8_Init(void)
{
    memset(&s_acq8, 0, sizeof(s_acq8));
    memset(s_dma_single0, 0, sizeof(s_dma_single0));
    memset(s_dma_dual1, 0, sizeof(s_dma_dual1));
    memset(s_dma_lane2, 0, sizeof(s_dma_lane2));
    backend_reset_handles();
    s_acq8.requested_topology = sanitize_topology(DSO_ACQ8_TOPOLOGY_DEFAULT);
    s_acq8.active_topology = s_acq8.requested_topology;
    s_acq8.runtime.tim8_arr = DSO_ACQ8_DEFAULT_ARR;

    config_pll2();
    config_gpio();
    refresh_runtime_info();
    configure_active_backend();
    reset_common_tracking();
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_Deinit
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq8_Deinit(void)
{
    DSO_Acq8_Stop();
    backend_deinit_peripherals();
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_Service
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq8_Service(void)
{
    DSO_Acq8FaultPolicy fault_policy;

    if (!s_acq8.running) {
        return;
    }

    refresh_dma_pos_sample_rates();

    fault_policy = DSO_Acq8_GetFaultPolicy();
    if (fault_policy.auto_fallback_to_12bit &&
        (s_acq8.phase_sync_error_count >= fault_policy.phase_sync_error_limit)) {
        DSO_Acq_SetSampleFormat(DSO_SAMPLE_FORMAT_10BIT);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_Start
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq8_Start(void)
{
    if (s_acq8.running) {
        return;
    }

    start_active_streaming();
#if (Y_T_AB_DISABLE_ACQ_DMA_IRQ_AFTER_START != 0)
    HAL_NVIC_DisableIRQ(DMA1_Stream0_IRQn);
    HAL_NVIC_DisableIRQ(DMA1_Stream1_IRQn);
    HAL_NVIC_DisableIRQ(DMA1_Stream2_IRQn);
    HAL_NVIC_DisableIRQ(DMA2_Stream0_IRQn);
    HAL_NVIC_DisableIRQ(DMA2_Stream1_IRQn);
#endif
#if (Y_T_AB_DISABLE_ACQ_DMA_HT_TC_IRQ_SOURCE != 0)
    __HAL_DMA_DISABLE_IT(&s_acq8.hdma_adc1, DMA_IT_HT | DMA_IT_TC);
    __HAL_DMA_DISABLE_IT(&s_acq8.hdma_adc2, DMA_IT_HT | DMA_IT_TC);
    __HAL_DMA_DISABLE_IT(&s_acq8.hdma_adc3, DMA_IT_HT | DMA_IT_TC);
#endif
    s_acq8.running = true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_Stop
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq8_Stop(void)
{
    if (!s_acq8.running) {
        return;
    }

    stop_active_streaming();
    s_acq8.running = false;
    s_acq8.data_ready = false;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_IsRunning
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_Acq8_IsRunning(void)
{
    return s_acq8.running;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_IsDataReady
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_Acq8_IsDataReady(void)
{
    if (!s_acq8.running) {
        return false;
    }
    (void)DSO_Acq8_GetPos();
    return s_acq8.data_ready;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetPos
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq8_GetPos(void)
{
    if (s_acq8.running) {
        snapshot_common_position();
    }
    return s_acq8.snapshot_common_pos;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetBuf
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const volatile uint32_t *DSO_Acq8_GetBuf(void)
{
    return (const volatile uint32_t *)s_dma_single0;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_IsCh2Available
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_Acq8_IsCh2Available(void)
{
    return false;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetCh2Pos
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq8_GetCh2Pos(void)
{
    return 0u;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetCh2Buf
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const volatile uint16_t *DSO_Acq8_GetCh2Buf(void)
{
    return NULL;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetCalOffset
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
int16_t DSO_Acq8_GetCalOffset(void)
{
    return 0;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetSampleRate
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq8_GetSampleRate(void)
{
    return s_acq8.runtime.sample_rate_hz;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetCh2SampleRate
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq8_GetCh2SampleRate(void)
{
    return 0u;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetAdcKernelClockHz
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq8_GetAdcKernelClockHz(void)
{
    return s_acq8.runtime.adc_core_clk_hz;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetTopology
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_Acq8Topology DSO_Acq8_GetTopology(void)
{
    return s_acq8.active_topology;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetLaneCount
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t DSO_Acq8_GetLaneCount(void)
{
    return topology_lane_count(s_acq8.active_topology);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_RequestTopology
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_Acq8_RequestTopology(DSO_Acq8Topology topology)
{
    DSO_Acq8Topology next = sanitize_topology((uint32_t)topology);
    bool was_running = s_acq8.running;

    s_acq8.requested_topology = next;
    if (s_acq8.active_topology == next) {
        return true;
    }

    if (was_running) {
        stop_active_streaming();
        s_acq8.running = false;
    }

    backend_deinit_peripherals();
    s_acq8.active_topology = next;
    refresh_runtime_info();
    configure_active_backend();
    reset_common_tracking();

    if (was_running) {
        start_active_streaming();
        s_acq8.running = true;
    }

    return true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetRuntimeInfo
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const DSO_AcqRuntimeInfo *DSO_Acq8_GetRuntimeInfo(void)
{
    return &s_acq8.runtime;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_DMA_IRQHandler
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq8_DMA_IRQHandler(void)
{
    if (s_acq8.hdma_adc1.Instance != NULL) {
        HAL_DMA_IRQHandler(&s_acq8.hdma_adc1);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_DMA2_IRQHandler
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq8_DMA2_IRQHandler(void)
{
    if (s_acq8.hdma_adc2.Instance != NULL) {
        HAL_DMA_IRQHandler(&s_acq8.hdma_adc2);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_CH2_DMA_IRQHandler
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq8_CH2_DMA_IRQHandler(void)
{
    if (s_acq8.hdma_adc3.Instance != NULL) {
        HAL_DMA_IRQHandler(&s_acq8.hdma_adc3);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_HandleAdcHalfTransfer
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq8_HandleAdcHalfTransfer(ADC_TypeDef *instance)
{
    if (instance == ADC1) {
#if (Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE == 0)
        DSO_AcqCommon_InvalidateDCacheWindow(s_dma_single0, sizeof(s_dma_single0) / 2u);
#endif
        note_dma_half_transfer(0u);
        note_dma_samples(0u, DSO_DMA_BUF_SIZE / 2u);
    } else if (instance == ADC2) {
#if (Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE == 0)
        DSO_AcqCommon_InvalidateDCacheWindow(s_dma_dual1, sizeof(s_dma_dual1) / 2u);
#endif
        note_dma_half_transfer(1u);
        note_dma_samples(1u, DSO_DMA_BUF_SIZE / 2u);
    } else if (instance == ADC3) {
#if (Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE == 0)
        DSO_AcqCommon_InvalidateDCacheWindow(s_dma_lane2, sizeof(s_dma_lane2) / 2u);
#endif
        note_dma_half_transfer(2u);
        note_dma_samples(2u, DSO_DMA_BUF_SIZE / 2u);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_HandleAdcTransferComplete
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq8_HandleAdcTransferComplete(ADC_TypeDef *instance)
{
    if (instance == ADC1) {
#if (Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE == 0)
        DSO_AcqCommon_InvalidateDCacheWindow(&s_dma_single0[DSO_DMA_BUF_SIZE / 2u], sizeof(s_dma_single0) / 2u);
#endif
        note_dma_full_transfer(0u);
        note_dma_samples(0u, DSO_DMA_BUF_SIZE / 2u);
    } else if (instance == ADC2) {
#if (Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE == 0)
        DSO_AcqCommon_InvalidateDCacheWindow(&s_dma_dual1[DSO_DMA_BUF_SIZE / 2u], sizeof(s_dma_dual1) / 2u);
#endif
        note_dma_full_transfer(1u);
        note_dma_samples(1u, DSO_DMA_BUF_SIZE / 2u);
    } else if (instance == ADC3) {
#if (Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE == 0)
        DSO_AcqCommon_InvalidateDCacheWindow(&s_dma_lane2[DSO_DMA_BUF_SIZE / 2u], sizeof(s_dma_lane2) / 2u);
#endif
        note_dma_full_transfer(2u);
        note_dma_samples(2u, DSO_DMA_BUF_SIZE / 2u);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_CheckOverrun
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq8_CheckOverrun(void)
{
    if (ADC1->ISR & ADC_FLAG_OVR) {
        ADC1->ISR = ADC_FLAG_OVR;
        s_acq8.ovr_count_lane[0]++;
        s_acq8.ovr_count++;
    }
    if (ADC2->ISR & ADC_FLAG_OVR) {
        ADC2->ISR = ADC_FLAG_OVR;
        s_acq8.ovr_count_lane[1]++;
        s_acq8.ovr_count++;
    }
    if (ADC3->ISR & ADC_FLAG_OVR) {
        ADC3->ISR = ADC_FLAG_OVR;
        s_acq8.ovr_count_lane[2]++;
        s_acq8.ovr_count++;
    }
    return s_acq8.ovr_count;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetOverrunCount
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq8_GetOverrunCount(void)
{
    return s_acq8.ovr_count;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetPhaseSyncErrorCount
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq8_GetPhaseSyncErrorCount(void)
{
    return s_acq8.phase_sync_error_count;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetDebugSnapshot
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_Acq8_GetDebugSnapshot(DSO_AcqDebugSnapshot *out)
{
    const uint32_t window = 64u;
    TIM_TypeDef *tim = active_timer_instance();

    if (out == NULL) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->format = DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED;
    out->running = s_acq8.running;
    out->hs8_topology = (uint8_t)s_acq8.active_topology;
    out->hs8_lane_count = topology_lane_count(s_acq8.active_topology);
    out->hs8_lane2_active = true;
    out->ch2_valid = DSO_Acq8_IsCh2Available();
    out->ch2_capture_active = s_acq8.running && DSO_Acq8_IsCh2Available();
    out->sample_rate_hz = s_acq8.runtime.sample_rate_hz;
    out->ch2_sample_rate_hz = DSO_Acq8_GetCh2SampleRate();
    out->trigger_rate_hz = s_acq8.runtime.trigger_rate_hz;
    out->accepted_trigger_div = s_acq8.accepted_trigger_div;
    refresh_dma_sample_rates();
    if (s_acq8.running) {
        refresh_dma_pos_sample_rates();
    }
    out->lane0_dma_sample_count = s_acq8.dma_sample_count[0];
    out->lane1_dma_sample_count = s_acq8.dma_sample_count[1];
    out->lane2_dma_sample_count = s_acq8.dma_sample_count[2];
    out->lane0_dma_half_count = s_acq8.dma_half_count[0];
    out->lane1_dma_half_count = s_acq8.dma_half_count[1];
    out->lane2_dma_half_count = s_acq8.dma_half_count[2];
    out->lane0_dma_full_count = s_acq8.dma_full_count[0];
    out->lane1_dma_full_count = s_acq8.dma_full_count[1];
    out->lane2_dma_full_count = s_acq8.dma_full_count[2];
    out->lane0_dma_sample_rate_hz = s_acq8.dma_sample_rate_hz[0];
    out->lane1_dma_sample_rate_hz = s_acq8.dma_sample_rate_hz[1];
    out->lane2_dma_sample_rate_hz = s_acq8.dma_sample_rate_hz[2];
    out->lane0_dma_pos_sample_rate_hz = s_acq8.dma_pos_sample_rate_hz[0];
    out->lane1_dma_pos_sample_rate_hz = s_acq8.dma_pos_sample_rate_hz[1];
    out->lane2_dma_pos_sample_rate_hz = s_acq8.dma_pos_sample_rate_hz[2];
    out->dma_sample_count = s_acq8.dma_sample_count[0] +
                            s_acq8.dma_sample_count[1] +
                            s_acq8.dma_sample_count[2];
    out->dma_sample_rate_hz = s_acq8.dma_sample_rate_hz[0] +
                              s_acq8.dma_sample_rate_hz[1] +
                              s_acq8.dma_sample_rate_hz[2];
    out->dma_pos_sample_rate_hz = s_acq8.dma_pos_sample_rate_hz[0] +
                                  s_acq8.dma_pos_sample_rate_hz[1] +
                                  s_acq8.dma_pos_sample_rate_hz[2];
    out->dma_pos_rate_max_elapsed_ms = s_acq8.dma_pos_rate_report_max_elapsed_ms;
    out->adc_kernel_clock_hz = s_acq8.runtime.adc_core_clk_hz;
    out->ch2_count = 0u;
    out->phase_sync_errors = s_acq8.phase_sync_error_count;
    out->overrun_count = s_acq8.ovr_count;
    out->lane0_overrun_count = s_acq8.ovr_count_lane[0];
    out->lane1_overrun_count = s_acq8.ovr_count_lane[1];
    out->lane2_overrun_count = s_acq8.ovr_count_lane[2];

    if (s_acq8.running) {
        snapshot_common_position();
    }

    out->dma_pos = s_acq8.snapshot_common_pos;
    out->ch2_dma_pos = DSO_Acq8_GetCh2Pos();
    out->lane0_pos = s_acq8.snapshot_pos0;
    out->lane1_pos = s_acq8.snapshot_pos1;
    out->lane2_pos = s_acq8.snapshot_pos2;
    out->lane0_delta = s_acq8.snapshot_lane0_delta;
    out->lane1_delta = s_acq8.snapshot_lane1_delta;
    out->lane2_delta = s_acq8.snapshot_lane2_delta;
    out->common_delta = s_acq8.snapshot_common_delta;
    out->phase_sync_error = s_acq8.snapshot_phase_sync_error;
    out->data_ready = s_acq8.data_ready;
    out->data_ready_lane_count = s_acq8.data_ready_lane_count;
    out->tim_enabled = (tim->CR1 & TIM_CR1_CEN) != 0u;
    out->tim_counter = tim->CNT;
    out->tim_arr = tim->ARR;
    out->tim_ccr1 = tim->CCR1;
    out->tim_ccr2 = tim->CCR2;
    out->tim_ccr3 = tim->CCR3;
    out->tim_cr1 = tim->CR1;
    out->tim_ccmr1 = tim->CCMR1;
    out->tim_ccmr2 = tim->CCMR2;
    out->tim_ccer = tim->CCER;
    out->tim_bdtr = tim->BDTR;
    out->tim_sr = tim->SR;
    out->dma_ndtr0 = DSO_AcqCommon_DmaCounterOrZero(&s_acq8.hdma_adc1);
    out->dma_ndtr1 = DSO_AcqCommon_DmaCounterOrZero(&s_acq8.hdma_adc2);
    out->dma_ndtr2 = DSO_AcqCommon_DmaCounterOrZero(&s_acq8.hdma_adc3);
    out->adc1_cr = ADC1->CR;
    out->adc2_cr = ADC2->CR;
    out->adc3_cr = ADC3->CR;
    out->adc1_cfgr = ADC1->CFGR;
    out->adc2_cfgr = ADC2->CFGR;
    out->adc3_cfgr = ADC3->CFGR;
    out->adc1_isr = ADC1->ISR;
    out->adc2_isr = ADC2->ISR;
    out->adc3_isr = ADC3->ISR;

    summarize_interleaved_lane_window(0u,
                                      s_acq8.snapshot_common_pos,
                                      window,
                                      &out->lane0_vmin,
                                      &out->lane0_vmax,
                                      &out->lane0_mean);
    out->lane_window_count = copy_interleaved_lane_window(0u,
                                                          s_acq8.snapshot_common_pos,
                                                          out->lane0_window,
                                                          DSO_SERIAL_DEBUG_HS8_WINDOW_POINTS,
                                                          &out->lane0_window_start);
    out->lane_physical_window_count = copy_interleaved_lane_window(0u,
                                       s_acq8.snapshot_pos0,
                                       out->lane0_physical_window,
                                       DSO_SERIAL_DEBUG_HS8_WINDOW_POINTS,
                                       &out->lane0_physical_window_start);
    summarize_interleaved_lane_window(1u,
                                      s_acq8.snapshot_common_pos,
                                      window,
                                      &out->lane1_vmin,
                                      &out->lane1_vmax,
                                      &out->lane1_mean);
    (void)copy_interleaved_lane_window(1u,
                                       s_acq8.snapshot_common_pos,
                                       out->lane1_window,
                                       out->lane_window_count,
                                       &out->lane1_window_start);
    (void)copy_interleaved_lane_window(1u,
                                       s_acq8.snapshot_pos1,
                                       out->lane1_physical_window,
                                       out->lane_physical_window_count,
                                       &out->lane1_physical_window_start);
    summarize_interleaved_lane_window(2u,
                                      s_acq8.snapshot_common_pos,
                                      window,
                                      &out->lane2_vmin,
                                      &out->lane2_vmax,
                                      &out->lane2_mean);
    (void)copy_interleaved_lane_window(2u,
                                       s_acq8.snapshot_common_pos,
                                       out->lane2_window,
                                       out->lane_window_count,
                                       &out->lane2_window_start);
    (void)copy_interleaved_lane_window(2u,
                                       s_acq8.snapshot_pos2,
                                       out->lane2_physical_window,
                                       out->lane_physical_window_count,
                                       &out->lane2_physical_window_start);
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetTimClkHz
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq8_GetTimClkHz(void)
{
    return s_acq8.runtime.tim8_clk_hz;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetTimArr
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq8_GetTimArr(void)
{
    return s_acq8.runtime.tim8_arr;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_SetTimArr
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq8_SetTimArr(uint32_t new_arr)
{
    s_acq8.runtime.tim8_arr = new_arr;
    refresh_runtime_info();

    if (s_acq8.htim1.Instance != NULL) {
        TIM1->CR1 |= TIM_CR1_ARPE;
        TIM1->ARR = new_arr;
        TIM1->CCR1 = tim1_cc_pulse(s_acq8.runtime.period_clocks, 0u);
        TIM1->CCR2 = tim1_cc_pulse(s_acq8.runtime.period_clocks, 1u);
        TIM1->CCR3 = tim1_cc_pulse(s_acq8.runtime.period_clocks, 2u);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_CopyRecentSamples
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq8_CopyRecentSamples(uint32_t dma_pos,
                                    uint16_t *out_samples,
                                    uint32_t max_samples)
{
    uint32_t common_end;
    DSO_LaneMergeWindow window;
    DSO_Acq8U16DecodeMode mode0;
    DSO_Acq8U16DecodeMode mode1;
    DSO_Acq8U16DecodeMode mode2;

    if ((out_samples == NULL) || (max_samples == 0u)) {
        return 0u;
    }

    if (dma_pos != s_acq8.snapshot_common_pos) {
        DSO_Acq8_GetPos();
    }
    common_end = s_acq8.snapshot_common_pos;

    window = DSO_LaneMerge_ResolveCommonRecentWindow3(common_end,
                                                      max_samples / 3u,
                                                      DSO_DMA_BUF_SIZE);
    if (window.lane_count == 0u) {
        return 0u;
    }

    DSO_AcqCommon_InvalidateDMAU16Window(s_dma_single0, window.start0, window.lane_count);
    DSO_AcqCommon_InvalidateDMAU16Window(s_dma_dual1, window.start1, window.lane_count);
    DSO_AcqCommon_InvalidateDMAU16Window(s_dma_lane2, window.start2, window.lane_count);
    mode0 = DSO_Acq8_DetectU16DecodeModeRing(s_dma_single0, DSO_DMA_BUF_SIZE, common_end, window.lane_count);
    mode1 = DSO_Acq8_DetectU16DecodeModeRing(s_dma_dual1, DSO_DMA_BUF_SIZE, common_end, window.lane_count);
    mode2 = DSO_Acq8_DetectU16DecodeModeRing(s_dma_lane2, DSO_DMA_BUF_SIZE, common_end, window.lane_count);
    for (uint32_t i = 0u; i < window.lane_count; ++i) {
        out_samples[3u * i] = DSO_Acq8_DecodeU16Sample(s_dma_single0[(window.start0 + i) % DSO_DMA_BUF_SIZE], mode0);
        out_samples[(3u * i) + 1u] = DSO_Acq8_DecodeU16Sample(s_dma_dual1[(window.start1 + i) % DSO_DMA_BUF_SIZE], mode1);
        out_samples[(3u * i) + 2u] = DSO_Acq8_DecodeU16Sample(s_dma_lane2[(window.start2 + i) % DSO_DMA_BUF_SIZE], mode2);
    }
    return 3u * window.lane_count;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_CopyNewSamples
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq8_CopyNewSamples(uint32_t prev_dma_pos,
                                 uint32_t dma_pos,
                                 uint16_t *out_samples,
                                 uint32_t max_samples)
{
    uint32_t common_end;
    DSO_LaneMergeWindow window;
    DSO_Acq8U16DecodeMode mode0;
    DSO_Acq8U16DecodeMode mode1;
    DSO_Acq8U16DecodeMode mode2;

    if ((out_samples == NULL) || (max_samples == 0u)) {
        return 0u;
    }

    if (dma_pos != s_acq8.snapshot_common_pos) {
        DSO_Acq8_GetPos();
    }

    common_end = s_acq8.snapshot_common_pos;

    window = DSO_LaneMerge_ResolveCommonNewWindow3(prev_dma_pos,
                                                   s_acq8.snapshot_common_pos,
                                                   max_samples / 3u,
                                                   DSO_DMA_BUF_SIZE);
    if (window.lane_count == 0u) {
        return 0u;
    }

    DSO_AcqCommon_InvalidateDMAU16Window(s_dma_single0, window.start0, window.lane_count);
    DSO_AcqCommon_InvalidateDMAU16Window(s_dma_dual1, window.start1, window.lane_count);
    DSO_AcqCommon_InvalidateDMAU16Window(s_dma_lane2, window.start2, window.lane_count);
    mode0 = DSO_Acq8_DetectU16DecodeModeRing(s_dma_single0, DSO_DMA_BUF_SIZE, common_end, window.lane_count);
    mode1 = DSO_Acq8_DetectU16DecodeModeRing(s_dma_dual1, DSO_DMA_BUF_SIZE, common_end, window.lane_count);
    mode2 = DSO_Acq8_DetectU16DecodeModeRing(s_dma_lane2, DSO_DMA_BUF_SIZE, common_end, window.lane_count);
    for (uint32_t i = 0u; i < window.lane_count; ++i) {
        out_samples[3u * i] = DSO_Acq8_DecodeU16Sample(s_dma_single0[(window.start0 + i) % DSO_DMA_BUF_SIZE], mode0);
        out_samples[(3u * i) + 1u] = DSO_Acq8_DecodeU16Sample(s_dma_dual1[(window.start1 + i) % DSO_DMA_BUF_SIZE], mode1);
        out_samples[(3u * i) + 2u] = DSO_Acq8_DecodeU16Sample(s_dma_lane2[(window.start2 + i) % DSO_DMA_BUF_SIZE], mode2);
    }
    return 3u * window.lane_count;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_CopyRecentCh2Samples
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq8_CopyRecentCh2Samples(uint32_t dma_pos,
                                       uint16_t *out_samples,
                                       uint32_t max_samples)
{
    (void)dma_pos;
    (void)out_samples;
    (void)max_samples;
    return 0u;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_CopyNewCh2Samples
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq8_CopyNewCh2Samples(uint32_t prev_dma_pos,
                                    uint32_t dma_pos,
                                    uint16_t *out_samples,
                                    uint32_t max_samples)
{
    (void)prev_dma_pos;
    (void)dma_pos;
    (void)out_samples;
    (void)max_samples;
    return 0u;
}
