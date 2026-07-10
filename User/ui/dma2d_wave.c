/**
 * @file dma2d_wave.c
 * @brief PlotBuf 波形位图：AXI SRAM 竖屏向缓冲 + DMA2D 画线/R2M，与 MainTask 波形区配合。
 *
 * 生命周期（示波器帧内，MainTask 调用）：
 *   RenderScopeLock → StartFrame → DrawLine(×N) → EndFrame → PreDisplaySync → Unlock
 * PreDisplaySync 做 D-Cache Clean/Invalidate，再经 LCDConf RenderLatch 拷到 LTDC 物理缓冲。
 * 坐标约定与 dma2d_wave.h、LCDConf ManualRotate 旋转方向须一致（CW/CCW 分支）。
 */
#include "dma2d_wave.h"
#include "dma2d_wait.h"
#include "bsp_vram_layout.h"
#include "dso/dso_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "GUI.h"
#include "stm32h7xx.h"
#include <stdio.h>
#include "stm32h7xx_hal_rcc.h"
#include "core_cm7.h"
#include <string.h>

/*
*********************************************************************************************************
*   函 数 名: DMA2D_PlotBuf_CheckGuards
*   功能说明: 维护 PlotBuf/DMA2D 波形绘制、缓存同步、保护区检查和渲染锁。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
int DMA2D_PlotBuf_CheckGuards(void);

/* Keil 预定义 DSO_DEBUG_FORCE_CPU_WAVEFORM=1：波形段不用 DMA2D R2M，便于二分 DMA 嫌疑 */
#if defined(DSO_DEBUG_FORCE_CPU_WAVEFORM) && (DSO_DEBUG_FORCE_CPU_WAVEFORM)
#undef USE_DMA2D_WAVEFORM_DRAW
/* 宏定义: USE_DMA2D_WAVEFORM_DRAW - 编译期开关，用于启用或关闭对应功能。 */
#define USE_DMA2D_WAVEFORM_DRAW 0
#endif

#if DSO_APP_INFO_PRINTF_ENABLE
/* 宏定义: APP_INFO_PRINTF - 当前模块使用的配置常量。 */
#define APP_INFO_PRINTF(...) printf(__VA_ARGS__)
#else
/* 宏定义: APP_INFO_PRINTF - 当前模块使用的配置常量。 */
#define APP_INFO_PRINTF(...) do { } while (0)
#endif

#ifndef PLOT_BUF_ALIGN
#define PLOT_BUF_ALIGN  32u  /* CM7 D-Cache 行；PlotBuf 缓冲须对齐，Cache 按行操作才不踩邻块 */
#endif

/* D-Cache：DMA2D 读 RAM 前 Clean；写 RAM 后 Invalidate（地址按 32 字节对齐扩展） */
static void dcache_align_range(const void *p, uint32_t nbytes, uint32_t *out_a, uint32_t *out_sz)
{
    uintptr_t start = (uintptr_t)p;
    uintptr_t end   = start + (uintptr_t)nbytes;

    if ((nbytes == 0u) || (end <= start)) {
        *out_a = 0u;
        *out_sz = 0u;
        return;
    }
    {
        uint32_t a = (uint32_t)(start & ~(uintptr_t)31u);
        uint32_t b = (uint32_t)((end + 31u) & ~(uintptr_t)31u);
        *out_a = a;
        *out_sz = (b > a) ? (b - a) : 0u;
    }
}

/*
*********************************************************************************************************
*   函 数 名: dcache_clean_range
*   功能说明: 维护 PlotBuf/DMA2D 波形绘制、缓存同步、保护区检查和渲染锁。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void dcache_clean_range(const void *ptr, uint32_t nbytes)
{
    uint32_t a, sz;

    dcache_align_range(ptr, nbytes, &a, &sz);
    if (sz > 0u) {
        SCB_CleanDCache_by_Addr((uint32_t *)a, (int32_t)sz);
    }
}

/*
*********************************************************************************************************
*   函 数 名: dcache_invalidate_range
*   功能说明: 维护 PlotBuf/DMA2D 波形绘制、缓存同步、保护区检查和渲染锁。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void dcache_invalidate_range(const void *ptr, uint32_t nbytes)
{
    uint32_t a, sz;

    dcache_align_range(ptr, nbytes, &a, &sz);
    if (sz > 0u) {
        SCB_InvalidateDCache_by_Addr((uint32_t *)a, (int32_t)sz);
    }
}

/* 像素区字节数向上取整到 32B，使 Clean/Inv 的按行扩展不越过本块尾、踩相邻 GUI_ALLOC */
static uint32_t plot_buf_cache_bytes(void)
{
    /* 与 PLOT_BUF_RGB565_BYTES 一致后再向上取整到 32B，与 SCB_Clean/Invalidate 按行一致 */
    uint32_t raw = (uint32_t)PLOT_BUF_W * (uint32_t)PLOT_BUF_H * 2U;
    return (raw + 31U) & ~31U;
}

volatile uint32_t g_dso_plotbuf_guard_fail;

#if DSO_PLOTBUF_GUARD_ENABLE
/* 宏定义: PLOT_ALLOC_SLOP - 当前模块使用的配置常量。 */
#define PLOT_ALLOC_SLOP  64u

struct plot_buf_guard_ {
    const uint8_t *f0;
    const uint8_t *f1;
    const uint8_t *b0;
    const uint8_t *b1;
};

static struct plot_buf_guard_ s_plot_guards[3];
static int s_plot_guards_inited;

/* 在像素块前各留 PLOT_BUF_GUARD_SIZE 字节、涂 0xA5，尾端同理；像素区起点 32B 对齐 */
static int plot_alloc_one_guarded(uint32_t cache_bytes, GUI_HMEM *out_h, uint16_t **out_px, struct plot_buf_guard_ *g)
{
    uint32_t total;
    uint8_t *raw;
    uint8_t *pxb;

    total = (uint32_t)PLOT_BUF_GUARD_SIZE + cache_bytes + (uint32_t)PLOT_BUF_GUARD_SIZE + PLOT_ALLOC_SLOP;
    *out_h = GUI_ALLOC_AllocNoInit(total);
    if (*out_h == 0) {
        return 0;
    }
    raw = (uint8_t *)GUI_ALLOC_h2p(*out_h);
    if (raw == NULL) {
        GUI_ALLOC_Free(*out_h);
        *out_h = 0;
        return 0;
    }
    pxb = (uint8_t *)(((uintptr_t)raw + (uintptr_t)PLOT_BUF_GUARD_SIZE + (uintptr_t)PLOT_BUF_ALIGN - 1U) & ~((uintptr_t)PLOT_BUF_ALIGN - 1U));
    g->f0 = raw;
    g->f1 = pxb;
    g->b0 = pxb + cache_bytes;
    g->b1 = g->b0 + (uint32_t)PLOT_BUF_GUARD_SIZE;
    if (g->b1 > raw + total) {
        GUI_ALLOC_Free(*out_h);
        *out_h = 0;
        return 0;
    }
    (void)memset((void *)g->f0, 0xA5, (size_t)(g->f1 - g->f0));
    (void)memset((void *)g->b0, 0xA5, (size_t)(g->b1 - g->b0));
    *out_px = (uint16_t *)pxb;
    return 1;
}

/*
*********************************************************************************************************
*   函 数 名: plot_guard_range_ok
*   功能说明: 维护 PlotBuf/DMA2D 波形绘制、缓存同步、保护区检查和渲染锁。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static int plot_guard_range_ok(const struct plot_buf_guard_ *g)
{
    const uint8_t *p;

    for (p = g->f0; p < g->f1; p++) {
        if (*p != 0xA5U) {
            return 0;
        }
    }
    for (p = g->b0; p < g->b1; p++) {
        if (*p != 0xA5U) {
            return 0;
        }
    }
    return 1;
}
#endif /* DSO_PLOTBUF_GUARD_ENABLE */

static SemaphoreHandle_t s_plotbuf_mx;

/*
*********************************************************************************************************
*   函 数 名: DMA2D_PlotBuf_RenderScopeLock
*   功能说明: 维护 PlotBuf/DMA2D 波形绘制、缓存同步、保护区检查和渲染锁。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DMA2D_PlotBuf_RenderScopeLock(void)
{
    if (s_plotbuf_mx != NULL) {
        (void)xSemaphoreTake(s_plotbuf_mx, portMAX_DELAY);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DMA2D_PlotBuf_RenderScopeUnlock
*   功能说明: 维护 PlotBuf/DMA2D 波形绘制、缓存同步、保护区检查和渲染锁。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DMA2D_PlotBuf_RenderScopeUnlock(void)
{
    if (s_plotbuf_mx != NULL) {
        (void)xSemaphoreGive(s_plotbuf_mx);
    }
}

/* 宏定义: PHYS_COLS - 当前模块使用的配置常量。 */
#define PHYS_COLS        VRAM_XPHYS
/* 宏定义: BYTES_PER_PIXEL - 当前模块使用的配置常量。 */
#define BYTES_PER_PIXEL  2U
/* 宏定义: DMA2D_OPFCCR_RGB565 - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DMA2D_OPFCCR_RGB565  0x02UL
/* 宏定义: DMA2D_FGPFCCR_RGB565 - 采集、DMA、ADC 或定时器相关配置常量。 */
#define DMA2D_FGPFCCR_RGB565 0x02UL

/* 1=Init 时运行 DMA2D 自检（AXI SRAM R2M），失败则返回 0；0=跳过 */
#define DMA2D_SELFTEST_ON_INIT  0

/* DMA2D 初始化：使能时钟（从 main.c 红色测试代码移植，不显示红块） */
static void DMA2D_EnsureInit(void)
{
    __HAL_RCC_DMA2D_CLK_ENABLE();
}

#if (DMA2D_SELFTEST_ON_INIT == 1)
/* DMA2D 自检：AXI SRAM R2M 验证，不涉及显示 */
static int DMA2D_SelfTest(void)
{
/* 宏定义: SELFTEST_W - 显示尺寸或布局尺寸常量。 */
#define SELFTEST_W    64
/* 宏定义: SELFTEST_H - 头文件保护宏。 */
#define SELFTEST_H    64
/* 宏定义: SELFTEST_PIX - 当前模块使用的配置常量。 */
#define SELFTEST_PIX  (SELFTEST_W * SELFTEST_H)
/* 宏定义: SELFTEST_BYT - 当前模块使用的配置常量。 */
#define SELFTEST_BYT  (SELFTEST_PIX * 2U)
/* 宏定义: SELFTEST_CLR - 当前模块使用的配置常量。 */
#define SELFTEST_CLR  0xF800U

    volatile uint16_t *buf = (volatile uint16_t *)0x2407E000U;
    uint32_t i;

    for (i = 0U; i < SELFTEST_PIX; i++) buf[i] = 0U;

    DMA2D_Wait_TransferComplete(portMAX_DELAY);
    DMA2D->CR     = 0x00030000UL;
    DMA2D->OCOLR  = (uint32_t)SELFTEST_CLR;
    DMA2D->OMAR   = (uint32_t)buf;
    DMA2D->OOR    = 0U;
    DMA2D->OPFCCR = 0x02U;
    DMA2D->NLR    = (SELFTEST_W << 16U) | SELFTEST_H;
    DMA2D->CR    |= DMA2D_CR_START;
    DMA2D_Wait_TransferComplete(portMAX_DELAY);

    SCB_InvalidateDCache_by_Addr((uint32_t *)buf, SELFTEST_BYT);
    for (i = 0U; i < SELFTEST_PIX; i++) {
        if (buf[i] != SELFTEST_CLR) return 0;
    }
    return 1;
}
#endif

/* 位图缓冲：双缓冲流水线，DMA2D 恢复与 ManualRotate 并行 */
static uint16_t *s_plot_bufs[2];   /* [0],[1] 交替使用 */
static uint16_t *s_plot_buf;       /* 当前帧绘制目标，指向 s_plot_bufs[s_buf_index] */
static uint16_t s_bg_rgb565;
static int s_plot_buf_ready;
static int s_buf_index;            /* 当前绘制缓冲索引 0/1 */
static int s_async_pending;         /* 上一帧是否已启动异步恢复 */

static uint16_t rgb888_to_rgb565(uint32_t rgb888)
{
    uint32_t r = (rgb888 >> 16U) & 0xFFU;
    uint32_t g = (rgb888 >>  8U) & 0xFFU;
    uint32_t b =  rgb888         & 0xFFU;
    return (uint16_t)(((r >> 3U) << 11U) | ((g >> 2U) << 5U) | (b >> 3U));
}

/* DMA2D R2M 填色单行（1 行 × width 像素），调用前须确保 DMA2D 空闲 */
static void dma2d_fill_hline(uint16_t *dst, int width, uint16_t color)
{
    uint32_t row_bytes;

    if (width <= 0) return;
    row_bytes = (uint32_t)width * 2U;

    if (width == 1) {
        DMA2D_Wait_TransferComplete(portMAX_DELAY);
        dcache_clean_range(dst, row_bytes);
        *dst = color;
        dcache_clean_range(dst, row_bytes);
        dcache_invalidate_range(dst, row_bytes);
        return;
    }
    DMA2D_Bus_RecursiveLock();
    DMA2D_Wait_TransferComplete(portMAX_DELAY);
    /* R2M 无 RAM 源：刷目标行，避免 cache 写回覆盖 DMA 写入的 SDRAM */
    dcache_clean_range(dst, row_bytes);
    DMA2D->CR      = 0x00030000UL;  /* R2M */
    DMA2D->OCOLR   = (uint32_t)color;
    DMA2D->OMAR    = (uint32_t)dst;
    DMA2D->OOR     = 0U;
    DMA2D->OPFCCR  = DMA2D_OPFCCR_RGB565;
    DMA2D->NLR     = ((uint32_t)width << 16U) | 1U;
    DMA2D->CR     |= DMA2D_CR_START;
    DMA2D_Wait_TransferComplete(portMAX_DELAY);
    dcache_invalidate_range(dst, row_bytes);
    DMA2D_Bus_RecursiveUnlock();
}

/* DMA2D R2M 填色竖条：每行 1 像素，共 height 行，列间距 PLOT_BUF_W（OOR = 宽 - 1） */
static void dma2d_fill_vline(uint16_t *dst_top, int height, uint16_t color)
{
    uintptr_t lo;
    uintptr_t hi;
    uint32_t span_bytes;

    if (height <= 0) return;

    if (height == 1) {
        DMA2D_Wait_TransferComplete(portMAX_DELAY);
        dcache_clean_range(dst_top, 2u);
        *dst_top = color;
        dcache_clean_range(dst_top, 2u);
        dcache_invalidate_range(dst_top, 2u);
        return;
    }
    lo = (uintptr_t)dst_top;
    hi = (uintptr_t)(dst_top + (height - 1) * PLOT_BUF_W) + sizeof(uint16_t);
    span_bytes = (uint32_t)(hi - lo);

    DMA2D_Bus_RecursiveLock();
    DMA2D_Wait_TransferComplete(portMAX_DELAY);
    dcache_clean_range((const void *)lo, span_bytes);
    DMA2D->CR      = 0x00030000UL;  /* R2M */
    DMA2D->OCOLR   = (uint32_t)color;
    DMA2D->OMAR    = (uint32_t)dst_top;
    DMA2D->OOR     = (uint32_t)(PLOT_BUF_W - 1);
    DMA2D->OPFCCR  = DMA2D_OPFCCR_RGB565;
    DMA2D->NLR     = (1U << 16U) | (uint32_t)height;
    DMA2D->CR     |= DMA2D_CR_START;
    DMA2D_Wait_TransferComplete(portMAX_DELAY);
    dcache_invalidate_range((const void *)lo, span_bytes);
    DMA2D_Bus_RecursiveUnlock();
}

/* 水平段：裁剪后写 PlotBuf；USE_DMA2D_WAVEFORM_DRAW 时用 R2M */
static void flush_hline_run(int y, int x0, int x1, uint16_t color)
{
    if (y < 0 || y >= PLOT_BUF_H) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (x0 < 0) x0 = 0;
    if (x1 >= PLOT_BUF_W) x1 = PLOT_BUF_W - 1;
    if (x0 > x1) return;
    int len = x1 - x0 + 1;
    uint16_t *p = s_plot_buf + y * PLOT_BUF_W + x0;
#if USE_DMA2D_WAVEFORM_DRAW
    dma2d_fill_hline(p, len, color);
#else
    {
        uint32_t pair = (uint32_t)color | ((uint32_t)color << 16U);
        int i = 0;
        if ((x0 & 1) != 0 && len > 0) {
            *p++ = color;
            i = 1;
        }
        uint32_t *p32 = (uint32_t *)p;
        for (; i + 2 <= len; i += 2) *p32++ = pair;
        p = (uint16_t *)p32;
        for (; i < len; i++) *p++ = color;
    }
#endif
}

/*
*********************************************************************************************************
*   函 数 名: flush_vline_run
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void flush_vline_run(int x, int y0, int y1, uint16_t color)
{
    if (x < 0 || x >= PLOT_BUF_W) return;
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if (y0 < 0) y0 = 0;
    if (y1 >= PLOT_BUF_H) y1 = PLOT_BUF_H - 1;
    if (y0 > y1) return;
    int h = y1 - y0 + 1;
    uint16_t *p = s_plot_buf + y0 * PLOT_BUF_W + x;
#if USE_DMA2D_WAVEFORM_DRAW
    dma2d_fill_vline(p, h, color);
#else
    for (int yy = y0; yy <= y1; yy++)
        s_plot_buf[yy * PLOT_BUF_W + x] = color;
#endif
}

/* Bresenham 画线到位图，坐标相对于 plot (0,0)
 * USE_DMA2D_WAVEFORM_DRAW：斜线拆成水平/竖直段，DMA2D R2M；否则逐像素 CPU */
static void draw_line(int x1, int y1, int x2, int y2, uint16_t color)
{
    int dx = x2 - x1, dy = y2 - y1;

    /* 单点 */
    if (dx == 0 && dy == 0) {
        if ((unsigned)x1 < (unsigned)PLOT_BUF_W && (unsigned)y1 < (unsigned)PLOT_BUF_H) {
#if USE_DMA2D_WAVEFORM_DRAW
            dma2d_fill_hline(s_plot_buf + y1 * PLOT_BUF_W + x1, 1, color);
#else
            s_plot_buf[y1 * PLOT_BUF_W + x1] = color;
#endif
        }
        return;
    }

    /* 水平线 */
    if (dy == 0) {
        if (y1 >= 0 && y1 < PLOT_BUF_H) {
            int xa = (x1 < x2) ? x1 : x2;
            int xb = (x1 < x2) ? x2 : x1;
            flush_hline_run(y1, xa, xb, color);
        }
        return;
    }

    /* 竖直线 */
    if (dx == 0) {
        if (x1 >= 0 && x1 < PLOT_BUF_W) {
            int ya = (y1 < y2) ? y1 : y2;
            int yb = (y1 < y2) ? y2 : y1;
            flush_vline_run(x1, ya, yb, color);
        }
        return;
    }

    /* 线段级粗裁剪 */
    if (x1 < 0 && x2 < 0) return;
    if (x1 >= PLOT_BUF_W && x2 >= PLOT_BUF_W) return;
    if (y1 < 0 && y2 < 0) return;
    if (y1 >= PLOT_BUF_H && y2 >= PLOT_BUF_H) return;

#if !USE_DMA2D_WAVEFORM_DRAW
    {
        int ux = (dx > 0) ? 1 : -1, uy = (dy > 0) ? 1 : -1;
        dx = (dx < 0) ? -dx : dx;
        dy = (dy < 0) ? -dy : dy;
        int x = x1, y = y1;

        if (dx >= dy) {
            int d = 2 * dy - dx;
            for (int i = 0; i <= dx; i++) {
                if ((unsigned)x < (unsigned)PLOT_BUF_W && (unsigned)y < (unsigned)PLOT_BUF_H)
                    s_plot_buf[y * PLOT_BUF_W + x] = color;
                if (d > 0) { y += uy; d -= 2 * dx; }
                d += 2 * dy;
                x += ux;
            }
        } else {
            int d = 2 * dx - dy;
            for (int i = 0; i <= dy; i++) {
                if ((unsigned)x < (unsigned)PLOT_BUF_W && (unsigned)y < (unsigned)PLOT_BUF_H)
                    s_plot_buf[y * PLOT_BUF_W + x] = color;
                if (d > 0) { x += ux; d -= 2 * dy; }
                d += 2 * dx;
                y += uy;
            }
        }
    }
    return;
#else
    {
        int ux = (x2 > x1) ? 1 : -1;
        int uy = (y2 > y1) ? 1 : -1;
        int adx = (dx < 0) ? -dx : dx;
        int ady = (dy < 0) ? -dy : dy;
        int x = x1, y = y1;

        if (adx >= ady) {
            int d = 2 * ady - adx;
            int run_y = y;
            int run_x0 = x;
            for (int i = 0; i <= adx; i++) {
                if (i == adx) {
                    flush_hline_run(run_y, run_x0, x, color);
                    break;
                }
                int xn = x + ux;
                int yn = y;
                int dn = d;
                if (dn > 0) { yn += uy; dn -= 2 * adx; }
                dn += 2 * ady;
                if (yn != run_y) {
                    flush_hline_run(run_y, run_x0, x, color);
                    run_y = yn;
                    run_x0 = xn;
                }
                x = xn;
                y = yn;
                d = dn;
            }
        } else {
            int d = 2 * adx - ady;
            int run_x = x;
            int run_y0 = y;
            for (int i = 0; i <= ady; i++) {
                if (i == ady) {
                    flush_vline_run(run_x, run_y0, y, color);
                    break;
                }
                int yn = y + uy;
                int xn = x;
                int dn = d;
                if (dn > 0) { xn += ux; dn -= 2 * ady; }
                dn += 2 * adx;
                if (xn != run_x) {
                    flush_vline_run(run_x, run_y0, y, color);
                    run_x = xn;
                    run_y0 = yn;
                }
                x = xn;
                y = yn;
                d = dn;
            }
        }
    }
#endif
}

/* 静态背景缓冲（bg+grid），初始化后不变，竖屏 PlotBuf 与 PLOT_BUF_W×PLOT_BUF_H 一致（见 dma2d_wave.h） */
static uint16_t *s_bg_buf;

/* 画到 s_bg_buf 的辅助函数，竖屏布局：phys_x=0..404, phys_y=0..771 */
static void draw_hline_static(int phys_y, int phys_x1, int phys_x2, uint16_t color)
{
    if (phys_y < 0 || phys_y >= PLOT_BUF_H) return;
    if (phys_x1 > phys_x2) { int t = phys_x1; phys_x1 = phys_x2; phys_x2 = t; }
    if (phys_x1 < 0) phys_x1 = 0;
    if (phys_x2 >= PLOT_BUF_W) phys_x2 = PLOT_BUF_W - 1;
    uint16_t *p = s_bg_buf + phys_y * PLOT_BUF_W + phys_x1;
    for (int i = phys_x1; i <= phys_x2; i++) *p++ = color;
}
/*
*********************************************************************************************************
*   函 数 名: draw_vline_static
*   功能说明: 完成当前 UI 模块的绘制、格式化或局部状态辅助处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void draw_vline_static(int phys_x, int phys_y1, int phys_y2, uint16_t color)
{
    if (phys_x < 0 || phys_x >= PLOT_BUF_W) return;
    if (phys_y1 > phys_y2) { int t = phys_y1; phys_y1 = phys_y2; phys_y2 = t; }
    if (phys_y1 < 0) phys_y1 = 0;
    if (phys_y2 >= PLOT_BUF_H) phys_y2 = PLOT_BUF_H - 1;
    uint16_t *p = s_bg_buf + phys_y1 * PLOT_BUF_W + phys_x;
    for (int i = phys_y1; i <= phys_y2; i++) { *p = color; p += PLOT_BUF_W; }
}

/* 5x7 点阵：C,H,1,2 用于 CH1/CH2 标签（LSB=上） */
static const uint8_t s_font_5x7_ch12[4][5] = {
    { 0x0E, 0x11, 0x10, 0x11, 0x0E },  /* C */
    { 0x11, 0x11, 0x1F, 0x11, 0x11 },  /* H */
    { 0x04, 0x0C, 0x04, 0x04, 0x0E },  /* 1 */
    { 0x0E, 0x11, 0x01, 0x02, 0x1F },  /* 2 */
};
/*
*********************************************************************************************************
*   函 数 名: draw_char_5x7_to_bg
*   功能说明: 完成当前 UI 模块的绘制、格式化或局部状态辅助处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void draw_char_5x7_to_bg(int x, int y, int ch_idx, uint16_t color)
{
    if (ch_idx < 0 || ch_idx > 3) return;
    const uint8_t *col = s_font_5x7_ch12[ch_idx];
    for (int c = 0; c < 5; c++) {
        uint8_t bits = col[c];
        for (int r = 0; r < 7; r++) {
            if (bits & (1U << r)) {
                int py = y + r, px = x + c;
                if (px >= 0 && px < PLOT_BUF_W && py >= 0 && py < PLOT_BUF_H)
                    s_bg_buf[py * PLOT_BUF_W + px] = color;
            }
        }
    }
}

/* emWin 无 GUI_ALLOC_AllocAlign：多申请 (align-1) 字节后把指针对齐到 PLOT_BUF_ALIGN */
static GUI_HMEM plot_alloc_aligned(uint32_t size_bytes, uint16_t **out_aligned)
{
    GUI_HMEM h;
    uint8_t *raw;
    uintptr_t adj;
    uint32_t gross;

    if (out_aligned == NULL) {
        return 0;
    }
    *out_aligned = NULL;
    gross = size_bytes + (uint32_t)PLOT_BUF_ALIGN - 1u;
    h     = GUI_ALLOC_AllocNoInit(gross);
    if (h == 0) {
        return 0;
    }
    raw = (uint8_t *)GUI_ALLOC_h2p(h);
    if (raw == NULL) {
        GUI_ALLOC_Free(h);
        return 0;
    }
    adj = ((uintptr_t)raw + (uintptr_t)PLOT_BUF_ALIGN - 1u) & ~((uintptr_t)PLOT_BUF_ALIGN - 1u);
    if ((adj + (uintptr_t)size_bytes) > (uintptr_t)raw + (uintptr_t)gross) {
        GUI_ALLOC_Free(h);
        return 0;
    }
    *out_aligned = (uint16_t *)adj;
    return h;
}

/*
*********************************************************************************************************
*   函 数 名: DMA2D_PlotBuf_GuiAllocHalt
*   功能说明: 维护 PlotBuf/DMA2D 波形绘制、缓存同步、保护区检查和渲染锁。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void DMA2D_PlotBuf_GuiAllocHalt(uint32_t block_bytes, const char *stage, GUI_HMEM h1, GUI_HMEM h2, GUI_HMEM h3)
{
    if (h1 != 0) {
        GUI_ALLOC_Free(h1);
    }
    if (h2 != 0) {
        GUI_ALLOC_Free(h2);
    }
    if (h3 != 0) {
        GUI_ALLOC_Free(h3);
    }
    s_plot_bufs[0] = NULL;
    s_plot_bufs[1] = NULL;
    s_bg_buf       = NULL;
    s_plot_buf     = NULL;
    s_plot_buf_ready = 0;
    APP_INFO_PRINTF("[PlotBuf] GUI_ALLOC failed @ %s, block=%lu x3 bytes\r\n",
                    stage, (unsigned long)block_bytes);
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DMA2D_PlotBuf_Init
*   功能说明: 维护 PlotBuf/DMA2D 波形绘制、缓存同步、保护区检查和渲染锁。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
int DMA2D_PlotBuf_Init(void)
{
#if !USE_DMA2D_PLOTBUF
    return 0;  /* 禁用时直接返回，走 GUI 绘制路径 */
#endif
    uint32_t size_gross = plot_buf_cache_bytes(); /* 含尾端补 32B，供 D-Cache 行操作不越界 */
    s_plot_buf_ready = 0;
    s_async_pending  = 0;
    s_buf_index      = 0;

    DMA2D_EnsureInit();
#if (DMA2D_SELFTEST_ON_INIT == 1)
    if (!DMA2D_SelfTest()) return 0;
#endif

#if DSO_PLOTBUF_GUARD_ENABLE
    s_plot_guards_inited = 0;
    {
        int i;
        int j;
        GUI_HMEM hh[3] = { 0, 0, 0 };
        uint16_t *pp[3];

        for (i = 0; i < 3; i++) {
            if (!plot_alloc_one_guarded(size_gross, &hh[i], &pp[i], &s_plot_guards[i])) {
                for (j = 0; j < i; j++) {
                    if (hh[j] != 0) {
                        GUI_ALLOC_Free(hh[j]);
                    }
                }
                DMA2D_PlotBuf_GuiAllocHalt(size_gross, "guard", 0, 0, 0);
            }
        }
        s_plot_bufs[0] = pp[0];
        s_plot_bufs[1] = pp[1];
        s_bg_buf       = pp[2];
        s_plot_guards_inited = 1;
    }
#else
    {
        GUI_HMEM h1;
        GUI_HMEM h2;
        GUI_HMEM h3;

        h1 = plot_alloc_aligned(size_gross, &s_plot_bufs[0]);
        if (h1 == 0) {
            DMA2D_PlotBuf_GuiAllocHalt(size_gross, "block1", 0, 0, 0);
        }
        h2 = plot_alloc_aligned(size_gross, &s_plot_bufs[1]);
        if (h2 == 0) {
            DMA2D_PlotBuf_GuiAllocHalt(size_gross, "block2", h1, 0, 0);
        }
        h3 = plot_alloc_aligned(size_gross, &s_bg_buf);
        if (h3 == 0) {
            DMA2D_PlotBuf_GuiAllocHalt(size_gross, "block3", h1, h2, 0);
        }
        if (s_plot_bufs[0] == NULL || s_plot_bufs[1] == NULL || s_bg_buf == NULL) {
            DMA2D_PlotBuf_GuiAllocHalt(size_gross, "h2p", h1, h2, h3);
        }
    }
#endif /* DSO_PLOTBUF_GUARD_ENABLE */
    s_plot_buf = s_plot_bufs[0];
    s_plot_buf_ready = 1;

    s_bg_rgb565 = rgb888_to_rgb565(CLR_BG_PLOT);

    /* 填充背景 */
    uint32_t n = (uint32_t)PLOT_BUF_W * PLOT_BUF_H;
    for (uint32_t i = 0; i < n; i++) s_bg_buf[i] = s_bg_rgb565;

    /* 网格：与 LCD_PLOTBUF_CCW_MIRROR_TIME_Y 一致（默认 1=镜像时间轴） */
    const int w = PLOT_BUF_W, h = PLOT_BUF_H;
#if LCD_USE_CCW_ROTATION && LCD_PLOTBUF_CCW_MIRROR_TIME_Y
/* 宏定义: PLOT_MY_CCW - 当前模块使用的配置常量。 */
#define PLOT_MY_CCW(py)  ((h - 1) - (py))
#else
/* 宏定义: PLOT_MY_CCW - 当前模块使用的配置常量。 */
#define PLOT_MY_CCW(py)  (py)
#endif
    /* 中心轴与 MainTask DrawGridLines 主格线整数除法对齐（勿用 w/2、h/2） */
    const int grid_col = 10, grid_row = 8;
    const int cx = (grid_row / 2 * (w - 1)) / grid_row;
    const int cy_maj = (grid_col / 2 * (h - 1)) / grid_col;
    uint16_t c_minor = rgb888_to_rgb565(0x0D1828u);  /* CLR_GRID_MINOR */
    uint16_t c_major = rgb888_to_rgb565(0x1A2E46u);  /* CLR_GRID_MAJOR */
    uint16_t c_axis  = rgb888_to_rgb565(0x2C4A78u);  /* CLR_AXIS */

    for (int i = 0; i <= grid_col * 2; i++) {
        int phys_y = PLOT_MY_CCW((i * (h - 1)) / (grid_col * 2));
        draw_hline_static(phys_y, 0, w - 1, c_minor);
    }
    for (int i = 0; i <= grid_row * 2; i++) {
        int phys_x = (i * (w - 1)) / (grid_row * 2);
        draw_vline_static(phys_x, 0, h - 1, c_minor);
    }
    for (int i = 0; i <= grid_col; i++) {
        int phys_y = PLOT_MY_CCW((i * (h - 1)) / grid_col);
        draw_hline_static(phys_y, 0, w - 1, c_major);
    }
    for (int i = 0; i <= grid_row; i++) {
        int phys_x = (i * (w - 1)) / grid_row;
        draw_vline_static(phys_x, 0, h - 1, c_major);
    }
    draw_vline_static(cx, 0, h - 1, c_axis);
    draw_hline_static(PLOT_MY_CCW(cy_maj), 0, w - 1, c_axis);

#undef PLOT_MY_CCW

/* 开关：User/ui/dma2d_wave.h → LCD_PLOTBUF_CCW_SEAM_FIX */
#if LCD_USE_CCW_ROTATION && LCD_PLOTBUF_CCW_SEAM_FIX
    /* 与 MainTask CLR_BORDER_IN 一致，减轻 PlotBuf 与旋转边距接缝处「边界线错位」 */
    {
        uint16_t c_bord = rgb888_to_rgb565(0x203858u);
        draw_hline_static(0,     0, w - 1, c_bord);
        draw_hline_static(h - 1, 0, w - 1, c_bord);
        draw_vline_static(0,     0, h - 1, c_bord);
        draw_vline_static(w - 1, 0, h - 1, c_bord);
    }
#endif

    /* CH1/CH2：与 MainTask PLOT_CY_CH1/CH2 一致；CCW 时交换 lx 使标签与波形条对齐 */
    uint16_t c_ch1 = rgb888_to_rgb565(0xFF3355u);  /* CLR_CH1 */
    uint16_t c_ch2 = rgb888_to_rgb565(0x22EE88u);  /* CLR_CH2 */
#if LCD_USE_CCW_ROTATION
    int lx_ch1 = (103 * (w - 1)) / 394;
    int lx_ch2 = (301 * (w - 1)) / 394;
    {
        const int y_um = ((h - 1) * 746) / 771;
        int ly = (LCD_PLOTBUF_CCW_MIRROR_TIME_Y) ? ((h - 1) - y_um) : y_um;

        draw_char_5x7_to_bg(lx_ch1,      ly, 0, c_ch1); draw_char_5x7_to_bg(lx_ch1 + 6,  ly, 1, c_ch1); draw_char_5x7_to_bg(lx_ch1 + 12, ly, 2, c_ch1);
        draw_char_5x7_to_bg(lx_ch2,      ly, 0, c_ch2); draw_char_5x7_to_bg(lx_ch2 + 6,  ly, 1, c_ch2); draw_char_5x7_to_bg(lx_ch2 + 12, ly, 3, c_ch2);
    }
#else
    int lx_ch1 = (301 * (w - 1)) / 394;
    int lx_ch2 = (103 * (w - 1)) / 394;
    {
        int ly = ((h - 1) * 746) / 771;

        draw_char_5x7_to_bg(lx_ch1,      ly, 0, c_ch1); draw_char_5x7_to_bg(lx_ch1 + 6,  ly, 1, c_ch1); draw_char_5x7_to_bg(lx_ch1 + 12, ly, 2, c_ch1);
        draw_char_5x7_to_bg(lx_ch2,      ly, 0, c_ch2); draw_char_5x7_to_bg(lx_ch2 + 6,  ly, 1, c_ch2); draw_char_5x7_to_bg(lx_ch2 + 12, ly, 3, c_ch2);
    }
#endif

    /* 尾像素区外补到 32B 边界，避免未初始化 RAM 被误读 */
    {
        uint32_t n_pix = (uint32_t)PLOT_BUF_W * PLOT_BUF_H;
        uint32_t n_tot = size_gross / 2U;
        uint32_t i;
        for (i = n_pix; i < n_tot; i++) {
            s_bg_buf[i] = s_bg_rgb565;
        }
    }

    /* CPU 画完静态背景，刷回 SDRAM，供后续 DMA2D M2M 读 */
    dcache_clean_range(s_bg_buf, size_gross);

    if (s_plotbuf_mx == NULL) {
        s_plotbuf_mx = xSemaphoreCreateMutex();
    }
    return 1;
}

/*
*********************************************************************************************************
*   函 数 名: DMA2D_PlotBuf_IsReady
*   功能说明: 维护 PlotBuf/DMA2D 波形绘制、缓存同步、保护区检查和渲染锁。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
int DMA2D_PlotBuf_IsReady(void)
{
#if !USE_DMA2D_PLOTBUF
    return 0;
#endif
    return s_plot_buf_ready;
}

/*
*********************************************************************************************************
*   函 数 名: DMA2D_PlotBuf_CheckGuards
*   功能说明: 维护 PlotBuf/DMA2D 波形绘制、缓存同步、保护区检查和渲染锁。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
int DMA2D_PlotBuf_CheckGuards(void)
{
#if !DSO_PLOTBUF_GUARD_ENABLE
    return 1;
#else
    int i;

    if (!s_plot_guards_inited) {
        return 1;
    }
    for (i = 0; i < 3; i++) {
        if (!plot_guard_range_ok(&s_plot_guards[i])) {
            return 0;
        }
    }
    return 1;
#endif
}

/*
*********************************************************************************************************
*   函 数 名: DMA2D_PlotBuf_GetBuffer
*   功能说明: 维护 PlotBuf/DMA2D 波形绘制、缓存同步、保护区检查和渲染锁。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const void * DMA2D_PlotBuf_GetBuffer(void)
{
    return (const void *)s_plot_buf;
}

/* 内部：DMA2D M2M 恢复 bg → dst，可选阻塞等待 */
static void do_restore_bg(uint16_t *dst, int wait)
{
    uint32_t cache_bytes;

    if (!dst || !s_bg_buf) return;
    cache_bytes = plot_buf_cache_bytes();
    /* M2M：DMA 读源前 Clean 源；写目标前 Clean 目标，避免脏线写回覆盖 DMA */
    dcache_clean_range(s_bg_buf, cache_bytes);
    dcache_clean_range(dst, cache_bytes);

    DMA2D_Bus_RecursiveLock();
    DMA2D_Wait_TransferComplete(portMAX_DELAY);
    DMA2D->CR      = 0x00000000UL | (1U << 9U);
    DMA2D->FGMAR   = (uint32_t)s_bg_buf;
    DMA2D->OMAR    = (uint32_t)dst;
    DMA2D->FGOR    = 0U;
    DMA2D->OOR     = 0U;
    DMA2D->FGPFCCR = DMA2D_FGPFCCR_RGB565;
    DMA2D->OPFCCR  = DMA2D_OPFCCR_RGB565;
    DMA2D->NLR     = ((uint32_t)PLOT_BUF_W << 16U) | (uint32_t)PLOT_BUF_H;
    DMA2D->CR     |= DMA2D_CR_START;
    if (wait) {
        DMA2D_Wait_TransferComplete(portMAX_DELAY);
        dcache_invalidate_range(dst, cache_bytes);
    }
    DMA2D_Bus_RecursiveUnlock();
}

/* 每帧开始：恢复当前缓冲背景，再画波形 */
void DMA2D_PlotBuf_StartFrame(void)
{
    if (!s_plot_buf_ready) return;

#if DSO_PLOTBUF_GUARD_ENABLE
    if (s_plot_guards_inited) {
        if (DMA2D_PlotBuf_CheckGuards() == 0) {
            g_dso_plotbuf_guard_fail = 1u;
            for (;;) {
            }
        }
    }
#endif

    if (s_async_pending) {
        uint32_t cache_bytes = plot_buf_cache_bytes();
        uint16_t *dst        = s_plot_bufs[s_buf_index];

        DMA2D_Wait_TransferComplete(portMAX_DELAY);
        dcache_invalidate_range(dst, cache_bytes);
        s_async_pending = 0;
    } else {
        /* 首帧：阻塞恢复当前缓冲 */
        do_restore_bg(s_plot_bufs[s_buf_index], 1);
    }
    s_plot_buf = s_plot_bufs[s_buf_index];

#if (DMA2D_PLOT_BUF_DEBUG == 1)
    /* 诊断1：整块填亮绿，验证 DMA2D 拷贝是否生效 */
    {
        uint32_t n = (uint32_t)PLOT_BUF_W * PLOT_BUF_H;
        uint16_t green = 0x07E0u;  /* RGB565 绿 */
        for (uint32_t i = 0; i < n; i++) s_plot_buf[i] = green;
    }
#elif (DMA2D_PLOT_BUF_DEBUG == 2)
    /* 诊断2：画一条水平红线到 s_plot_buf，验证画线逻辑 */
    {
        int y = PLOT_BUF_H / 2;
        uint16_t *p = s_plot_buf + y * PLOT_BUF_W;
        for (int i = 0; i < PLOT_BUF_W; i++) p[i] = 0xF800u;  /* RGB565 红 */
    }
#endif
}

/* 每帧结束：切换下一缓冲。默认不并发恢复，避免与 ManualRotate/LTDC 抢 SDRAM。 */
void DMA2D_PlotBuf_EndFrame(void)
{
    if (!s_plot_buf_ready || !s_plot_bufs[0] || !s_plot_bufs[1]) return;

    {
        int next = 1 - s_buf_index;

#if DSO_PLOTBUF_ASYNC_BG_RESTORE
        do_restore_bg(s_plot_bufs[next], 0);  /* 不等待，与 ManualRotate 并行 */
#endif
        s_buf_index       = next;
#if DSO_PLOTBUF_ASYNC_BG_RESTORE
        s_async_pending   = 1;
#else
        s_async_pending   = 0;
#endif
    }
}

/* ManualRotate 拷屏前：与异步 DMA2D 串行，仅同步当前 Plot 缓冲的 D-Cache（勿全片 Clean/Inv） */
void DMA2D_PlotBuf_PreDisplaySync(void)
{
#if !USE_DMA2D_PLOTBUF
    return;
#else
    uint32_t cache_bytes;

    if (!s_plot_buf_ready || !s_plot_buf) {
        return;
    }
    cache_bytes = plot_buf_cache_bytes();
    DMA2D_Wait_TransferComplete(portMAX_DELAY);
    dcache_clean_range(s_plot_buf, cache_bytes);
    dcache_invalidate_range(s_plot_buf, cache_bytes);
#endif
}

/*
*********************************************************************************************************
*   函 数 名: DMA2D_PlotBuf_ResyncOnScopeEnter
*   功能说明: 维护 PlotBuf/DMA2D 波形绘制、缓存同步、保护区检查和渲染锁。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DMA2D_PlotBuf_ResyncOnScopeEnter(void)
{
#if !USE_DMA2D_PLOTBUF
    return;
#endif
    if (!s_plot_buf_ready || !s_plot_bufs[0] || !s_plot_bufs[1] || !s_bg_buf) {
        return;
    }
    DMA2D_Wait_TransferComplete(portMAX_DELAY);
    s_async_pending = 0;
    do_restore_bg(s_plot_bufs[0], 1);
    do_restore_bg(s_plot_bufs[1], 1);
    s_buf_index = 0;
    s_plot_buf  = s_plot_bufs[0];
}

/*
*********************************************************************************************************
*   函 数 名: DMA2D_PlotBuf_DrawLine
*   功能说明: 维护 PlotBuf/DMA2D 波形绘制、缓存同步、保护区检查和渲染锁。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DMA2D_PlotBuf_DrawLine(int x1, int y1, int x2, int y2, uint32_t color_rgb888)
{
    if (!s_plot_buf) return;
    draw_line(x1, y1, x2, y2, rgb888_to_rgb565(color_rgb888));
}

/*
*********************************************************************************************************
*   函 数 名: DMA2D_PlotBuf_Flush
*   功能说明: 维护 PlotBuf/DMA2D 波形绘制、缓存同步、保护区检查和渲染锁。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DMA2D_PlotBuf_Flush(void)
{
    /* 已改用 GUI_DrawBitmapExp 绘制，此函数保留供兼容 */
}
