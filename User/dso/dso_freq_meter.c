/**
 * @file dso_freq_meter.c
 * @brief FreqMeter Init/Start/Service 与帧字段更新。
 */

#include "dso_freq_meter.h"
#include "dso_freq_meter_hw_tim12.h"

#include "stm32h7xx_hal.h"

#include <string.h>

/* 宏定义: DSO_FREQ_METER_NO_SIGNAL_MS - 时间间隔、周期或时基相关配置常量。 */
#define DSO_FREQ_METER_NO_SIGNAL_MS       2500u
/* 宏定义: DSO_FREQ_METER_DEFAULT_GATE_US - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_FREQ_METER_DEFAULT_GATE_US    100000u
/* 宏定义: DSO_FREQ_METER_CAPTURE_TARGET_HZ - 计数、阈值、容量或深度配置常量。 */
#define DSO_FREQ_METER_CAPTURE_TARGET_HZ  10000000u

typedef struct {
    bool initialized;
    bool running;
    DSO_FreqMeterState state;
    DSO_FreqMeterMode mode;
    uint32_t core_hz;
    uint32_t timer_hz;
    uint32_t capture_timer_hz;
    uint16_t capture_prescaler;
    uint32_t last_edge_ms;
    uint32_t mode_enter_ms;
    uint32_t gate_us;
    uint32_t gate_start_cycles;
    uint64_t freq_millihz;
    uint32_t edge_count;
    uint32_t ref_ticks;
    uint32_t overflow_count;
    bool overrange;
} DSO_FreqMeterStateData;

static DSO_FreqMeterStateData s_meter;

/*
*********************************************************************************************************
*   函 数 名: dwt_counter_enable_if_needed
*   功能说明: 在外部频率计模块中完成计数扩展、频率换算、状态服务或显示数据更新。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void dwt_counter_enable_if_needed(void)
{
    if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) == 0u) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    }
    if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0u) {
        DWT->CYCCNT = 0u;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
}

/*
*********************************************************************************************************
*   函 数 名: dwt_cycles_now
*   功能说明: 在外部频率计模块中完成计数扩展、频率换算、状态服务或显示数据更新。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t dwt_cycles_now(void)
{
    dwt_counter_enable_if_needed();
    return DWT->CYCCNT;
}

/*
*********************************************************************************************************
*   函 数 名: current_core_clock_hz
*   功能说明: 在外部频率计模块中完成计数扩展、频率换算、状态服务或显示数据更新。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t current_core_clock_hz(void)
{
    uint32_t hz;

    SystemCoreClockUpdate();
    hz = SystemCoreClock;
    if (hz == 0u) {
        hz = HAL_RCC_GetSysClockFreq();
    }
    return hz;
}

/*
*********************************************************************************************************
*   函 数 名: current_tim12_clock_hz
*   功能说明: 在外部频率计模块中完成计数扩展、频率换算、状态服务或显示数据更新。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t current_tim12_clock_hz(void)
{
    uint32_t pclk = HAL_RCC_GetPCLK1Freq();
    uint32_t ppre = RCC->D2CFGR & RCC_D2CFGR_D2PPRE1;

    if (pclk == 0u) {
        return current_core_clock_hz() / 2u;
    }
    if (ppre != RCC_D2CFGR_D2PPRE1_DIV1) {
        pclk *= 2u;
    }
    return pclk;
}

/*
*********************************************************************************************************
*   函 数 名: capture_prescaler_for_timer
*   功能说明: 在外部频率计模块中完成计数扩展、频率换算、状态服务或显示数据更新。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t capture_prescaler_for_timer(uint32_t timer_hz,
                                            uint32_t *actual_hz)
{
    uint32_t div;

    if (timer_hz < DSO_FREQ_METER_CAPTURE_TARGET_HZ) {
        if (actual_hz != NULL) {
            *actual_hz = timer_hz;
        }
        return 0u;
    }

    div = timer_hz / DSO_FREQ_METER_CAPTURE_TARGET_HZ;
    if (div == 0u) {
        div = 1u;
    }
    if (div > 0x10000u) {
        div = 0x10000u;
    }
    if (actual_hz != NULL) {
        *actual_hz = timer_hz / div;
    }
    return (uint16_t)(div - 1u);
}

/*
*********************************************************************************************************
*   函 数 名: reset_measurement_fields
*   功能说明: 在外部频率计模块中完成计数扩展、频率换算、状态服务或显示数据更新。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void reset_measurement_fields(uint32_t now_ms)
{
    s_meter.state = DSO_FREQ_METER_STATE_WAIT_EDGE;
    s_meter.last_edge_ms = now_ms;
    s_meter.mode_enter_ms = now_ms;
    s_meter.gate_start_cycles = dwt_cycles_now();
    s_meter.freq_millihz = 0ull;
    s_meter.edge_count = 0u;
    s_meter.ref_ticks = 0u;
    s_meter.overflow_count = 0u;
    s_meter.overrange = false;
}

/*
*********************************************************************************************************
*   函 数 名: enter_low_capture_mode
*   功能说明: 在外部频率计模块中完成计数扩展、频率换算、状态服务或显示数据更新。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void enter_low_capture_mode(uint32_t now_ms)
{
    DSO_FreqMeterHwTim12_Stop();
    DSO_FreqMeterHwTim12_ConfigureCaptureMode(s_meter.capture_prescaler);
    DSO_FreqMeterHwTim12_Start();

    reset_measurement_fields(now_ms);
    s_meter.mode = DSO_FREQ_METER_MODE_LOW_CAPTURE;
    s_meter.gate_us = 0u;
}

/*
*********************************************************************************************************
*   函 数 名: enter_gated_mode
*   功能说明: 在外部频率计模块中完成计数扩展、频率换算、状态服务或显示数据更新。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void enter_gated_mode(uint32_t now_ms)
{
    DSO_FreqMeterHwTim12_Stop();
    DSO_FreqMeterHwTim12_ConfigureCounterMode();
    DSO_FreqMeterHwTim12_Start();

    reset_measurement_fields(now_ms);
    s_meter.mode = DSO_FREQ_METER_MODE_GATED_COUNT;
    s_meter.state = DSO_FREQ_METER_STATE_MEASURING;
    s_meter.gate_us = DSO_FREQ_METER_DEFAULT_GATE_US;
    s_meter.gate_start_cycles = dwt_cycles_now();
}

/*
*********************************************************************************************************
*   函 数 名: mark_no_signal
*   功能说明: 在外部频率计模块中完成计数扩展、频率换算、状态服务或显示数据更新。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void mark_no_signal(uint32_t now_ms)
{
    if ((uint32_t)(now_ms - s_meter.last_edge_ms) > DSO_FREQ_METER_NO_SIGNAL_MS) {
        s_meter.state = DSO_FREQ_METER_STATE_NO_SIGNAL;
        s_meter.freq_millihz = 0ull;
    }
}

/*
*********************************************************************************************************
*   函 数 名: service_low_capture
*   功能说明: 在外部频率计模块中完成计数扩展、频率换算、状态服务或显示数据更新。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void service_low_capture(uint32_t now_ms)
{
    DSO_FreqMeterHwCaptureResult result = DSO_FreqMeterHwTim12_TakeCaptureResult();

    if (result.overcapture) {
        s_meter.state = DSO_FREQ_METER_STATE_OVERRANGE;
        s_meter.freq_millihz = 0ull;
        s_meter.overrange = true;
        return;
    }

    if (!result.ready) {
        mark_no_signal(now_ms);
        return;
    }

    s_meter.last_edge_ms = now_ms;
    s_meter.overflow_count = result.overflow_count;
    s_meter.edge_count = result.periods;
    s_meter.ref_ticks = (result.ticks > 0xFFFFFFFFull)
                      ? 0xFFFFFFFFu
                      : (uint32_t)result.ticks;
    s_meter.freq_millihz = DSO_FreqMeterMath_PeriodToMilliHz(s_meter.capture_timer_hz,
                                                             result.periods,
                                                             result.ticks);
    s_meter.state = (s_meter.freq_millihz == 0ull)
                  ? DSO_FREQ_METER_STATE_FAULT
                  : DSO_FREQ_METER_STATE_VALID;
}

/*
*********************************************************************************************************
*   函 数 名: service_gated
*   功能说明: 在外部频率计模块中完成计数扩展、频率换算、状态服务或显示数据更新。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void service_gated(uint32_t now_ms)
{
    uint32_t now_cycles = dwt_cycles_now();
    uint32_t elapsed_cycles = now_cycles - s_meter.gate_start_cycles;
    uint64_t elapsed_us;

    if (s_meter.core_hz == 0u) {
        s_meter.state = DSO_FREQ_METER_STATE_FAULT;
        s_meter.freq_millihz = 0ull;
        return;
    }

    elapsed_us = ((uint64_t)elapsed_cycles * 1000000ull) / (uint64_t)s_meter.core_hz;
    if (elapsed_us < (uint64_t)s_meter.gate_us) {
        mark_no_signal(now_ms);
        return;
    }

    {
        DSO_FreqMeterHwCounterSnapshot snap = DSO_FreqMeterHwTim12_LatchAndResetCounter();
        uint64_t delta_edges = DSO_FreqMeterMath_Extend16(snap.overflow_count,
                                                          snap.counter);

        s_meter.edge_count = (delta_edges > 0xFFFFFFFFull)
                           ? 0xFFFFFFFFu
                           : (uint32_t)delta_edges;
        s_meter.overflow_count = snap.overflow_count;
        s_meter.ref_ticks = elapsed_cycles;
        s_meter.freq_millihz = DSO_FreqMeterMath_GateToMilliHz(delta_edges,
                                                               (uint32_t)elapsed_us);
        s_meter.state = (delta_edges == 0ull)
                      ? DSO_FREQ_METER_STATE_NO_SIGNAL
                      : DSO_FREQ_METER_STATE_VALID;
        if (delta_edges != 0ull) {
            s_meter.last_edge_ms = now_ms;
        }
        s_meter.gate_start_cycles = now_cycles;
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeter_Init
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeter_Init(void)
{
    if (s_meter.initialized) {
        return;
    }

    memset(&s_meter, 0, sizeof(s_meter));
    s_meter.core_hz = current_core_clock_hz();
    s_meter.timer_hz = current_tim12_clock_hz();
    s_meter.capture_prescaler = capture_prescaler_for_timer(s_meter.timer_hz,
                                                            &s_meter.capture_timer_hz);
    s_meter.state = DSO_FREQ_METER_STATE_STOPPED;
    s_meter.mode = DSO_FREQ_METER_MODE_GATED_COUNT;
    s_meter.gate_us = DSO_FREQ_METER_DEFAULT_GATE_US;

    DSO_FreqMeterHwTim12_InitGpio();
    s_meter.initialized = true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeter_Start
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeter_Start(void)
{
    uint32_t now_ms;

    if (!s_meter.initialized) {
        DSO_FreqMeter_Init();
    }

    now_ms = HAL_GetTick();
    s_meter.running = true;
    s_meter.core_hz = current_core_clock_hz();
    s_meter.timer_hz = current_tim12_clock_hz();
    if (s_meter.core_hz == 0u) {
        s_meter.core_hz = 1u;
    }
    if (s_meter.timer_hz == 0u) {
        s_meter.timer_hz = 1u;
    }
    s_meter.capture_prescaler = capture_prescaler_for_timer(s_meter.timer_hz,
                                                            &s_meter.capture_timer_hz);
    enter_gated_mode(now_ms);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeter_Stop
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeter_Stop(void)
{
    DSO_FreqMeterHwTim12_Stop();
    s_meter.running = false;
    s_meter.state = DSO_FREQ_METER_STATE_STOPPED;
    s_meter.freq_millihz = 0ull;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeter_Service
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeter_Service(uint32_t now_ms)
{
    DSO_FreqMeterMode next_mode;

    if (!s_meter.running) {
        return;
    }

    if (s_meter.mode == DSO_FREQ_METER_MODE_LOW_CAPTURE) {
        service_low_capture(now_ms);
    } else {
        service_gated(now_ms);
    }

    if (s_meter.state == DSO_FREQ_METER_STATE_OVERRANGE) {
        enter_gated_mode(now_ms);
        return;
    }

    if (s_meter.state != DSO_FREQ_METER_STATE_VALID) {
        return;
    }

    next_mode = DSO_FreqMeterMath_SelectMode(s_meter.mode,
                                             s_meter.freq_millihz,
                                             s_meter.edge_count);
    if (next_mode == s_meter.mode) {
        return;
    }

    if (next_mode == DSO_FREQ_METER_MODE_LOW_CAPTURE) {
        enter_low_capture_mode(now_ms);
    } else {
        enter_gated_mode(now_ms);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeter_GetSnapshot
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_FreqMeter_GetSnapshot(DSO_FreqMeterSnapshot *out)
{
    uint32_t now_ms;

    if (out == NULL) {
        return false;
    }

    now_ms = HAL_GetTick();
    out->valid = s_meter.running && (s_meter.state == DSO_FREQ_METER_STATE_VALID);
    out->state = s_meter.running ? s_meter.state : DSO_FREQ_METER_STATE_STOPPED;
    out->mode = s_meter.mode;
    out->freq_millihz = out->valid ? s_meter.freq_millihz : 0ull;
    out->gate_us = s_meter.gate_us;
    out->edge_count = s_meter.edge_count;
    out->ref_ticks = s_meter.ref_ticks;
    out->overflow_count = s_meter.overflow_count;
    out->last_edge_age_ms = now_ms - s_meter.last_edge_ms;
    out->overrange = s_meter.overrange;

    return out->valid;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_FreqMeter_TIM12_IRQHandler
*   功能说明: 处理外部硬件频率计的计数换算、模式选择、状态服务或显示数据输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_FreqMeter_TIM12_IRQHandler(void)
{
    DSO_FreqMeterHwTim12_IRQHandler();
}
