/**
 * @file main.c
 * @brief 固件入口：System_Init、SDRAM/LTDC 初始化、FreeRTOS 任务创建与调度器启动。
 */

#include "includes.h"
#include "dma2d_wait.h"
#include "bsp_fmc_sdram.h"
#include "gui_heap_experiment.h"
#include "bsp_lcd_rgb.h"
#include "y_t_stress_ab.h"
#include "dso_integration.h"
#include "dso/dso_config.h"
#include "ui/MainTask.h"
#include "ui/UI_WaveformCtrl.h"
#include "ui/roll_history_ui.h"
#include "rtos_cpu_stats.h"
#include "dso/dso_engine.h"
#include "dso/dso_wavegen.h"
#include "dso/dso_eth_upload.h"
#include "dso/dso_switch_cmd.h"
#include "dso/dso_sht40.h"
#include "dso/dso_ina226.h"
#include "dso/dso_ina226_u44.h"
#include "dso/dso_atecc608b.h"
#include "dso/dso_module_upload.h"
#include "dso/dso_watchdog.h"
#include "dso/dso_fmc_telemetry.h"
#include "dso/fmc16/fmc16_integration.h"
#include "ping.h"
#include "bsp_vram_layout.h"
#include "vram_sentinels.h"
#include "core_cm7.h"
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#if DSO_APP_INFO_PRINTF_ENABLE
/* 宏定义: APP_INFO_PRINTF - APP_INFO_PRINTF 的宏定义。 */
#define APP_INFO_PRINTF(...) printf(__VA_ARGS__)
#else
/* 宏定义: APP_INFO_PRINTF - APP_INFO_PRINTF 的宏定义。 */
#define APP_INFO_PRINTF(...) do { } while (0)
#endif

/* DSO 引擎任务句柄：供 MainTask 栈高水位等调试；无 DSO 时保持 NULL。 */
TaskHandle_t g_dso_engine_task = NULL;

/*
 * 显存地址定义（必须与 LCDConf_Lin_Template.c 中的宏完全一致）：
 *
 *   0xC0000000 ~ 0xC00BB7FF  逻辑横屏子帧 0（800×480×2 = 768 KB）
 *   0xC00BB800 ~ 0xC0176FFF  逻辑横屏子帧 1（800×480×2 = 768 KB）
 *   0xC0200000 ~ 0xC02BB7FF  物理竖屏缓冲 0（480×800×2 = 768 KB）← LTDC Ping
 *   0xC0300000 ~ 0xC03BB7FF  物理竖屏缓冲 1（480×800×2 = 768 KB）← LTDC Pong
 *
 * 逻辑子帧 1 最高地址 0xC0176FFF < 物理缓冲 0 起始地址 0xC0200000，无重叠。
 * 地址宏见 bsp_vram_layout.h。
 */

/*
 * 硬件诊断模式：直接测试 LTDC 输出，绕过 emWin 和旋转
 * 用于隔离问题是 LTDC/面板硬件问题还是软件问题
 */
#define LCD_DIAGNOSTIC_MODE     0

/*
 * 绕过旋转测试：emWin 直接使用物理缓冲区（竖屏模式）
 * 用于确认问题是否在旋转算法层面
 * 0 = 正常模式（横屏逻辑缓冲区 + 旋转到竖屏）
 * 1 = 竖屏直接模式（跳过旋转，调试用）
 */
#define LCD_BYPASS_ROTATION     0

/*
 * DMA2D 阶段一测试：在 AXI SRAM 上验证 DMA2D R2M 硬件是否正常
 * 1 = 运行测试，失败则 Error_Handler，通过则继续
 * 0 = 跳过测试
 */
/* DMA2D 初始化已移植到 dma2d_wave.c，此处仅保留 Phase1 可选自检（GUI 前） */
#define DMA2D_PHASE1_TEST       0   /* 1=运行测试 0=跳过 */
#define DMA2D_PHASE2_TEST       0   /* 已废弃：DMA2D 初始化在 dma2d_wave，不再显示红块 */

/* FMC16 云桥 FPGA 通信：1=启用。需 XO2 已烧录 FMC16_PERSONALITY 位流 + BFPS_nOE_JP=1-2 */
#define FMC16_ENABLE            1

#if (DMA2D_PHASE1_TEST == 1)
/**
 * DMA2D 阶段一：隔离测试，使用 AXI SRAM（非 SDRAM）
 * 验证 DMA2D R2M 填色能否正确写入内存并读回。
 * 通过则返回 true，失败则 Error_Handler。
 */
static bool DMA2D_Phase1_Test(void)
{
/* 宏定义: PHASE1_W - PHASE1_W 的宏定义。 */
#define PHASE1_W        64
/* 宏定义: PHASE1_H - 头文件保护宏。 */
#define PHASE1_H        64
/* 宏定义: PHASE1_PIXELS - PHASE1_PIXELS 的宏定义。 */
#define PHASE1_PIXELS   (PHASE1_W * PHASE1_H)
/* 宏定义: PHASE1_BYTES - PHASE1_BYTES 的宏定义。 */
#define PHASE1_BYTES    (PHASE1_PIXELS * 2U)
#define PHASE1_COLOR    0xF800U   /* RGB565 红色 */

    /* AXI SRAM 末尾 8KB（0x2407E000），避免覆盖 0x24000000 处的 BSS/堆等关键数据 */
    volatile uint16_t *buf = (volatile uint16_t *)0x2407E000U;
    uint32_t i;

    __HAL_RCC_DMA2D_CLK_ENABLE();

    /* 先清零，避免读到未初始化数据 */
    for (i = 0U; i < PHASE1_PIXELS; i++) {
        buf[i] = 0U;
    }

    /* DMA2D R2M 填色 */
    while (DMA2D->CR & DMA2D_CR_START) {}
    DMA2D->CR     = 0x00030000UL;                    /* R2M 模式 */
    DMA2D->OCOLR  = (uint32_t)PHASE1_COLOR;
    DMA2D->OMAR   = (uint32_t)buf;
    DMA2D->OOR    = 0U;
    DMA2D->OPFCCR = 0x02U;                            /* RGB565 */
    DMA2D->NLR    = (PHASE1_W << 16U) | PHASE1_H;
    DMA2D->CR    |= DMA2D_CR_START;
    while (DMA2D->CR & DMA2D_CR_START) {}

    /* AXI SRAM 为 Write-Back Cache，DMA2D 直写内存，CPU 读前需 Invalidate */
    SCB_InvalidateDCache_by_Addr((uint32_t *)buf, PHASE1_BYTES);

    for (i = 0U; i < PHASE1_PIXELS; i++) {
        if (buf[i] != PHASE1_COLOR) {
            return false;
        }
    }
    return true;
}
#endif

/*
*********************************************************************************************************
*   函 数 名: SDRAM_QuickCheck
*   功能说明: SDRAM_QuickCheck 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void SDRAM_QuickCheck(void)
{
    volatile uint32_t *p;
    uint32_t old0;
    uint32_t old1;

    p = (volatile uint32_t *)EXT_SDRAM_ADDR;
    old0 = p[0];
    old1 = p[1];

    p[0] = 0x55AA55AAU;
    p[1] = 0xAA55AA55U;

    if ((p[0] != 0x55AA55AAU) || (p[1] != 0xAA55AA55U))
    {
        Error_Handler(__FILE__, __LINE__);
    }

    p[0] = old0;
    p[1] = old1;
}

/* 初始化所有帧缓冲区，防止显示随机数据 */
static void Framebuffer_Init(void)
{
    uint32_t i;
    uint16_t *p_logic = (uint16_t *)VRAM_LOGICAL_ADDR;
    uint16_t *p0 = (uint16_t *)VRAM_PHYSICAL_0_ADDR;
    uint16_t *p1 = (uint16_t *)VRAM_PHYSICAL_1_ADDR;
    /*
     * NUM_BUFFERS=1：emWin 使用 VRAM_LOGICAL_ADDR 作为单个逻辑横屏缓冲。
     * 启动时仍清零两个逻辑帧大小的预留区，兼容旧多缓冲残留并确保 SDRAM 干净。
     */
    const uint32_t logic_pixels = 2U * 800U * 480U;  /* 覆盖逻辑帧和相邻预留区 */
    const uint32_t phys_pixels  = 480U * 800U;

    /* 清零逻辑缓冲区及相邻预留区 */
    for (i = 0U; i < logic_pixels; i++)
    {
        p_logic[i] = 0U;
    }

    /* 初始化物理缓冲区为黑色（背光开启前 LTDC 显示此内容） */
    for (i = 0U; i < phys_pixels; i++)
    {
        p0[i] = 0x0000U;
        p1[i] = 0x0000U;
    }

    /* Clean D-Cache：确保上述写入从 L1 Cache 推入 SDRAM */
    SCB_CleanDCache_by_Addr((uint32_t *)VRAM_LOGICAL_ADDR, logic_pixels * 2U);
    SCB_CleanDCache_by_Addr((uint32_t *)VRAM_PHYSICAL_0_ADDR, phys_pixels * 2U);
    SCB_CleanDCache_by_Addr((uint32_t *)VRAM_PHYSICAL_1_ADDR, phys_pixels * 2U);
}

#if (LCD_DIAGNOSTIC_MODE == 1)
/*
 * 硬件诊断测试：直接填充显存测试 LTDC 输出
 * 绕过 emWin 和旋转，用于确认是硬件问题还是软件问题
 */
static void LCD_DiagnosticTest(void)
{
    uint32_t i;
    uint16_t *p = (uint16_t *)VRAM_PHYSICAL_0_ADDR;
    const uint32_t pixels = 480U * 800U;

    /* 测试1: 纯红色 */
    for (i = 0U; i < pixels; i++) { p[i] = 0xF800U; }
    SCB_CleanDCache_by_Addr((uint32_t *)VRAM_PHYSICAL_0_ADDR, pixels * 2U);
    HAL_Delay(1000U);

    /* 测试2: 纯绿色 */
    for (i = 0U; i < pixels; i++) { p[i] = 0x07E0U; }
    SCB_CleanDCache_by_Addr((uint32_t *)VRAM_PHYSICAL_0_ADDR, pixels * 2U);
    HAL_Delay(1000U);

    /* 测试3: 纯蓝色 */
    for (i = 0U; i < pixels; i++) { p[i] = 0x001FU; }
    SCB_CleanDCache_by_Addr((uint32_t *)VRAM_PHYSICAL_0_ADDR, pixels * 2U);
    HAL_Delay(1000U);

    /* 测试4: 白色 */
    for (i = 0U; i < pixels; i++) { p[i] = 0xFFFFU; }
    SCB_CleanDCache_by_Addr((uint32_t *)VRAM_PHYSICAL_0_ADDR, pixels * 2U);
    HAL_Delay(1000U);

    /* 测试5: 水平渐变条纹 (检测行同步) */
    for (i = 0U; i < pixels; i++) {
        uint16_t row = (uint16_t)(i / 480U);
        if (row < 200U) {
            p[i] = 0xF800U;  /* 红色 */
        } else if (row < 400U) {
            p[i] = 0x07E0U;  /* 绿色 */
        } else if (row < 600U) {
            p[i] = 0x001FU;  /* 蓝色 */
        } else {
            p[i] = 0xFFFFU;  /* 白色 */
        }
    }
    SCB_CleanDCache_by_Addr((uint32_t *)VRAM_PHYSICAL_0_ADDR, pixels * 2U);
    HAL_Delay(2000U);

    /* 测试6: 垂直渐变条纹 (检测列同步) */
    for (i = 0U; i < pixels; i++) {
        uint16_t col = (uint16_t)(i % 480U);
        if (col < 120U) {
            p[i] = 0xF800U;  /* 红色 */
        } else if (col < 240U) {
            p[i] = 0x07E0U;  /* 绿色 */
        } else if (col < 360U) {
            p[i] = 0x001FU;  /* 蓝色 */
        } else {
            p[i] = 0xFFFFU;  /* 白色 */
        }
    }
    SCB_CleanDCache_by_Addr((uint32_t *)VRAM_PHYSICAL_0_ADDR, pixels * 2U);
    /* 此测试无限循环，观察屏幕 */
    while (1) { HAL_Delay(1000U); }
}
#endif

/*
 * 假数据生成任务 —— 用于在没有真实 ADC 接入时验证波形显示管线
 *
 * 直接构造 DSO_Frame（预渲染 y 像素坐标）并通过 DSO_UI_SendFrame 发送给 UI。
 * CH1：3 周期正弦；CH2：2 周期余弦。
 */
static void ADC_FakeData_Task(void *arg)
{
    static DSO_Frame fake;
    float  phase = 0.0f;
    uint32_t x;

    (void)arg;

    for (;;)
    {
        if (!UI_Waveform_IsStreamActive()) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        memset(&fake, 0, sizeof(fake));

        for (x = 0; x < DSO_DISPLAY_WIDTH; x++) {
            float t  = (float)x / (float)DSO_DISPLAY_WIDTH;
            float y1 = 0.5f + 0.4f * sinf(2.0f * 3.14159265f * 3.0f * t + phase);
            float y2 = 0.5f + 0.25f * cosf(2.0f * 3.14159265f * 2.0f * t + phase * 0.6f);
            int py1  = (int)(y1 * (float)(DSO_DISPLAY_HEIGHT - 1) + 0.5f);
            int py2  = (int)(y2 * (float)(DSO_DISPLAY_HEIGHT - 1) + 0.5f);
            if (py1 < 0) py1 = 0; else if (py1 > (int)(DSO_DISPLAY_HEIGHT - 1)) py1 = (int)(DSO_DISPLAY_HEIGHT - 1);
            if (py2 < 0) py2 = 0; else if (py2 > (int)(DSO_DISPLAY_HEIGHT - 1)) py2 = (int)(DSO_DISPLAY_HEIGHT - 1);
            fake.y_coords[x]     = (int16_t)py1;
            fake.y_coords_ch2[x] = (int16_t)py2;
        }

        fake.sample_rate     = 1000000u;
        fake.ch2_sample_rate = 1000000u;
        fake.ch2_present     = true;
        fake.freq_hz         = 3000.0f;
        fake.freq_valid      = true;
        fake.ch2_freq_hz     = 2000.0f;
        fake.ch2_freq_valid  = true;
        fake.vpp             = 3000u;
        fake.ch2_vpp         = 1800u;
        fake.vert_mv_per_div = 500u;
        fake.ch2_vert_mv_per_div = 500u;
        fake.vert_scale_min_v    = 0.0f;
        fake.vert_scale_max_v    = DSO_VREF;
        fake.ch2_vert_scale_min_v = 0.0f;
        fake.ch2_vert_scale_max_v = DSO_VREF;
        fake.tb_sec_div_ns   = 1000000u;
        fake.tb_mode         = 0u;
        fake.trig_mode       = 0u;
        fake.trig_level_v    = DSO_VREF / 2.0f;
        fake.triggered       = true;
        fake.duty_cycle      = 0.5f;
        fake.ch2_duty_cycle  = 0.5f;

        DSO_UI_SendFrame(&fake);

        phase += 0.05f;
        if (phase >= 2.0f * 3.14159265f)
            phase -= 2.0f * 3.14159265f;

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

/*
*********************************************************************************************************
*   函 数 名: main
*   功能说明: main 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
int main(void)
{
    System_Init();

    HAL_SuspendTick();
    __set_PRIMASK(1);

    bsp_Init();
    bsp_Uart1SendStringPolled("\r\n[BOOT] UART1 direct OK\r\n");
    APP_INFO_PRINTF("\r\n[BOOT] OK\r\n");

#if (DMA2D_PHASE1_TEST == 1)
    if (!DMA2D_Phase1_Test()) {
        Error_Handler(__FILE__, __LINE__);
    }
#endif

    bsp_InitExtSDRAM();
    SDRAM_QuickCheck();
    APP_INFO_PRINTF("[BOOT] SDRAM OK\r\n");
    { /* FMC bus integrity test — verify cloud bridge not corrupting SDRAM */
        volatile uint16_t *p = (volatile uint16_t *)0xC0000000U;
        uint16_t r;
        *p = 0x55AA; r = *p;
        printf("[BOOT] SDRAM test 0x55AA: read=0x%04X %s\r\n", r, r==0x55AA?"PASS":"FAIL");
        *p = 0xAA55; r = *p;
        printf("[BOOT] SDRAM test 0xAA55: read=0x%04X %s\r\n", r, r==0xAA55?"PASS":"FAIL");
    }
    VRAM_Sentinels_Init();

#if (SDRAM_GUI_HEAP_TEST_AT_BOOT != 0)
    {
        uint32_t e = bsp_TestExtSDRAM_Block((uint32_t)SDRAM_APP_BUF, GUI_CONF_EXPERIMENT_NUMBYTES);
        APP_INFO_PRINTF("[BOOT] SDRAM GUI heap region test err=%lu (0=ok, size=%lu)\r\n",
                        (unsigned long)e, (unsigned long)GUI_CONF_EXPERIMENT_NUMBYTES);
        if (e != 0U) {
            Error_Handler(__FILE__, __LINE__);
        }
    }
#endif

    /* 初始化帧缓冲区，防止花屏 */
    bsp_Uart1SendStringPolled("[BOOT] pre-Framebuffer\r\n");
    Framebuffer_Init();
    bsp_Uart1SendStringPolled("[BOOT] post-Framebuffer\r\n");

    bsp_Uart1SendStringPolled("[BOOT] pre-LCD_RGB_Init\r\n");
    LCD_RGB_Init();
    bsp_Uart1SendStringPolled("[BOOT] post-LCD_RGB_Init\r\n");
    APP_INFO_PRINTF("[BOOT] LTDC FB=0x%08lX\r\n", (unsigned long)LCD_RGB_FB_ADDR);
    { /* GUI heap region check (emWin 2MB @ 0xC0400000) */
        volatile uint16_t *g = (volatile uint16_t *)0xC0400000U;
        uint16_t ov = *g;
        *g = 0xAA55; if (*g != 0xAA55)
            bsp_Uart1SendStringPolled("[GUIHEAP] FAIL @c0400000\r\n");
        else bsp_Uart1SendStringPolled("[GUIHEAP] OK @c0400000\r\n");
        *g = ov; /* restore */
    }

    /* Release cloud-bridge FPGA reset — SDRAM/LCD are now configured.
     * PH8 (Cloud_EN) stays HIGH to keep SN74LVC257 bus isolated from the
     * unconfigured FPGA, preventing DMA2D/LTDC SDRAM corruption. */
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOH, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOH, GPIO_PIN_8, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOH, GPIO_PIN_7, GPIO_PIN_RESET);
    printf("[PH8] IDR=%d\r\n", (int)((GPIOH->IDR >> 8) & 1));
    APP_INFO_PRINTF("[BOOT] FPGA reset released\r\n");

    /* 试验：PG10 拉高 */
    {
        GPIO_InitTypeDef gpio_pg10 = {0};

        __HAL_RCC_GPIOG_CLK_ENABLE();
        gpio_pg10.Pin   = GPIO_PIN_10;
        gpio_pg10.Mode  = GPIO_MODE_OUTPUT_PP;
        gpio_pg10.Pull  = GPIO_NOPULL;
        gpio_pg10.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOG, &gpio_pg10);
        HAL_GPIO_WritePin(GPIOG, GPIO_PIN_10, GPIO_PIN_SET);
    }

    /* 与 LCD_RGB_FB_ADDR / 物理缓冲 0 一致 */
    if (LTDC_Layer1->CFBAR != VRAM_PHYSICAL_0_ADDR) {
        LTDC_Layer1->CFBAR = VRAM_PHYSICAL_0_ADDR;
        LTDC->SRCR = LTDC_SRCR_IMR;
    }

    bsp_Uart1SendStringPolled("[BOOT] pre-Backlight\r\n");
    LCD_RGB_BacklightOn();
    bsp_Uart1SendStringPolled("[BOOT] post-Backlight\r\n");

    /* DMA2D 时钟使能：须在 MainTask 首次 GUI 绘制前完成（原 Phase2 测试在此处执行）
     * 否则 GUI_FillRect、DrawGridFull 等使用 DMA2D 时时钟未开，导致背景显示不全 */
    bsp_Uart1SendStringPolled("[BOOT] pre-DMA2D\r\n");
    __HAL_RCC_DMA2D_CLK_ENABLE();
    DMA2D_Wait_Init();
    /* emWin 启动前再清一次 UNALIGN_TRP（若此前固件未烧录或某路径曾改写 CCR） */
    Bsp_DisableUnalignedFaultTrap();
    bsp_Uart1SendStringPolled("[BOOT] post-DMA2D\r\n");
    { /* DMA2D→SDRAM master test: DMA2D bypasses CPU cache, different bus path */
        volatile uint16_t *dst = (volatile uint16_t *)0xC0600000U;
        uint32_t i, err = 0;
        DMA2D->CR = 0x00000000UL;
        DMA2D->CR = 0x00030000UL;                /* R2M */
        DMA2D->OCOLR = 0xF800U;
        DMA2D->OMAR = (uint32_t)dst;
        DMA2D->OOR = 0U;
        DMA2D->OPFCCR = 0x02U;                   /* RGB565 */
        DMA2D->NLR = (64UL << 16) | 64UL;
        DMA2D->CR |= DMA2D_CR_START;
        for (i = 0U; i < 1000000UL; i++) {
            if ((DMA2D->CR & DMA2D_CR_START) == 0U) break;
        }
        if (DMA2D->CR & DMA2D_CR_START) {
            bsp_Uart1SendStringPolled("[DMA2DSDRAM] TIMEOUT\r\n");
        } else {
            for (i = 0U; i < 4096U; i++) if (dst[i] != 0xF800U) err++;
            if (err) printf("[DMA2DSDRAM] FAIL err=%lu\r\n", (unsigned long)err);
            else bsp_Uart1SendStringPolled("[DMA2DSDRAM] OK\r\n");
        }
    }
    { /* CPU bulk SDRAM write test — within known-valid 4MB region */
        volatile uint16_t *p = (volatile uint16_t *)0xC0100000U;
        uint32_t i, err = 0;
        for (i = 0U; i < 8192U; i++) p[i] = 0xA55AU;
        SCB_CleanDCache_by_Addr((uint32_t *)p, 8192U * 2U);
        for (i = 0U; i < 8192U; i++) if (p[i] != 0xA55AU) err++;
        if (err) printf("[CPUBULK] FAIL err=%lu\r\n", (unsigned long)err);
        else bsp_Uart1SendStringPolled("[CPUBULK] OK\r\n");
    }
    { /* CPU small SDRAM test — 8 words at base region */
        volatile uint16_t *q = (volatile uint16_t *)0xC0001000U;
        uint32_t i, err = 0;
        for (i = 0U; i < 8U; i++) q[i] = 0x1234U;
        SCB_CleanDCache_by_Addr((uint32_t *)q, 8U * 2U);
        for (i = 0U; i < 8U; i++) if (q[i] != 0x1234U) err++;
        printf("[CPUSMALL] err=%lu\r\n", (unsigned long)err);
    }

    /*
     * 在创建任务之前初始化波形队列（可先于调度器调用），避免 emWin/GUI 先占满 FreeRTOS 堆
     * 导致 xQueueCreate 失败 → configASSERT 死循环，表现为全机无响应（与注释中 30KB 堆问题同类）。
     */
    UI_Waveform_Init();
#if (USE_DSO_ENGINE != 0)
    RollHistory_Init();
#endif
    DSO_ETH_Upload_Init();

#if FMC16_ENABLE
    /* FMC16 云桥 FPGA 通信初始化（最小通信测试入口）：
     *   1. fmc16_bus_init()     — FMC Bank1 16-bit 异步 SRAM + GPIO
     *   2. fmc16_crc_selftest() — CRC golden vector 验证
     *   3. 链路层初始化，等待 XO2 LINK_HELLO
     */
    if (fmc16_system_init()) {
        printf("[BOOT] FMC16 init OK, waiting LINK_HELLO...\r\n");
    } else {
        printf("[BOOT] FMC16 CRC selftest FAILED\r\n");
    }
#endif

    /* 开关面板初始化必须放在 FMC16 init 之后：
     * bsp_fmc_io 会把 PH8(G#/Cloud_EN) 重配为 AF12_FMC(上拉)，覆盖 PH8=LOW，
     * 导致 SN74LVC257 输出禁用、开关信号悬空回 3.3V。 */
    DSO_Switch_Cmd_Init();

    /* 过桥板 SHT40 温度采样任务（1s 周期，与 PCAL6524 共用 PB7/PB8 软件 I2C） */
    DSO_Sht40_Init();

    /* 过桥板 INA226 电流监测任务（1s 周期，同 PB7/PB8 软件 I2C 总线） */
    DSO_Ina226_Init();

    /* 核心板 U44-INA226 电流监测任务（0x40/20mΩ，同总线） */
    DSO_Ina226U44_Init();

    /* 密码芯片 ATECC608B：一次性上电探测（识别/序列号/锁状态打印） */
    DSO_Atecc608b_Init();

#if DSO_ATECC_PROVISION_ONESHOT
    /* Stage 2 最小步骤（**开发用**，默认关闭）：一次性预置 Slot0 —— 见 dso_atecc608b.h 说明。
     * 只在明确要执行预置的那次烧录里把宏置 1；不 Lock、不碰 OTP、不写配置区。 */
    DSO_Atecc608b_Provision_Init();
#endif

    /* module_telemetry 模块数据上报（sht40 / ina226_u10，spec-00 信封走遥测 TCP） */
    DSO_ModuleUpload_Init();

    /* SP706T 看门狗：PB9 喂狗 + PFO# 读 + 复位原因上报 */
    DSO_Watchdog_Init();

    /* FMC 遥测推送：任务内部等待 LwIP netif up 后连接服务器 */
    DSO_FmcTelemetry_Init();

#if (LCD_DIAGNOSTIC_MODE == 1)
    /* 硬件诊断测试：绕过 emWin 直接测试 LTDC */
    LCD_DiagnosticTest();
#else
#if (Y_T_AB_DISABLE_GUI_TASK != 1)
    if (xTaskCreate((TaskFunction_t)MainTask, "emWin", 14336, NULL, 2, NULL) != pdPASS) {
        printf("[BOOT] FAIL emWin\r\n");
        Error_Handler(__FILE__, __LINE__);
    }
#else
    /* ETH 吞吐测试模式：不创建 MainTask，避免卡死的 GUI_Init 在高负载下走 NULL 设备 API 崩溃。
     * 以太网路径不依赖 GUI。恢复 GUI 时把 y_t_stress_ab.h 的 Y_T_AB_DISABLE_GUI_TASK 置 0。 */
    printf("[BOOT] GUI disabled (ETH test mode)\r\n");
#endif

#if (USE_DSO_ENGINE != 0) && (Y_T_AB_DISABLE_DSO_ENGINE_TASK == 0)
    /* DSO：ETS 等路径 + Y-T 下 MainTask/引擎交互栈较深；与 heap 中 configTOTAL_HEAP_SIZE 同步裕量。 */
    if (xTaskCreate(DSO_Engine_Task, "DSOEng", 10240, NULL, 1, &g_dso_engine_task) != pdPASS) {
        printf("[BOOT] FAIL DSO_Engine_Task\r\n");
        Error_Handler(__FILE__, __LINE__);
    }
#else
    /* 假 ADC 数据（无探头时调 UI） */
    if (xTaskCreate(ADC_FakeData_Task, "FakeADC", 2048, NULL, 1, NULL) != pdPASS) {
        printf("[BOOT] FAIL FakeADC\r\n");
        Error_Handler(__FILE__, __LINE__);
    }
#endif

#if DSO_WAVEGEN_ENABLE && (Y_T_AB_DISABLE_WAVEGEN_TASK == 0)
    if (xTaskCreate(DSO_Wavegen_Task, "Wavegen", 2048, NULL, 2, NULL) != pdPASS) {
        printf("[BOOT] FAIL Wavegen\r\n");
        Error_Handler(__FILE__, __LINE__);
    }
#endif

#if RTOS_CPU_STATS_ENABLE
    /* TIM6 50us 时基，供 FreeRTOS vTaskGetRunTimeStats（须在调度器启动前） */
    RtosCpuStats_TimerInit();
    RtosCpuStats_TaskCreate();
#endif

    if (xTaskCreate(DSO_ETH_Upload_Task, "ETH_Up", 2560, NULL, 2, NULL) != pdPASS)
    { printf("[BOOT] FAIL ETH_Up\r\n"); Error_Handler(__FILE__, __LINE__); }
    if (xTaskCreate(DSO_Ping_Task, "Ping", 1024, NULL, 1, NULL) != pdPASS)
    { printf("[BOOT] FAIL Ping\r\n"); Error_Handler(__FILE__, __LINE__); }
    DSO_ETH_UDP_RX_Init();

#if FMC16_ENABLE
    /* FMC16 云桥服务任务：1ms 轮询 FMC 总线、解析协议包、维护链路 */
    if (xTaskCreate(FMC16_Service_Task, "FMC16", 2048, NULL, 1, NULL) != pdPASS)
    { printf("[BOOT] FAIL FMC16\r\n"); Error_Handler(__FILE__, __LINE__); }
#endif

    /* 启动调度器前恢复系统滴答 */
    HAL_ResumeTick();
    __set_PRIMASK(0);

    APP_INFO_PRINTF("[BOOT] scheduler\r\n");
    /* 调度器启动前：整片 D-Cache 与 SDRAM 一致，减少 DMA2D/LTDC 与 Cache 交错脏数据 */
    __DSB();
    SCB_CleanInvalidateDCache();
    __DSB();
    vTaskStartScheduler();
#endif

    /* 1. 定义 SDRAM 显存的基地址（通常 FMC SDRAM Bank 1 起始地址） */
#define SDRAM_BASE_ADDR   ((uint32_t)0xC0000000)

/* 2. 定义测试用的常量颜色值（比如 16位 白色 0xFFFF，或者 32位 RGBA 0xFFFFFFFF） */
#define TEST_COLOR_16BIT  0xFFFF   /* 16-bit RGB565 白色 */
#define TEST_COLOR_32BIT  0xFFFFFFFF /* 32-bit ARGB 白色 */

/* 3. 定义存储读取结果的变量 */
uint16_t read_back_16 = 0;
uint32_t read_back_32 = 0;

/* 4. 写入测试（将值写入 SDRAM 的起始地址） */
/* 如果您使用的是 16-bit 数据总线（如 W9825G6），需强制使用 16-bit 指针 */
*(volatile uint16_t *)(SDRAM_BASE_ADDR) = TEST_COLOR_16BIT;

/* 如果您的 SDRAM 是 32-bit 数据总线，或者代码中配置为 32-bit 访问，则用下面这句： */
// *(volatile uint32_t *)(SDRAM_BASE_ADDR) = TEST_COLOR_32BIT;

/* 5. 读回对比（从同一地址读出） */
read_back_16 = *(volatile uint16_t *)(SDRAM_BASE_ADDR);

/* 6. 判断对比结果（通过串口打印或断点查看） */
if (read_back_16 == TEST_COLOR_16BIT) {
    printf("SDRAM 读写测试 PASSED! 写入 0x%X, 读到 0x%X\r\n", TEST_COLOR_16BIT, read_back_16);
} else {
    printf("SDRAM 读写测试 FAILED! 写入 0x%X, 读到 0x%X\r\n", TEST_COLOR_16BIT, read_back_16);
}
    while (1)
    {
    }
}

#if ( configCHECK_FOR_STACK_OVERFLOW > 0 )
/*
*********************************************************************************************************
*   函 数 名: vApplicationStackOverflowHook
*   功能说明: vApplicationStackOverflowHook 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    printf("\r\n[RTOS] STACK OVERFLOW: %s\r\n", (pcTaskName != NULL) ? pcTaskName : "?");
    for (;;) {}
}
#endif

#if ( configUSE_MALLOC_FAILED_HOOK == 1 )
/*
*********************************************************************************************************
*   函 数 名: vApplicationMallocFailedHook
*   功能说明: vApplicationMallocFailedHook 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void vApplicationMallocFailedHook(void)
{
    printf("\r\n[RTOS] pvPortMalloc failed (heap exhausted)\r\n");
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}
#endif
