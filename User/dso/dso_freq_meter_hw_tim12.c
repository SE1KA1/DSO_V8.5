/**
 * @file dso_freq_meter_hw_tim12.c
 * @brief TIM12 IRQ、input capture、external clock 实现。
 */

#include "dso_freq_meter_hw_tim12.h"
#include "dso_freq_meter_math.h"

#include "stm32h7xx_hal.h"

/* 宏定义: DSO_FREQ_METER_CAPTURE_MIN_TICKS - 计数、阈值、容量或深度配置常量。 */
#define DSO_FREQ_METER_CAPTURE_MIN_TICKS 5000ull

static volatile uint32_t s_overflow_count;
static volatile bool s_capture_overrun;
static volatile bool s_capture_have_prev;
static volatile bool s_capture_ready;
static volatile uint64_t s_capture_prev_tick;
static volatile uint64_t s_capture_ready_ticks;
static volatile uint32_t s_capture_ready_periods;
static volatile uint32_t s_capture_irq_count;
static volatile uint32_t s_update_irq_count;

/*
*********************************************************************************************************
*   函 数 名: irq_save
*   功能说明: 在外部频率计模块中完成计数扩展、频率换算、状态服务或显示数据更新。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t irq_save(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

/*
*********************************************************************************************************
*   函 数 名: irq_restore
*   功能说明: 在外部频率计模块中完成计数扩展、频率换算、状态服务或显示数据更新。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void irq_restore(uint32_t primask)
{
    if (primask == 0u) {
        __enable_irq();
    }
}

/*
*********************************************************************************************************
*   函 数 名: tim8_clock_enabled
*   功能说明: 在外部频率计模块中完成计数扩展、频率换算、状态服务或显示数据更新。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static bool tim8_clock_enabled(void)
{
    return (RCC->APB2ENR & RCC_APB2ENR_TIM8EN) != 0u;
}

/*
*********************************************************************************************************
*   函 数 名: clear_tim8_break_flags_if_any
*   功能说明: 在外部频率计模块中完成计数扩展、频率换算、状态服务或显示数据更新。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void clear_tim8_break_flags_if_any(void)
{
    if (tim8_clock_enabled()) {
        uint32_t flags = TIM8->SR & (TIM_SR_BIF | TIM_SR_B2IF | TIM_SR_SBIF);
        if (flags != 0u) {
            TIM8->SR = ~flags;
        }
    }
}

/*
*********************************************************************************************************
*   函 数 名: reset_capture_accum
*   功能说明: 在外部频率计模块中完成计数扩展、频率换算、状态服务或显示数据更新。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void reset_capture_accum(void)
{
    s_capture_overrun = false;
    s_capture_have_prev = false;
    s_capture_ready = false;
    s_capture_prev_tick = 0ull;
    s_capture_ready_ticks = 0ull;
    s_capture_ready_periods = 0u;
    s_capture_irq_count = 0u;
    s_update_irq_count = 0u;
}

/*
*********************************************************************************************************
*   函 数 名: disable_capture_interrupt
*   功能说明: 在外部频率计模块中完成计数扩展、频率换算、状态服务或显示数据更新。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void disable_capture_interrupt(void)
{
    TIM12->DIER &= ~TIM_DIER_CC2IE;
    TIM12->CCER &= ~TIM_CCER_CC2E;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterHwTim12_InitGpio
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeterHwTim12_InitGpio(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_15;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF2_TIM12;
    HAL_GPIO_Init(GPIOB, &gpio);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterHwTim12_ConfigureCaptureMode
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeterHwTim12_ConfigureCaptureMode(uint16_t prescaler)
{
    uint32_t ccmr1;

    __HAL_RCC_TIM12_CLK_ENABLE();
    __HAL_RCC_TIM12_FORCE_RESET();
    __HAL_RCC_TIM12_RELEASE_RESET();

    TIM12->CR1 = 0u;
    TIM12->CR2 = 0u;
    TIM12->SMCR = 0u;
    TIM12->DIER = 0u;
    TIM12->CCER = 0u;
    TIM12->PSC = prescaler;
    TIM12->ARR = 0xFFFFu;
    TIM12->CNT = 0u;

    ccmr1 = TIM12->CCMR1;
    ccmr1 &= ~(TIM_CCMR1_CC2S | TIM_CCMR1_IC2PSC | TIM_CCMR1_IC2F);
    ccmr1 |= TIM_CCMR1_CC2S_0;
    TIM12->CCMR1 = ccmr1;
    TIM12->CCER &= ~(TIM_CCER_CC2P | TIM_CCER_CC2NP);
    TIM12->CCER |= TIM_CCER_CC2E;
    TIM12->DIER = TIM_DIER_UIE | TIM_DIER_CC2IE;
    TIM12->SR = 0u;
    TIM12->EGR = TIM_EGR_UG;
    TIM12->SR = 0u;
    TIM12->CR1 = TIM_CR1_ARPE;

    s_overflow_count = 0u;
    reset_capture_accum();

    NVIC_ClearPendingIRQ(TIM8_BRK_TIM12_IRQn);
    clear_tim8_break_flags_if_any();
    HAL_NVIC_SetPriority(TIM8_BRK_TIM12_IRQn, 7u, 0u);
    HAL_NVIC_EnableIRQ(TIM8_BRK_TIM12_IRQn);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterHwTim12_ConfigureCounterMode
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeterHwTim12_ConfigureCounterMode(void)
{
    uint32_t ccmr1;

    __HAL_RCC_TIM12_CLK_ENABLE();
    __HAL_RCC_TIM12_FORCE_RESET();
    __HAL_RCC_TIM12_RELEASE_RESET();

    TIM12->CR1 = 0u;
    TIM12->CR2 = 0u;
    TIM12->SMCR = 0u;
    TIM12->DIER = 0u;
    TIM12->CCER = 0u;
    TIM12->PSC = 0u;
    TIM12->ARR = 0xFFFFu;
    TIM12->CNT = 0u;

    ccmr1 = TIM12->CCMR1;
    ccmr1 &= ~(TIM_CCMR1_CC2S | TIM_CCMR1_IC2PSC | TIM_CCMR1_IC2F);
    ccmr1 |= TIM_CCMR1_CC2S_0;
    TIM12->CCMR1 = ccmr1;
    TIM12->CCER &= ~(TIM_CCER_CC2E | TIM_CCER_CC2P | TIM_CCER_CC2NP);
    TIM12->SMCR = TIM_TS_TI2FP2 | TIM_SLAVEMODE_EXTERNAL1;
    TIM12->DIER = TIM_DIER_UIE;
    TIM12->SR = 0u;
    TIM12->CR1 = TIM_CR1_ARPE;

    s_overflow_count = 0u;
    reset_capture_accum();

    NVIC_ClearPendingIRQ(TIM8_BRK_TIM12_IRQn);
    clear_tim8_break_flags_if_any();
    HAL_NVIC_SetPriority(TIM8_BRK_TIM12_IRQn, 7u, 0u);
    HAL_NVIC_EnableIRQ(TIM8_BRK_TIM12_IRQn);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterHwTim12_Start
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeterHwTim12_Start(void)
{
    uint32_t primask = irq_save();

    TIM12->CNT = 0u;
    TIM12->SR = 0u;
    s_overflow_count = 0u;
    reset_capture_accum();
    TIM12->CR1 |= TIM_CR1_CEN;

    irq_restore(primask);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterHwTim12_Stop
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeterHwTim12_Stop(void)
{
    TIM12->CR1 &= ~TIM_CR1_CEN;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterHwTim12_LatchAndResetCounter
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_FreqMeterHwCounterSnapshot DSO_FreqMeterHwTim12_LatchAndResetCounter(void)
{
    DSO_FreqMeterHwCounterSnapshot out;
    uint32_t primask = irq_save();

    TIM12->CR1 &= ~TIM_CR1_CEN;
    out.overflow_count = s_overflow_count;
    out.counter = (uint16_t)TIM12->CNT;
    out.pending_update = (TIM12->SR & TIM_SR_UIF) != 0u;
    if (out.pending_update) {
        out.overflow_count++;
    }

    s_overflow_count = 0u;
    TIM12->CNT = 0u;
    TIM12->SR = 0u;
    NVIC_ClearPendingIRQ(TIM8_BRK_TIM12_IRQn);
    TIM12->CR1 |= TIM_CR1_CEN;

    irq_restore(primask);
    return out;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterHwTim12_TakeCaptureResult
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_FreqMeterHwCaptureResult DSO_FreqMeterHwTim12_TakeCaptureResult(void)
{
    DSO_FreqMeterHwCaptureResult out;
    uint32_t primask = irq_save();

    out.ready = s_capture_ready;
    out.overcapture = s_capture_overrun;
    out.ticks = s_capture_ready_ticks;
    out.periods = s_capture_ready_periods;
    out.overflow_count = s_overflow_count;
    out.capture_irq_count = s_capture_irq_count;
    out.update_irq_count = s_update_irq_count;
    s_capture_ready = false;
    s_capture_overrun = false;

    irq_restore(primask);
    return out;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterHwTim12_ReadDebug
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeterHwTim12_ReadDebug(DSO_FreqMeterHwTim12DebugSnapshot *out)
{
    uint32_t primask;

    if (out == NULL) {
        return;
    }

    primask = irq_save();
    out->cr1 = TIM12->CR1;
    out->smcr = TIM12->SMCR;
    out->dier = TIM12->DIER;
    out->sr = TIM12->SR;
    out->ccmr1 = TIM12->CCMR1;
    out->ccer = TIM12->CCER;
    out->counter = (uint16_t)TIM12->CNT;
    out->capture = (uint16_t)TIM12->CCR2;
    out->overflow_count = s_overflow_count;
    out->pending_update = (TIM12->SR & TIM_SR_UIF) != 0u;
    out->capture_ready = s_capture_ready;
    out->capture_overrun = s_capture_overrun || ((TIM12->SR & TIM_SR_CC2OF) != 0u);
    out->capture_ticks = s_capture_ready_ticks;
    out->capture_periods = s_capture_ready_periods;
    out->capture_irq_count = s_capture_irq_count;
    out->update_irq_count = s_update_irq_count;
    irq_restore(primask);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeterHwTim12_IRQHandler
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeterHwTim12_IRQHandler(void)
{
    uint32_t sr = TIM12->SR;
    uint32_t dier = TIM12->DIER;
    uint32_t clear = 0u;

    if (((sr & TIM_SR_CC2IF) != 0u) && ((dier & TIM_DIER_CC2IE) != 0u)) {
        uint16_t capture = (uint16_t)TIM12->CCR2;
        bool pending_update = (sr & TIM_SR_UIF) != 0u;
        uint64_t tick = DSO_FreqMeterMath_ExtendCapture(s_overflow_count,
                                                        capture,
                                                        pending_update);

        s_capture_irq_count++;
        if (s_capture_have_prev) {
            uint64_t delta = tick - s_capture_prev_tick;
            if ((delta == 0ull) || (delta < DSO_FREQ_METER_CAPTURE_MIN_TICKS)) {
                s_capture_overrun = true;
                disable_capture_interrupt();
            } else {
                s_capture_ready_ticks = delta;
                s_capture_ready_periods = 1u;
                s_capture_ready = true;
            }
        }
        s_capture_prev_tick = tick;
        s_capture_have_prev = true;
        clear |= TIM_SR_CC2IF;
    }

    if (((sr & TIM_SR_UIF) != 0u) && ((dier & TIM_DIER_UIE) != 0u)) {
        s_overflow_count++;
        s_update_irq_count++;
        clear |= TIM_SR_UIF;
    }

    if ((sr & TIM_SR_CC2OF) != 0u) {
        s_capture_overrun = true;
        disable_capture_interrupt();
        clear |= TIM_SR_CC2OF;
    }

    if (clear != 0u) {
        TIM12->SR = ~clear;
    }
}

/*
*********************************************************************************************************
*   函 数 名: TIM8_BRK_TIM12_IRQHandler
*   功能说明: 在外部频率计模块中完成计数扩展、频率换算、状态服务或显示数据更新。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void TIM8_BRK_TIM12_IRQHandler(void)
{
    DSO_FreqMeterHwTim12_IRQHandler();
    clear_tim8_break_flags_if_any();
}
