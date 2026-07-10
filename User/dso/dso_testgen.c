/**
 * @file dso_testgen.c
 * @brief TestGen 启动与样本注入。
 */

/*
 * dso_testgen.c — Internal test signal generator
 *
 * Outputs a ~303 kHz sine wave on PA4 (DAC1_OUT1) using TIM6 + DMA.
 * Connect PA4 → PA6 with a jumper wire to feed the DSO input.
 *
 * TIM6 clock : 200 MHz  (APB1 timer clock)
 * LUT size   : 20 samples
 * ARR        : 32  →  200 MHz / 33 / 20 = 303 030 Hz ≈ 303 kHz
 * DMA        : DMA1_Stream5, circular, 16-bit
 * Signal     : center 1.65 V (2048), amplitude ±1200 counts
 *              → 0.68 V … 2.62 V, Vpp ≈ 1.93 V
 */

#include "dso_testgen.h"
#include "dso_acq_common.h"
#include "dso_config.h"
#include "stm32h7xx_hal.h"
#include "debug_uart_lock.h"
#include <stdio.h>

#if DSO_RUNTIME_LOG_ENABLE
/* 宏定义: DSO_LOG_PRINTF - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_LOG_PRINTF(...) \
    do { \
        DebugUartLock(); \
        printf(__VA_ARGS__); \
        DebugUartUnlock(); \
    } while (0)
#else
/* 宏定义: DSO_LOG_PRINTF - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_LOG_PRINTF(...) do { } while (0)
#endif

/* ------------------------------------------------------------------ */
/* LUT点数 — 修改这里可以调整波形精度与最大频率的平衡                    */
/* ------------------------------------------------------------------ */
#define TESTGEN_LUT_SIZE  20

/* ------------------------------------------------------------------ */
/* 20-point sine LUT  (center=2048, amplitude=1200)                    */
/* sin(2*pi*n/20) for n=0..19                                         */
/* ------------------------------------------------------------------ */
static const uint16_t s_sineLUT[TESTGEN_LUT_SIZE] = {
    2048, 2419, 2753, 3019, 3189,
    3248, 3189, 3019, 2753, 2419,
    2048, 1677, 1343, 1077,  907,
     848,  907, 1077, 1343, 1677
};

/* DMA source buffer — must be in DMA-accessible memory (not DTCM) */
__attribute__((section(".RAM_D2"), aligned(4)))
static uint16_t s_dma_buf[TESTGEN_LUT_SIZE];

static DAC_HandleTypeDef  s_dac;
static DMA_HandleTypeDef  s_dma;

#if DSO_DEBUG_PIPELINE
/* timer_clock_from_pclk moved to dso_acq_common.c */

static void print_testgen_debug(void)
{
    RCC_ClkInitTypeDef clk_cfg = {0};
    uint32_t latency = 0;
    uint32_t pclk1_hz;
    uint32_t tim6_clk_hz;
    uint32_t update_hz;
    uint32_t sine_hz;

    HAL_RCC_GetClockConfig(&clk_cfg, &latency);
    pclk1_hz = HAL_RCC_GetPCLK1Freq();
    tim6_clk_hz = DSO_AcqCommon_TimerClockFromPclk(pclk1_hz, clk_cfg.APB1CLKDivider);
    update_hz = tim6_clk_hz / ((uint32_t)TIM6->PSC + 1u) / ((uint32_t)TIM6->ARR + 1u);
    sine_hz = update_hz / TESTGEN_LUT_SIZE;

    printf("[TESTGEN_DBG] PCLK1=%u TIM6CLK=%u PSC=%u ARR=%u update=%uHz LUT=%u sine=%uHz\r\n",
           (unsigned)pclk1_hz,
           (unsigned)tim6_clk_hz,
           (unsigned)TIM6->PSC,
           (unsigned)TIM6->ARR,
           (unsigned)update_hz,
           (unsigned)TESTGEN_LUT_SIZE,
           (unsigned)sine_hz);
    printf("[TESTGEN_DBG] TIM6_CR1=0x%08lX TIM6_CR2=0x%08lX DAC_CR=0x%08lX\r\n",
           (unsigned long)TIM6->CR1,
           (unsigned long)TIM6->CR2,
           (unsigned long)DAC1->CR);
}
#endif

/* ------------------------------------------------------------------ */
/* DSO_TestGen_Start                                                   */
/* ------------------------------------------------------------------ */
void DSO_TestGen_Start(void)
{
    /* Copy const LUT to DMA-accessible RAM */
    for (int i = 0; i < TESTGEN_LUT_SIZE; i++)
        s_dma_buf[i] = s_sineLUT[i];

    /* ---- TIM6: 200 MHz / (32+1) = 6.06 MHz update → 303 kHz sine ---- */
    __HAL_RCC_TIM6_CLK_ENABLE();

    TIM6->CR1 = 0;                       /* Stop first             */
    TIM6->PSC = 0;                        /* No prescaler           */
    TIM6->ARR = 99;                       /* 200MHz/100/20 = 100kHz */
    TIM6->CR2 = TIM_CR2_MMS_1;           /* TRGO = Update event    */
    TIM6->EGR = TIM_EGR_UG;              /* Load shadow registers  */
    TIM6->SR  = 0;                        /* Clear update flag      */
    TIM6->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;  /* Auto-reload + Start */

    /* ---- DMA1_Stream5: DAC1, circular, halfword ---- */
    __HAL_RCC_DMA1_CLK_ENABLE();

    s_dma.Instance                 = DMA1_Stream5;
    HAL_DMA_DeInit(&s_dma);

    s_dma.Init.Request             = DMA_REQUEST_DAC1;
    s_dma.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    s_dma.Init.PeriphInc           = DMA_PINC_DISABLE;
    s_dma.Init.MemInc              = DMA_MINC_ENABLE;
    s_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    s_dma.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    s_dma.Init.Mode                = DMA_CIRCULAR;
    s_dma.Init.Priority            = DMA_PRIORITY_LOW;

    if (HAL_DMA_Init(&s_dma) != HAL_OK) {
        DSO_LOG_PRINTF("[TESTGEN] DMA init FAIL\r\n");
        return;
    }

    /* ---- DAC1 CH1 on PA4 ---- */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin  = GPIO_PIN_4;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    __HAL_RCC_DAC12_CLK_ENABLE();

    s_dac.Instance = DAC1;
    HAL_DAC_DeInit(&s_dac);

    if (HAL_DAC_Init(&s_dac) != HAL_OK) {
        DSO_LOG_PRINTF("[TESTGEN] DAC init FAIL\r\n");
        return;
    }

    __HAL_LINKDMA(&s_dac, DMA_Handle1, s_dma);

    DAC_ChannelConfTypeDef ch = {0};
    ch.DAC_Trigger      = DAC_TRIGGER_T6_TRGO;
    ch.DAC_OutputBuffer  = DAC_OUTPUTBUFFER_ENABLE;

    if (HAL_DAC_ConfigChannel(&s_dac, &ch, DAC_CHANNEL_1) != HAL_OK) {
        DSO_LOG_PRINTF("[TESTGEN] DAC config FAIL\r\n");
        return;
    }

    if (HAL_DAC_Start_DMA(&s_dac, DAC_CHANNEL_1,
                           (uint32_t *)s_dma_buf, TESTGEN_LUT_SIZE,
                           DAC_ALIGN_12B_R) != HAL_OK) {
        DSO_LOG_PRINTF("[TESTGEN] DAC DMA start FAIL\r\n");
        return;
    }

    /* No IRQ handler needed — disable DMA interrupts for free-running */
    __HAL_DMA_DISABLE_IT(&s_dma, DMA_IT_TC | DMA_IT_HT | DMA_IT_TE);

    DSO_LOG_PRINTF("[TESTGEN] PA4 DAC1: 100kHz sine started (%dpts ARR=99)\r\n", TESTGEN_LUT_SIZE);

#if DSO_DEBUG_PIPELINE
    print_testgen_debug();
#endif
}

/* ------------------------------------------------------------------ */
/* DSO_TestGen_Stop                                                    */
/* ------------------------------------------------------------------ */
void DSO_TestGen_Stop(void)
{
    HAL_DAC_Stop_DMA(&s_dac, DAC_CHANNEL_1);
    TIM6->CR1 &= ~TIM_CR1_CEN;
    DSO_LOG_PRINTF("[TESTGEN] stopped\r\n");
}
