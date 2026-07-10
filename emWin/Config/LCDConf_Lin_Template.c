/**
 * @file LCDConf_Lin_Template.c
 * @brief emWin 显示驱动配置：逻辑横屏缓冲、90° 旋转、RenderLatch、VSYNC 提交到 LTDC 物理 ping/pong。
 */

#include "bsp.h"
#include "y_t_stress_ab.h"
#include "profile_opts.h"
#include "lcd_rotate_profile.h"
#include "GUI.h"
#include "GUI_Private.h"
#include "GUIDRV_Lin.h"
#include "bsp_fmc_sdram.h"
#include "bsp_vram_layout.h"
#include "bsp_lcd_rgb.h"
#include "dma2d_wave.h"
#include "dma2d_wait.h"
#include "dma2d_nlr.h"
#include "lcd_rotate_request.h"
#include "ui_display_light_path.h"
#include "ui_app_config.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include <stdint.h>
#include <string.h>

/* Render Latch：DWT 周期累计 + 每帧 fu 采样（串口周期日志取平均） */
static void lcdconf_ensure_dwt_cyccnt(void)
{
    if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0U;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
}

/* fu 跨度起点：示波器帧由 MarkScopeFrameFuStart 在帧首采样；其它路径在 Prepare 开头采样。 */
static uint32_t s_rl_fu_span_start;
static uint8_t s_rl_fu_span_scope_mark;
static uint32_t s_rl_prep_saved_cy;
static uint32_t s_rl_frames_acc;
static uint64_t s_rl_sum_prep_cy;
static uint64_t s_rl_sum_commit_cy;
static uint64_t s_rl_sum_fu_delta;

void LCDConf_RenderLatch_MarkScopeFrameFuStart(void)
{
    s_rl_fu_span_start       = g_lcdconf_ltdc_fu_cnt;
    s_rl_fu_span_scope_mark  = 1u;
}

void LCDConf_RenderLatchMetrics_FlushForLog(uint32_t *out_frames,
                                            uint32_t *out_avg_prep_cy,
                                            uint32_t *out_avg_commit_cy,
                                            uint32_t *out_avg_fu_delta,
                                            uint64_t *out_sum_fu_delta)
{
    uint32_t n = s_rl_frames_acc;

    if (out_frames != NULL) {
        *out_frames = n;
    }
    if (out_sum_fu_delta != NULL) {
        *out_sum_fu_delta = s_rl_sum_fu_delta;
    }
    if (n > 0U) {
        if (out_avg_prep_cy != NULL) {
            *out_avg_prep_cy = (uint32_t)(s_rl_sum_prep_cy / (uint64_t)n);
        }
        if (out_avg_commit_cy != NULL) {
            *out_avg_commit_cy = (uint32_t)(s_rl_sum_commit_cy / (uint64_t)n);
        }
        if (out_avg_fu_delta != NULL) {
            *out_avg_fu_delta = (uint32_t)(s_rl_sum_fu_delta / (uint64_t)n);
        }
    } else {
        if (out_avg_prep_cy != NULL) {
            *out_avg_prep_cy = 0U;
        }
        if (out_avg_commit_cy != NULL) {
            *out_avg_commit_cy = 0U;
        }
        if (out_avg_fu_delta != NULL) {
            *out_avg_fu_delta = 0U;
        }
    }
    s_rl_frames_acc     = 0U;
    s_rl_sum_prep_cy    = 0ULL;
    s_rl_sum_commit_cy  = 0ULL;
    s_rl_sum_fu_delta   = 0ULL;
}

/* 绕过旋转测试开关 */
#ifndef LCD_BYPASS_ROTATION
#define LCD_BYPASS_ROTATION     0  /* 1=绕过旋转, 0=正常旋转 */
#endif

/* ManualRotate 等 VSYNC：行中断未在时限内到达时勿永久阻塞（上底板噪声/电源可能导致偶发丢 LIF） */
#ifndef LCDCONF_VSYNC_WAIT_MS
#define LCDCONF_VSYNC_WAIT_MS  120U
#endif

/* ?? BSP ???? LTDC ????? LCDConf ?????? hltdc */
#define hltdc g_hltdc




typedef struct
{
  int32_t      address;          
  __IO int32_t      pending_buffer;   
  int32_t      buffer_index;     
  int32_t      xSize;            
  int32_t      ySize;            
  int32_t      BytesPerPixel;
  LCD_API_COLOR_CONV   *pColorConvAPI;
}LCD_LayerPropTypedef;


#define DrawBitmapA4Enalbe    0
#define ClearCacheHookEnalbe  0  /* CPU 直写帧缓冲，DMA2D 不再访问 SDRAM，无需在每次 FillRect 前清 Cache */
#if (Y_T_AB_DISABLE_DMA2D_FILL_COPY != 0)
#define DMA2D_USE_IN_FILL     0  /* A/B：关 DMA2D，走 CPU */
#define DMA2D_USE_IN_COPY     0
#else
#define DMA2D_USE_IN_FILL     1  /* 1=DMA2D R2M（逻辑缓冲 WT 时可用），0=CPU 循环 */
#define DMA2D_USE_IN_COPY     1  /* 1=DMA2D M2M（逻辑缓冲 WT 时可用），0=CPU 循环 */
#endif

#define XSIZE_PHYS       800
#define YSIZE_PHYS       480

/* 2. ??????????????????????????????????????? */
#define ROTATION_0       0
#define ROTATION_CW      1
#define ROTATION_180     2
#define ROTATION_CCW     3


#define CMS_ARGB8888 1
#define CMS_RGB888   2
#define CMS_RGB565   3
#define CMS_ARGB1555 4
#define CMS_ARGB4444 5
#define CMS_L8       6
#define CMS_AL44     7
#define CMS_AL88     8

/* 4. ???? / ?????????????????????????????emWin????? */
#define NUM_BUFFERS      1 /* 单缓冲：emWin 仅用一个逻辑缓冲，旋转由 ManualRotateToPhysical() 手动触发 */
#define NUM_VSCREENS     1 /* ?????????????? */


#undef  GUI_NUM_LAYERS
#define GUI_NUM_LAYERS 1


/*
 * 显存地址布局（必须与 main.c 中的宏保持一致）：
 *
 *   0xC0000000 ~ 0xC00BB7FF  逻辑横屏子帧 0（800×480×2 = 768 KB）  emWin 绘制
 *   0xC00BB800 ~ 0xC0176FFF  逻辑横屏子帧 1（800×480×2 = 768 KB）  emWin 绘制
 *   0xC0200000 ~ 0xC02BB7FF  物理竖屏缓冲 0（480×800×2 = 768 KB）  LTDC 扫描
 *   0xC0300000 ~ 0xC03BB7FF  物理竖屏缓冲 1（480×800×2 = 768 KB）  LTDC 扫描
 *
 * 逻辑子帧 1 最高地址 0xC0176FFF < 物理缓冲 0 起始地址 0xC0200000，无重叠。
 * VRAM_* 宏见 bsp_vram_layout.h。
 */



/* 7. ???????1??????????????? */
#define COLOR_MODE_0  CMS_RGB565
#define ORIENTATION_0 ROTATION_0
#define XSIZE_0       XSIZE_PHYS
#define YSIZE_0       YSIZE_PHYS

/* 8. ???????2???????????????? */
#define COLOR_MODE_1  CMS_RGB565
#define ORIENTATION_1 ROTATION_CW
#define XSIZE_1       XSIZE_PHYS
#define YSIZE_1       YSIZE_PHYS

/* 9. ????????????????????, ??????? */
#define BK_COLOR      GUI_DARKBLUE


/* 10. ??????????????????????????????????????1??emWin??????? */
#if   (COLOR_MODE_0 == CMS_ARGB8888)
  #define COLOR_CONVERSION_0 GUICC_M8888I
#elif (COLOR_MODE_0 == CMS_RGB888)
  #define COLOR_CONVERSION_0 GUICC_M888
#elif (COLOR_MODE_0 == CMS_RGB565)
  #define COLOR_CONVERSION_0 GUICC_M565
#elif (COLOR_MODE_0 == CMS_ARGB1555)
  #define COLOR_CONVERSION_0 GUICC_M1555I
#elif (COLOR_MODE_0 == CMS_ARGB4444)
  #define COLOR_CONVERSION_0 GUICC_M4444I
#elif (COLOR_MODE_0 == CMS_L8)
  #define COLOR_CONVERSION_0 GUICC_8666
#elif (COLOR_MODE_0 == CMS_AL44)
  #define COLOR_CONVERSION_0 GUICC_1616I
#elif (COLOR_MODE_0 == CMS_AL88)
  #define COLOR_CONVERSION_0 GUICC_88666I
#else
  #error Illegal color mode 0!
#endif

/* 11. ??????????????????????????????????????1??emWin?????? */
#if   (COLOR_MODE_0 == CMS_ARGB8888)
  #if   (ORIENTATION_0 == ROTATION_0)
    #define DISPLAY_DRIVER_0   GUIDRV_LIN_32
  #elif (ORIENTATION_0 == ROTATION_CW)
    #define DISPLAY_DRIVER_0   GUIDRV_LIN_OSX_32
  #elif (ORIENTATION_0 == ROTATION_180)
    #define DISPLAY_DRIVER_0   GUIDRV_LIN_OXY_32
  #elif (ORIENTATION_0 == ROTATION_CCW)
    #define DISPLAY_DRIVER_0   GUIDRV_LIN_OSY_32
  #endif
#elif (COLOR_MODE_0 == CMS_RGB888)
  #if   (ORIENTATION_0 == ROTATION_0)
    #define DISPLAY_DRIVER_0   GUIDRV_LIN_24
  #elif (ORIENTATION_0 == ROTATION_CW)
    #define DISPLAY_DRIVER_0   GUIDRV_LIN_OSX_24
  #elif (ORIENTATION_0 == ROTATION_180)
    #define DISPLAY_DRIVER_0   GUIDRV_LIN_OXY_24
  #elif (ORIENTATION_0 == ROTATION_CCW)
    #define DISPLAY_DRIVER_0   GUIDRV_LIN_OSY_24
  #endif
#elif (COLOR_MODE_0 == CMS_RGB565)   \
   || (COLOR_MODE_0 == CMS_ARGB1555) \
   || (COLOR_MODE_0 == CMS_ARGB4444) \
   || (COLOR_MODE_0 == CMS_AL88)
  #if   (ORIENTATION_0 == ROTATION_0)
    #define DISPLAY_DRIVER_0   GUIDRV_LIN_16
  #elif (ORIENTATION_0 == ROTATION_CW)
    #define DISPLAY_DRIVER_0   GUIDRV_LIN_OSX_16
  #elif (ORIENTATION_0 == ROTATION_180)
    #define DISPLAY_DRIVER_0   GUIDRV_LIN_OXY_16
  #elif (ORIENTATION_0 == ROTATION_CCW)
    #define DISPLAY_DRIVER_0   GUIDRV_LIN_OSY_16
  #endif
#elif (COLOR_MODE_0 == CMS_L8)   \
   || (COLOR_MODE_0 == CMS_AL44)
  #if   (ORIENTATION_0 == ROTATION_0)
    #define DISPLAY_DRIVER_0   GUIDRV_LIN_8
  #elif (ORIENTATION_0 == ROTATION_CW)
    #define DISPLAY_DRIVER_0   GUIDRV_LIN_OSX_8
  #elif (ORIENTATION_0 == ROTATION_180)
    #define DISPLAY_DRIVER_0   GUIDRV_LIN_OXY_8
  #elif (ORIENTATION_0 == ROTATION_CCW)
    #define DISPLAY_DRIVER_0   GUIDRV_LIN_OSY_8
  #endif
#endif


/* 12. ?????????????????????????????????????2??emWin??????? */
#if (GUI_NUM_LAYERS > 1)
#if   (COLOR_MODE_1 == CMS_ARGB8888)
  #define COLOR_CONVERSION_1 GUICC_M8888I
#elif (COLOR_MODE_1 == CMS_RGB888)
  #define COLOR_CONVERSION_1 GUICC_M888
#elif (COLOR_MODE_1 == CMS_RGB565)
  #define COLOR_CONVERSION_1 GUICC_M565
#elif (COLOR_MODE_1 == CMS_ARGB1555)
  #define COLOR_CONVERSION_1 GUICC_M1555I
#elif (COLOR_MODE_1 == CMS_ARGB4444)
  #define COLOR_CONVERSION_1 GUICC_M4444I
#elif (COLOR_MODE_1 == CMS_L8)
  #define COLOR_CONVERSION_1 GUICC_8666
#elif (COLOR_MODE_1 == CMS_AL44)
  #define COLOR_CONVERSION_1 GUICC_1616I
#elif (COLOR_MODE_1 == CMS_AL88)
  #define COLOR_CONVERSION_1 GUICC_88666I
#else
  #error Illegal color mode 0!
#endif

/* 13. ?????????????????????????????????????2??emWin?????? */
#if   (COLOR_MODE_1 == CMS_ARGB8888)
  #if   (ORIENTATION_1 == ROTATION_0)
    #define DISPLAY_DRIVER_1   GUIDRV_LIN_32
  #elif (ORIENTATION_1 == ROTATION_CW)
    #define DISPLAY_DRIVER_1   GUIDRV_LIN_OSX_32
  #elif (ORIENTATION_1 == ROTATION_180)
    #define DISPLAY_DRIVER_1   GUIDRV_LIN_OXY_32
  #elif (ORIENTATION_1 == ROTATION_CCW)
    #define DISPLAY_DRIVER_1   GUIDRV_LIN_OSY_32
  #endif
#elif (COLOR_MODE_1 == CMS_RGB888)
  #if   (ORIENTATION_1 == ROTATION_0)
    #define DISPLAY_DRIVER_1   GUIDRV_LIN_24
  #elif (ORIENTATION_1 == ROTATION_CW)
    #define DISPLAY_DRIVER_1   GUIDRV_LIN_OSX_24
  #elif (ORIENTATION_1 == ROTATION_180)
    #define DISPLAY_DRIVER_1   GUIDRV_LIN_OXY_24
  #elif (ORIENTATION_1 == ROTATION_CCW)
    #define DISPLAY_DRIVER_1   GUIDRV_LIN_OSY_24
  #endif
#elif (COLOR_MODE_1 == CMS_RGB565)   \
   || (COLOR_MODE_1 == CMS_ARGB1555) \
   || (COLOR_MODE_1 == CMS_ARGB4444) \
   || (COLOR_MODE_1 == CMS_AL88)
  #if   (ORIENTATION_1 == ROTATION_0)
    #define DISPLAY_DRIVER_1   GUIDRV_LIN_16
  #elif (ORIENTATION_1 == ROTATION_CW)
    #define DISPLAY_DRIVER_1   GUIDRV_LIN_OSX_16
  #elif (ORIENTATION_1 == ROTATION_180)
    #define DISPLAY_DRIVER_1   GUIDRV_LIN_OXY_16
  #elif (ORIENTATION_1 == ROTATION_CCW)
    #define DISPLAY_DRIVER_1   GUIDRV_LIN_OSY_16
  #endif
#elif (COLOR_MODE_1 == CMS_L8)   \
   || (COLOR_MODE_1 == CMS_AL44)
  #if   (ORIENTATION_1 == ROTATION_0)
    #define DISPLAY_DRIVER_1   GUIDRV_LIN_8
  #elif (ORIENTATION_1 == ROTATION_CW)
    #define DISPLAY_DRIVER_1   GUIDRV_LIN_OSX_8
  #elif (ORIENTATION_1 == ROTATION_180)
    #define DISPLAY_DRIVER_1   GUIDRV_LIN_OXY_8
  #elif (ORIENTATION_1 == ROTATION_CCW)
    #define DISPLAY_DRIVER_1   GUIDRV_LIN_OSY_8
  #endif
#endif

#else

#undef XSIZE_0
#undef YSIZE_0
#define XSIZE_0 XSIZE_PHYS
#define YSIZE_0 YSIZE_PHYS
     
#endif

/*14. ???????????????????????????????????*/
#if NUM_BUFFERS > 3
  #error More than 3 buffers make no sense and are not supported in this configuration file!
#endif
#ifndef   XSIZE_PHYS
  #error Physical X size of display is not defined!
#endif
#ifndef   YSIZE_PHYS
  #error Physical Y size of display is not defined!
#endif
#ifndef   NUM_BUFFERS
  #define NUM_BUFFERS 1
#else
  #if (NUM_BUFFERS <= 0)
    #error At least one buffer needs to be defined!
  #endif
#endif
#ifndef   NUM_VSCREENS
  #define NUM_VSCREENS 1
#else
  #if (NUM_VSCREENS <= 0)
    #error At least one screeen needs to be defined!
  #endif
#endif
#if (NUM_VSCREENS > 1) && (NUM_BUFFERS > 1)
  #error Virtual screens together with multiple buffers are not allowed!
#endif
     
/*
**********************************************************************************************************
									???DMA2D?????????????????
**********************************************************************************************************
*/
#define DEFINE_DMA2D_COLORCONVERSION(PFIX, PIXELFORMAT)                                                        \
static void _Color2IndexBulk_##PFIX##_DMA2D(LCD_COLOR * pColor, void * pIndex, U32 NumItems, U8 SizeOfIndex) { \
  _DMA_Color2IndexBulk(pColor, pIndex, NumItems, SizeOfIndex, PIXELFORMAT);                                    \
}                                                                                                              \
static void _Index2ColorBulk_##PFIX##_DMA2D(void * pIndex, LCD_COLOR * pColor, U32 NumItems, U8 SizeOfIndex) { \
  _DMA_Index2ColorBulk(pIndex, pColor, NumItems, SizeOfIndex, PIXELFORMAT);                                    \
}
 
/* ???????? */
static void _DMA_Index2ColorBulk(void * pIndex, LCD_COLOR * pColor, U32 NumItems, U8 SizeOfIndex, U32 PixelFormat);
static void _DMA_Color2IndexBulk(LCD_COLOR * pColor, void * pIndex, U32 NumItems, U8 SizeOfIndex, U32 PixelFormat);

/* ?????? */
DEFINE_DMA2D_COLORCONVERSION(M8888I, LTDC_PIXEL_FORMAT_ARGB8888)
DEFINE_DMA2D_COLORCONVERSION(M888,   LTDC_PIXEL_FORMAT_ARGB8888) 
DEFINE_DMA2D_COLORCONVERSION(M565,   LTDC_PIXEL_FORMAT_RGB565)
DEFINE_DMA2D_COLORCONVERSION(M1555I, LTDC_PIXEL_FORMAT_ARGB1555)
DEFINE_DMA2D_COLORCONVERSION(M4444I, LTDC_PIXEL_FORMAT_ARGB4444)


/* 当前 LTDC 正在扫描的物理缓冲序号（0 或 1）。ISR 和任务均访问，必须 volatile。 */
static volatile uint8_t active_phys_buffer = 0U;
/* VSYNC 同步标志：_CustomCopyBuffer 置 1，LTDC_IRQHandler 清 0。 */
static volatile uint8_t vsync_pending = 0U;
/* LTDC 行中断在 VFP 完成 IMR 后 Give，任务侧 Take 阻塞，替代 vTaskDelay(1) 轮询 vsync_pending */
static SemaphoreHandle_t s_ltdc_vsync_sem;

/* 超时次数：调试或串口周期性打印可观察；底板环境下若持续递增需查电源/LTDC 时钟 */
volatile uint32_t g_lcdconf_vsync_timeout_cnt = 0U;
/* LTDC FIFO underrun 次数：持续增长通常表示 LTDC 读显存被 DMA/CPU/DMA2D/ADC DMA 抢占过久。 */
volatile uint32_t g_lcdconf_ltdc_fu_cnt = 0U;

static void LCDConf_WaitVsyncReload(void)
{
#if (Y_T_AB_USE_LTDC_VBR_RELOAD != 0)
    uint32_t start;

    start = HAL_GetTick();
    while ((LTDC->ISR & LTDC_ISR_RRIF) == 0U) {
        if ((uint32_t)(HAL_GetTick() - start) >= LCDCONF_VSYNC_WAIT_MS) {
            g_lcdconf_vsync_timeout_cnt++;
            return;
        }
        taskYIELD();
    }
    LTDC->ICR = LTDC_ICR_CRRIF;
    active_phys_buffer = (active_phys_buffer == 0U) ? 1U : 0U;
#else
    if (s_ltdc_vsync_sem == NULL) {
        s_ltdc_vsync_sem = xSemaphoreCreateBinary();
    }
    while (xSemaphoreTake(s_ltdc_vsync_sem, 0) == pdTRUE) {
    }
    vsync_pending = 1U;
    if (xSemaphoreTake(s_ltdc_vsync_sem, pdMS_TO_TICKS(LCDCONF_VSYNC_WAIT_MS)) != pdTRUE) {
        /* 偏正确性：不在任务里伪造 IMR / 翻 active_phys_buffer / 清 vsync_pending。
         * 账本与「LTDC 实际在扫哪块」仅由 LTDC_IRQHandler 在 VFP 翻面，避免 Ping-Pong 与
         * 硬件不一致→整屏上下错位。代价：本帧可能晚几线～一帧才真正切到新 CFBAR。
         * vsync_pending 保持 1，行中断仍可到后补完重载（见 LTDC_IRQHandler）。
         * 若 IRQ 长期不达，会连续超时；仅靠 g_lcdconf_vsync_timeout_cnt 观测。 */
        g_lcdconf_vsync_timeout_cnt++;
    }
#endif
}
volatile uint32_t g_copy_buffer_count = 0U;  /* 调试计数器 */

/* _aBufferIndex：记录 emWin 当前绘制目标的逻辑子帧索引（0 或 1）。
 * 供 _LCD_FillRect / LCD_DMA2D_GetDrawBufBase 计算正确的逻辑缓冲地址。
 * 声明提前到此处，确保 _CustomCopyBuffer 可见（C 作用域规则）。      */
static int _aBufferIndex[GUI_NUM_LAYERS];

/*
 * 简单旋转算法（非优化版本，用于调试）
 * 顺时针旋转 90 度：逻辑横屏(800x480) -> 物理竖屏(480x800)
 */
void Simple_Rotate_90CW_RGB565(const uint16_t *src, uint16_t *dst)
{
    int src_x, src_y;
    for (src_y = 0; src_y < 480; src_y++) {
        for (src_x = 0; src_x < 800; src_x++) {
            int dst_x = 479 - src_y;
            int dst_y = src_x;
            dst[dst_y * 480 + dst_x] = src[src_y * 800 + src_x];
        }
    }
}

/*
 * 极致优化的 RGB565 分块旋转算法
 * 顺时针旋转 90 度：逻辑横屏(800x480) -> 物理竖屏(480x800)
 * 32x32 像素块：更好利用 L1 Cache（32×32×2=2KB）
 */
#define ROTATE_BLOCK  32
#if defined(__GNUC__) || (defined(__ARMCC_VERSION) && __ARMCC_VERSION >= 6010050)
#define ROTATE_PREFETCH(addr)  __builtin_prefetch((addr), 0, 3)
#else
#define ROTATE_PREFETCH(addr)  ((void)0)
#endif

#if defined(__CC_ARM)
#pragma O3
void Optimize_Rotate_90CW_RGB565(const uint16_t * __restrict src, uint16_t * __restrict dst)
#else
__attribute__((optimize("O3")))
void Optimize_Rotate_90CW_RGB565(const uint16_t * __restrict src, uint16_t * __restrict dst)
#endif
{
    const int logical_w = 800;
    const int logical_h = 480;
    const int phys_w    = 480;
    const int OFFSET    = 5;

    for (int y = 0; y < logical_h; y += ROTATE_BLOCK) {
        for (int x = 0; x < logical_w; x += ROTATE_BLOCK) {
            int next_x = x + ROTATE_BLOCK, next_y = y;
            if (next_x >= logical_w) { next_x = 0; next_y = y + ROTATE_BLOCK; }
            if (next_y < logical_h)
                ROTATE_PREFETCH(&src[next_y * logical_w + next_x]);

            int j_max = (y + ROTATE_BLOCK <= logical_h) ? ROTATE_BLOCK : (logical_h - y);
            int i_max = (x + ROTATE_BLOCK <= logical_w) ? ROTATE_BLOCK : (logical_w - x);
            for (int j = 0; j < j_max; j++) {
                int src_y  = y + j;
                int phys_x = (479 - OFFSET) - src_y;
                if (phys_x < 0) continue;
                const uint16_t *pSrc = &src[src_y * logical_w + x];
                uint16_t       *pDst = &dst[x * phys_w + phys_x];
                int i = 0;
                for (; i + 4 <= i_max; i += 4) {
                    pDst[0] = pSrc[0]; pDst[phys_w] = pSrc[1];
                    pDst[phys_w * 2] = pSrc[2]; pDst[phys_w * 3] = pSrc[3];
                    pSrc += 4; pDst += phys_w * 4;
                }
                for (; i < i_max; i++) {
                    *pDst = *pSrc++;
                    pDst += phys_w;
                }
            }
        }
    }
}

/* 逆时针 90° 整屏（与 dma2d_wave.h 中 LCD_USE_CCW_ROTATION 配套） */
#if defined(__CC_ARM)
#pragma O3
void Optimize_Rotate_90CCW_RGB565(const uint16_t * __restrict src, uint16_t * __restrict dst)
#else
__attribute__((optimize("O3")))
void Optimize_Rotate_90CCW_RGB565(const uint16_t * __restrict src, uint16_t * __restrict dst)
#endif
{
    const int logical_w = (int)VRAM_XLOG;
    const int logical_h = (int)VRAM_YLOG;
    const int phys_w    = (int)VRAM_XPHYS;
    const int phys_h    = (int)VRAM_YPHYS;

    if (src == NULL || dst == NULL) {
        return;
    }

    for (int y = 0; y < logical_h; y += ROTATE_BLOCK) {
        for (int x = 0; x < logical_w; x += ROTATE_BLOCK) {
            int next_x = x + ROTATE_BLOCK, next_y = y;
            if (next_x >= logical_w) { next_x = 0; next_y = y + ROTATE_BLOCK; }
            if (next_y < logical_h)
                ROTATE_PREFETCH(&src[(uint32_t)next_y * (uint32_t)logical_w + (uint32_t)next_x]);

            int j_max = (y + ROTATE_BLOCK <= logical_h) ? ROTATE_BLOCK : (logical_h - y);
            int i_max = (x + ROTATE_BLOCK <= logical_w) ? ROTATE_BLOCK : (logical_w - x);
            for (int j = 0; j < j_max; j++) {
                int src_y  = y + j;
                int phys_x = src_y + LCD_CCW_PHYS_OFFSET_X;
                if (phys_x < 0 || phys_x >= phys_w) {
                    continue;
                }
                int phys_y_start = phys_h - 1 - (x + i_max - 1) + LCD_CCW_PHYS_OFFSET_Y;
                int copy_w = i_max;
                int skip   = 0;
                const uint16_t *pSrc = &src[(uint32_t)src_y * (uint32_t)logical_w + (uint32_t)x];
                if (phys_y_start < 0) {
                    skip = -phys_y_start;
                    if (skip >= copy_w) {
                        continue;
                    }
                    pSrc += skip;
                    copy_w -= skip;
                    phys_y_start = 0;
                }
                if (phys_y_start + copy_w > phys_h) {
                    copy_w = phys_h - phys_y_start;
                }
                /* 本块在源行内实际可读列 = i_max - skip（与物理裁剪无关，防越界读） */
                {
                    int src_cols_max = i_max - skip;
                    if (src_cols_max > 0 && copy_w > src_cols_max) {
                        copy_w = src_cols_max;
                    }
                }
                if (copy_w <= 0) {
                    continue;
                }
                uint16_t *pDst = &dst[(uint32_t)phys_y_start * (uint32_t)phys_w + (uint32_t)phys_x];
                int i = 0;
                for (; i + 4 <= copy_w; i += 4) {
                    pDst[0] = pSrc[0]; pDst[phys_w] = pSrc[1];
                    pDst[phys_w * 2] = pSrc[2]; pDst[phys_w * 3] = pSrc[3];
                    pSrc += 4; pDst += phys_w * 4;
                }
                for (; i < copy_w; i++) {
                    *pDst = *pSrc++;
                    pDst += phys_w;
                }
            }
        }
    }
}

#if LCD_USE_CCW_ROTATION
#define ROTATE_FULL_SCREEN  Optimize_Rotate_90CCW_RGB565
#else
#define ROTATE_FULL_SCREEN  Optimize_Rotate_90CW_RGB565
#endif

/* 1=部分旋转+PlotBuf；0=整屏 CPU 旋转 */
#ifndef USE_PARTIAL_ROTATE
#define USE_PARTIAL_ROTATE  1
#endif

#if USE_PARTIAL_ROTATE
/* header 区域：模式/档位变化时需重绘，须随每帧旋转 */
#define ROTATE_HEADER_X  0
#define ROTATE_HEADER_Y  0
#define ROTATE_HEADER_W  800
#define ROTATE_HEADER_H  160  /* 与 MainTask HDR_H 一致 */
/* plot 区域：须与 MainTask PLOT_X/PLOT_Y/PLOT_W/PLOT_H 一致（HDR_H + 2 + 10） */
#define ROTATE_PLOT_X  24
#define ROTATE_PLOT_Y  172
#define ROTATE_PLOT_W  608
#define ROTATE_PLOT_H  248
/* PlotBuf 路径：Header 紧下方起整带一次旋转，再 DMA2D 盖波形区。
 * BODY_Y=HEADER_Y+HEADER_H；PLOT_Y 须与此处 ROTATE_PLOT_Y 与 MainTask PLOT_Y 一致。 */
#define ROTATE_BODY_X  0
#define ROTATE_BODY_Y  (ROTATE_HEADER_Y + ROTATE_HEADER_H)
#define ROTATE_BODY_W  800
#define ROTATE_BODY_H  (480 - ROTATE_BODY_Y)
/* PlotBuf 路径：1=仅第一次进入 PlotBuf 分支时 CPU 旋转 ROTATE_BODY，之后靠 Ping-Pong 拷 front 继承边距（省 CPU）。
 * 逻辑边距/刻度若与首帧不同，屏上可能不刷新；接缝异常时改为 0。 */
#ifndef USE_ROTATE_BODY_STATIC_SKIP
#define USE_ROTATE_BODY_STATIC_SKIP  1
#endif
/* USE_ROTATE_HEADER_STATIC_SKIP 默认见 lcd_rotate_request.h：Header 仅首帧/按需 CPU 旋转 */

#if !UI_LIGHT_PATH_RELAX
#if USE_PARTIAL_ROTATE != 1
#error "轻路径要求 USE_PARTIAL_ROTATE==1；整屏旋转对比请定义 UI_LIGHT_PATH_RELAX=1"
#endif
#if USE_ROTATE_BODY_STATIC_SKIP != 1
#error "轻路径要求 USE_ROTATE_BODY_STATIC_SKIP==1；排障请定义 UI_LIGHT_PATH_RELAX=1"
#endif
#endif /* !UI_LIGHT_PATH_RELAX */

/*
 * 物理 plot 区（DMA2D 贴 PlotBuf）：CW→(4,24)；CCW→(76, Y)。
 *
 * 【约 28px 空隙 ≠ 32×32 旋转】ROTATE_BLOCK 仅影响 CPU 旋转循环顺序，每个像素仍会写入；
 * 左/右边距矩形等与 PlotBuf 几何对齐由公式保证，不会因 32 分块少画一条带。
 *
 * 竖屏缓冲高 800，PlotBuf 高 PHYS_PLOT_H（=逻辑 plot 宽 ROTATE_PLOT_W）。
 * CCW 旋转：逻辑 (lx,ly)→物理 phys_x=ly, phys_y=799-lx（见 Optimize_Rotate_90CCW）。
 * plot 右边界 lx=ROTATE_PLOT_X+ROTATE_PLOT_W-1 → 最小 phys_y=800-ROTATE_PLOT_X-ROTATE_PLOT_W，
 * DMA2D 贴图起点须与此一致；勿用 (800-H)/2「居中」，否则会与 ROTATE_BODY 旋出的逻辑网格
 * 错开约 (800-H)/2 - 上式 行，出现双线/网格杂乱/缺角。
 */
#define PHYS_PLOT_W    248
#define PHYS_PLOT_H    608
#if LCD_USE_CCW_ROTATION
/* 整块 PlotBuf 在竖屏上微调：负值 = 向「时间结束侧」平移（逻辑 plot 右侧 → phys_y 减小 → 靠屏上方/波形消失端）。
 * 默认 -3；过大易与边距接缝，改符号可反向。 */
#ifndef PHYS_PLOT_NUDGE_Y_CCW
#define PHYS_PLOT_NUDGE_Y_CCW  (15)
#endif
/*
 * CCW 下逻辑 ly → 物理 phys_x（见 Optimize_Rotate_90CCW_RGB565_Region：phys_x = src_y + offset）。
 * PlotBuf 贴在物理矩形左上角 (PHYS_PLOT_X, PHYS_PLOT_Y)，须随逻辑 plot 顶边 ROTATE_PLOT_Y 一起平移。
 * 旧布局 ROTATE_PLOT_Y=76、经调参得 PHYS_PLOT_X≈90，故基准偏置 (90-76)=14。
 */
#define PHYS_PLOT_X    ((ROTATE_PLOT_Y) + (90 - 76) + (LCD_CCW_PHYS_OFFSET_X))
#define PHYS_PLOT_Y    (((800) - (ROTATE_PLOT_X) - (ROTATE_PLOT_W)) + (PHYS_PLOT_NUDGE_Y_CCW) + (LCD_CCW_PHYS_OFFSET_Y)-10)
#else
#define PHYS_PLOT_X    4
#define PHYS_PLOT_Y    24
#endif

/* 显示优先：条带 M2M 跳过 Plot 区，减少每帧物理前台→后台的 SDRAM 搬运峰值。 */
#ifndef LCDCONF_PHYS_M2M_STRIPED
#define LCDCONF_PHYS_M2M_STRIPED  1
#endif

#if defined(__CC_ARM)
#pragma O3
static void Optimize_Rotate_90CW_RGB565_Region(const uint16_t * __restrict src,
    uint16_t * __restrict dst, int src_x0, int src_y0, int src_w, int src_h, int logical_w)
#else
__attribute__((optimize("O3")))
static void Optimize_Rotate_90CW_RGB565_Region(const uint16_t * __restrict src,
    uint16_t * __restrict dst, int src_x0, int src_y0, int src_w, int src_h, int logical_w)
#endif
{
    const int phys_w  = 480;
    const int OFFSET  = 5;

    for (int y = 0; y < src_h; y += ROTATE_BLOCK) {
        for (int x = 0; x < src_w; x += ROTATE_BLOCK) {
            int next_x = x + ROTATE_BLOCK, next_y = y;
            if (next_x >= src_w) { next_x = 0; next_y = y + ROTATE_BLOCK; }
            if (next_y < src_h)
                ROTATE_PREFETCH(&src[(src_y0 + next_y) * logical_w + src_x0 + next_x]);

            int j_max = (y + ROTATE_BLOCK <= src_h) ? ROTATE_BLOCK : (src_h - y);
            int i_max = (x + ROTATE_BLOCK <= src_w) ? ROTATE_BLOCK : (src_w - x);
            for (int j = 0; j < j_max; j++) {
                int src_y  = src_y0 + y + j;
                int phys_x = (479 - OFFSET) - src_y;
                if (phys_x < 0) continue;
                const uint16_t *pSrc = &src[src_y * logical_w + src_x0 + x];
                uint16_t       *pDst = &dst[(src_x0 + x) * phys_w + phys_x];
                int i = 0;
                for (; i + 4 <= i_max; i += 4) {
                    pDst[0] = pSrc[0]; pDst[phys_w] = pSrc[1];
                    pDst[phys_w * 2] = pSrc[2]; pDst[phys_w * 3] = pSrc[3];
                    pSrc += 4; pDst += phys_w * 4;
                }
                for (; i < i_max; i++) {
                    *pDst = *pSrc++;
                    pDst += phys_w;
                }
            }
        }
    }
}

#if defined(__CC_ARM)
#pragma O3
static void Optimize_Rotate_90CCW_RGB565_Region(const uint16_t * __restrict src,
    uint16_t * __restrict dst, int src_x0, int src_y0, int src_w, int src_h, int logical_w)
#else
__attribute__((optimize("O3")))
static void Optimize_Rotate_90CCW_RGB565_Region(const uint16_t * __restrict src,
    uint16_t * __restrict dst, int src_x0, int src_y0, int src_w, int src_h, int logical_w)
#endif
{
    const int phys_w  = (int)VRAM_XPHYS;
    const int phys_h  = (int)VRAM_YPHYS;
    const int lgh     = (int)VRAM_YLOG;

    if (src == NULL || dst == NULL || src_w <= 0 || src_h <= 0) {
        return;
    }
    if (src_x0 < 0 || src_y0 < 0 || src_x0 + src_w > logical_w
        || src_y0 + src_h > lgh) {
        return;
    }

    for (int y = 0; y < src_h; y += ROTATE_BLOCK) {
        for (int x = 0; x < src_w; x += ROTATE_BLOCK) {
            int next_x = x + ROTATE_BLOCK, next_y = y;
            if (next_x >= src_w) { next_x = 0; next_y = y + ROTATE_BLOCK; }
            if (next_y < src_h)
                ROTATE_PREFETCH(&src[(uint32_t)(src_y0 + next_y) * (uint32_t)logical_w + (uint32_t)src_x0 + (uint32_t)next_x]);

            int j_max = (y + ROTATE_BLOCK <= src_h) ? ROTATE_BLOCK : (src_h - y);
            int i_max = (x + ROTATE_BLOCK <= src_w) ? ROTATE_BLOCK : (src_w - x);
            for (int j = 0; j < j_max; j++) {
                int src_y  = src_y0 + y + j;
                int phys_x = src_y + LCD_CCW_PHYS_OFFSET_X;
                if (phys_x < 0 || phys_x >= phys_w) {
                    continue;
                }
                int phys_y_start = phys_h - 1 - (src_x0 + x + i_max - 1) + LCD_CCW_PHYS_OFFSET_Y;
                int copy_w = i_max;
                int skip   = 0;
                const uint16_t *pSrc = &src[(uint32_t)src_y * (uint32_t)logical_w + (uint32_t)src_x0 + (uint32_t)x];
                if (phys_y_start < 0) {
                    skip = -phys_y_start;
                    if (skip >= copy_w) {
                        continue;
                    }
                    pSrc += skip;
                    copy_w -= skip;
                    phys_y_start = 0;
                }
                if (phys_y_start + copy_w > phys_h) {
                    copy_w = phys_h - phys_y_start;
                }
                {
                    int src_cols_max = i_max - skip;
                    if (src_cols_max > 0 && copy_w > src_cols_max) {
                        copy_w = src_cols_max;
                    }
                }
                if (copy_w <= 0) {
                    continue;
                }
                uint16_t *pDst = &dst[(uint32_t)phys_y_start * (uint32_t)phys_w + (uint32_t)phys_x];
                int i = 0;
                for (; i + 4 <= copy_w; i += 4) {
                    pDst[0] = pSrc[0]; pDst[phys_w] = pSrc[1];
                    pDst[phys_w * 2] = pSrc[2]; pDst[phys_w * 3] = pSrc[3];
                    pSrc += 4; pDst += phys_w * 4;
                }
                for (; i < copy_w; i++) {
                    *pDst = *pSrc++;
                    pDst += phys_w;
                }
            }
        }
    }
}

/*
 * Header 专用（CCW）：phys_x=ly 不变；phys_y 与源列由 dma2d_wave.h 中
 * LCD_HEADER_CCW_HEADER_MIRROR_LX / LCD_HEADER_CCW_HEADER_PHYS_Y_MAP_LX 组合。
 * 勿对 phys_x 做 65-ly 替换 ly（标题与 CH 带会上下对调）。
 */
#if defined(__CC_ARM)
#pragma O3
static void Optimize_Rotate_90CCW_RGB565_Header_Custom(const uint16_t * __restrict src,
    uint16_t * __restrict dst, int src_x0, int src_y0, int src_w, int src_h, int logical_w)
#else
__attribute__((optimize("O3")))
static void Optimize_Rotate_90CCW_RGB565_Header_Custom(const uint16_t * __restrict src,
    uint16_t * __restrict dst, int src_x0, int src_y0, int src_w, int src_h, int logical_w)
#endif
{
    const int phys_w  = (int)VRAM_XPHYS;
    const int phys_h  = (int)VRAM_YPHYS;
    const int lgh     = (int)VRAM_YLOG;

    if (src == NULL || dst == NULL || src_w <= 0 || src_h <= 0) {
        return;
    }
    if (src_x0 < 0 || src_y0 < 0 || src_x0 + src_w > logical_w
        || src_y0 + src_h > lgh) {
        return;
    }

    for (int ly = src_y0; ly < src_y0 + src_h; ly++) {
        int phys_x = ly + LCD_CCW_PHYS_OFFSET_X;
        if (phys_x < 0 || phys_x >= phys_w) {
            continue;
        }
        for (int lx = src_x0; lx < src_x0 + src_w; lx++) {
#if LCD_HEADER_CCW_HEADER_MIRROR_LX
            int lx_read = src_x0 + src_w - 1 - (lx - src_x0);
#else
            int lx_read = lx;
#endif
            uint16_t c = src[(uint32_t)ly * (uint32_t)logical_w + (uint32_t)lx_read];
#if LCD_HEADER_CCW_HEADER_PHYS_Y_MAP_LX
            int phys_y = lx + LCD_CCW_PHYS_OFFSET_Y;
#else
            int phys_y = phys_h - 1 - lx + LCD_CCW_PHYS_OFFSET_Y;
#endif
            if (phys_y < 0 || phys_y >= phys_h) {
                continue;
            }
            dst[(uint32_t)phys_y * (uint32_t)phys_w + (uint32_t)phys_x] = c;
        }
    }
}

#if LCD_USE_CCW_ROTATION
#define ROTATE_REGION  Optimize_Rotate_90CCW_RGB565_Region
#else
#define ROTATE_REGION  Optimize_Rotate_90CW_RGB565_Region
#endif

#endif /* USE_PARTIAL_ROTATE */

/*
 * _CustomCopyBuffer — 拦截 emWin 的帧缓冲拷贝事件，实现：
 *   1. 从 emWin 当前逻辑横屏子帧（IndexSrc）读取像素
 *   2. 使用 16×16 Cache-Line 分块旋转写入物理竖屏后台缓冲（Ping-Pong）
 *   3. Clean D-Cache → 将数据从 L1 Cache 推入 SDRAM（LTDC 直接读 SDRAM）
 *   4. 在 VSYNC 消隐期（VFP）切换 LTDC 地址，彻底消除画面撕裂
 *   5. 向 emWin 确认缓冲切换完成，释放旧帧供下次绘制
 *
 * 调用时机：emWin 在 GUI_MULTIBUF_End() 后自动调用（NUM_BUFFERS > 1 时有效）。
 */
static void _CustomCopyBuffer(int LayerIndex, int IndexSrc, int IndexDst)
{
    uint32_t src_addr;
    uint32_t dst_phys_addr;
    const uint16_t *src_logic;
    uint16_t       *dst_phys;

    g_copy_buffer_count++;

    /* ── 步骤 1：计算本次 emWin 绘制所用的逻辑子帧起始地址 ─────────────────
     * emWin 用 IndexSrc 区分两块逻辑子帧，偏移量 = IndexSrc × 单帧字节数。
     * 单帧：800×480×2 = 768 000 B = 0x000BB800 B。
     * IndexSrc=0 → 0xC0000000；IndexSrc=1 → 0xC00BB800。
     * ──────────────────────────────────────────────────────────────────────── */
    src_addr  = VRAM_LOGICAL_ADDR + (uint32_t)IndexSrc * (800U * 480U * 2U);
    src_logic = (const uint16_t *)src_addr;

    /* ── 步骤 2：选择物理后台缓冲（当前 LTDC 扫描哪块，就写另一块）──────── */
    dst_phys_addr = (active_phys_buffer == 0U) ? VRAM_PHYSICAL_1_ADDR
                                                : VRAM_PHYSICAL_0_ADDR;
    dst_phys = (uint16_t *)dst_phys_addr;

    /* ── 步骤 3：旋转前强制刷新逻辑缓冲区 D-Cache ──────────────────────────
     * 背景原理（必读）：
     *   DMA2D 是 AHB 总线主设备，直接读写 SDRAM，完全绕过 CPU 的 L1 D-Cache。
     *   当 emWin 调用 GUI_FillRect（内部用 DMA2D 填背景色）时：
     *     1. _ClearCacheHook 先 Clean D-Cache（把 CPU 脏行推入 SDRAM）
     *     2. DMA2D 写 SDRAM → SDRAM 里是新背景色
     *   但此刻 D-Cache 仍保有旧数据（Clean 不 Invalidate = Stale）。
     *   下面旋转函数是纯 CPU 读操作，会优先命中 D-Cache——读到的是
     *   旧的波形数据，而非 DMA2D 刚填好的背景色。
     *   结果：旋转输出的物理缓冲里背景区域依然是旧波形 → 残影。
     *
     * 修复：CleanInvalidate 整个逻辑子帧
     *   Clean  → 把 CPU 刚画的网格线/波形（Dirty行）推入 SDRAM
     *   Invalidate → 强制驱逐所有 Stale 行（包括 DMA2D 写之前被 Clean 留下的旧行）
     *   之后旋转函数读 SDRAM → 得到"DMA2D背景 + CPU网格 + CPU波形"完整正确帧
     * ──────────────────────────────────────────────────────────────────────── */
    SCB_CleanInvalidateDCache_by_Addr((uint32_t *)src_logic, 800U * 480U * 2U);

    /* ── 步骤 4：32×32 分块整屏旋转（方向见 dma2d_wave.h LCD_USE_CCW_ROTATION）── */
    ROTATE_FULL_SCREEN(src_logic, dst_phys);

    /* ── 步骤 5：Clean 物理缓冲 D-Cache ────────────────────────────────────
     * SDRAM 区域配置为 Write-Back，旋转写入的结果留在 L1 Cache 里。
     * LTDC 是 AHB 主设备，直接读 SDRAM，看不到 Cache 中的新数据。
     * 必须在切换 LTDC 地址前把 Cache 推入 SDRAM，否则 LTDC 读到旧数据。
     * ──────────────────────────────────────────────────────────────────────── */
    SCB_CleanDCache_by_Addr((uint32_t *)dst_phys, 480U * 800U * 2U);
    __DMB();  /* 确保 Cache Clean 完成后再操作外设寄存器 */

    /* ── 步骤 6：把新的物理帧地址写入 LTDC CFBAR（Shadow 寄存器）────────── */
    __HAL_LTDC_LAYER(&hltdc, 0)->CFBAR = dst_phys_addr;

    /* ── 步骤 7～8：置 vsync_pending 并阻塞至 LTDC_IRQHandler（VFP）IMR 重载完成
     * 使用二值信号量唤醒，避免 vTaskDelay(1) 轮询 vsync_pending 的 1ms 粒度抖动。
     * ──────────────────────────────────────────────────────────────────────── */
    LCDConf_WaitVsyncReload();

    /* ── 步骤 9：同步 _aBufferIndex ────────────────────────────────────────
     * emWin 在 COPYBUFFER 返回后，会把 IndexDst 作为下一个绘制目标缓冲。
     * _LCD_FillRect / LCD_DMA2D_GetDrawBufBase 等使用 _aBufferIndex 计算
     * 地址，如果不更新，它们将永远寻址子帧0，导致写入错误的逻辑帧缓冲。
     * ──────────────────────────────────────────────────────────────────────── */
    _aBufferIndex[LayerIndex] = IndexDst;

    /* ── 步骤 10：通知 emWin 缓冲切换完成 ──────────────────────────────────
     *
     * 必须调用 ConfirmEx(IndexDst) ——  这是 emWin 状态机每帧正确交替
     * IndexSrc/IndexDst 的唯一驱动：
     *
     *   ConfirmEx(IndexDst) → emWin front = IndexDst
     *   → 下次 GUI_MULTIBUF_Begin 调用 COPYBUFFER(IndexDst, IndexSrc)
     *   → IndexSrc↔IndexDst 每帧翻转 ✓
     *
     * 为什么不是 ConfirmEx(IndexSrc)：
     *   ConfirmEx(IndexSrc) → emWin front = IndexSrc（始终为 0 或始终为 1）
     *   → COPYBUFFER 永远传入相同 (src, dst) → 旋转永远读同一帧 → 黑屏。
     *
     * emWin 帧内一致性说明：
     *   GUI_MULTIBUF_Begin() 在调用 COPYBUFFER 前已确定本帧绘制目标 = IndexDst。
     *   ConfirmEx(IndexDst) 并不改变本帧目标，只影响 下一帧 的 COPYBUFFER 参数。
     *   本帧内 emWin 像素写 → IndexDst，DMA2D（_LCD_FillRect）→ IndexDst
     *   (_aBufferIndex = IndexDst 已在步骤 9 设置)，两者一致。
     *   旋转从 IndexSrc 读取，本帧不会写入 IndexSrc，无冲突。
     * ──────────────────────────────────────────────────────────────────────── */
    GUI_MULTIBUF_ConfirmEx(LayerIndex, IndexDst);
}

/*
 * ManualRotateToPhysical —— 手动触发旋转 + VSYNC 切换
 *
 * 调用时机：MainTask.c 每帧 GUI_MULTIBUF_End() 之后直接调用。
 *
 * 步骤：
 *   1. Rotate：横屏(800×480) → 竖屏物理后台缓冲(480×800)，Ping-Pong 选择
 *   2. 写 LTDC CFBAR（Shadow 寄存器）
 *   3. VSYNC 等待：LCDConf_WaitVsyncReload() 置 vsync_pending，LTDC_IRQHandler 在 VFP
 *      执行 IMR 重载后清标志并 xSemaphoreGiveFromISR，任务侧 Take 返回。
 *      超时仅递增 g_lcdconf_vsync_timeout_cnt，不在任务侧翻 active_phys_buffer 或清 pending。
 *
 * 【Cache 策略说明】
 *   逻辑/物理缓冲均为 Write-Through（Region2，无 Region3）：
 *   DMA2D 与 CPU 写直达 SDRAM，无 Dirty Cache，可安全启用 _DMA_Fill 的 DMA2D。
 *   旋转前 CleanInvalidate 逻辑缓冲，清除读缓存，确保从 SDRAM 读最新帧。
 *
 * 注意：
 *   此函数在 FreeRTOS 任务上下文中调用；VSYNC 同步使用信号量阻塞。
 *   LTDC_IRQHandler 已在 LCD_X_Config 中通过 _EnableLTDCLineIRQ() 永久启用，
 *   每帧 VFP 必然触发，不会死锁。
 */
#if USE_PARTIAL_ROTATE
/* 思路二：仅对「本帧 CPU 将从逻辑缓冲读取」的矩形做 CleanInvalidate（32B 对齐），
 * 避免 Header/Body 均被静态跳过仍整屏刷 Cache。 */
static void LCDConf_CleanInvalidateLogicRegion(uint32_t base_addr, uint32_t size_bytes)
{
    if (size_bytes == 0U) {
        return;
    }
    {
        uint32_t end = base_addr + size_bytes;
        uint32_t a   = base_addr & ~31U;
        uint32_t ae  = (end + 31U) & ~31U;
        int32_t  sz  = (int32_t)(ae - a);
        if (sz > 0) {
            SCB_CleanInvalidateDCache_by_Addr((uint32_t *)a, sz);
        }
    }
}

#if LCDCONF_PHYS_M2M_STRIPED
/* 物理竖屏 480×800 RGB565：front→dst，拷贝矩形 (x,y,w,h) 像素 */
static void LCDConf_DMA2D_MemCpyPhysRect(const uint16_t *front, uint16_t *dst,
    uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    const uint32_t phys_w = VRAM_XPHYS;
    const uint32_t phys_h = VRAM_YPHYS;

    if (w == 0U || h == 0U) {
        return;
    }
    /* 越界则禁止起 DMA2D，避免 M2M 写穿物理帧（PL/NL 与 OOR 与 phys_w 一致时末地址才合法） */
    if ((x + w) > phys_w || (y + h) > phys_h) {
        return;
    }
    DMA2D_Wait_TransferComplete(portMAX_DELAY);
    DMA2D->CR      = 0x00000000UL | (1U << 9U);
    DMA2D->FGMAR   = (uint32_t)front + (y * phys_w + x) * 2U;
    DMA2D->OMAR    = (uint32_t)dst + (y * phys_w + x) * 2U;
    DMA2D->FGOR    = phys_w - w;
    DMA2D->OOR     = phys_w - w;
    DMA2D->FGPFCCR = 0x02UL;
    DMA2D->OPFCCR  = 0x02UL;
    DMA2D->NLR     = (w << 16U) | h;
    DMA2D->CR     |= DMA2D_CR_START;
    DMA2D_Wait_TransferComplete(portMAX_DELAY);
}

/* 除 PHYS_PLOT 外的上/下/左/右条带；Plot 区留给后续 Plot DMA 或 CPU 旋 plot 覆盖 */
static void LCDConf_DMA2D_PhysFrontToDstStriped(const uint16_t *front, uint16_t *dst)
{
    const uint32_t phys_w = VRAM_XPHYS;
    const uint32_t phys_h = VRAM_YPHYS;
    uint32_t       px     = (uint32_t)PHYS_PLOT_X;
    uint32_t       py     = (uint32_t)PHYS_PLOT_Y;
    uint32_t       pw     = (uint32_t)PHYS_PLOT_W;
    uint32_t       ph     = (uint32_t)PHYS_PLOT_H;

    if ((px + pw) > phys_w || (py + ph) > phys_h || pw == 0U || ph == 0U
        || (px == 0U && py == 0U && pw >= phys_w && ph >= phys_h)) {
        DMA2D_Wait_TransferComplete(portMAX_DELAY);
        DMA2D->CR      = 0x00000000UL | (1U << 9U);
        DMA2D->FGMAR   = (uint32_t)front;
        DMA2D->OMAR    = (uint32_t)dst;
        DMA2D->FGOR    = 0U;
        DMA2D->OOR     = 0U;
        DMA2D->FGPFCCR = 0x02UL;
        DMA2D->OPFCCR  = 0x02UL;
        DMA2D->NLR     = DMA2D_NLR_PACK_PL_NL(phys_w, phys_h);
        DMA2D->CR     |= DMA2D_CR_START;
        DMA2D_Wait_TransferComplete(portMAX_DELAY);
#if LCDCONF_PHYS_STRIPED_INTER_YIELD
        taskYIELD();
#endif
        return;
    }

    if (py > 0U) {
        LCDConf_DMA2D_MemCpyPhysRect(front, dst, 0U, 0U, phys_w, py);
#if LCDCONF_PHYS_STRIPED_INTER_YIELD
        taskYIELD();
#endif
    }
    if (py + ph < phys_h) {
        uint32_t y0 = py + ph;
        LCDConf_DMA2D_MemCpyPhysRect(front, dst, 0U, y0, phys_w, phys_h - y0);
#if LCDCONF_PHYS_STRIPED_INTER_YIELD
        taskYIELD();
#endif
    }
    if (px > 0U) {
        LCDConf_DMA2D_MemCpyPhysRect(front, dst, 0U, py, px, ph);
#if LCDCONF_PHYS_STRIPED_INTER_YIELD
        taskYIELD();
#endif
    }
    if (px + pw < phys_w) {
        uint32_t x0 = px + pw;
        LCDConf_DMA2D_MemCpyPhysRect(front, dst, x0, py, phys_w - x0, ph);
#if LCDCONF_PHYS_STRIPED_INTER_YIELD
        taskYIELD();
#endif
    }
}
#endif /* LCDCONF_PHYS_M2M_STRIPED */

static uint32_t s_rotate_frame_count = 0U;  /* 首 2 帧整屏旋转，确保物理缓冲初始化 */
#define LCD_RL_REGION_HEADER  0U
#define LCD_RL_REGION_BODY    1U
#define LCD_RL_REGION_COUNT   2U
static uint32_t s_rl_logic_rev[LCD_RL_REGION_COUNT] = { 1U, 1U };
static uint32_t s_rl_phys_rev[2][LCD_RL_REGION_COUNT];
static uint8_t s_force_full_screen_rotate;
#endif /* USE_PARTIAL_ROTATE */

#if USE_PARTIAL_ROTATE
static void LCDConf_RenderLatch_BumpRegion(uint32_t region)
{
    if (region >= LCD_RL_REGION_COUNT) {
        return;
    }
    s_rl_logic_rev[region]++;
    if (s_rl_logic_rev[region] == 0U) {
        s_rl_logic_rev[region] = 1U;
        s_rl_phys_rev[0][region] = 0U;
        s_rl_phys_rev[1][region] = 0U;
    }
}

static int LCDConf_RenderLatch_RegionNeedsSync(uint32_t phys_index, uint32_t region)
{
    if (phys_index >= 2U || region >= LCD_RL_REGION_COUNT) {
        return 0;
    }
    return (s_rl_phys_rev[phys_index][region] != s_rl_logic_rev[region]) ? 1 : 0;
}

static void LCDConf_RenderLatch_MarkRegionSynced(uint32_t phys_index, uint32_t region)
{
    if (phys_index >= 2U || region >= LCD_RL_REGION_COUNT) {
        return;
    }
    s_rl_phys_rev[phys_index][region] = s_rl_logic_rev[region];
}

static void LCDConf_RenderLatch_MarkAllSynced(uint32_t phys_index)
{
    LCDConf_RenderLatch_MarkRegionSynced(phys_index, LCD_RL_REGION_HEADER);
    LCDConf_RenderLatch_MarkRegionSynced(phys_index, LCD_RL_REGION_BODY);
}

void LCDConf_MarkHeaderDirty(void)
{
    LCDConf_RenderLatch_BumpRegion(LCD_RL_REGION_HEADER);
}

void LCDConf_MarkBodyDirty(void)
{
    LCDConf_RenderLatch_BumpRegion(LCD_RL_REGION_BODY);
}

void LCDConf_MarkFullDirty(void)
{
    LCDConf_RenderLatch_BumpRegion(LCD_RL_REGION_HEADER);
    LCDConf_RenderLatch_BumpRegion(LCD_RL_REGION_BODY);
}
#else
void LCDConf_MarkHeaderDirty(void) { }
void LCDConf_MarkBodyDirty(void) { }
void LCDConf_MarkFullDirty(void) { }
#endif

void LCDConf_RequestFullScreenRotate(void)
{
#if USE_PARTIAL_ROTATE
    LCDConf_MarkFullDirty();
    s_force_full_screen_rotate = 1U;
#endif
}

void LCDConf_RequestHeaderRotate(void)
{
    LCDConf_MarkHeaderDirty();
}

void LCDConf_RequestBodyRotate(void)
{
    LCDConf_MarkBodyDirty();
}

#if LCD_USE_CCW_ROTATION
/* CCW 整体平移时，phys_x/phys_y 越界被跳过的条带不会被旋转写入，易呈黑条；用底色填平 */
static void LCDConf_FillCCWPhysOffsetMargins(uint16_t *dst, uint16_t rgb565)
{
    const int W = 480;
    const int H = 800;
    const int ox = LCD_CCW_PHYS_OFFSET_X;
    const int oy = LCD_CCW_PHYS_OFFSET_Y;
    int x, y;

    if (ox > 0) {
        for (y = 0; y < H; y++) {
            uint16_t *row = dst + (uint32_t)y * (uint32_t)W;
            for (x = 0; x < ox; x++) {
                row[x] = rgb565;
            }
        }
    } else if (ox < 0) {
        const int x0 = W + ox;
        for (y = 0; y < H; y++) {
            uint16_t *row = dst + (uint32_t)y * (uint32_t)W;
            for (x = x0; x < W; x++) {
                row[x] = rgb565;
            }
        }
    }
    if (oy > 0) {
        for (y = 0; y < oy; y++) {
            uint16_t *row = dst + (uint32_t)y * (uint32_t)W;
            for (x = 0; x < W; x++) {
                row[x] = rgb565;
            }
        }
    } else if (oy < 0) {
        const int y0 = H + oy;
        for (y = y0; y < H; y++) {
            uint16_t *row = dst + (uint32_t)y * (uint32_t)W;
            for (x = 0; x < W; x++) {
                row[x] = rgb565;
            }
        }
    }
}
#endif /* LCD_USE_CCW_ROTATION */

#if USE_PARTIAL_ROTATE
static uint8_t LCDConf_RenderLatch_UsePlotBuf(const LCD_RenderLatchFramePlan *plan)
{
    if (plan == NULL) {
        Error_Handler(__FILE__, __LINE__);
        return 0U;
    }
#if USE_DMA2D_PLOTBUF
    return (plan->use_plotbuf != 0U) ? 1U : 0U;
#else
    return 0U;
#endif
}

static uint32_t LCDConf_RenderLatch_LogicBase(uint32_t x, uint32_t y)
{
    return VRAM_LOGICAL_ADDR + ((y * VRAM_XLOG) + x) * VRAM_BPP;
}

static void LCDConf_RenderLatch_CleanLogicRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    LCDConf_CleanInvalidateLogicRegion(LCDConf_RenderLatch_LogicBase(x, y), w * h * VRAM_BPP);
}

static void LCDConf_RenderLatch_RotateHeader(const uint16_t *src, uint16_t *dst)
{
    LCDConf_RenderLatch_CleanLogicRect(ROTATE_HEADER_X, ROTATE_HEADER_Y,
                                       ROTATE_HEADER_W, ROTATE_HEADER_H);
#if LCD_USE_CCW_ROTATION && LCD_HEADER_CCW_MIRROR_LOGICAL_X
    Optimize_Rotate_90CCW_RGB565_Header_Custom(src, dst, ROTATE_HEADER_X, ROTATE_HEADER_Y,
        ROTATE_HEADER_W, ROTATE_HEADER_H, 800);
#else
    ROTATE_REGION(src, dst, ROTATE_HEADER_X, ROTATE_HEADER_Y,
        ROTATE_HEADER_W, ROTATE_HEADER_H, 800);
#endif
}

static void LCDConf_RenderLatch_RotateBody(const uint16_t *src, uint16_t *dst)
{
    LCDConf_RenderLatch_CleanLogicRect(ROTATE_BODY_X, ROTATE_BODY_Y,
                                       ROTATE_BODY_W, ROTATE_BODY_H);
#if LCD_USE_CCW_ROTATION && LCD_HEADER_CCW_MIRROR_LOGICAL_X
    Optimize_Rotate_90CCW_RGB565_Header_Custom(src, dst, ROTATE_BODY_X, ROTATE_BODY_Y,
        ROTATE_BODY_W, ROTATE_BODY_H, 800);
#else
    ROTATE_REGION(src, dst, ROTATE_BODY_X, ROTATE_BODY_Y, ROTATE_BODY_W, ROTATE_BODY_H, 800);
#endif
}

static void LCDConf_RenderLatch_RotatePlot(const uint16_t *src, uint16_t *dst)
{
    LCDConf_RenderLatch_CleanLogicRect(ROTATE_PLOT_X, ROTATE_PLOT_Y,
                                       ROTATE_PLOT_W, ROTATE_PLOT_H);
    ROTATE_REGION(src, dst, ROTATE_PLOT_X, ROTATE_PLOT_Y,
        ROTATE_PLOT_W, ROTATE_PLOT_H, 800);
}

static void LCDConf_RenderLatch_CopyPlotBuf(uint16_t *dst)
{
    const uint16_t *plot_src = (const uint16_t *)DMA2D_PlotBuf_GetBuffer();
    uint32_t dst_plot;
    uint32_t src_sz;
    uint32_t src_align;
    uint32_t src_align_sz;

    if (plot_src == NULL) {
        Error_Handler(__FILE__, __LINE__);
        return;
    }

    dst_plot = (uint32_t)dst + ((uint32_t)PHYS_PLOT_Y * VRAM_XPHYS + (uint32_t)PHYS_PLOT_X) * VRAM_BPP;
    src_sz = (uint32_t)PHYS_PLOT_W * (uint32_t)PHYS_PLOT_H * VRAM_BPP;
    src_align = ((uint32_t)plot_src & ~31U);
    src_align_sz = ((((uint32_t)plot_src - src_align) + src_sz + 31U) & ~31U);
    SCB_CleanDCache_by_Addr((uint32_t *)src_align, (int32_t)src_align_sz);
    DMA2D_Wait_TransferComplete(portMAX_DELAY);
    DMA2D->CR      = 0x00000000UL | (1U << 9U);
    DMA2D->FGMAR   = (uint32_t)plot_src;
    DMA2D->OMAR    = dst_plot;
    DMA2D->FGOR    = 0U;
    DMA2D->OOR     = (VRAM_XPHYS - (uint32_t)PHYS_PLOT_W);
    DMA2D->FGPFCCR = 0x02UL;
    DMA2D->OPFCCR  = 0x02UL;
    DMA2D->NLR     = ((uint32_t)PHYS_PLOT_W << 16U) | (uint32_t)PHYS_PLOT_H;
    DMA2D->CR     |= DMA2D_CR_START;
    DMA2D_Wait_TransferComplete(portMAX_DELAY);
}
#endif

static void LCDConf_RenderLatch_CleanPhysicalFrame(uint32_t phys_addr)
{
    SCB_CleanDCache_by_Addr((uint32_t *)phys_addr, (int32_t)VRAM_PHYS_FRAME_BYTES);
    __DMB();
}

#if (Y_T_AB_MIRROR_PHYSICAL_FRAMEBUFFERS != 0)
static void LCDConf_RenderLatch_MirrorPhysicalFrame(uint32_t phys_addr)
{
    uint32_t other_addr = (phys_addr == VRAM_PHYSICAL_0_ADDR) ? VRAM_PHYSICAL_1_ADDR
                                                              : VRAM_PHYSICAL_0_ADDR;

    memcpy((void *)other_addr, (const void *)phys_addr, VRAM_PHYS_FRAME_BYTES);
    LCDConf_RenderLatch_CleanPhysicalFrame(other_addr);
}
#endif

/*
 * Render Latch 阶段一：逻辑帧 → 物理后台缓冲（DMA2D/CPU），不写 CFBAR、不等 VSYNC。
 * 阶段二：LCDConf_RenderLatch_Commit() 写 Shadow CFBAR 并在 VFP 等待重载。
 * MainTask 可在两阶段之间 taskYIELD（LCDCONF_RENDER_LATCH_YIELD_BEFORE_COMMIT）。
 */
void LCDConf_RenderLatch_PreparePhysical(const LCD_RenderLatchFramePlan *plan)
{
    uint32_t t_prep_begin;
    uint32_t phys_index;
    uint32_t phys_addr;
    uint8_t  use_plotbuf = 0U;
    const uint16_t *src = (const uint16_t *)VRAM_LOGICAL_ADDR;
    uint16_t       *dst;

    lcdconf_ensure_dwt_cyccnt();
    t_prep_begin = DWT->CYCCNT;
    if (s_rl_fu_span_scope_mark == 0u) {
        /* RestoreLayout / 非示波器帧等未打标：退化为仅统计 Prepare→Commit 窄窗（常与 fu 不同步 → avg≈0） */
        s_rl_fu_span_start = g_lcdconf_ltdc_fu_cnt;
    }

    LcdRotateProfile_ManualRotate_Enter();
    /* 整段旋转与内联 DMA2D M2M 同外设，串行本路径与其它 emWin/PlotBuf 路径，避免重入配置。 */
    DMA2D_Bus_RecursiveLock();

    phys_index = (active_phys_buffer == 0U) ? 1U : 0U;
    phys_addr = (phys_index == 0U) ? VRAM_PHYSICAL_0_ADDR : VRAM_PHYSICAL_1_ADDR;
    dst = (uint16_t *)phys_addr;

#if USE_PARTIAL_ROTATE
    use_plotbuf = LCDConf_RenderLatch_UsePlotBuf(plan);
#if (Y_T_AB_FORCE_FULL_ROTATE_EVERY_FRAME != 0)
    SCB_CleanInvalidateDCache_by_Addr((uint32_t *)VRAM_LOGICAL_ADDR, 800U * 480U * 2U);
#if LCD_USE_CCW_ROTATION && LCD_HEADER_CCW_MIRROR_LOGICAL_X
    Optimize_Rotate_90CCW_RGB565_Header_Custom(src, dst, ROTATE_HEADER_X, ROTATE_HEADER_Y,
        ROTATE_HEADER_W, ROTATE_HEADER_H, 800);
    Optimize_Rotate_90CCW_RGB565_Header_Custom(src, dst, ROTATE_BODY_X, ROTATE_BODY_Y,
        ROTATE_BODY_W, ROTATE_BODY_H, 800);
#else
    ROTATE_FULL_SCREEN(src, dst);
#endif
    LCDConf_RenderLatch_MarkAllSynced(phys_index);
    if (use_plotbuf != 0U) {
        LCDConf_RenderLatch_CopyPlotBuf(dst);
    }
    LcdRotateProfile_6a_SubTick(5U);
#else
    if (s_force_full_screen_rotate != 0U) {
        /* 全屏 UI：CCW+逻辑X镜像时 Header 走 Header_Custom，Body 须同一套映射，勿先整屏 CCW 再只盖 Header
         *（否则下半屏左右反、字镜像，与示波器头带不一致）。 */
        SCB_CleanInvalidateDCache_by_Addr((uint32_t *)VRAM_LOGICAL_ADDR, 800U * 480U * 2U);
#if LCD_USE_CCW_ROTATION && LCD_HEADER_CCW_MIRROR_LOGICAL_X
        Optimize_Rotate_90CCW_RGB565_Header_Custom(src, dst, ROTATE_HEADER_X, ROTATE_HEADER_Y,
            ROTATE_HEADER_W, ROTATE_HEADER_H, 800);
        Optimize_Rotate_90CCW_RGB565_Header_Custom(src, dst, ROTATE_BODY_X, ROTATE_BODY_Y,
            ROTATE_BODY_W, ROTATE_BODY_H, 800);
#else
        ROTATE_FULL_SCREEN(src, dst);
#endif
        s_force_full_screen_rotate = 0U;
        LCDConf_RenderLatch_MarkAllSynced(phys_index);
        LcdRotateProfile_6a_SubTick(5U);
    } else if (s_rotate_frame_count < 2U) {
        /* 首 2 帧：填满双物理缓冲；CCW+镜像时 Header+Body 均 Header_Custom，与部分旋转路径一致 */
        SCB_CleanInvalidateDCache_by_Addr((uint32_t *)VRAM_LOGICAL_ADDR, 800U * 480U * 2U);
#if LCD_USE_CCW_ROTATION && LCD_HEADER_CCW_MIRROR_LOGICAL_X
        Optimize_Rotate_90CCW_RGB565_Header_Custom(src, dst, ROTATE_HEADER_X, ROTATE_HEADER_Y,
            ROTATE_HEADER_W, ROTATE_HEADER_H, 800);
        Optimize_Rotate_90CCW_RGB565_Header_Custom(src, dst, ROTATE_BODY_X, ROTATE_BODY_Y,
            ROTATE_BODY_W, ROTATE_BODY_H, 800);
#else
        ROTATE_FULL_SCREEN(src, dst);
#endif
        s_rotate_frame_count++;
        LCDConf_RenderLatch_MarkAllSynced(phys_index);
        LcdRotateProfile_6a_SubTick(5U);
    } else {
        int need_header = LCDConf_RenderLatch_RegionNeedsSync(phys_index, LCD_RL_REGION_HEADER);
        int need_body   = LCDConf_RenderLatch_RegionNeedsSync(phys_index, LCD_RL_REGION_BODY);

        /* 稳态不再做物理前台→后台继承拷贝；每块物理缓冲按自己的区域版本补齐。 */
        LcdRotateProfile_6a_SubTick(0U);

        if (need_header != 0) {
            LCDConf_RenderLatch_RotateHeader(src, dst);
            LCDConf_RenderLatch_MarkRegionSynced(phys_index, LCD_RL_REGION_HEADER);
        }
        LcdRotateProfile_6a_SubTick(2U);

        if (need_body != 0) {
            LCDConf_RenderLatch_RotateBody(src, dst);
            LCDConf_RenderLatch_MarkRegionSynced(phys_index, LCD_RL_REGION_BODY);
        }
        LcdRotateProfile_6a_SubTick(3U);

        if (use_plotbuf != 0U) {
            LCDConf_RenderLatch_CopyPlotBuf(dst);
        } else {
            if (need_body == 0) {
                LCDConf_RenderLatch_RotatePlot(src, dst);
            }
        }
        LcdRotateProfile_6a_SubTick(4U);
    }
#endif
#else
    (void)plan;
    (void)use_plotbuf;
    /* 步骤 1：CleanInvalidate 逻辑缓冲区 */
    SCB_CleanInvalidateDCache_by_Addr((uint32_t *)VRAM_LOGICAL_ADDR, 800U * 480U * 2U);

    /* 步骤 2：整屏顺时针旋转 */
    ROTATE_FULL_SCREEN(src, dst);
    LcdRotateProfile_6a_SubTick(7U);
#endif

    /* DMA2D 段已结束；VSYNC 等待可释放总线锁，避免长时间占锁阻碍 emWin 其它 DMA2D 填充。 */
    DMA2D_Bus_RecursiveUnlock();

#if LCD_USE_CCW_ROTATION
#if (LCD_CCW_PHYS_OFFSET_X != 0) || (LCD_CCW_PHYS_OFFSET_Y != 0)
    LCDConf_FillCCWPhysOffsetMargins(dst, (uint16_t)LCD_CCW_PHYS_MARGIN_RGB565);
#endif
#endif

#if (Y_T_AB_MIRROR_PHYSICAL_FRAMEBUFFERS != 0)
    LCDConf_RenderLatch_MirrorPhysicalFrame(phys_addr);
#endif
    LCDConf_RenderLatch_CleanPhysicalFrame(phys_addr);
    s_rl_prep_saved_cy = DWT->CYCCNT - t_prep_begin;
}

void LCDConf_RenderLatch_Commit(void)
{
    uint32_t phys_addr;
    uint32_t t_commit_begin;
    uint32_t commit_cy;
    uint32_t fu_now;
    uint32_t fu_d;

    phys_addr = (active_phys_buffer == 0U) ? VRAM_PHYSICAL_1_ADDR
                                           : VRAM_PHYSICAL_0_ADDR;

    lcdconf_ensure_dwt_cyccnt();
    t_commit_begin = DWT->CYCCNT;

    LcdRotateProfile_BeforeVsyncWait();

    /* 步骤 3：写新地址到 LTDC Shadow 寄存器 */
    __HAL_LTDC_LAYER(&hltdc, 0)->CFBAR = phys_addr;

    /* 步骤 4：等待 VFP 消隐期切换（ISR：IMR + Give 信号量） */
#if (Y_T_AB_USE_LTDC_VBR_RELOAD != 0)
    LTDC->ICR = LTDC_ICR_CRRIF;
    LTDC->SRCR = LTDC_SRCR_VBR;
#endif
    LCDConf_WaitVsyncReload();
    /* active_phys_buffer 已由 LTDC_IRQHandler 翻转，此处不再重复翻转 */

    LcdRotateProfile_AfterVsyncWait();

    commit_cy = DWT->CYCCNT - t_commit_begin;
    fu_now    = g_lcdconf_ltdc_fu_cnt;
    fu_d      = fu_now - s_rl_fu_span_start;
    s_rl_fu_span_scope_mark = 0u;

    s_rl_frames_acc++;
    s_rl_sum_prep_cy += (uint64_t)s_rl_prep_saved_cy;
    s_rl_sum_commit_cy += (uint64_t)commit_cy;
    s_rl_sum_fu_delta += (uint64_t)fu_d;
}

void ManualRotateToPhysical(void)
{
    LCD_RenderLatchFramePlan plan = { 0U };

    LCDConf_RenderLatch_PreparePhysical(&plan);
    LCDConf_RenderLatch_Commit();
}

static LCD_LayerPropTypedef   layer_prop[GUI_NUM_LAYERS];
static const U32   _aAddr[]   = {VRAM_LOGICAL_ADDR, VRAM_LOGICAL_ADDR};
static int _aPendingBuffer[2] = { -1, -1 };
volatile U32 g_emwin_showbuffer_cnt = 0U;
volatile U32 g_emwin_ltdc_irq_cnt = 0U;
volatile U32 g_emwin_confirm_cnt = 0U;
/* ?? GUI_MULTIBUF_End() ???? LCD_X_SHOWBUFFER ??????????????????"??   ??????????= (g_emwin_last_draw_buf + 1) % NUM_BUFFERS??         */
volatile int g_emwin_last_draw_buf = -1;
/* _aBufferIndex 已提前声明在文件顶部（_CustomCopyBuffer 之前），此处不重复定义。 */
static int _axSize[GUI_NUM_LAYERS];
static int _aySize[GUI_NUM_LAYERS];
static int _aBytesPerPixels[GUI_NUM_LAYERS];

static void _EnableLTDCLineIRQ(void)
{
	/* STM32H7 RM0433: LIPCR=0 is explicitly invalid ("cannot be positioned on
	   line number 0").  Setting it to 0 causes the interrupt to fire at an
	   unpredictable point inside the active scan, leading to mid-frame
	   IMMEDIATE reloads and the characteristic "right-half tearing" seen with
	   ROTATION_CW (physical row 0 = logical left edge; mid-frame swap produces
	   a visible horizontal tear that drifts across the right half).

	   Fix: fire the interrupt at the first line of VFP (vertical front porch),
	   i.e. AccumulatedActiveHeight + 1.  All active pixels have already been
	   output at that point, so an IMMEDIATE reload during VFP changes the CFBAR
	   before the next frame's active scan begins ??zero tearing.

	   AWCR[27:16] = AccumulatedActiveHeight (hardware register, mode-
	   independent).  Read it directly so this code works for both portrait and
	   landscape configurations without hard-coding timing numbers. */
	LTDC->LIPCR = ((LTDC->AWCR >> 16U) & 0xFFFU) + 1U;
	LTDC->ICR   = LTDC_ICR_CLIF | LTDC_ICR_CFUIF;
	LTDC->IER  |= LTDC_IER_LIE | LTDC_IER_FUIE;
	HAL_NVIC_SetPriority(LTDC_IRQn, 0x2, 0);
	HAL_NVIC_EnableIRQ(LTDC_IRQn);
	HAL_NVIC_SetPriority(LTDC_ER_IRQn, 0x2, 0);
	HAL_NVIC_EnableIRQ(LTDC_ER_IRQn);
	if (s_ltdc_vsync_sem == NULL) {
		s_ltdc_vsync_sem = xSemaphoreCreateBinary();
	}
}

/* ?????????????? */
#if DrawBitmapA4Enalbe == 1
static U32 _aBuffer[XSIZE_PHYS * sizeof(U32) * 3]__attribute__((at(0x24000000)));
static U32 * _pBuffer_DMA2D = &_aBuffer[XSIZE_PHYS * sizeof(U32) * 0];

/* ???? A4 bitmaps ??? */
static const U8 _aMirror[] = {
  0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0,
  0x01, 0x11, 0x21, 0x31, 0x41, 0x51, 0x61, 0x71, 0x81, 0x91, 0xA1, 0xB1, 0xC1, 0xD1, 0xE1, 0xF1,
  0x02, 0x12, 0x22, 0x32, 0x42, 0x52, 0x62, 0x72, 0x82, 0x92, 0xA2, 0xB2, 0xC2, 0xD2, 0xE2, 0xF2,
  0x03, 0x13, 0x23, 0x33, 0x43, 0x53, 0x63, 0x73, 0x83, 0x93, 0xA3, 0xB3, 0xC3, 0xD3, 0xE3, 0xF3,
  0x04, 0x14, 0x24, 0x34, 0x44, 0x54, 0x64, 0x74, 0x84, 0x94, 0xA4, 0xB4, 0xC4, 0xD4, 0xE4, 0xF4,
  0x05, 0x15, 0x25, 0x35, 0x45, 0x55, 0x65, 0x75, 0x85, 0x95, 0xA5, 0xB5, 0xC5, 0xD5, 0xE5, 0xF5,
  0x06, 0x16, 0x26, 0x36, 0x46, 0x56, 0x66, 0x76, 0x86, 0x96, 0xA6, 0xB6, 0xC6, 0xD6, 0xE6, 0xF6,
  0x07, 0x17, 0x27, 0x37, 0x47, 0x57, 0x67, 0x77, 0x87, 0x97, 0xA7, 0xB7, 0xC7, 0xD7, 0xE7, 0xF7,
  0x08, 0x18, 0x28, 0x38, 0x48, 0x58, 0x68, 0x78, 0x88, 0x98, 0xA8, 0xB8, 0xC8, 0xD8, 0xE8, 0xF8,
  0x09, 0x19, 0x29, 0x39, 0x49, 0x59, 0x69, 0x79, 0x89, 0x99, 0xA9, 0xB9, 0xC9, 0xD9, 0xE9, 0xF9,
  0x0A, 0x1A, 0x2A, 0x3A, 0x4A, 0x5A, 0x6A, 0x7A, 0x8A, 0x9A, 0xAA, 0xBA, 0xCA, 0xDA, 0xEA, 0xFA,
  0x0B, 0x1B, 0x2B, 0x3B, 0x4B, 0x5B, 0x6B, 0x7B, 0x8B, 0x9B, 0xAB, 0xBB, 0xCB, 0xDB, 0xEB, 0xFB,
  0x0C, 0x1C, 0x2C, 0x3C, 0x4C, 0x5C, 0x6C, 0x7C, 0x8C, 0x9C, 0xAC, 0xBC, 0xCC, 0xDC, 0xEC, 0xFC,
  0x0D, 0x1D, 0x2D, 0x3D, 0x4D, 0x5D, 0x6D, 0x7D, 0x8D, 0x9D, 0xAD, 0xBD, 0xCD, 0xDD, 0xED, 0xFD,
  0x0E, 0x1E, 0x2E, 0x3E, 0x4E, 0x5E, 0x6E, 0x7E, 0x8E, 0x9E, 0xAE, 0xBE, 0xCE, 0xDE, 0xEE, 0xFE,
  0x0F, 0x1F, 0x2F, 0x3F, 0x4F, 0x5F, 0x6F, 0x7F, 0x8F, 0x9F, 0xAF, 0xBF, 0xCF, 0xDF, 0xEF, 0xFF,
};
#else
static U32 _aBuffer[1];
static U32 * _pBuffer_DMA2D = &_aBuffer[0];
#endif

/* ???????? */
static const LCD_API_COLOR_CONV *_apColorConvAPI[] = {
  COLOR_CONVERSION_0,
#if GUI_NUM_LAYERS > 1
  COLOR_CONVERSION_1,
#endif
};

/* ??????? */
static const int _aOrientation[] = 
{
  ORIENTATION_0,
#if GUI_NUM_LAYERS > 1
  ORIENTATION_1,
#endif
};

/*
*********************************************************************************************************
*	?? ?? ??: _ClearCacheHook
*	???????: ??Cache
*	??    ??: LayerIndex  ???
*	?? ?? ?: ??
*********************************************************************************************************
*/
#if ClearCacheHookEnalbe == 1
static void _ClearCacheHook(U32 LayerMask) 
{
	int i;
	for (i = 0; i < GUI_NUM_LAYERS; i++) 
	{
		if (LayerMask & (1 << i)) 
		{
			/* RGB565 = 2 bytes/pixel；原来 sizeof(U32)=4 会多清一倍范围（无害但浪费）*/
			SCB_CleanDCache_by_Addr ((uint32_t *)_aAddr[i], XSIZE_PHYS * YSIZE_PHYS * 2);
		}
	}
}
#endif

/*
*********************************************************************************************************
*	?? ?? ??: _GetPixelformat
*	???????: ??????1???????2??????????
*	??    ??: LayerIndex  ???
*	?? ?? ?: ??????
*********************************************************************************************************
*/
static U32 _GetPixelformat(int LayerIndex) 
{
	const LCD_API_COLOR_CONV * pColorConvAPI;

	if (LayerIndex >= GUI_COUNTOF(_apColorConvAPI)) 
	{
		return 0;
	}
	
	pColorConvAPI = _apColorConvAPI[LayerIndex];
	
	if (pColorConvAPI == GUICC_M8888I) 
	{
		return LTDC_PIXEL_FORMAT_ARGB8888;
	}
	else if (pColorConvAPI == GUICC_M888) 
	{
		return LTDC_PIXEL_FORMAT_RGB888;
	}
	else if (pColorConvAPI == GUICC_M565) 
	{
		return LTDC_PIXEL_FORMAT_RGB565;
	}
	else if (pColorConvAPI == GUICC_M1555I)
	{
		return LTDC_PIXEL_FORMAT_ARGB1555;
	}
	else if (pColorConvAPI == GUICC_M4444I) 
	{
	return LTDC_PIXEL_FORMAT_ARGB4444;
	}
	else if (pColorConvAPI == GUICC_8666  ) 
	{
		return LTDC_PIXEL_FORMAT_L8;
	}
	else if (pColorConvAPI == GUICC_1616I ) 
	{
		return LTDC_PIXEL_FORMAT_AL44;
	}
	else if (pColorConvAPI == GUICC_88666I) 
	{
		return LTDC_PIXEL_FORMAT_AL88;
	}
	
	/* ??????????????? */
	while (1);
}

/*
*********************************************************************************************************
*	?? ?? ??: _GetPixelformat
*	???????: ??????1???????2??????????
*	??    ??: LayerIndex  ???
*	?? ?? ?: ??????
*********************************************************************************************************
*/
static void LCD_LL_LayerInit(U32 LayerIndex) 
{  
	LTDC_LayerCfgTypeDef  layer_cfg;  
	static uint32_t       LUT[256];
	uint32_t              i;

	if (LayerIndex < GUI_NUM_LAYERS)
	{
		/* ????????????? */ 
		layer_cfg.WindowX0 = 0;
		layer_cfg.WindowX1 = LCD_RGB_WIDTH;
		layer_cfg.WindowY0 = 0;
		layer_cfg.WindowY1 = LCD_RGB_HEIGHT;
		
		/* ?????????? */ 
		layer_cfg.PixelFormat = _GetPixelformat(LayerIndex);
		
		/* ??????*/
		layer_cfg.FBStartAdress = layer_prop[LayerIndex].address;
		
		/* Alpha???? (255 ???????????) */
		layer_cfg.Alpha = 255;
		
		/* ?????? */
		layer_cfg.Alpha0 = 0;   /* ?????? */
		layer_cfg.Backcolor.Blue = 0;
		layer_cfg.Backcolor.Green = 0;
		layer_cfg.Backcolor.Red = 0;
		
		/* ????????????? */
		layer_cfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
		layer_cfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;

		/* ?????????? */
		layer_cfg.ImageWidth = LCD_RGB_WIDTH;
		layer_cfg.ImageHeight = LCD_RGB_HEIGHT;

		/* ???????1 */
		HAL_LTDC_ConfigLayer(&hltdc, &layer_cfg, LayerIndex);

		/* ???LUT */
		if (LCD_GetBitsPerPixelEx(LayerIndex) <= 8)
		{
			HAL_LTDC_EnableCLUT(&hltdc, LayerIndex);
		}
		else
		{
			/*  AL88??(16bpp) */
			if (layer_prop[LayerIndex].pColorConvAPI == GUICC_88666I)
			{
				HAL_LTDC_EnableCLUT(&hltdc, LayerIndex);

				for (i = 0; i < 256; i++)
				{
					LUT[i] = LCD_API_ColorConv_8666.pfIndex2Color(i);
				}
				
				HAL_LTDC_ConfigCLUT(&hltdc, LUT, 256, LayerIndex);
			}
		}
	}  
}

/*
*********************************************************************************************************
*	?? ?? ??: LCD_LL_Init
*	???????: ????LTDC
*	??    ??: ??
*	?? ?? ?: ??
*   ??    ??:
*       LCD_TFT ????????????????????????????????????????
*       ----------------------------------------------------------------------------
*    
*                                                 Total Width
*                             <--------------------------------------------------->
*                       Hsync width HBP             Active Width                HFP
*                             <---><--><--------------------------------------><-->
*                         ____    ____|_______________________________________|____ 
*                             |___|   |                                       |    |
*                                     |                                       |    |
*                         __|         |                                       |    |
*            /|\    /|\  |            |                                       |    |
*             | VSYNC|   |            |                                       |    |
*             |Width\|/  |__          |                                       |    |
*             |     /|\     |         |                                       |    |
*             |  VBP |      |         |                                       |    |
*             |     \|/_____|_________|_______________________________________|    |
*             |     /|\     |         | / / / / / / / / / / / / / / / / / / / |    |
*             |      |      |         |/ / / / / / / / / / / / / / / / / / / /|    |
*    Total    |      |      |         |/ / / / / / / / / / / / / / / / / / / /|    |
*    Heigh    |      |      |         |/ / / / / / / / / / / / / / / / / / / /|    |
*             |Active|      |         |/ / / / / / / / / / / / / / / / / / / /|    |
*             |Heigh |      |         |/ / / / / / Active Display Area / / / /|    |
*             |      |      |         |/ / / / / / / / / / / / / / / / / / / /|    |
*             |      |      |         |/ / / / / / / / / / / / / / / / / / / /|    |
*             |      |      |         |/ / / / / / / / / / / / / / / / / / / /|    |
*             |      |      |         |/ / / / / / / / / / / / / / / / / / / /|    |
*             |      |      |         |/ / / / / / / / / / / / / / / / / / / /|    |
*             |     \|/_____|_________|_______________________________________|    |
*             |     /|\     |                                                      |
*             |  VFP |      |                                                      |
*            \|/    \|/_____|______________________________________________________|
*            
*     
*     ???LCD????????????????????
*     Horizontal Synchronization (Hsync) 
*     Horizontal Back Porch (HBP)       
*     Active Width                      
*     Horizontal Front Porch (HFP)     
*   
*     Vertical Synchronization (Vsync)  
*     Vertical Back Porch (VBP)         
*     Active Heigh                       
*     Vertical Front Porch (VFP)         
*     
*     LCD_TFT ????????????????????????? :
*     ----------------------------------------------------------------
*   
*     HorizontalStart = (Offset_X + Hsync + HBP);
*     HorizontalStop  = (Offset_X + Hsync + HBP + Window_Width - 1); 
*     VarticalStart   = (Offset_Y + Vsync + VBP);
*     VerticalStop    = (Offset_Y + Vsync + VBP + Window_Heigh - 1);
*
*********************************************************************************************************
*/
static void LCD_LL_Init(void) 
{
	/* ????LCD????GPIO */
	{
		/* GPIOs Configuration */
		/*
		+------------------------+-----------------------+----------------------------+
		+                       LCD pins assignment                                   +
		+------------------------+-----------------------+----------------------------+
		|  LCDH7_TFT R0 <-> PI.15  |  LCDH7_TFT G0 <-> PJ.07 |  LCDH7_TFT B0 <-> PJ.12      |
		|  LCDH7_TFT R1 <-> PJ.00  |  LCDH7_TFT G1 <-> PJ.08 |  LCDH7_TFT B1 <-> PJ.13      |
		|  LCDH7_TFT R2 <-> PJ.01  |  LCDH7_TFT G2 <-> PJ.09 |  LCDH7_TFT B2 <-> PJ.14      |
		|  LCDH7_TFT R3 <-> PJ.02  |  LCDH7_TFT G3 <-> PJ.10 |  LCDH7_TFT B3 <-> PJ.15      |
		|  LCDH7_TFT R4 <-> PJ.03  |  LCDH7_TFT G4 <-> PJ.11 |  LCDH7_TFT B4 <-> PK.03      |
		|  LCDH7_TFT R5 <-> PJ.04  |  LCDH7_TFT G5 <-> PK.00 |  LCDH7_TFT B5 <-> PK.04      |
		|  LCDH7_TFT R6 <-> PJ.05  |  LCDH7_TFT G6 <-> PK.01 |  LCDH7_TFT B6 <-> PK.05      |
		|  LCDH7_TFT R7 <-> PJ.06  |  LCDH7_TFT G7 <-> PK.02 |  LCDH7_TFT B7 <-> PK.06      |
		-------------------------------------------------------------------------------
		|  LCDH7_TFT HSYNC <-> PI.12  | LCDTFT VSYNC <->  PI.13 |
		|  LCDH7_TFT CLK   <-> PI.14  | LCDH7_TFT DE   <->  PK.07 |
		-----------------------------------------------------
		*/		
		GPIO_InitTypeDef GPIO_Init_Structure;

		/*##-1- Enable peripherals and GPIO Clocks #################################*/  
		/* ???LTDC??DMA2D??? */
		__HAL_RCC_LTDC_CLK_ENABLE();
		__HAL_RCC_DMA2D_CLK_ENABLE();  
		
		/* ???GPIO??? */
		__HAL_RCC_GPIOI_CLK_ENABLE();
		__HAL_RCC_GPIOJ_CLK_ENABLE();
		__HAL_RCC_GPIOK_CLK_ENABLE();

		/* GPIOI ???? */
		GPIO_Init_Structure.Pin       = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15; 
		GPIO_Init_Structure.Mode      = GPIO_MODE_AF_PP;
		GPIO_Init_Structure.Pull      = GPIO_NOPULL;
		GPIO_Init_Structure.Speed     = GPIO_SPEED_FREQ_HIGH;
		GPIO_Init_Structure.Alternate = GPIO_AF14_LTDC;  
		HAL_GPIO_Init(GPIOI, &GPIO_Init_Structure);

		/* GPIOJ ???? */  
		GPIO_Init_Structure.Pin       = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | \
									  GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | \
									  GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | \
									  GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15; 
		GPIO_Init_Structure.Mode      = GPIO_MODE_AF_PP;
		GPIO_Init_Structure.Pull      = GPIO_NOPULL;
		GPIO_Init_Structure.Speed     = GPIO_SPEED_FREQ_HIGH;
		GPIO_Init_Structure.Alternate = GPIO_AF14_LTDC;  
		HAL_GPIO_Init(GPIOJ, &GPIO_Init_Structure);  

		/* GPIOK ???? */  
		GPIO_Init_Structure.Pin       = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | \
									  GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7; 
		GPIO_Init_Structure.Mode      = GPIO_MODE_AF_PP;
		GPIO_Init_Structure.Pull      = GPIO_NOPULL;
		GPIO_Init_Structure.Speed     = GPIO_SPEED_FREQ_HIGH;
		GPIO_Init_Structure.Alternate = GPIO_AF14_LTDC;  
		HAL_GPIO_Init(GPIOK, &GPIO_Init_Structure);  	
	}
	
	/*##-2- LTDC????? #############################################################*/  
	{	
		uint16_t Width, Height, HSYNC_W, HBP, HFP, VSYNC_W, VBP, VFP;
		RCC_PeriphCLKInitTypeDef  PeriphClkInitStruct;

		/* ???6??????*/
		Width = LCD_RGB_WIDTH;
		Height = LCD_RGB_HEIGHT;
		HSYNC_W = 96;
		HBP = 10;
		HFP = 10;
		VSYNC_W = 2;
		VBP = 10;
		VFP = 10;
		PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
		PeriphClkInitStruct.PLL3.PLL3M = 5;
		PeriphClkInitStruct.PLL3.PLL3N = 48;
		PeriphClkInitStruct.PLL3.PLL3P = 2;
		PeriphClkInitStruct.PLL3.PLL3Q = 5;
		PeriphClkInitStruct.PLL3.PLL3R = 10;
		HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

#if 0
		/* Old code disabled */
		switch (g_LcdType)
		{
			case LCD_35_480X320:	/* 3.5?? 480 * 320 */	
				Width = 480;
				Height = 272;
				HSYNC_W = 10;
				HBP = 20;
				HFP = 20;
				VSYNC_W = 20;
				VBP = 20;
				VFP = 20;
				break;
			
			case LCD_43_480X272:		/* 4.3?? 480 * 272 */			
				Width = 480;
				Height = 272;

				HSYNC_W = 40;
				HBP = 2;
				HFP = 2;
				VSYNC_W = 9;
				VBP = 2;
				VFP = 2;
		
				/* LCD ??????? */
				/* PLL3_VCO Input = HSE_VALUE/PLL3M = 25MHz/5 = 5MHz */
				/* PLL3_VCO Output = PLL3_VCO Input * PLL3N = 5MHz * 48 = 240MHz */
				/* PLLLCDCLK = PLL3_VCO Output/PLL3R = 240 / 10 = 24MHz */
				/* LTDC clock frequency = PLLLCDCLK = 24MHz */
				/*
					????? = 24MHz /((Width + HSYNC_W  + HBP  + HFP)*(Height + VSYNC_W +  VBP  + VFP))
                   		   = 24000000/((480 + 40  + 2  + 2)*(272 + 9 +  2  + 2)) 
			               = 24000000/(524*285)
                           = 160Hz	

					???????????????????PLL3Q??????8MHz????USB????
			    */
				PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
				PeriphClkInitStruct.PLL3.PLL3M = 5;
				PeriphClkInitStruct.PLL3.PLL3N = 48;
				PeriphClkInitStruct.PLL3.PLL3P = 2;
				PeriphClkInitStruct.PLL3.PLL3Q = 5;
				PeriphClkInitStruct.PLL3.PLL3R = 10;				
				HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);     			
				break;
			
			case LCD_50_480X272:		/* 5.0?? 480 * 272 */
				Width = 480;
				Height = 272;
			
				HSYNC_W = 40;
				HBP = 2;
				HFP = 2;
				VSYNC_W = 9;
				VBP = 2;
				VFP = 2;			
				break;
			
			case LCD_50_800X480:		/* 5.0?? 800 * 480 */
			case LCD_70_800X480:		/* 7.0?? 800 * 480 */					
				Width = 800;
				Height = 480;

				HSYNC_W = 96;	/* =10???????????20????????????,80????OK */
				HBP = 10;
				HFP = 10;
				VSYNC_W = 2;
				VBP = 10;
				VFP = 10;			

				/* LCD ??????? */
				/* PLL3_VCO Input = HSE_VALUE/PLL3M = 25MHz/5 = 5MHz */
				/* PLL3_VCO Output = PLL3_VCO Input * PLL3N = 5MHz * 48 = 240MHz */
				/* PLLLCDCLK = PLL3_VCO Output/PLL3R = 240 / 10 = 24MHz */
				/* LTDC clock frequency = PLLLCDCLK = 24MHz */
				/*
					????? = 24MHz /((Width + HSYNC_W  + HBP  + HFP)*(Height + VSYNC_W +  VBP  + VFP))
                   		   = 24000000/((800 + 96  + 10  + 10)*(480 + 2 +  10  + 10)) 
			               = 24000000/(916*502)
                           = 52Hz	
			
					?????????????100Hz?????????????????PeriphClkInitStruct.PLL3.PLL3N = 100????
					?????LTDC?????50MHz
					????? = 50MHz /(??Width + HSYNC_W  + HBP  + HFP ??*(Height + VSYNC_W +  VBP  +VFP  )) 
					       = 5000000/(916*502) 
					       = 108.7Hz

					???????????????????PLL3Q??????8MHz????USB????
			    */ 
				PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
				PeriphClkInitStruct.PLL3.PLL3M = 5;
				PeriphClkInitStruct.PLL3.PLL3N = 48;
				PeriphClkInitStruct.PLL3.PLL3P = 2;
				PeriphClkInitStruct.PLL3.PLL3Q = 5;
				PeriphClkInitStruct.PLL3.PLL3R = 10; 
				HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);     			
				break;
			
			case LCD_70_1024X600:		/* 7.0?? 1024 * 600 */
				/* ?????????? = 53.7M */
				Width = 1024;
				Height = 600;

				HSYNC_W = 2;	/* =10???????????20????????????,80????OK */
				HBP = 157;
				HFP = 160;
				VSYNC_W = 2;
				VBP = 20;
				VFP = 12;		
			
				PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
				PeriphClkInitStruct.PLL3.PLL3M = 5;
				PeriphClkInitStruct.PLL3.PLL3N = 48;
				PeriphClkInitStruct.PLL3.PLL3P = 2;
				PeriphClkInitStruct.PLL3.PLL3Q = 5;
				PeriphClkInitStruct.PLL3.PLL3R = 10;
				HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct); 			
				break;
			
			default:
				Width = 800;
				Height = 480;

				HSYNC_W = 80;	/* =10???????????20????????????,80????OK */
				HBP = 10;
				HFP = 10;
				VSYNC_W = 10;
				VBP = 10;
				VFP = 10;		
			
				/* LCD ??????? */
				/* PLL3_VCO Input = HSE_VALUE/PLL3M = 25MHz/5 = 5MHz */
				/* PLL3_VCO Output = PLL3_VCO Input * PLL3N = 5MHz * 48 = 240MHz */
				/* PLLLCDCLK = PLL3_VCO Output/PLL3R = 240 / 10 = 24MHz */
				/* LTDC clock frequency = PLLLCDCLK = 24MHz */
				/*
					????? = 24MHz /((Width + HSYNC_W  + HBP  + HFP)*(Height + VSYNC_W +  VBP  + VFP))
                   		   = 24000000/((800 + 96  + 10  + 10)*(480 + 2 +  10  + 10)) 
			               = 24000000/(916*502)
                           = 52Hz

					?????????????100Hz?????????????????PeriphClkInitStruct.PLL3.PLL3N = 100????
					?????LTDC?????50MHz
					????? = 50MHz /(??Width + HSYNC_W  + HBP  + HFP ??*(Height + VSYNC_W +  VBP  +VFP  )) 
					       = 5000000/(916*502) 
					       = 108.7Hz

					???????????????????PLL3Q??????8MHz????USB????
			    */ 
				PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
				PeriphClkInitStruct.PLL3.PLL3M = 5;
				PeriphClkInitStruct.PLL3.PLL3N = 48;
				PeriphClkInitStruct.PLL3.PLL3P = 2;
				PeriphClkInitStruct.PLL3.PLL3Q = 5;
				PeriphClkInitStruct.PLL3.PLL3R = 10;  
				HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct); 			
				break;
		}
#endif
		
		/* ?????????? */	
		hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;	/* HSYNC ??????? */
		hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL; 	/* VSYNC ??????? */
		hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL; 	/* DE ??????? */
		hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;

		/* ??????? */    
		hltdc.Init.HorizontalSync = (HSYNC_W - 1);
		hltdc.Init.VerticalSync = (VSYNC_W - 1);
		hltdc.Init.AccumulatedHBP = (HSYNC_W + HBP - 1);
		hltdc.Init.AccumulatedVBP = (VSYNC_W + VBP - 1);  
		hltdc.Init.AccumulatedActiveH = (Height + VSYNC_W + VBP - 1);
		hltdc.Init.AccumulatedActiveW = (Width + HSYNC_W + HBP - 1);
		hltdc.Init.TotalHeigh = (Height + VSYNC_W + VBP + VFP - 1);
		hltdc.Init.TotalWidth = (Width + HSYNC_W + HBP + HFP - 1); 

		/* ???????????? */
		hltdc.Init.Backcolor.Blue = 0;
		hltdc.Init.Backcolor.Green = 0;
		hltdc.Init.Backcolor.Red = 0;

		hltdc.Instance = LTDC;

		/* ????LTDC  */  
		if (HAL_LTDC_Init(&hltdc) != HAL_OK)
		{
			/* ????????? */
			Error_Handler(__FILE__, __LINE__);
		}
	}  

	/* ???????? */
	HAL_LTDC_ProgramLineEvent(&hltdc, 0);
  
    /* ???Dither */
    HAL_LTDC_EnableDither(&hltdc);

	/* ???LTDC????????????????? */
	HAL_NVIC_SetPriority(LTDC_IRQn, 0x2, 0);
	HAL_NVIC_EnableIRQ(LTDC_IRQn);
}

/*
*********************************************************************************************************
*	?? ?? ??: _DMA_Copy
*	???????: ???DMA2D?????????????????????????????????*	??    ??: LayerIndex    ???
*             pSrc          ???????????
*             pDst          ????????????
*             xSize         ??????????X??????????????????*             ySize         ??????????Y?????????????*             OffLineSrc    ???????????????*             OffLineDst    ???????????*	?? ?? ?: ??
*********************************************************************************************************
*/
static void _DMA_Copy(int LayerIndex, void * pSrc, void * pDst, int xSize, int ySize, int OffLineSrc, int OffLineDst) 
{
#if (DMA2D_USE_IN_COPY == 1)
    DMA2D_Bus_RecursiveLock();
    /* 逻辑缓冲 WT，DMA2D M2M 写直达 SDRAM，拷贝后 Invalidate 目标区 */
    uint32_t dst_addr   = (uint32_t)pDst;
    uint32_t region     = (uint32_t)(xSize + OffLineDst) * (uint32_t)ySize * 2U;
    uint32_t aligned_addr = dst_addr & ~31U;
    uint32_t aligned_size = ((dst_addr - aligned_addr) + region + 31U) & ~31U;

    DMA2D_Wait_TransferComplete(portMAX_DELAY);
    DMA2D->CR      = 0x00000000UL | (1U << 9U);
    DMA2D->FGMAR   = (uint32_t)pSrc;
    DMA2D->OMAR    = dst_addr;
    DMA2D->FGOR    = (uint32_t)OffLineSrc;
    DMA2D->OOR     = (uint32_t)OffLineDst;
    DMA2D->FGPFCCR = LTDC_PIXEL_FORMAT_RGB565;
    DMA2D->OPFCCR  = LTDC_PIXEL_FORMAT_RGB565;
    DMA2D->NLR     = ((uint32_t)xSize << 16U) | (uint32_t)ySize;
    DMA2D->CR     |= DMA2D_CR_START;
    DMA2D_Wait_TransferComplete(portMAX_DELAY);

    SCB_InvalidateDCache_by_Addr((uint32_t *)aligned_addr, (int32_t)aligned_size);
    DMA2D_Bus_RecursiveUnlock();
#else
    U16 *src      = (U16 *)pSrc;
    U16 *dst      = (U16 *)pDst;
    int  strideSrc = xSize + OffLineSrc;
    int  strideDst = xSize + OffLineDst;
    int  x, y;

    for (y = 0; y < ySize; y++) {
        for (x = 0; x < xSize; x++) {
            dst[x] = src[x];
        }
        src += strideSrc;
        dst += strideDst;
    }
#endif
}

/*
*********************************************************************************************************
*	?? ?? ??: _DMA_CopyRGB565
*	???????: ???DMA2D?????????????????????????????????*	??    ??: pSrc          ???????????
*             pDst          ????????????
*             xSize         ??????????X??????????????????*             ySize         ??????????Y?????????????*             OffLineSrc    ???????????????*             OffLineDst    ???????????*	?? ?? ?: ??
*********************************************************************************************************
*/
static void _DMA_CopyRGB565(const void * pSrc, void * pDst, int xSize, int ySize, int OffLineSrc, int OffLineDst)
{
	DMA2D_Bus_RecursiveLock();
	DMA2D_Wait_TransferComplete(portMAX_DELAY);
	DMA2D->CR      = 0x00000000UL | (1 << 9);
	DMA2D->FGMAR   = (U32)pSrc;
	DMA2D->OMAR    = (U32)pDst;
	DMA2D->FGOR    = OffLineSrc;
	DMA2D->OOR     = OffLineDst;
	DMA2D->FGPFCCR = LTDC_PIXEL_FORMAT_RGB565;
	DMA2D->NLR     = (U32)(xSize << 16) | (U16)ySize;

	DMA2D->CR   |= DMA2D_CR_START;
	DMA2D_Wait_TransferComplete(portMAX_DELAY);
	DMA2D_Bus_RecursiveUnlock();
}

/*
 * RGB565：起始列为奇数 x 时像素地址仅为 2 字节对齐。STM32 DMA2D OMAR 要求字对齐；
 * GUIDRV_LIN_16 的 CPU _FillRect 可能用 32 位写，在 UNALIGN_TRP=1 时触发 UsageFault(HFSR.FORCED)。
 * 此处仅用 16 位访存，与奇/偶列均兼容。
 */
static void LCDConf_FillRGB565_CPU(void *pDst, int xSize, int ySize, int OffLine, U16 color, int doXor)
{
	U16 *p      = (U16 *)pDst;
	int  stride = xSize + OffLine;
	int  x, y;

	for (y = 0; y < ySize; y++) {
		for (x = 0; x < xSize; x++) {
			if (doXor) {
				p[x] ^= color;
			} else {
				p[x] = color;
			}
		}
		p += stride;
	}
}

/*
*********************************************************************************************************
*	?? ?? ??: _DMA_Fill
*	???????: ???DMA2D????????????????????
*	??    ??: LayerIndex    ???
*             pDst          ????????????
*             xSize         ??????????X??????????????????*             ySize         ??????????Y?????????????*             OffLine       ???????????????*             ColorIndex    ?????????
*	?? ?? ?: ??
*********************************************************************************************************
*/
static void _DMA_Fill(int LayerIndex, void * pDst, int xSize, int ySize, int OffLine, U32 ColorIndex) 
{
#if (DMA2D_USE_IN_FILL == 1)
	if ((((uint32_t)pDst) & 3u) != 0u) {
		LCDConf_FillRGB565_CPU(pDst, xSize, ySize, OffLine, (U16)ColorIndex, 0);
		return;
	}
    DMA2D_Bus_RecursiveLock();
    /* 逻辑缓冲已改为 Write-Through，DMA2D 与 CPU 写直达 SDRAM，无需 Invalidate。
     * ManualRotateToPhysical 帧末会 CleanInvalidate 整块逻辑缓冲。 */
        DMA2D_Wait_TransferComplete(portMAX_DELAY);
    DMA2D->CR     = 0x00030000UL;
    DMA2D->OCOLR  = (uint32_t)(U16)ColorIndex;
    DMA2D->OMAR   = (uint32_t)pDst;
    DMA2D->OOR    = (uint32_t)OffLine;
    DMA2D->OPFCCR = LTDC_PIXEL_FORMAT_RGB565;
    DMA2D->NLR    = ((uint32_t)xSize << 16U) | (uint32_t)ySize;
    DMA2D->CR    |= DMA2D_CR_START;
    DMA2D_Wait_TransferComplete(portMAX_DELAY);
    DMA2D_Bus_RecursiveUnlock();
#else
	LCDConf_FillRGB565_CPU(pDst, xSize, ySize, OffLine, (U16)ColorIndex, 0);
#endif
	(void)LayerIndex;
}

/*
*********************************************************************************************************
*	?? ?? ??: _DMA_AlphaBlendingBulk
*	???????: ????????????????
*	??    ??: pColorFG    ??????????????
*             pColorBG    ???????????????
*             pColorDst   ????????????*             NumItems    ???????
*	?? ?? ?: ??
*********************************************************************************************************
*/
static void _DMA_AlphaBlendingBulk(LCD_COLOR * pColorFG, LCD_COLOR * pColorBG, LCD_COLOR * pColorDst, U32 NumItems) 
{  
	DMA2D_Bus_RecursiveLock();
	DMA2D_Wait_TransferComplete(portMAX_DELAY);
	DMA2D->CR      = 0x00020000UL | (1 << 9);
	DMA2D->FGMAR   = (U32)pColorFG;
	DMA2D->BGMAR   = (U32)pColorBG;
	DMA2D->OMAR    = (U32)pColorDst;
	DMA2D->FGOR    = 0;
	DMA2D->BGOR    = 0;
	DMA2D->OOR     = 0;
	DMA2D->FGPFCCR = LTDC_PIXEL_FORMAT_ARGB8888;
	DMA2D->BGPFCCR = LTDC_PIXEL_FORMAT_ARGB8888;
	DMA2D->OPFCCR  = LTDC_PIXEL_FORMAT_ARGB8888;
	DMA2D->NLR     = (U32)(NumItems << 16) | 1;

	DMA2D->CR   |= DMA2D_CR_START;
	DMA2D_Wait_TransferComplete(portMAX_DELAY);
	DMA2D_Bus_RecursiveUnlock();
}

/*
*********************************************************************************************************
*	?? ?? ??: _DMA_MixColorsBulk
*	???????: ????????????????????Alpha???????
*	??    ??: pColorFG    ??????????????
*             pColorBG    ???????????????
*             pColorDst   ????????????*             Intens      Alpha???????
*             NumItems    ???????
*	?? ?? ?: ??
*********************************************************************************************************
*/
static void _DMA_MixColorsBulk(LCD_COLOR * pColorFG, LCD_COLOR * pColorBG, LCD_COLOR * pColorDst, U8 Intens, U32 NumItems) 
{
	DMA2D_Bus_RecursiveLock();
	DMA2D_Wait_TransferComplete(portMAX_DELAY);
	DMA2D->CR      = 0x00020000UL | (1 << 9);
	DMA2D->FGMAR   = (U32)pColorFG;
	DMA2D->BGMAR   = (U32)pColorBG;
	DMA2D->OMAR    = (U32)pColorDst;
	DMA2D->FGPFCCR = LTDC_PIXEL_FORMAT_ARGB8888
				 | (1UL << 16)
				 | ((U32)Intens << 24);
	DMA2D->BGPFCCR = LTDC_PIXEL_FORMAT_ARGB8888
				 | (0UL << 16)
				 | ((U32)(255 - Intens) << 24);
	DMA2D->OPFCCR  = LTDC_PIXEL_FORMAT_ARGB8888;
	DMA2D->NLR     = (U32)(NumItems << 16) | 1;

	DMA2D->CR   |= DMA2D_CR_START;
	DMA2D_Wait_TransferComplete(portMAX_DELAY);
	DMA2D_Bus_RecursiveUnlock();
}

/*
*********************************************************************************************************
*	?? ?? ??: _DMA_ConvertColor
*	???????: ?????????
*	??    ??: pSrc             ???????
*             pDst             ?????????
*             PixelFormatSrc   ???????????
*             PixelFormatDst   ???????????
*             NumItems         ???????
*	?? ?? ?: ??
*********************************************************************************************************
*/
static void _DMA_ConvertColor(void * pSrc, void * pDst,  U32 PixelFormatSrc, U32 PixelFormatDst, U32 NumItems) 
{
	DMA2D_Bus_RecursiveLock();
	DMA2D_Wait_TransferComplete(portMAX_DELAY);
	DMA2D->CR      = 0x00010000UL | (1 << 9);
	DMA2D->FGMAR   = (U32)pSrc;
	DMA2D->OMAR    = (U32)pDst;
	DMA2D->FGOR    = 0;
	DMA2D->OOR     = 0;
	DMA2D->FGPFCCR = PixelFormatSrc;
	DMA2D->OPFCCR  = PixelFormatDst;
	DMA2D->NLR     = (U32)(NumItems << 16) | 1;

	DMA2D->CR   |= DMA2D_CR_START;
	DMA2D_Wait_TransferComplete(portMAX_DELAY);
	DMA2D_Bus_RecursiveUnlock();
}

/*
*********************************************************************************************************
*	?? ?? ??: _DMA_DrawBitmapL8
*	???????: ????L8?????
*	??    ??: pSrc             ???????
*             pDst             ?????????
*             OffSrc           ??????????
*             OffDst           ????????????
*             PixelFormatDst   ???????????
*             xSize            ????
*             ySize            ????
*	?? ?? ?: ??
*********************************************************************************************************
*/
static void _DMA_DrawBitmapL8(void * pSrc, void * pDst,  U32 OffSrc, U32 OffDst, U32 PixelFormatDst, U32 xSize, U32 ySize) 
{
	DMA2D_Bus_RecursiveLock();
	DMA2D_Wait_TransferComplete(portMAX_DELAY);
	DMA2D->CR      = 0x00010000UL | (1 << 9);
	DMA2D->FGMAR   = (U32)pSrc;
	DMA2D->OMAR    = (U32)pDst;
	DMA2D->FGOR    = OffSrc;
	DMA2D->OOR     = OffDst;
	DMA2D->FGPFCCR = LTDC_PIXEL_FORMAT_L8;
	DMA2D->OPFCCR  = PixelFormatDst;
	DMA2D->NLR     = (U32)(xSize << 16) | ySize;

	DMA2D->CR   |= DMA2D_CR_START;
	DMA2D_Wait_TransferComplete(portMAX_DELAY);
	DMA2D_Bus_RecursiveUnlock();
}

#if DrawBitmapA4Enalbe == 1
/*
*********************************************************************************************************
*	?? ?? ??: _DMA_DrawBitmapA4
*	???????: ????A4?????
*	??    ??: pSrc             ???????
*             pDst             ?????????
*             OffSrc           ??????????
*             OffDst           ????????????
*             PixelFormatDst   ???????????
*             xSize            ????
*             ySize            ????
*	?? ?? ?: 0
*********************************************************************************************************
*/
static int _DMA_DrawBitmapA4(void * pSrc, void * pDst,  U32 OffSrc, U32 OffDst, U32 PixelFormatDst, U32 xSize, U32 ySize) 
{
	U8 * pRD;
	U8 * pWR;
	U32 NumBytes, Color, Index;

	NumBytes = ((xSize + 1) & ~1) * ySize;
	if ((NumBytes > sizeof(_aBuffer)) || (NumBytes == 0)) 
	{
		return 1;
	}
	
	pWR = (U8 *)_aBuffer;
	pRD = (U8 *)pSrc;
	do 
	{
		*pWR++ = _aMirror[*pRD++];
	} while (--NumBytes);

	Index = LCD_GetColorIndex();
	Color = LCD_Index2Color(Index);

	DMA2D_Bus_RecursiveLock();
	DMA2D_Wait_TransferComplete(portMAX_DELAY);
	DMA2D->CR = 0x00020000UL;
	DMA2D->FGCOLR  = ((Color & 0xFF) << 16)
			       |  (Color & 0xFF00)
			       | ((Color >> 16) & 0xFF);
	DMA2D->FGMAR   = (U32)_aBuffer;
	DMA2D->FGOR    = 0;
	DMA2D->FGPFCCR = 0xA;
	DMA2D->NLR     = (U32)((xSize + OffSrc) << 16) | ySize;
	DMA2D->BGMAR   = (U32)pDst;
	DMA2D->BGOR    = OffDst - OffSrc;
	DMA2D->BGPFCCR = PixelFormatDst;
	DMA2D->OMAR    = DMA2D->BGMAR;
	DMA2D->OOR     = DMA2D->BGOR;
	DMA2D->OPFCCR  = DMA2D->BGPFCCR;

	DMA2D->CR   |= DMA2D_CR_START;
	DMA2D_Wait_TransferComplete(portMAX_DELAY);
	DMA2D_Bus_RecursiveUnlock();

	return 0;
}
#endif

/*
*********************************************************************************************************
*	?? ?? ??: _DMA_DrawAlphaBitmap
*	???????: ???????????????
*	??    ??: pSrc             ???????
*             pDst             ?????????
*             xSize            ????
*             ySize            ????
*             OffLineSrc       ??????????
*             OffLineDst       ????????????
*             PixelFormatDst   ???????????
*	?? ?? ?: 0
*********************************************************************************************************
*/
static void _DMA_DrawAlphaBitmap(void * pDst, const void * pSrc, int xSize, int ySize, int OffLineSrc, int OffLineDst, int PixelFormat) 
{
	DMA2D_Bus_RecursiveLock();
	DMA2D_Wait_TransferComplete(portMAX_DELAY);
	DMA2D->CR      = 0x00020000UL | (1 << 9);
	DMA2D->FGMAR   = (U32)pSrc;
	DMA2D->BGMAR   = (U32)pDst;
	DMA2D->OMAR    = (U32)pDst;
	DMA2D->FGOR    = OffLineSrc;
	DMA2D->BGOR    = OffLineDst;
	DMA2D->OOR     = OffLineDst;
	DMA2D->FGPFCCR = LTDC_PIXEL_FORMAT_ARGB8888;
	DMA2D->BGPFCCR = PixelFormat;
	DMA2D->OPFCCR  = PixelFormat;
	DMA2D->NLR     = (U32)(xSize << 16) | (U16)ySize;

	DMA2D->CR   |= DMA2D_CR_START;
	DMA2D_Wait_TransferComplete(portMAX_DELAY);
	DMA2D_Bus_RecursiveUnlock();
}

/*
*********************************************************************************************************
*	?? ?? ??: _DMA_LoadLUT
*	???????: ?????????
*	??    ??: pColor     ????????
*             NumItems   ?????????
*	?? ?? ?: 0
*********************************************************************************************************
*/
static void _DMA_LoadLUT(LCD_COLOR * pColor, U32 NumItems)
{
	/* ????DMA2D */
	DMA2D->FGCMAR  = (U32)pColor;
	DMA2D->FGPFCCR  = LTDC_PIXEL_FORMAT_RGB888
				  | ((NumItems - 1) & 0xFF) << 8;
	
	/* ???????? */
	DMA2D->FGPFCCR |= (1 << 5);
}

/*
*********************************************************************************************************
*	?? ?? ??: _DMA_AlphaBlending
*	???????: ????????????????
*	??    ??: pColorFG    ??????????????
*             pColorBG    ???????????????
*             pColorDst   ????????????*             NumItems    ???????
*	?? ?? ?: ??
*********************************************************************************************************
*/
static void _DMA_AlphaBlending(LCD_COLOR * pColorFG, LCD_COLOR * pColorBG, LCD_COLOR * pColorDst, U32 NumItems) 
{
	_DMA_AlphaBlendingBulk(pColorFG, pColorBG, pColorDst, NumItems);
}

/*
*********************************************************************************************************
*	?? ?? ??: _DMA_Index2ColorBulk
*	???????: ???DMA2D?????????????????????????emWin??32?ARGB????????
*	??    ??: pIndex       ???????????
*             pColor       ???????????emWin????????
*             NumItems     ????????????
*             SizeOfIndex  ????
*             PixelFormat  ??????????????????
*	?? ?? ?: ??
*********************************************************************************************************
*/
static void _DMA_Index2ColorBulk(void * pIndex, LCD_COLOR * pColor, U32 NumItems, U8 SizeOfIndex, U32 PixelFormat) 
{
	_DMA_ConvertColor(pIndex, pColor, PixelFormat, LTDC_PIXEL_FORMAT_ARGB8888, NumItems);
}

/*
*********************************************************************************************************
*	?? ?? ??: _DMA_Color2IndexBulk
*	???????: ???DMA2D????emWin??32?ARGB?????????????????????????????????
*	??    ??: pIndex       ???????????
*             pColor       ???????????emWin????????
*             NumItems     ????????????
*             SizeOfIndex  ????
*             PixelFormat  ??????????????????
*	?? ?? ?: ??
*********************************************************************************************************
*/
static void _DMA_Color2IndexBulk(LCD_COLOR * pColor, void * pIndex, U32 NumItems, U8 SizeOfIndex, U32 PixelFormat)
{
	_DMA_ConvertColor(pColor, pIndex, LTDC_PIXEL_FORMAT_ARGB8888, PixelFormat, NumItems);
}

/*
*********************************************************************************************************
*	?? ?? ??: _DMA_MixColorsBulk
*	???????: ?????????????????????????????*	??    ??: pFG   ???????
*             pBG   ????????
*             pDst  ??????????????*             OffFG    ?????????
*             OffBG    ??????????
*             OffDest  ??????????*             xSize    ?????x?????*             ySize    ?????y?????*             Intens   ??alpha?
*	?? ?? ?: ??
*********************************************************************************************************
*/
static void _LCD_MixColorsBulk(U32 * pFG, U32 * pBG, U32 * pDst, unsigned OffFG, unsigned OffBG, unsigned OffDest, unsigned xSize, unsigned ySize, U8 Intens) 
{
	int y;

	GUI_USE_PARA(OffFG);
	GUI_USE_PARA(OffDest);

	for (y = 0; y < ySize; y++) 
	{
		_DMA_MixColorsBulk(pFG, pBG, pDst, Intens, xSize);
		pFG  += xSize + OffFG;
		pBG  += xSize + OffBG;
		pDst += xSize + OffDest;
	}
}

/*
*********************************************************************************************************
*	?? ?? ??: _GetBufferSize
*	???????: ??????????????*	??    ??: LayerIndex    ???
*	?? ?? ?: ??????*********************************************************************************************************
*/
static U32 _GetBufferSize(int LayerIndex) 
{
	U32 BufferSize;

	BufferSize = _axSize[LayerIndex] * _aySize[LayerIndex] * _aBytesPerPixels[LayerIndex];

	return BufferSize;
}

/*
*********************************************************************************************************
*	?? ?? ??: _LCD_CopyBuffer
*	???????: ?????????????????????????????????????????????
*	??    ??: LayerIndex    ???
*             IndexSrc      ?????????*             IndexDst      ?????????*	?? ?? ?: ??
*********************************************************************************************************
*/
static void _LCD_CopyBuffer(int LayerIndex, int IndexSrc, int IndexDst) 
{
	U32 BufferSize, AddrSrc, AddrDst;

	BufferSize = _GetBufferSize(LayerIndex);
	AddrSrc    = _aAddr[LayerIndex] + BufferSize * IndexSrc;
	AddrDst    = _aAddr[LayerIndex] + BufferSize * IndexDst;
	_DMA_Copy(LayerIndex, (void *)AddrSrc, (void *)AddrDst, _axSize[LayerIndex], _aySize[LayerIndex], 0, 0);
	
	/* ????????????????Buffer[IndexDst] */
	_aBufferIndex[LayerIndex] = IndexDst;
}

/*
*********************************************************************************************************
*	?? ?? ??: _LCD_CopyRect
*	???????: ???????????????????????????????????????????????
*	??    ??: LayerIndex    ???
*             x0            ?????x?????
*             y0            ?????y?????
*             x1            ????x?????
*             y1            ????y?????
*             xSize         ??????x?????*             ySize         ??????y?????*	?? ?? ?: ??
*********************************************************************************************************
*/
static void _LCD_CopyRect(int LayerIndex, int x0, int y0, int x1, int y1, int xSize, int ySize)
{
	U32 BufferSize, AddrSrc, AddrDst;
	int OffLine;

	BufferSize = _GetBufferSize(LayerIndex);
	AddrSrc = _aAddr[LayerIndex] + BufferSize * _aBufferIndex[LayerIndex] + (y0 * _axSize[LayerIndex] + x0) * _aBytesPerPixels[LayerIndex];
	AddrDst = _aAddr[LayerIndex] + BufferSize * _aBufferIndex[LayerIndex] + (y1 * _axSize[LayerIndex] + x1) * _aBytesPerPixels[LayerIndex];
	OffLine = _axSize[LayerIndex] - xSize;
	_DMA_Copy(LayerIndex, (void *)AddrSrc, (void *)AddrDst, xSize, ySize, OffLine, OffLine);
}

/*
*********************************************************************************************************
*	?? ?? ??: _LCD_FillRect
*	???????: ????????????????????
*	??    ??: LayerIndex    ???
*             x0            ???x?????
*             y0            ???y?????
*             x1            ????x?????
*             y1            ????y?????
*             PixelIndex    ?????????
*	?? ?? ?: ??
*********************************************************************************************************
*/
static void _LCD_FillRect(int LayerIndex, int x0, int y0, int x1, int y1, U32 PixelIndex) 
{
	U32 BufferSize, AddrDst;
	int xSize, ySize;

	if (GUI_GetDrawMode() == GUI_DM_XOR) 
	{
		/* 勿调用库 LCD_FillRect：GUIDRV_LIN_16 在奇数列会做 32 位写，触发非对齐 UsageFault */
		if (_aOrientation[LayerIndex] == ROTATION_CW) {
			int phys_c0 = _axSize[LayerIndex] - 1 - y1;
			xSize = y1 - y0 + 1;
			ySize = x1 - x0 + 1;
			BufferSize = _GetBufferSize(LayerIndex);
			AddrDst = _aAddr[LayerIndex] + BufferSize * _aBufferIndex[LayerIndex]
			        + (x0 * _axSize[LayerIndex] + phys_c0) * _aBytesPerPixels[LayerIndex];
			LCDConf_FillRGB565_CPU((void *)AddrDst, xSize, ySize, _axSize[LayerIndex] - xSize, (U16)PixelIndex, 1);
		} else {
			xSize = x1 - x0 + 1;
			ySize = y1 - y0 + 1;
			BufferSize = _GetBufferSize(LayerIndex);
			AddrDst = _aAddr[LayerIndex] + BufferSize * _aBufferIndex[LayerIndex]
			        + (y0 * _axSize[LayerIndex] + x0) * _aBytesPerPixels[LayerIndex];
			LCDConf_FillRGB565_CPU((void *)AddrDst, xSize, ySize, _axSize[LayerIndex] - xSize, (U16)PixelIndex, 1);
		}
	}
	else if (_aOrientation[LayerIndex] == ROTATION_CW)
	{
		int phys_c0 = _axSize[LayerIndex] - 1 - y1;
		xSize = y1 - y0 + 1;
		ySize = x1 - x0 + 1;
		BufferSize = _GetBufferSize(LayerIndex);
		AddrDst = _aAddr[LayerIndex] + BufferSize * _aBufferIndex[LayerIndex]
		        + (x0 * _axSize[LayerIndex] + phys_c0) * _aBytesPerPixels[LayerIndex];
		_DMA_Fill(LayerIndex, (void *)AddrDst, xSize, ySize, _axSize[LayerIndex] - xSize, PixelIndex);
	}
	else
	{
		xSize = x1 - x0 + 1;
		ySize = y1 - y0 + 1;
		BufferSize = _GetBufferSize(LayerIndex);
		AddrDst = _aAddr[LayerIndex] + BufferSize * _aBufferIndex[LayerIndex] + (y0 * _axSize[LayerIndex] + x0) * _aBytesPerPixels[LayerIndex];
		_DMA_Fill(LayerIndex, (void *)AddrDst, xSize, ySize, _axSize[LayerIndex] - xSize, PixelIndex);
	}
}

/*
*********************************************************************************************************
*	?? ?? ??: _LCD_DrawBitmap32bpp
*	???????: ????32bpp?????
*	??    ??: LayerIndex      ???   
*             x               X????????
*             y               Y????????
*             p               ???????
*             xSize           ????
*             ySize           ????
*             BytesPerLine    ????????
*	?? ?? ?: ??
*********************************************************************************************************
*/
static void _LCD_DrawBitmap32bpp(int LayerIndex, int x, int y, U16 const * p, int xSize, int ySize, int BytesPerLine) 
{
	U32 BufferSize, AddrDst;
	int OffLineSrc, OffLineDst;

	BufferSize = _GetBufferSize(LayerIndex);
	AddrDst = _aAddr[LayerIndex] + BufferSize * _aBufferIndex[LayerIndex] + (y * _axSize[LayerIndex] + x) * _aBytesPerPixels[LayerIndex];
	OffLineSrc = (BytesPerLine / 4) - xSize;
	OffLineDst = _axSize[LayerIndex] - xSize;
	_DMA_Copy(LayerIndex, (void *)p, (void *)AddrDst, xSize, ySize, OffLineSrc, OffLineDst);
}

/*
*********************************************************************************************************
*	?? ?? ??: _LCD_DrawBitmap16bpp
*	???????: ????16bpp?????
*	??    ??: LayerIndex      ???   
*             x               X????????
*             y               Y????????
*             p               ???????
*             xSize           ????
*             ySize           ????
*             BytesPerLine    ????????
*	?? ?? ?: ??
*********************************************************************************************************
*/
void _LCD_DrawBitmap16bpp(int LayerIndex, int x, int y, U16 const * p, int xSize, int ySize, int BytesPerLine) 
{
	U32 BufferSize, AddrDst;
	int OffLineSrc, OffLineDst;

	BufferSize = _GetBufferSize(LayerIndex);
	AddrDst = _aAddr[LayerIndex] + BufferSize * _aBufferIndex[LayerIndex] + (y * _axSize[LayerIndex] + x) * _aBytesPerPixels[LayerIndex];
	OffLineSrc = (BytesPerLine / 2) - xSize;
	OffLineDst = _axSize[LayerIndex] - xSize;
	_DMA_Copy(LayerIndex, (void *)p, (void *)AddrDst, xSize, ySize, OffLineSrc, OffLineDst);
}

/*
*********************************************************************************************************
*	?? ?? ??: _LCD_DrawBitmap8bpp
*	???????: ????8bpp?????
*	??    ??: LayerIndex      ???   
*             x               X????????
*             y               Y????????
*             p               ???????
*             xSize           ????
*             ySize           ????
*             BytesPerLine    ????????
*	?? ?? ?: ??
*********************************************************************************************************
*/
static void _LCD_DrawBitmap8bpp(int LayerIndex, int x, int y, U8 const * p, int xSize, int ySize, int BytesPerLine) {
	U32 BufferSize, AddrDst;
	int OffLineSrc, OffLineDst;
	U32 PixelFormat;

	PixelFormat = _GetPixelformat(LayerIndex);
	BufferSize = _GetBufferSize(LayerIndex);
	AddrDst = _aAddr[LayerIndex] + BufferSize * _aBufferIndex[LayerIndex] + (y * _axSize[LayerIndex] + x) * _aBytesPerPixels[LayerIndex];
	OffLineSrc = BytesPerLine - xSize;
	OffLineDst = _axSize[LayerIndex] - xSize;
	_DMA_DrawBitmapL8((void *)p, (void *)AddrDst, OffLineSrc, OffLineDst, PixelFormat, xSize, ySize);
}

#if DrawBitmapA4Enalbe == 1
/*
*********************************************************************************************************
*	?? ?? ??: _LCD_DrawBitmap4bpp
*	???????: ????4bpp?????
*	??    ??: LayerIndex      ???   
*             x               X????????
*             y               Y????????
*             p               ???????
*             xSize           ????
*             ySize           ????
*             BytesPerLine    ????????
*	?? ?? ?: ??
*********************************************************************************************************
*/
static int _LCD_DrawBitmap4bpp(int LayerIndex, int x, int y, U8 const * p, int xSize, int ySize, int BytesPerLine) {
	U32 BufferSize, AddrDst;
	int OffLineSrc, OffLineDst;
	U32 PixelFormat;

	if (x < 0) 
	{
		return 1;
	}
	
	if ((x + xSize) >= _axSize[LayerIndex]) 
	{
		return 1;
	}
	
	if (y < 0) 
	{
		return 1;
	}
	
	if ((y + ySize) >= _aySize[LayerIndex]) 
	{
		return 1;
	}
	
	PixelFormat = _GetPixelformat(LayerIndex);

	if (PixelFormat > LTDC_PIXEL_FORMAT_ARGB4444) 
	{
		return 1;
	}
	
	BufferSize = _GetBufferSize(LayerIndex);
	AddrDst = _aAddr[LayerIndex] + BufferSize * _aBufferIndex[LayerIndex] + (y * _axSize[LayerIndex] + x) * _aBytesPerPixels[LayerIndex];
	OffLineSrc = (BytesPerLine * 2) - xSize;
	OffLineDst = _axSize[LayerIndex] - xSize;
	return _DMA_DrawBitmapA4((void *)p, (void *)AddrDst, OffLineSrc, OffLineDst, PixelFormat, xSize, ySize);;
}
#endif

/*
*********************************************************************************************************
*	?? ?? ??: _LCD_DrawMemdev16bpp
*	???????: ????16bpp????
*	??    ??: pDst               ???????   
*             pSrc               ?????????
*             xSize              ??????
*             ySize              ??????
*             BytesPerLineDst    ?????????????
*             BytesPerLineSrc    ???????????????
*	?? ?? ?: ??
*********************************************************************************************************
*/
static void _LCD_DrawMemdev16bpp(void * pDst, const void * pSrc, int xSize, int ySize, int BytesPerLineDst, int BytesPerLineSrc) 
{
	int OffLineSrc, OffLineDst;

	OffLineSrc = (BytesPerLineSrc / 2) - xSize;
	OffLineDst = (BytesPerLineDst / 2) - xSize;
	_DMA_CopyRGB565(pSrc, pDst, xSize, ySize, OffLineSrc, OffLineDst);
}

/*
*********************************************************************************************************
*	?? ?? ??: _LCD_DrawMemdevAlpha
*	???????: ?????Alpha????????
*	??    ??: pDst               ???????   
*             pSrc               ?????????
*             xSize              ??????
*             ySize              ??????
*             BytesPerLineDst    ?????????????
*             BytesPerLineSrc    ???????????????
*	?? ?? ?: ??
*********************************************************************************************************
*/
static void _LCD_DrawMemdevAlpha(void * pDst, const void * pSrc, int xSize, int ySize, int BytesPerLineDst, int BytesPerLineSrc) 
{
	int OffLineSrc, OffLineDst;

	OffLineSrc = (BytesPerLineSrc / 4) - xSize;
	OffLineDst = (BytesPerLineDst / 4) - xSize;
	_DMA_DrawAlphaBitmap(pDst, pSrc, xSize, ySize, OffLineSrc, OffLineDst, LTDC_PIXEL_FORMAT_ARGB8888);
}

/*
*********************************************************************************************************
*	?? ?? ??: _LCD_DrawBitmapAlpha
*	???????: ?????Alpha???????
*	??    ??: LayerIndex      ???   
*             x               X????????
*             y               Y????????
*             p               ???????
*             xSize           ????
*             ySize           ????
*             BytesPerLine    ????????
*	?? ?? ?: ??
*********************************************************************************************************
*/
static void _LCD_DrawBitmapAlpha(int LayerIndex, int x, int y, const void * p, int xSize, int ySize, int BytesPerLine) 
{
	U32 BufferSize, AddrDst;
	int OffLineSrc, OffLineDst;
	U32 PixelFormat;

	PixelFormat = _GetPixelformat(LayerIndex);
	BufferSize = _GetBufferSize(LayerIndex);
	AddrDst = _aAddr[LayerIndex] + BufferSize * _aBufferIndex[LayerIndex] + (y * _axSize[LayerIndex] + x) * _aBytesPerPixels[LayerIndex];
	OffLineSrc = (BytesPerLine / 4) - xSize;
	OffLineDst = _axSize[LayerIndex] - xSize;
	_DMA_DrawAlphaBitmap((void *)AddrDst, p, xSize, ySize, OffLineSrc, OffLineDst, PixelFormat);
}

/*
*********************************************************************************************************
*	?? ?? ??: _LCD_GetpPalConvTable
*	???????: ????????????????????????????????
*	??    ??: pLogPal   ?????????*             pBitmap   ?????
*             LayerIndex  ???????
*	?? ?? ?: ?????????????
*********************************************************************************************************
*/
static LCD_PIXELINDEX * _LCD_GetpPalConvTable(const LCD_LOGPALETTE GUI_UNI_PTR * pLogPal, const GUI_BITMAP GUI_UNI_PTR * pBitmap, int LayerIndex) 
{
	void (* pFunc)(void);
	int DoDefault = 0;

	/* 8bpp */
	if (pBitmap->BitsPerPixel == 8) 
	{
		pFunc = LCD_GetDevFunc(LayerIndex, LCD_DEVFUNC_DRAWBMP_8BPP);
		if (pFunc) 
		{
			if (pBitmap->pPal) 
			{
				if (pBitmap->pPal->HasTrans) 
				{
					DoDefault = 1;
				}
			}
			else
			{
				DoDefault = 1;
			}
		}
		else
		{
			DoDefault = 1;
		}
	}
	else 
	{
	DoDefault = 1;
	}

	/* ????????????*/
	if (DoDefault) 
	{
		/* ????????? */
		return LCD_GetpPalConvTable(pLogPal);
	}

	/* DMA2D????LUT */
	_DMA_LoadLUT((U32 *)pLogPal->pPalEntries, pLogPal->NumEntries);

	/* ?????NULL */
	return _pBuffer_DMA2D;
}

/*
*********************************************************************************************************
*	?? ?? ??: LCD_X_DisplayDriver
*	???????: ???????????
*	??    ??: LayerIndex   ???
*             Cmd          ????
*             pData        ??????
*	?? ?? ?: ???????0????????-1
*********************************************************************************************************
*/
int LCD_X_DisplayDriver(unsigned LayerIndex, unsigned Cmd, void * pData) 
{
	int r = 0;
	U32 addr;

	switch (Cmd) 
	{
		case LCD_X_INITCONTROLLER: 
		{
			/* LTDC 已在 BSP 中初始化，此处不再重复配置 */
			/* 只记录帧缓冲区地址 */
			layer_prop[LayerIndex].address = VRAM_PHYSICAL_0_ADDR;
			break;
		}
		
		case LCD_X_SETORG:
		{
			/* 禁止修改 LTDC 地址！LTDC 始终指向物理缓冲区 */
			/* emWin 渲染到逻辑缓冲区，_CustomCopyBuffer 负责旋转到物理缓冲区 */
			(void)pData;
			break;
		}
		
		case LCD_X_SHOWBUFFER: 
		{
			/* ???????????Index????????????????? */
			LCD_X_SHOWBUFFER_INFO * p;

			p = (LCD_X_SHOWBUFFER_INFO *)pData;
			g_emwin_showbuffer_cnt++;
			g_emwin_last_draw_buf = p->Index;   /* ??????????????*/
			_aPendingBuffer[LayerIndex] = p->Index;
			break;
		}
		
		case LCD_X_SETLUTENTRY: 
		{
			/* ???????????? */
			LCD_X_SETLUTENTRY_INFO * p;

			p = (LCD_X_SETLUTENTRY_INFO *)pData;
			HAL_LTDC_ConfigCLUT(&hltdc, (uint32_t*)p->Color, p->Pos, LayerIndex);
			break;
		}
		
		case LCD_X_ON: 
		{
			/* ???LTDC  */
			__HAL_LTDC_ENABLE(&hltdc);
			break;
		}
		
		case LCD_X_OFF: 
		{
			/* ???LTDC */
			__HAL_LTDC_DISABLE(&hltdc);
			break;
		}
		
		case LCD_X_SETVIS: 
		{
			/* ????????*/
			LCD_X_SETVIS_INFO * p;

			p = (LCD_X_SETVIS_INFO *)pData;
			if(p->OnOff == ENABLE )
			{
				__HAL_LTDC_LAYER_ENABLE(&hltdc, LayerIndex); 
			}
			else
			{
				__HAL_LTDC_LAYER_DISABLE(&hltdc, LayerIndex);
			}
			
			__HAL_LTDC_RELOAD_CONFIG(&hltdc);
			break;
		}
		
		case LCD_X_SETPOS:
		{
			/* ????????????? */
			LCD_X_SETPOS_INFO * p;

			p = (LCD_X_SETPOS_INFO *)pData;    
			HAL_LTDC_SetWindowPosition(&hltdc, p->xPos, p->yPos, LayerIndex);
			break;
		}
		
		case LCD_X_SETSIZE:
		{
			/* ??????????*/
			LCD_X_SETSIZE_INFO * p;
			int xPos, yPos;

			GUI_GetLayerPosEx(LayerIndex, &xPos, &yPos);
			p = (LCD_X_SETSIZE_INFO *)pData;
			if (LCD_GetSwapXYEx(LayerIndex))
			{
				_axSize[LayerIndex] = p->ySize;
				_aySize[LayerIndex] = p->xSize;
			}
			else
			{
				_axSize[LayerIndex] = p->xSize;
				_aySize[LayerIndex] = p->ySize;
			}
			
			HAL_LTDC_SetWindowPosition(&hltdc, xPos, yPos, LayerIndex);
			break;
		}
		
		case LCD_X_SETALPHA: 
		{
			/* ??????? */
			LCD_X_SETALPHA_INFO * p;

			p = (LCD_X_SETALPHA_INFO *)pData;
			HAL_LTDC_SetAlpha(&hltdc, p->Alpha, LayerIndex);
			break;
		}
		
		case LCD_X_SETCHROMAMODE: 
		{
			/* ??????????*/
			LCD_X_SETCHROMAMODE_INFO * p;

			p = (LCD_X_SETCHROMAMODE_INFO *)pData;
			if(p->ChromaMode != 0)
			{
				HAL_LTDC_EnableColorKeying(&hltdc, LayerIndex);
			}
			else
			{
				HAL_LTDC_DisableColorKeying(&hltdc, LayerIndex);      
			}
			break;
		}
		
		case LCD_X_SETCHROMA: 
		{
			/* ?????? */
			LCD_X_SETCHROMA_INFO * p;
			U32 Color;

			p = (LCD_X_SETCHROMA_INFO *)pData;
			Color = ((p->ChromaMin & 0xFF0000) >> 16) | (p->ChromaMin & 0x00FF00) | ((p->ChromaMin & 0x0000FF) << 16);
			HAL_LTDC_ConfigColorKeying(&hltdc, Color, LayerIndex);
			break;
		}
		
		default:
			r = -1;
			break;
	}

	return r;
}

/*
*********************************************************************************************************
*	?? ?? ??: LCD_X_DisplayDriver
*	???????: ?????????
*	??    ??: ??
*	?? ?? ?: ??
*********************************************************************************************************
*/
void LCD_X_Config(void) 
{
	int i;
	U32 PixelFormat;

	/* ????????*/
	// LCD_LL_Init ();  /* Skip: Already initialized by LCD_RGB_Init() */

#if ClearCacheHookEnalbe == 1
	GUI_DCACHE_SetClearCacheHook(_ClearCacheHook);
#endif

	/* 单缓冲模式：不配置 emWin 多缓冲，旋转由 ManualRotateToPhysical() 手动控制 */
	/* LTDC 行中断必须启用：ManualRotateToPhysical() 的 VSYNC 等待依赖此中断 */
	_EnableLTDCLineIRQ();

	/* ?????????????????????? */
	GUI_DEVICE_CreateAndLink(DISPLAY_DRIVER_0, COLOR_CONVERSION_0, 0, 0);

	/* Layer0: ???? 800x480?emWin ?????????DMA2D ???? */
	LCD_SetSizeEx (0, XSIZE_PHYS, YSIZE_PHYS);
	LCD_SetVSizeEx(0, XSIZE_PHYS, YSIZE_PHYS);
	
#if (GUI_NUM_LAYERS > 1)
	/* ?????????????????????? */
	GUI_DEVICE_CreateAndLink(DISPLAY_DRIVER_1, COLOR_CONVERSION_1, 0, 1);

	/* ???????2 */
	if (LCD_GetSwapXYEx(1)) 
	{
		LCD_SetSizeEx (1, LCD_RGB_HEIGHT, LCD_RGB_WIDTH);
		LCD_SetVSizeEx(1,  LCD_RGB_HEIGHT * NUM_VSCREENS, LCD_RGB_WIDTH);
	}
	else 
	{
		LCD_SetSizeEx (1, LCD_RGB_WIDTH, LCD_RGB_HEIGHT);
		LCD_SetVSizeEx(1, LCD_RGB_WIDTH, LCD_RGB_HEIGHT * NUM_VSCREENS);
	}
#endif
	
	/* ????RAM?????????????????? */
	for (i = 0; i < GUI_NUM_LAYERS; i++) 
	{
		/* ????RAM??? */
		LCD_SetVRAMAddrEx(i, (void *)(_aAddr[i]));
		
		/* ????????????? */
		_aBytesPerPixels[i] = LCD_GetBitsPerPixelEx(i) >> 3;
	}
	
	/* ???????????? */
	for (i = 0; i < GUI_NUM_LAYERS; i++) 
	{
		PixelFormat = _GetPixelformat(i);
		
		/* CopyBuffer???????????? */
		LCD_SetDevFunc(i, LCD_DEVFUNC_COPYBUFFER, (void(*)(void))_CustomCopyBuffer);

		/* ROTATION_CW ??DMA2D FillRect ???????? FillRect ???? */
		if ((PixelFormat <= LTDC_PIXEL_FORMAT_ARGB4444) && (_aOrientation[i] == ROTATION_0))
		{
			LCD_SetDevFunc(i, LCD_DEVFUNC_FILLRECT, (void(*)(void))_LCD_FillRect);
		}

		if (_aOrientation[i] == ROTATION_0)
		{
			/* CopyRect????? */
			LCD_SetDevFunc(i, LCD_DEVFUNC_COPYRECT, (void(*)(void))_LCD_CopyRect);

			/* 8bpp????? */
			if (PixelFormat <= LTDC_PIXEL_FORMAT_ARGB4444) 
			{
				LCD_SetDevFunc(i, LCD_DEVFUNC_DRAWBMP_8BPP, (void(*)(void))_LCD_DrawBitmap8bpp);
			}

			/* 16bpp????? */
			if (PixelFormat == LTDC_PIXEL_FORMAT_RGB565) 
			{
				LCD_SetDevFunc(i, LCD_DEVFUNC_DRAWBMP_16BPP, (void(*)(void))_LCD_DrawBitmap16bpp);
			}

			/* 32bpp????? */
			if (PixelFormat == LTDC_PIXEL_FORMAT_ARGB8888) 
			{
				LCD_SetDevFunc(i, LCD_DEVFUNC_DRAWBMP_32BPP, (void(*)(void))_LCD_DrawBitmap32bpp);
			}
		}
	}
	/* DMA2D for ARGB1555 */
	GUICC_M1555I_SetCustColorConv(_Color2IndexBulk_M1555I_DMA2D, _Index2ColorBulk_M1555I_DMA2D);

	/* DMA2D for RGB565 */  
	GUICC_M565_SetCustColorConv  (_Color2IndexBulk_M565_DMA2D,   _Index2ColorBulk_M565_DMA2D);

	/* DMA2D for ARGB4444 */
	GUICC_M4444I_SetCustColorConv(_Color2IndexBulk_M4444I_DMA2D, _Index2ColorBulk_M4444I_DMA2D);

	/* DMA2D for RGB888 */
	GUICC_M888_SetCustColorConv  (_Color2IndexBulk_M888_DMA2D,   _Index2ColorBulk_M888_DMA2D);

	/* DMA2D for ARGB8888 */
	GUICC_M8888I_SetCustColorConv(_Color2IndexBulk_M8888I_DMA2D, _Index2ColorBulk_M8888I_DMA2D);

	/* Alpha?????????*/
	GUI_SetFuncAlphaBlending(_DMA_AlphaBlending);

	GUI_SetFuncGetpPalConvTable(_LCD_GetpPalConvTable);

	/* ????????????*/
	GUI_SetFuncMixColorsBulk(_LCD_MixColorsBulk);

#if DrawBitmapA4Enalbe == 1
	GUI_AA_SetpfDrawCharAA4(_LCD_DrawBitmap4bpp); /* ??????????????????*/
#endif

	/* 16bpp????????? */
	GUI_MEMDEV_SetDrawMemdev16bppFunc(_LCD_DrawMemdev16bpp);

	/* Alpha????????? */
	GUI_SetFuncDrawAlpha(_LCD_DrawMemdevAlpha, _LCD_DrawBitmapAlpha);
}

/*
*********************************************************************************************************
*	?????? LCD_DMA2D_GetDrawBufBase
*	????: ???? emWin ???????????????? DMA2D ?????????*	          emWin ???? GUI_MULTIBUF_Begin() ???? LCD_DEVFUNC_COPYBUFFER
*	          ?? _aBufferIndex?????????? emWin ???????????*	??   ?? ??*	?????? ????????????*********************************************************************************************************
*/
uint32_t LCD_DMA2D_GetDrawBufBase(void)
{
	/* _aBufferIndex[0] ??GUI_MULTIBUF_Begin() ?? _LCD_CopyBuffer ????
	   ???? emWin ???????????	   DMA2D_FillBand ??GUI_MULTIBUF_Begin() ???????????????*/
	uint32_t buf_size = (uint32_t)_axSize[0]
	                  * (uint32_t)_aySize[0]
	                  * (uint32_t)_aBytesPerPixels[0];
	return _aAddr[0] + buf_size * (uint32_t)_aBufferIndex[0];
}

/*
 * LTDC 行中断处理器（在 VFP 消隐期第一行触发，由 _EnableLTDCLineIRQ 配置）
 *
 * 职责：
 *   1. 清除 LIF 标志（必须，否则持续重入 ISR）
 *   2. 若 vsync_pending=1，立即执行 LTDC Shadow Register Reload（IMR 模式）
 *      → CFBAR 已由 _CustomCopyBuffer 预写，IMR 使其立即生效
 *      → 此时活跃扫描行尚未开始（处于消隐期），故零撕裂
 *   3. 翻转 active_phys_buffer 标记，供下一帧 Ping-Pong 选择后台缓冲
 *   4. 清除 vsync_pending，xSemaphoreGiveFromISR 解除 LCDConf_WaitVsyncReload 阻塞
 *       （Wait 超时仅在任务里递增 g_lcdconf_vsync_timeout_cnt，不翻 active、不清 pending）。
 *
 * 注意：GUI_MULTIBUF_ConfirmEx 在 _CustomCopyBuffer 解除阻塞后由任务调用，
 *       不在 ISR 中调用，以避免 FreeRTOS API 的 ISR 安全性问题。
*/
void LTDC_IRQHandler(void) 
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t isr = LTDC->ISR;

    if ((isr & LTDC_ISR_FUIF) != 0U) {
        LTDC->ICR = LTDC_ICR_CFUIF;
        g_lcdconf_ltdc_fu_cnt++;
    }

    if ((isr & LTDC_ISR_LIF) == 0U) {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        return;
    }

    /* 清除行中断标志（必须先清标志，防止 ISR 重入） */
    LTDC->ICR = LTDC_ICR_CLIF;
    g_emwin_ltdc_irq_cnt++;

    if (vsync_pending != 0U) {
        /* 立即重载 Shadow Register：CFBAR 新地址正式生效
         * 此时处于 VFP（垂直前肩），所有活跃像素已输出完毕，
         * 后续的 VFP→VSW→VBP→新一帧活跃区全部使用新缓冲，零撕裂。 */
        LTDC->SRCR = LTDC_SRCR_IMR;

        /* 翻转 Ping-Pong 索引（新的"前台"就是刚刚切入的缓冲） */
        active_phys_buffer = (active_phys_buffer == 0U) ? 1U : 0U;

        vsync_pending = 0U;
        if (s_ltdc_vsync_sem != NULL) {
            (void)xSemaphoreGiveFromISR(s_ltdc_vsync_sem, &xHigherPriorityTaskWoken);
        }
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void LTDC_ER_IRQHandler(void)
{
    if ((LTDC->ISR & LTDC_ISR_FUIF) != 0U) {
        LTDC->ICR = LTDC_ICR_CFUIF;
        g_lcdconf_ltdc_fu_cnt++;
    }
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
