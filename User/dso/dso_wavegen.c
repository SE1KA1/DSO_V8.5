/**
 * @file dso_wavegen.c
 * @brief Wavegen FreeRTOS 任务与 profile 调度。
 */

#include "dso_wavegen.h"

#include "dso_config.h"

#include <stddef.h>

static const DSO_WavegenProfile s_profiles[] = {
    {DSO_WAVEGEN_WAVE_KIND_SINE, "sine", 468000u, 32u, DSO_WAVEGEN_SQUARE_DUTY_PERMILLE},
    {DSO_WAVEGEN_WAVE_KIND_SINE, "sine", 937000u, 16u, DSO_WAVEGEN_SQUARE_DUTY_PERMILLE},
    {DSO_WAVEGEN_WAVE_KIND_SINE, "sine", 1500000u, 10u, DSO_WAVEGEN_SQUARE_DUTY_PERMILLE},
    {DSO_WAVEGEN_WAVE_KIND_TRIANGLE, "triangle", 750000u, 20u, DSO_WAVEGEN_SQUARE_DUTY_PERMILLE},
    {DSO_WAVEGEN_WAVE_KIND_TRIANGLE, "triangle", 468000u, 32u, DSO_WAVEGEN_SQUARE_DUTY_PERMILLE},
    {DSO_WAVEGEN_WAVE_KIND_SQUARE, "square", 7500000u, 2u, DSO_WAVEGEN_SQUARE_DUTY_PERMILLE},
    {DSO_WAVEGEN_WAVE_KIND_SQUARE, "square", 1000u, 2u, DSO_WAVEGEN_SQUARE_DUTY_PERMILLE},
    {DSO_WAVEGEN_WAVE_KIND_SQUARE, "square", 100000u, 2u, DSO_WAVEGEN_SQUARE_DUTY_PERMILLE},
    {DSO_WAVEGEN_WAVE_KIND_SQUARE, "square", 500000u, 2u, DSO_WAVEGEN_SQUARE_DUTY_PERMILLE},
    {DSO_WAVEGEN_WAVE_KIND_SQUARE, "square", 1000000u, 2u, DSO_WAVEGEN_SQUARE_DUTY_PERMILLE},
    {DSO_WAVEGEN_WAVE_KIND_SQUARE, "square", 2000000u, 2u, DSO_WAVEGEN_SQUARE_DUTY_PERMILLE},
    {DSO_WAVEGEN_WAVE_KIND_SQUARE, "square", 3000000u, 2u, DSO_WAVEGEN_SQUARE_DUTY_PERMILLE},
    {DSO_WAVEGEN_WAVE_KIND_SQUARE, "square", 3500000u, 2u, DSO_WAVEGEN_SQUARE_DUTY_PERMILLE},
    {DSO_WAVEGEN_WAVE_KIND_SQUARE, "square", 250000u, 2u, DSO_WAVEGEN_SQUARE_DUTY_PERMILLE},
};

/*
*********************************************************************************************************
*   函 数 名: DSO_Wavegen_GetProfile
*   功能说明: 处理 PA4/DAC 波形发生器的配置、启动停止、查表或定时器参数计算。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_Wavegen_GetProfile(uint32_t profile_id, DSO_WavegenProfile *out)
{
    if ((out == NULL) || (profile_id >= (uint32_t)(sizeof(s_profiles) / sizeof(s_profiles[0])))) {
        return false;
    }

    *out = s_profiles[profile_id];
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Wavegen_GetSelectedProfile
*   功能说明: 处理 PA4/DAC 波形发生器的配置、启动停止、查表或定时器参数计算。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_Wavegen_GetSelectedProfile(DSO_WavegenProfile *out)
{
    return DSO_Wavegen_GetProfile(DSO_WAVEGEN_PROFILE, out);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Wavegen_CalcUpdateHz
*   功能说明: 处理 PA4/DAC 波形发生器的配置、启动停止、查表或定时器参数计算。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_Wavegen_CalcUpdateHz(const DSO_WavegenProfile *profile, uint32_t *out_update_hz)
{
    uint64_t update_hz;

    if ((profile == NULL) || (out_update_hz == NULL) ||
        (profile->wave_hz == 0u) || (profile->lut_count == 0u)) {
        return false;
    }

    update_hz = (uint64_t)profile->wave_hz * (uint64_t)profile->lut_count;
    if ((update_hz == 0u) ||
        (update_hz > 0xFFFFFFFFull) ||
        (update_hz > (uint64_t)DSO_WAVEGEN_DAC_UPDATE_BUDGET_HZ)) {
        *out_update_hz = 0u;
        return false;
    }

    *out_update_hz = (uint32_t)update_hz;
    return true;
}

#if DSO_WAVEGEN_ENABLE
#include "dso_wavegen_dac_pa4.h"
#include "dso_wavegen_lut.h"

#if DSO_WAVEGEN_HOST_SYNTAX
/* 宏定义: pdMS_TO_TICKS - 时间间隔、周期或时基相关配置常量。 */
#define pdMS_TO_TICKS(ms_) (ms_)
/*
*********************************************************************************************************
*   函 数 名: vTaskDelay
*   功能说明: 在波形发生器模块中完成 DAC/定时器配置、波形查表、参数换算或运行状态控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void vTaskDelay(uint32_t ticks)
{
    (void)ticks;
}
/*
*********************************************************************************************************
*   函 数 名: vTaskSuspend
*   功能说明: 在波形发生器模块中完成 DAC/定时器配置、波形查表、参数换算或运行状态控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void vTaskSuspend(void *task)
{
    (void)task;
}
#else
#include "FreeRTOS.h"
#include "task.h"
#endif

#include <stdio.h>

/*
*********************************************************************************************************
*   函 数 名: DSO_Wavegen_StartFromConfig
*   功能说明: 处理 PA4/DAC 波形发生器的配置、启动停止、查表或定时器参数计算。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_WavegenStatus DSO_Wavegen_StartFromConfig(void)
{
    DSO_WavegenProfile profile = {
        DSO_WAVEGEN_WAVE_KIND_SINE,
        "",
        0u,
        0u,
        DSO_WAVEGEN_SQUARE_DUTY_PERMILLE
    };
    DSO_WavegenLutConfig lut_cfg;
    uint16_t lut[DSO_WAVEGEN_MAX_LUT_SAMPLES];
    uint16_t generated = 0u;
    uint32_t update_hz = 0u;
    DSO_WavegenStatus status;

    if (!DSO_Wavegen_GetSelectedProfile(&profile) ||
        !DSO_Wavegen_CalcUpdateHz(&profile, &update_hz)) {
        printf("[WAVEGEN] start status=invalid_profile profile=%lu\r\n",
               (unsigned long)DSO_WAVEGEN_PROFILE);
        return DSO_WAVEGEN_INVALID;
    }

    lut_cfg.wave = profile.wave;
    lut_cfg.lut_count = profile.lut_count;
    lut_cfg.offset = 2048u;
    lut_cfg.amplitude = 1200u;
    lut_cfg.duty_permille = profile.duty_permille;

    status = DSO_WavegenLut_Build(&lut_cfg, lut, DSO_WAVEGEN_MAX_LUT_SAMPLES, &generated);
    if ((status != DSO_WAVEGEN_OK) || (generated != profile.lut_count)) {
        printf("[WAVEGEN] start status=lut_fail wave=%s lut=%u\r\n",
               profile.name,
               profile.lut_count);
        return DSO_WAVEGEN_INVALID;
    }

    status = DSO_WavegenDacPa4_Start(lut, generated, update_hz);
    if (status != DSO_WAVEGEN_OK) {
        printf("[WAVEGEN] start status=%u path=PA4/DAC1_CH1 trigger=TIM5_TRGO wave=%s lut=%u wave_hz=%lu update_hz=%lu\r\n",
               (unsigned)status,
               profile.name,
               profile.lut_count,
               (unsigned long)profile.wave_hz,
               (unsigned long)update_hz);
        return status;
    }

    {
        DSO_WavegenSnapshot snapshot = {0};
        (void)DSO_WavegenDacPa4_Poll(&snapshot);
        printf("[WAVEGEN] start path=PA4/DAC1_CH1 trigger=TIM5_TRGO dma=DMA1_Stream5 wave=%s lut=%u wave_hz=%lu update_hz=%lu actual_update_hz=%lu psc=%lu arr=%lu vref_mv=%lu\r\n",
               profile.name,
               profile.lut_count,
               (unsigned long)profile.wave_hz,
               (unsigned long)update_hz,
               (unsigned long)snapshot.actual_update_hz,
               (unsigned long)snapshot.tim_psc,
               (unsigned long)snapshot.tim_arr,
               (unsigned long)DSO_WAVEGEN_VREF_MV);
    }

    return DSO_WAVEGEN_OK;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Wavegen_Poll
*   功能说明: 处理 PA4/DAC 波形发生器的配置、启动停止、查表或定时器参数计算。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_WavegenStatus DSO_Wavegen_Poll(DSO_WavegenSnapshot *out)
{
    return DSO_WavegenDacPa4_Poll(out);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Wavegen_Stop
*   功能说明: 处理 PA4/DAC 波形发生器的配置、启动停止、查表或定时器参数计算。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Wavegen_Stop(void)
{
    DSO_WavegenDacPa4_Stop();
}

/*
*********************************************************************************************************
*   函 数 名: DSO_Wavegen_Task
*   功能说明: 处理 PA4/DAC 波形发生器的配置、启动停止、查表或定时器参数计算。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Wavegen_Task(void *argument)
{
    DSO_WavegenStatus status;

    (void)argument;

    status = DSO_Wavegen_StartFromConfig();
    if (status != DSO_WAVEGEN_OK) {
        printf("[WAVEGEN] task_stop status=%u\r\n", (unsigned)status);
        vTaskSuspend(NULL);
    }

    for (;;) {
        DSO_WavegenSnapshot snapshot = {0};
        status = DSO_Wavegen_Poll(&snapshot);
        if (status != DSO_WAVEGEN_OK) {
            printf("[WAVEGEN] fault status=%u running=%u underrun=%u actual_update_hz=%lu\r\n",
                   (unsigned)status,
                   snapshot.running ? 1u : 0u,
                   snapshot.dma_underrun ? 1u : 0u,
                   (unsigned long)snapshot.actual_update_hz);
            vTaskSuspend(NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(DSO_WAVEGEN_POLL_MS));
    }
}
#endif
