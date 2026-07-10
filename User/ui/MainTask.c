/**
 * @file MainTask.c
 * @brief DSO 示波器 UI 主循环：emWin 绘制、PlotBuf/DMA2D 波形、逻辑→物理旋转提交。
 *
 * ── 在整体架构中的位置 ─────────────────────────────────────────────
 *
 *   ADC/TIM/DMA ──► DSO_Engine_Task (prio 1, ~1 ms 服务环)
 *                        │ 采集/触发/时基/垂直映射 → DSO_Frame
 *                        ▼
 *                  DSO_UI_SendFrame → 2-slot 帧环 (UI_WaveformCtrl.c)
 *                        │
 *   旋钮/按键 ──► UI_EcPollTask (prio 3, 8 ms) ──► UI_Nav / 参数
 *                        │
 *                        ▼
 *                  MainTask (prio 2, 本文件)
 *                        │ UpdateWaveformData → 拉取最新 DSO_UI_Frame
 *                        │ 头栏/网格/波形/游标/侧栏 → 逻辑 framebuffer
 *                        │ PlotBuf+DMA2D 或纯 emWin 绘制波形区
 *                        ▼
 *                  LCDConf_RenderLatch → ManualRotate → LTDC 物理竖屏 ping/pong
 *
 * ── 任务调度策略 ───────────────────────────────────────────────────
 *
 * 1. RUN 模式：vTaskDelayUntil 固定帧周期（默认 25 ms ≈ 40 fps，见 ui_app_config.h）。
 *    FAST 时基且 sec/div ≤ 5 ms 时可走 MAINTASK_FAST_FRAME_PERIOD_MS。
 * 2. STOP 模式：仍每帧渲染（冻结最后一帧），但 vTaskDelay(50 ms) 降 CPU。
 * 3. UI_APP_DEMO_CYCLE_SCREENS=1：多页面 Demo；仅 UI_SCR_SCOPE / UI_SCR_MEASURE
 *    开启波形流（UI_Waveform_SetStreamActive），其它页 idle 8 ms 轮询。
 * 4. UI_EcPollTask 由 MainTask 启动一次，与渲染环并行，避免阻塞 emWin。
 *
 * ── 单帧渲染流水线 (MainTask_Scope_RenderFrame) ───────────────────
 *
 *   UpdateWaveformData → RollHistory 预填 → GUI_MULTIBUF_Begin
 *   → 头栏状态条 → PlotBuf/DMA2D 或 emWin 波形 → 叠加层（触发线/游标/GND）
 *   → GUI_MULTIBUF_End → RenderLatch Prepare/Commit → LTDC 可见
 *
 * 显存布局见 bsp_vram_layout.h；旋转与 VSYNC 见 emWin/Config/LCDConf_Lin_Template.c。
 */

#include "bsp.h"
#include "y_t_stress_ab.h"
#include "FreeRTOS.h"
#include "task.h"
#include "GUI.h"
#include "WM.h"
#include "bsp_lcd_rgb.h"
#include "dma2d_wave.h"
#include "UI_WaveformCtrl.h"
#include "roll_history_ui.h"
#include "lcd_rotate_request.h"
#include "ui_display_light_path.h"
#include "MainTask_profile.h"
#include "ui_app_config.h"
#include "dso/dso_config.h"
#if (Y_T_AB_DISABLE_MAINTASK_PERIODIC_LOG == 0) && \
    (DSO_VPP_DIAG_ENABLE == 0) && \
    (MAINTASK_STACK_LOG_INTERVAL_MS > 0U) && \
    (INCLUDE_uxTaskGetStackHighWaterMark == 1)
#include "debug_uart_lock.h"
#endif
#include "ui_screens.h"
#include "ui_nav.h"
#include "ui_channel_ctrl.h"
#include "ui_ec_poll_task.h"
#include "ui_trigger_ui.h"
#include "ui_freq_format.h"
#include "dso/dso_ui_bridge.h"
#include "dso/dso_timebase.h"
#include "dso_integration.h"
#include "dso/dso_engine.h"
#include "vram_sentinels.h"
#include "dso/dso_10bit_lane_cal.h"
#include "dso/dso_sample_spec.h"
#include "dso/dso_vertical.h"
#include "bsp_vram_layout.h"
#include "core_cm7.h"
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* 宏定义: GRID_COL_DIV - 当前模块使用的配置常量。 */
#define GRID_COL_DIV         10
/* 宏定义: GRID_ROW_DIV - 当前模块使用的配置常量。 */
#define GRID_ROW_DIV         8

/* Logical display size */
#define DISP_W  800
/* 宏定义: DISP_H - 头文件保护宏。 */
#define DISP_H  480

#if DSO_APP_INFO_PRINTF_ENABLE
/* 宏定义: APP_INFO_PRINTF - 当前模块使用的配置常量。 */
#define APP_INFO_PRINTF(...) printf(__VA_ARGS__)
#else
/* 宏定义: APP_INFO_PRINTF - 当前模块使用的配置常量。 */
#define APP_INFO_PRINTF(...) do { } while (0)
#endif

static volatile uint32_t s_maintask_scope_frame_period_ms = MAINTASK_FRAME_PERIOD_MS;
static volatile uint8_t  s_maintask_fast_display_active;

#if (Y_T_AB_DISABLE_MAINTASK_PERIODIC_LOG == 0) && \
    (DSO_VPP_DIAG_ENABLE == 0) && \
    (MAINTASK_STACK_LOG_INTERVAL_MS > 0U) && \
    (INCLUDE_uxTaskGetStackHighWaterMark == 1)
/*
*********************************************************************************************************
*   函 数 名: MainTask_LogStackWatermarkIfDue
*   功能说明: 执行 emWin 主界面初始化、示波器页面绘制、帧刷新或显示诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void MainTask_LogStackWatermarkIfDue(void)
{
    static TickType_t s_last_stack_log_tick;

    TickType_t now = xTaskGetTickCount();
    if ((TickType_t)(now - s_last_stack_log_tick) < pdMS_TO_TICKS(MAINTASK_STACK_LOG_INTERVAL_MS)) {
        return;
    }
    s_last_stack_log_tick = now;

    DebugUartLock();
    {
        UBaseType_t wm = uxTaskGetStackHighWaterMark(xTaskGetCurrentTaskHandle());
        /* 栈水位：不用 APP_INFO_PRINTF，避免 DSO_APP_INFO_PRINTF_ENABLE=0 时场测无输出。 */
        printf("[MainTask] stack min-free (high-water) words=%lu\r\n", (unsigned long)wm);
    }
#if (USE_DSO_ENGINE != 0)
    if (g_dso_engine_task != NULL) {
        UBaseType_t wm_dso = uxTaskGetStackHighWaterMark(g_dso_engine_task);
        printf("[DSOEng] stack min-free (high-water) words=%lu\r\n", (unsigned long)wm_dso);
    }
#endif
    /* 与 LCDConf_WaitVsyncReload 同源：若超时计数持续大增再查 LTDC/行中断/电源 */
    printf("[LCD] g_lcdconf_vsync_timeout_cnt=%lu\r\n", (unsigned long)g_lcdconf_vsync_timeout_cnt);
    {
        static uint32_t s_last_ltdc_fu_cnt;
        uint32_t fu = g_lcdconf_ltdc_fu_cnt;
        printf("[LCD] fu=%lu delta=%lu\r\n",
               (unsigned long)fu,
               (unsigned long)(fu - s_last_ltdc_fu_cnt));
        s_last_ltdc_fu_cnt = fu;
    }
    printf("[LCD] scope_frame_ms=%lu fast_display=%u\r\n",
           (unsigned long)s_maintask_scope_frame_period_ms,
           (unsigned int)s_maintask_fast_display_active);
    {
        uint32_t rl_frames = 0U;
        uint32_t rl_avg_prep = 0U;
        uint32_t rl_avg_commit = 0U;
        uint32_t rl_avg_fu_d = 0U;
        uint64_t rl_sum_fu_d = 0ULL;

        LCDConf_RenderLatchMetrics_FlushForLog(&rl_frames, &rl_avg_prep, &rl_avg_commit, &rl_avg_fu_d,
                                               &rl_sum_fu_d);
        /* 勿用 %llu：不少嵌入式 newlib-nano / 精简 printf 不支持，会恒显示 0 */
        {
            unsigned long avg_fu_x1000 = 0U;
            if (rl_frames > 0U) {
                avg_fu_x1000 = (unsigned long)((rl_sum_fu_d * 1000ULL) / (uint64_t)rl_frames);
            }
            printf("[LCD] render_latch frames=%lu avg_prep_cy=%lu avg_commit_cy=%lu avg_fu_delta=%lu "
                   "avg_fu_delta_x1000=%lu sum_fu_delta=%lu",
                   (unsigned long)rl_frames,
                   (unsigned long)rl_avg_prep,
                   (unsigned long)rl_avg_commit,
                   (unsigned long)rl_avg_fu_d,
                   avg_fu_x1000,
                   (unsigned long)rl_sum_fu_d);
        }
        if (rl_frames == 0U) {
            printf(" (hint: frames=0 示波器帧未 Commit，sum 不累加；请在 UI_SCR_SCOPE 下看此行)\r\n");
        } else {
            printf(" (avg_fu_delta_x1000≈每帧 underrun×1000；对照 sum 与 [LCD] fu delta)\r\n");
        }
    }
    if (VRAM_Sentinels_Check() == 0) {
        printf("[VRAM] sentinel fail count=%lu (SDRAM 越界嫌疑；与 0x24 栈区区分)\r\n",
               (unsigned long)VRAM_Sentinels_FailCount());
    }
    DebugUartUnlock();
}
#endif

/* Header strip（与 LCDConf ROTATE_HEADER_H、ui_theme UI_HDR_H 一致） */
#define HDR_H   160
/* 顶栏内 y：按旧 80px 布局比例映射到当前 HDR_H，避免改 HDR 后控件堆挤 */
#define HDR_MAP_Y(y)  (((int)(y) * (HDR_H)) / 80)
/* 标题条高度 = 原「顶/底」分界 HDR_MAP_Y(33) 的一半；其余为信息栏底色区 */
#define HDR_TITLE_LAST   ((HDR_MAP_Y(33)) / 2)
/* 信息栏首行基准：紧挨标题条下分割线与间距 */
#define HDR_INFO_TOP     ((HDR_TITLE_LAST) + 8)
/* 测量区文字边界：与 FillRect(..., DISP_W-12, ...) 内缩 2px；分两列并 InRect 裁剪防 MODE/CH1 与 CH2 重叠 */
#define HDR_MEAS_XL      400
/* 宏定义: HDR_MEAS_XR - 当前模块使用的配置常量。 */
#define HDR_MEAS_XR      ((DISP_W) - 14)
/* 将旧 80px 头内信息区参考纵坐标 36..72 映射到当前头内信息栏 — 已由分行走密取代，仅保留 HDR_INFO_TOP */
/* 标题条内旋钮/RUN 徽记下边（须 ≤ HDR_TITLE_LAST） */
#define HDR_TITLE_BTN_B  (((HDR_TITLE_LAST) > 8) ? ((HDR_TITLE_LAST) - 3) : (HDR_TITLE_LAST))
/* Waveform区：左 24px 刻度；右侧 SCOPE_SIDE 信息栏；底留白由 DISP_H - PLOT_Y - PLOT_H 自然得出 */
#define SCOPE_SIDE_PANEL_W   156
/* 宏定义: SCOPE_SIDE_GAP - 当前模块使用的配置常量。 */
#define SCOPE_SIDE_GAP       8
/* 宏定义: PLOT_RIGHT_MARGIN - 当前模块使用的配置常量。 */
#define PLOT_RIGHT_MARGIN    4
/* 宏定义: PLOT_X - 当前模块使用的配置常量。 */
#define PLOT_X               24
#define PLOT_Y               (HDR_H + 2 + 10)  /* +10px 向下偏移 */
#define PLOT_W               ((int)DSO_DISPLAY_WIDTH)
/* 宏定义: PLOT_H - 头文件保护宏。 */
#define PLOT_H               ((int)DSO_DISPLAY_HEIGHT)
/* 宏定义: SCOPE_PANEL_X - 当前模块使用的配置常量。 */
#define SCOPE_PANEL_X        ((PLOT_X) + (PLOT_W) + (SCOPE_SIDE_GAP))

/* 预处理器里不能展开带 (int) 转换的 PLOT_W，故用 DSO_DISPLAY_WIDTH 做编译期检查 */
#if ((PLOT_X) + (DSO_DISPLAY_WIDTH) + (SCOPE_SIDE_GAP) + (SCOPE_SIDE_PANEL_W) + (PLOT_RIGHT_MARGIN)) != (DISP_W)
#error Scope layout: PLOT_X+DSO_DISPLAY_WIDTH+GAP+PANEL+RIGHT_MARGIN must equal DISP_W
#endif

/* Channel ground-Y is now computed at runtime from DSO_Frame metadata
   via compute_channel_ground_y(). */

/* ── color palette (0x00RRGGBB) ─────────────────────────────── */
#define CLR_BG_SCREEN   0x080C10u
/* 宏定义: CLR_BG_HEADER - 界面颜色常量。 */
#define CLR_BG_HEADER   0x10161Eu
/* 宏定义: CLR_BG_INFO - 界面颜色常量。 */
#define CLR_BG_INFO     0x0C1018u
/* CLR_BG_PLOT 见 dma2d_wave.h，PlotBuf 与 GUI 共用 */
#define CLR_GRID_MINOR  0x0D1828u  /* 细分网格线（极淡，不干扰波形） */
#define CLR_GRID_MAJOR  0x1A2E46u  /* 主网格线（与 CLR_BORDER_IN 呼应） */
#define CLR_AXIS        0x2C4A78u  /* 中心轴线（亮蓝，清晰可辨） */
#define CLR_BORDER_OUT  0x0A1828u
/* 宏定义: CLR_BORDER_IN - 界面颜色常量。 */
#define CLR_BORDER_IN   0x203858u
/* 宏定义: CLR_CH1 - 界面颜色常量。 */
#define CLR_CH1         0xFF3355u
/* 宏定义: CLR_CH2 - 界面颜色常量。 */
#define CLR_CH2         0x22EE88u
/* 宏定义: CLR_CH1_BADGE - 界面颜色常量。 */
#define CLR_CH1_BADGE   0x380010u
/* 宏定义: CLR_CH2_BADGE - 界面颜色常量。 */
#define CLR_CH2_BADGE   0x003820u
/* 宏定义: CLR_RUN_BG - 界面颜色常量。 */
#define CLR_RUN_BG      0x143220u
/* 宏定义: CLR_RUN_FG - 界面颜色常量。 */
#define CLR_RUN_FG      0x44FF66u
#define CLR_STOP_FG     0xFF4466u   /* STOP 状态：红点 + 红字 */
#define CLR_TEXT_HI     0xD0E4F8u
/* 宏定义: CLR_TEXT_MID - 界面颜色常量。 */
#define CLR_TEXT_MID    0x6888A8u
/* 宏定义: CLR_TEXT_DIM - 界面颜色常量。 */
#define CLR_TEXT_DIM    0x304050u
/* 宏定义: CLR_DIVIDER - 界面颜色常量。 */
#define CLR_DIVIDER     0x1A2A40u
#define CLR_MEAS_VALUE  0xFFFFFFu  /* 详细信息栏数值（纯白）；标签仍用 CLR_TEXT_MID / 占位用 CLR_TEXT_DIM */

static DSO_UI_Frame s_frame;
static bool      s_frame_valid = false;

extern volatile unsigned long g_emwin_showbuffer_cnt;
extern volatile unsigned long g_emwin_ltdc_irq_cnt;
extern volatile unsigned long g_emwin_confirm_cnt;

/* ── grid ──────────────────────────────────────────────────── */

/* 仅绘制网格线（不含背景填充、不含边框）
   主循环每帧调用：执行区域严格在 [PLOT_X, PLOT_Y, PLOT_X+PLOT_W-1, PLOT_Y+PLOT_H-1]
   不会触碰 header、边距、边框区域 → header 保持静态无闪烁。       */
static void DrawGridLines(void)
{
    int i;
    const int w = PLOT_W, h = PLOT_H;
    /* 与主/次网格整数除法一致，避免 w/2 与「第 GRID/2 条主格线」错 1px */
    const int cx = (GRID_COL_DIV / 2 * (w - 1)) / GRID_COL_DIV;
    const int cy = (GRID_ROW_DIV / 2 * (h - 1)) / GRID_ROW_DIV;

    GUI_SetColor(CLR_GRID_MINOR);
    for (i = 0; i <= GRID_COL_DIV * 2; i++) {
        int x = PLOT_X + (i * (w - 1)) / (GRID_COL_DIV * 2);
        GUI_DrawVLine(x, PLOT_Y, PLOT_Y + h - 1);
    }
    for (i = 0; i <= GRID_ROW_DIV * 2; i++) {
        int y = PLOT_Y + (i * (h - 1)) / (GRID_ROW_DIV * 2);
        GUI_DrawHLine(y, PLOT_X, PLOT_X + w - 1);
    }

    GUI_SetColor(CLR_GRID_MAJOR);
    for (i = 0; i <= GRID_COL_DIV; i++) {
        int x = PLOT_X + (i * (w - 1)) / GRID_COL_DIV;
        GUI_DrawVLine(x, PLOT_Y, PLOT_Y + h - 1);
    }
    for (i = 0; i <= GRID_ROW_DIV; i++) {
        int y = PLOT_Y + (i * (h - 1)) / GRID_ROW_DIV;
        GUI_DrawHLine(y, PLOT_X, PLOT_X + w - 1);
    }

    GUI_SetColor(CLR_AXIS);
    GUI_DrawHLine(PLOT_Y + cy, PLOT_X, PLOT_X + w - 1);
    GUI_DrawVLine(PLOT_X + cx, PLOT_Y, PLOT_Y + h - 1);
}

/* 完整 plot 区域初始化绘制（背景 + 网格线 + 双层边框）
   仅在初始化阶段向每个缓冲调用一次；主循环不调用此函数。        */
static void DrawGridFull(void)
{
    GUI_SetColor(CLR_BG_PLOT);
    GUI_FillRect(PLOT_X, PLOT_Y, PLOT_X + PLOT_W - 1, PLOT_Y + PLOT_H - 1);

    DrawGridLines();

    /* 双层边框：位于 plot 区域外侧，FillRect/DrawGridLines 均不覆盖，
       初始化后永久保留，无需每帧重绘。                              */
    GUI_SetColor(CLR_BORDER_OUT);
    GUI_DrawRect(PLOT_X - 2, PLOT_Y - 2, PLOT_X + PLOT_W + 1, PLOT_Y + PLOT_H + 1);
    GUI_SetColor(CLR_BORDER_IN);
    GUI_DrawRect(PLOT_X - 1, PLOT_Y - 1, PLOT_X + PLOT_W,     PLOT_Y + PLOT_H    );
}

/* ── waveforms ─────────────────────────────────────────────── */

static uint16_t main_plot_adc_fullscale(void)
{
    return DSO_SampleSpec_GetActive()->adc_max;
}

/*
*********************************************************************************************************
*   函 数 名: main_scope_plotbuf_wave_active
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static int main_scope_plotbuf_wave_active(void)
{
#if MAINTASK_AB_DISABLE_PLOTBUF_WAVEFORM
    return 0;
#else
    return DMA2D_PlotBuf_IsReady() ? 1 : 0;
#endif
}

/* DSO 幅度坐标 0=顶、PLOT_H-1=底；PlotBuf 波形用 phys_x=PLOT_H-1-amp，叠加层须同映射。 */
static int main_plot_dso_y_to_gui(int dso_y)
{
    if (dso_y < 0) {
        dso_y = 0;
    } else if (dso_y > PLOT_H - 1) {
        dso_y = PLOT_H - 1;
    }
    if (main_scope_plotbuf_wave_active() != 0) {
        return PLOT_Y + (PLOT_H - 1 - dso_y);
    }
    return PLOT_Y + dso_y;
}

/*
*********************************************************************************************************
*   函 数 名: volts_to_plot_dso_y
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static int volts_to_plot_dso_y(float volts, float scale_min_v, float scale_max_v)
{
    uint16_t adc_max    = main_plot_adc_fullscale();
    float lsb           = DSO_VREF / (float)adc_max;
    float adc           = volts / lsb;
    float scale_min_adc = scale_min_v / lsb;
    float scale_max_adc = scale_max_v / lsb;
    float denom         = scale_max_adc - scale_min_adc;
    float y_range, y_bottom, norm, y;

    if (denom < 1.0f) {
        denom = 1.0f;
    }

    y_range  = (float)(DSO_DISPLAY_HEIGHT - 1u - 2u * DSO_PIXEL_MARGIN);
    y_bottom = (float)(DSO_DISPLAY_HEIGHT - 1u - DSO_PIXEL_MARGIN);
    norm     = (adc - scale_min_adc) / denom;
    y        = y_bottom - (norm * y_range);

    if (y < 0.0f) {
        y = 0.0f;
    }
    if (y > (float)(DSO_DISPLAY_HEIGHT - 1u)) {
        y = (float)(DSO_DISPLAY_HEIGHT - 1u);
    }
    return (int)(y + 0.5f);
}

/*
 * ADC 中点（0 V 偏置）在 plot 区的 DSO 幅度坐标，供地线箭头与 CH 标签使用。
 */
static int compute_channel_ground_y(float scale_min_v, float scale_max_v)
{
    uint16_t adc_max = main_plot_adc_fullscale();
    float mid_v      = ((float)adc_max + 1.0f) * 0.5f * (DSO_VREF / (float)adc_max);

    return main_plot_dso_y_to_gui(volts_to_plot_dso_y(mid_v, scale_min_v, scale_max_v));
}

/** Solo（单路显示）与通道页 CH1/CH2 ON/OFF 组合后的格区可见性。 */
static void MainTask_ScopeChannelShowFlags(int *show1, int *show2)
{
    UI_Waveform_Solo_t s = UI_Waveform_GetSolo();
    int s1 = (s != UI_WAVEFORM_SOLO_CH2) ? 1 : 0;
    int s2 = (s != UI_WAVEFORM_SOLO_CH1) ? 1 : 0;
    if (show1 != NULL) {
        *show1 = (s1 != 0 && UI_ChannelCtrl_GetEnabled(DSO_CHANNEL_1)) ? 1 : 0;
    }
    if (show2 != NULL) {
        *show2 = (s2 != 0 && UI_ChannelCtrl_GetEnabled(DSO_CHANNEL_2)) ? 1 : 0;
    }
}

/*
*********************************************************************************************************
*   函 数 名: DrawWaveformsToBuf
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void DrawWaveformsToBuf(void)
{
    int x;
    int show1, show2;

    if (!s_frame_valid) return;
    MainTask_ScopeChannelShowFlags(&show1, &show2);

    if (show1 != 0) {
        if (s_frame.waveform_column_envelope_ch1) {
            for (x = 0; x < PLOT_W; x++) {
                int py = LCD_PLOTBUF_PHYS_Y_FROM_PX(x);
                int yt = (int)s_frame.y_column_env_top_ch1[x];
                int yb = (int)s_frame.y_column_env_bot_ch1[x];
                int tmp;

                if (yt == INT16_MIN || yb == INT16_MIN) {
                    continue;
                }
                if (yt > yb) {
                    tmp = yt;
                    yt = yb;
                    yb = tmp;
                }
                if (yt < 0) yt = 0;
                else if (yt > PLOT_H - 1) yt = PLOT_H - 1;
                if (yb < 0) yb = 0;
                else if (yb > PLOT_H - 1) yb = PLOT_H - 1;
                DMA2D_PlotBuf_DrawLine(PLOT_H - 1 - yb,
                                       py,
                                       PLOT_H - 1 - yt,
                                       py,
                                       CLR_CH1);
            }
        } else {
            for (x = 1; x < PLOT_W; x++) {
                int py_prev = LCD_PLOTBUF_PHYS_Y_FROM_PX(x - 1);
                int py_curr = LCD_PLOTBUF_PHYS_Y_FROM_PX(x);
                int amp_p = (int)s_frame.y_coords[x - 1];
                int amp_c = (int)s_frame.y_coords[x];

                if (amp_p == INT16_MIN || amp_c == INT16_MIN) {
                    continue;
                }
                if (amp_p < 0) amp_p = 0;
                else if (amp_p > PLOT_H - 1) amp_p = PLOT_H - 1;
                if (amp_c < 0) amp_c = 0;
                else if (amp_c > PLOT_H - 1) amp_c = PLOT_H - 1;
                DMA2D_PlotBuf_DrawLine(PLOT_H - 1 - amp_p, py_prev,
                                       PLOT_H - 1 - amp_c, py_curr, CLR_CH1);
            }
        }
    }

    if (show2 != 0) {
        if (s_frame.waveform_column_envelope_ch2) {
            for (x = 0; x < PLOT_W; x++) {
                int py = LCD_PLOTBUF_PHYS_Y_FROM_PX(x);
                int yt = (int)s_frame.y_column_env_top_ch2[x];
                int yb = (int)s_frame.y_column_env_bot_ch2[x];
                int tmp;

                if (yt == INT16_MIN || yb == INT16_MIN) {
                    continue;
                }
                if (yt > yb) {
                    tmp = yt;
                    yt = yb;
                    yb = tmp;
                }
                if (yt < 0) yt = 0;
                else if (yt > PLOT_H - 1) yt = PLOT_H - 1;
                if (yb < 0) yb = 0;
                else if (yb > PLOT_H - 1) yb = PLOT_H - 1;
                DMA2D_PlotBuf_DrawLine(PLOT_H - 1 - yb,
                                       py,
                                       PLOT_H - 1 - yt,
                                       py,
                                       CLR_CH2);
            }
        } else {
            for (x = 1; x < PLOT_W; x++) {
                int py_prev = LCD_PLOTBUF_PHYS_Y_FROM_PX(x - 1);
                int py_curr = LCD_PLOTBUF_PHYS_Y_FROM_PX(x);
                int amp_p = (int)s_frame.y_coords_ch2[x - 1];
                int amp_c = (int)s_frame.y_coords_ch2[x];

                if (amp_p == INT16_MIN || amp_c == INT16_MIN) {
                    continue;
                }
                if (amp_p < 0) amp_p = 0;
                else if (amp_p > PLOT_H - 1) amp_p = PLOT_H - 1;
                if (amp_c < 0) amp_c = 0;
                else if (amp_c > PLOT_H - 1) amp_c = PLOT_H - 1;
                DMA2D_PlotBuf_DrawLine(PLOT_H - 1 - amp_p, py_prev,
                                       PLOT_H - 1 - amp_c, py_curr, CLR_CH2);
            }
        }
    }
}

/*
*********************************************************************************************************
*   函 数 名: DrawWaveforms
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void DrawWaveforms(void)
{
    int x;
    int show1, show2;

    if (!s_frame_valid) return;
    MainTask_ScopeChannelShowFlags(&show1, &show2);

    if (show1 != 0) {
        GUI_SetColor(CLR_CH1);
        if (s_frame.waveform_column_envelope_ch1) {
            for (x = 0; x < PLOT_W; x++) {
                int yt = (int)s_frame.y_column_env_top_ch1[x];
                int yb = (int)s_frame.y_column_env_bot_ch1[x];
                int tmp;

                if (yt == INT16_MIN || yb == INT16_MIN) {
                    continue;
                }
                if (yt > yb) {
                    tmp = yt;
                    yt = yb;
                    yb = tmp;
                }
                GUI_DrawLine(PLOT_X + x, PLOT_Y + yt, PLOT_X + x, PLOT_Y + yb);
            }
        } else {
            int prev_x = -1;
            int prev_py = 0;

            for (x = 0; x < PLOT_W; x++) {
                int yi = (int)s_frame.y_coords[x];
                int py;

                if (yi == INT16_MIN) {
                    continue;
                }
                py = PLOT_Y + yi;
                if (prev_x >= 0) {
                    GUI_DrawLine(PLOT_X + prev_x, prev_py, PLOT_X + x, py);
                }
                prev_x = x;
                prev_py = py;
            }
        }
    }

    if (show2 != 0) {
        GUI_SetColor(CLR_CH2);
        if (s_frame.waveform_column_envelope_ch2) {
            for (x = 0; x < PLOT_W; x++) {
                int yt = (int)s_frame.y_column_env_top_ch2[x];
                int yb = (int)s_frame.y_column_env_bot_ch2[x];
                int tmp;

                if (yt == INT16_MIN || yb == INT16_MIN) {
                    continue;
                }
                if (yt > yb) {
                    tmp = yt;
                    yt = yb;
                    yb = tmp;
                }
                GUI_DrawLine(PLOT_X + x, PLOT_Y + yt, PLOT_X + x, PLOT_Y + yb);
            }
        } else {
            int prev_x = -1;
            int prev_py = 0;

            for (x = 0; x < PLOT_W; x++) {
                int yi = (int)s_frame.y_coords_ch2[x];
                int py;

                if (yi == INT16_MIN) {
                    continue;
                }
                py = PLOT_Y + yi;
                if (prev_x >= 0) {
                    GUI_DrawLine(PLOT_X + prev_x, prev_py, PLOT_X + x, py);
                }
                prev_x = x;
                prev_py = py;
            }
        }
    }

    GUI_SetTextMode(GUI_TM_TRANS);
    GUI_SetFont(&GUI_Font8_ASCII);
    if (show1 != 0) {
        int cy1 = compute_channel_ground_y(s_frame.vert_scale_min_v, s_frame.vert_scale_max_v);
        GUI_SetColor(CLR_CH1);
        GUI_DispStringAt("CH1", PLOT_X + PLOT_W - 26, cy1 - 5);
    }
    if (show2 != 0) {
        int cy2 = compute_channel_ground_y(s_frame.ch2_vert_scale_min_v, s_frame.ch2_vert_scale_max_v);
        GUI_SetColor(CLR_CH2);
        GUI_DispStringAt("CH2", PLOT_X + PLOT_W - 26, cy2 - 5);
    }
}

/* ── header: CH1/CH2 徽标旁耦合 + 探头倍率（物理通道读数风格） ───────── */
static void fmt_ch_phy_badge_line(const DSO_VerticalInfo *vi, char *buf, size_t n)
{
    const char *cpl;
    const char *pr;

    if (vi == NULL || buf == NULL || n < 8u) {
        return;
    }
    if (vi->coupling == DSO_COUPLING_AC) {
        cpl = "AC";
    } else if (vi->coupling == DSO_COUPLING_GND) {
        cpl = "GND";
    } else {
        cpl = "DC";
    }
    pr = (vi->probe == DSO_PROBE_X10) ? "10x" : "1x";
    (void)snprintf(buf, n, "%s %s", cpl, pr);
}

/*
*********************************************************************************************************
*   函 数 名: draw_ch1_badge_with_phy
*   功能说明: 完成当前 UI 模块的绘制、格式化或局部状态辅助处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void draw_ch1_badge_with_phy(const DSO_VerticalInfo *vi, int y_top)
{
    char tag[16];
    const int y_bot = y_top + 21;

    GUI_SetColor(CLR_CH1_BADGE);
    GUI_FillRoundedRect(8, y_top, 48, y_bot, 4);
    GUI_SetColor(CLR_CH1);
    GUI_DrawRoundedRect(8, y_top, 48, y_bot, 4);
    GUI_SetFont(&GUI_Font16_ASCII);
    GUI_SetColor(CLR_CH1);
    GUI_DispStringAt("CH1", 12, y_top + 2);
    fmt_ch_phy_badge_line(vi, tag, sizeof(tag));
    GUI_SetFont(&GUI_Font8_ASCII);
    GUI_SetColor(CLR_TEXT_DIM);
    GUI_DispStringAt(tag, 10, y_top + 12);
}

/*
*********************************************************************************************************
*   函 数 名: draw_ch2_badge_with_phy
*   功能说明: 完成当前 UI 模块的绘制、格式化或局部状态辅助处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void draw_ch2_badge_with_phy(const DSO_VerticalInfo *vi, int y_top)
{
    char tag[16];
    const int y_bot = y_top + 21;

    GUI_SetColor(CLR_CH2_BADGE);
    GUI_FillRoundedRect(8, y_top, 48, y_bot, 4);
    GUI_SetColor(CLR_CH2);
    GUI_DrawRoundedRect(8, y_top, 48, y_bot, 4);
    GUI_SetFont(&GUI_Font16_ASCII);
    GUI_SetColor(CLR_CH2);
    GUI_DispStringAt("CH2", 12, y_top + 2);
    fmt_ch_phy_badge_line(vi, tag, sizeof(tag));
    GUI_SetFont(&GUI_Font8_ASCII);
    GUI_SetColor(CLR_TEXT_DIM);
    GUI_DispStringAt(tag, 10, y_top + 12);
}

/* 信息栏：GUI_Font20B_ASCII 行距（与 DrawHeader / DrawMeasurements / DrawTrigBadge 一致） */
static int hdr_status_line_step(void)
{
    GUI_SetFont(&GUI_Font20B_ASCII);
    return GUI_GetFontDistY() + 2;
}

/*
*********************************************************************************************************
*   函 数 名: hdr_status_y_ch1
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static int hdr_status_y_ch1(void)
{
    return HDR_INFO_TOP + 4;
}

/*
*********************************************************************************************************
*   函 数 名: hdr_status_y_ch2
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static int hdr_status_y_ch2(int step)
{
    return hdr_status_y_ch1() + 2 * step + 6;
}

/*
*********************************************************************************************************
*   函 数 名: hdr_status_y_trig
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static int hdr_status_y_trig(int step)
{
    return hdr_status_y_ch2(step) + 2 * step + 6;
}

/*
*********************************************************************************************************
*   函 数 名: hdr_meas_column_bounds
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void hdr_meas_column_bounds(int *c1_x0, int *c1_x1, int *c2_x0, int *c2_x1)
{
    int mxl = HDR_MEAS_XL;
    int mxr = HDR_MEAS_XR;
    int xs  = (mxl + mxr + 1) / 2;

    *c1_x0 = mxl;
    *c1_x1 = xs - 3;
    *c2_x0 = xs + 1;
    *c2_x1 = mxr;
}

/*
*********************************************************************************************************
*   函 数 名: hdr_meas_disp_col
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void hdr_meas_disp_col(const char *s, int col_x0, int col_x1, int y, int line_step)
{
    GUI_RECT r;
    int fh = line_step - 2;

    if (fh < 4) {
        fh = GUI_GetFontDistY();
    }
    r.x0 = col_x0;
    r.y0 = y;
    r.x1 = col_x1;
    r.y1 = y + fh - 1;
    GUI_DispStringInRect(s, &r, GUI_TA_LEFT | GUI_TA_TOP);
}

/* 标签用粗体 + label_clr，数值用常规体纯白（如 CH1 Vpp: 与 44mV） */
static void hdr_meas_disp_label_value(const char *label, const char *value,
                                      int col_x0, int col_x1, int y, int line_step,
                                      GUI_COLOR label_clr)
{
    GUI_RECT r;
    int x;
    int fh = line_step - 2;

    if (fh < 4) {
        fh = GUI_GetFontDistY();
    }
    GUI_SetFont(&GUI_Font20B_ASCII);
    GUI_SetColor(label_clr);
    GUI_DispStringAt(label, col_x0, y);
    x = col_x0 + GUI_GetStringDistX(label);
    GUI_SetFont(&GUI_Font20_ASCII);
    GUI_SetColor(CLR_MEAS_VALUE);
    r.x0 = x;
    r.y0 = y;
    r.x1 = col_x1;
    r.y1 = y + fh - 1;
    GUI_DispStringInRect(value, &r, GUI_TA_LEFT | GUI_TA_TOP);
}

/*
*********************************************************************************************************
*   函 数 名: hdr_meas_disp_mode_cal_ext
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void hdr_meas_disp_mode_cal_ext(int cal_collecting,
                                       const char *mode_label, unsigned cal_progress_pct,
                                       const char *cal_label, const char *extf,
                                       int col_x0, int col_x1, int y, int line_step,
                                       GUI_COLOR label_clr)
{
    char cal_val[20];
    int x = col_x0;
    int fh = line_step - 2;
    GUI_RECT r;

    if (fh < 4) {
        fh = GUI_GetFontDistY();
    }
    if (cal_collecting != 0) {
        (void)snprintf(cal_val, sizeof(cal_val), "%u%%", (unsigned)cal_progress_pct);
    } else {
        (void)snprintf(cal_val, sizeof(cal_val), "%s", cal_label);
    }

    GUI_SetFont(&GUI_Font20B_ASCII);
    GUI_SetColor(label_clr);
    GUI_DispStringAt("MODE ", x, y);
    x += GUI_GetStringDistX("MODE ");
    GUI_SetFont(&GUI_Font20_ASCII);
    GUI_SetColor(CLR_MEAS_VALUE);
    GUI_DispStringAt(mode_label, x, y);
    x += GUI_GetStringDistX(mode_label);
    GUI_SetFont(&GUI_Font20B_ASCII);
    GUI_SetColor(label_clr);
    GUI_DispStringAt("   CAL ", x, y);
    x += GUI_GetStringDistX("   CAL ");
    GUI_SetFont(&GUI_Font20_ASCII);
    GUI_SetColor(CLR_MEAS_VALUE);
    GUI_DispStringAt(cal_val, x, y);
    x += GUI_GetStringDistX(cal_val);
    GUI_SetFont(&GUI_Font20B_ASCII);
    GUI_SetColor(label_clr);
    GUI_DispStringAt("   EXT:", x, y);
    x += GUI_GetStringDistX("   EXT:");
    GUI_SetFont(&GUI_Font20_ASCII);
    GUI_SetColor(CLR_MEAS_VALUE);
    r.x0 = x;
    r.y0 = y;
    r.x1 = col_x1;
    r.y1 = y + fh - 1;
    GUI_DispStringInRect(extf, &r, GUI_TA_LEFT | GUI_TA_TOP);
}

/* ── header ────────────────────────────────────────────────── */
static void DrawHeader(void)
{
    GUI_SetTextMode(GUI_TM_TRANS);

    GUI_SetColor(CLR_BG_HEADER);
    GUI_FillRect(0, 0, DISP_W - 1, HDR_TITLE_LAST);
    GUI_SetColor(CLR_BG_INFO);
    GUI_FillRect(0, HDR_TITLE_LAST + 1, DISP_W - 1, HDR_H - 1);

    GUI_SetColor(CLR_DIVIDER);
    GUI_DrawHLine(HDR_TITLE_LAST, 0, DISP_W - 1);
    GUI_DrawHLine(HDR_H, 0, DISP_W - 1);

    GUI_SetFont(&GUI_Font20_ASCII);
    GUI_SetColor(CLR_TEXT_HI);
    {
        const char *title = "DUAL CHANNEL OSCILLOSCOPE";
        int tw = GUI_GetStringDistX(title);
        int ty = (HDR_TITLE_LAST > 22) ? ((HDR_TITLE_LAST - GUI_GetFontDistY()) / 2) : 1;
        GUI_DispStringAt(title, DISP_W / 2 - tw / 2, ty);
    }

    /* 旋钮模式 badge（左上，标题条内紧凑） */
    {
        const char *mode_lbl = UI_Nav_GetKnobModeLabel();
        uint32_t mode_bg = (UI_Nav_GetKnobMode() == 0) ? 0x183018u : 0x302818u;
        uint32_t mode_fg = (UI_Nav_GetKnobMode() == 0) ? 0x66DD66u : 0xDDAA44u;
        int tmy = (3 + HDR_TITLE_BTN_B - GUI_GetFontDistY()) / 2;
        if (tmy < 3) {
            tmy = 3;
        }
        GUI_SetColor(mode_bg);
        GUI_FillRoundedRect(8, 3, 88, HDR_TITLE_BTN_B, 4);
        GUI_SetColor(mode_fg);
        GUI_DrawRoundedRect(8, 3, 88, HDR_TITLE_BTN_B, 4);
        GUI_SetFont(&GUI_Font16_ASCII);
        GUI_DispStringAt(mode_lbl, 14, tmy);
    }

    GUI_SetColor(CLR_RUN_BG);
    GUI_FillRoundedRect(DISP_W - 78, 3, DISP_W - 8, HDR_TITLE_BTN_B, 4);
    GUI_SetColor(CLR_BORDER_IN);
    GUI_DrawRoundedRect(DISP_W - 78, 3, DISP_W - 8, HDR_TITLE_BTN_B, 4);
    GUI_SetColor(CLR_RUN_FG);
    {
        int rmy = (3 + HDR_TITLE_BTN_B) / 2;
        GUI_FillCircle(DISP_W - 62, rmy, 4);
    }
    GUI_SetFont(&GUI_Font16_ASCII);
    GUI_SetColor(CLR_RUN_FG);
    {
        int rty = (3 + HDR_TITLE_BTN_B - GUI_GetFontDistY()) / 2;
        if (rty < 3) {
            rty = 3;
        }
        GUI_DispStringAt("RUN", DISP_W - 54, rty);
    }

    {
        int step, y_ch1, y_ch2, y_trig;
        char trig_line[24];
        DSO_VerticalInfo vi1 = {0};
        DSO_VerticalInfo vi2 = {0};

        step  = hdr_status_line_step();
        y_ch1 = hdr_status_y_ch1();
        y_ch2 = hdr_status_y_ch2(step);
        y_trig = hdr_status_y_trig(step);

        DSO_Vertical_GetInfoForChannel(DSO_CHANNEL_1, &vi1);
        DSO_Vertical_GetInfoForChannel(DSO_CHANNEL_2, &vi2);
        draw_ch1_badge_with_phy(&vi1, y_ch1);
        draw_ch2_badge_with_phy(&vi2, y_ch2);

        GUI_SetFont(&GUI_Font20B_ASCII);
        GUI_SetColor(CLR_TEXT_MID);
        {
            char scale_str[24];
            char fs_str[20];
            UI_Waveform_FormatScaleStr(scale_str, sizeof(scale_str));
            UI_Waveform_FormatSampleRateHz(UI_Waveform_GetEffectiveSampleRateHz(), fs_str, sizeof(fs_str));
            GUI_DispStringAt(scale_str, 54, y_ch1 + 1);
            GUI_DispStringAt(fs_str, 54, y_ch1 + 1 + step);
        }
        {
            char scale_str[24];
            char fs_str[20];
            UI_Waveform_FormatScaleStrCh2(scale_str, sizeof(scale_str));
            UI_Waveform_FormatSampleRateHz(UI_Waveform_GetCh2SampleRateHz(), fs_str, sizeof(fs_str));
            GUI_DispStringAt(scale_str, 54, y_ch2 + 1);
            GUI_DispStringAt(fs_str, 54, y_ch2 + 1 + step);
        }
        (void)snprintf(trig_line, sizeof(trig_line), "TRIG %s", UI_Waveform_GetTrigModeLabel());
        GUI_DispStringAt(trig_line, 54, y_trig);
    }

    /* 测量区由 DrawMeasurements 每帧刷新；下列占位与字号一致便于首次布局 */
    {
        int st = hdr_status_line_step();
        int c1a, c1b, c2a, c2b;
        int y = HDR_INFO_TOP + 4;

        hdr_meas_column_bounds(&c1a, &c1b, &c2a, &c2b);
        GUI_SetColor(CLR_TEXT_DIM);
        hdr_meas_disp_label_value("CH1 F: ", "--", c1a, c1b, y, st, CLR_TEXT_DIM);
        hdr_meas_disp_label_value("CH2 F: ", "--", c2a, c2b, y, st, CLR_TEXT_DIM);
        y += st;
        hdr_meas_disp_label_value("CH1 Vpp: ", "--", c1a, c1b, y, st, CLR_TEXT_DIM);
        hdr_meas_disp_label_value("CH2 Vpp: ", "--", c2a, c2b, y, st, CLR_TEXT_DIM);
        y += st;
        hdr_meas_disp_label_value("CH1 Vrms: ", "--", c1a, c1b, y, st, CLR_TEXT_DIM);
        hdr_meas_disp_label_value("CH2 Vrms: ", "--", c2a, c2b, y, st, CLR_TEXT_DIM);
        y += st;
        hdr_meas_disp_label_value("CH1 T: ", "--", c1a, c1b, y, st, CLR_TEXT_DIM);
        hdr_meas_disp_label_value("CH2 T: ", "--", c2a, c2b, y, st, CLR_TEXT_DIM);
    }
}

/* ── data update ───────────────────────────────────────────── */
/**
 * 从 UI_WaveformCtrl 双缓冲环非阻塞取最新帧。
 * @return true 表示本周期收到 DSO_Engine_Task 新发布的 DSO_UI_Frame。
 *         false 表示无新帧，s_frame 仍为上帧内容（STOP 冻结或引擎尚未发布）。
 */
static bool UpdateWaveformData(void)
{
    if (UI_Waveform_CopyUIFrame(&s_frame)) {
        s_frame_valid = true;
        UI_Trigger_SyncFrameState(&s_frame);
        return true;
    }
    return false;
}

/* ── 静态区域（左侧 Y 轴刻度线）────────────────────────────── */
static void DrawYAxisTicks(void)
{
    int j;
    GUI_SetColor(CLR_BORDER_IN);
    for (j = 0; j <= GRID_ROW_DIV; j++) {
        int y = PLOT_Y + (j * (PLOT_H - 1)) / GRID_ROW_DIV;
        GUI_DrawHLine(y, PLOT_X - 5, PLOT_X - 1);
    }
}

/* ── 通道基线箭头（GND 参考标记，模仿真实示波器左侧指示器）─── */
static void draw_right_arrow(int x_tip, int y_center, int half_h, uint32_t color)
{
    int dy;
    GUI_SetColor(color);
    for (dy = -half_h; dy <= half_h; dy++) {
        int w = half_h - ((dy < 0) ? -dy : dy);
        GUI_DrawHLine(y_center + dy, x_tip - w, x_tip);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DrawGroundRefMarkers
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void DrawGroundRefMarkers(void)
{
    int show1, show2;
    const int tip_x  = PLOT_X - 7;
    const int arr_sz = 4;

    if (!s_frame_valid) {
        return;
    }

    MainTask_ScopeChannelShowFlags(&show1, &show2);

    GUI_SetColor(CLR_BG_SCREEN);
    GUI_FillRect(0, PLOT_Y, tip_x, PLOT_Y + PLOT_H - 1);

    if (show1 != 0) {
        int y = compute_channel_ground_y(s_frame.vert_scale_min_v,
                                         s_frame.vert_scale_max_v);
        if (y < PLOT_Y + arr_sz) y = PLOT_Y + arr_sz;
        if (y > PLOT_Y + PLOT_H - 1 - arr_sz) y = PLOT_Y + PLOT_H - 1 - arr_sz;
        draw_right_arrow(tip_x, y, arr_sz, CLR_CH1);
    }
    if (show2 != 0) {
        int y = compute_channel_ground_y(s_frame.ch2_vert_scale_min_v,
                                         s_frame.ch2_vert_scale_max_v);
        if (y < PLOT_Y + arr_sz) y = PLOT_Y + arr_sz;
        if (y > PLOT_Y + PLOT_H - 1 - arr_sz) y = PLOT_Y + PLOT_H - 1 - arr_sz;
        draw_right_arrow(tip_x, y, arr_sz, CLR_CH2);
    }
}

/* ── 动态测量值显示（每帧更新 header 右侧区域）───────────────── */
#define CLR_TRIG_LINE   0xFFAA00u

/* CH2：由波形测量频率换算周期。CH1 周期见 UI_FreqFormat_ExtPeriod（外计闸门）。 */
static void hdr_fmt_period(float hz, int valid, char *buf, size_t n)
{
    double t;

    if ((buf == NULL) || (n < 4u)) {
        return;
    }
    if (!valid || hz <= 0.0f) {
        (void)snprintf(buf, n, "--");
        return;
    }
    t = 1.0 / (double)hz;
    if (t >= 1.0) {
        (void)snprintf(buf, n, "%.3f s", t);
    } else if (t >= 1e-3) {
        (void)snprintf(buf, n, "%.3f ms", t * 1e3);
    } else if (t >= 1e-6) {
        (void)snprintf(buf, n, "%.2f us", t * 1e6);
    } else {
        (void)snprintf(buf, n, "%.0f ns", t * 1e9);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DrawMeasurements
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void DrawMeasurements(void)
{
    UI_Waveform_Meas_t m, m2;
    const char *mode_label = "RT";
    const char *cal_label = "--";

    UI_Waveform_GetMeasurements(&m);
    UI_Waveform_GetMeasurementsCh2(&m2);

    if (s_frame.live_record_paused) {
        mode_label = "HIST";
    } else if (s_frame.recon_mode == DSO_UI_RECON_ETS) {
        mode_label = "ETS";
    } else if (s_frame.display_quality == DSO_UI_QUALITY_REPEAT_REQUIRED) {
        mode_label = "REP?";
    } else if (s_frame.display_quality == (uint8_t)DSO_UI_QUALITY_LIMITED) {
        mode_label = "LIM";
    }

    if (s_frame.cal_state == DSO_UI_CAL_COLLECTING) {
        cal_label = "---";
    } else if (s_frame.ch1_interleave_cal_valid) {
        cal_label = "OK";
    } else if (s_frame.cal_state == DSO_UI_CAL_FAILED) {
        cal_label = "FAIL";
    }

    {
        char c1f[20], c1p[12], c2f[16], c2p[12], extf[24];
        char c1r[12], c2r[12];
        char t1[20], t2[20];
        int step, y;
        int c1a, c1b, c2a, c2b;

        hdr_meas_column_bounds(&c1a, &c1b, &c2a, &c2b);

        UI_FreqFormat_ExtMilliHzCompact(s_frame.ext_freq_millihz,
                                        s_frame.ext_freq_valid,
                                        s_frame.ext_freq_state,
                                        c1f,
                                        sizeof(c1f));
        if (m.vpp_v >= 1.0f)
            (void)snprintf(c1p, sizeof(c1p), "%.2fV", (double)m.vpp_v);
        else
            (void)snprintf(c1p, sizeof(c1p), "%.0fmV", (double)(m.vpp_v * 1000.0f));

        if (s_frame.ch2_present && m2.freq_valid && m2.freq_hz > 0.0f) {
            if (m2.freq_hz >= 1e6f)
                (void)snprintf(c2f, sizeof(c2f), "%.1fM", (double)(m2.freq_hz / 1e6f));
            else if (m2.freq_hz >= 1e3f)
                (void)snprintf(c2f, sizeof(c2f), "%.1fk", (double)(m2.freq_hz / 1e3f));
            else
                (void)snprintf(c2f, sizeof(c2f), "%.0f", (double)m2.freq_hz);
        } else {
            (void)snprintf(c2f, sizeof(c2f), "--");
        }
        if (s_frame.ch2_present) {
            if (m2.vpp_v >= 1.0f)
                (void)snprintf(c2p, sizeof(c2p), "%.2fV", (double)m2.vpp_v);
            else
                (void)snprintf(c2p, sizeof(c2p), "%.0fmV", (double)(m2.vpp_v * 1000.0f));
        } else {
            (void)snprintf(c2p, sizeof(c2p), "--");
        }

        if (m.vrms_v >= 1.0f)
            (void)snprintf(c1r, sizeof(c1r), "%.2fV", (double)m.vrms_v);
        else
            (void)snprintf(c1r, sizeof(c1r), "%.0fmV", (double)(m.vrms_v * 1000.0f));
        if (s_frame.ch2_present) {
            if (m2.vrms_v >= 1.0f)
                (void)snprintf(c2r, sizeof(c2r), "%.2fV", (double)m2.vrms_v);
            else
                (void)snprintf(c2r, sizeof(c2r), "%.0fmV", (double)(m2.vrms_v * 1000.0f));
        } else {
            (void)snprintf(c2r, sizeof(c2r), "--");
        }
        UI_FreqFormat_ExtPeriod(s_frame.ext_freq_millihz,
                                s_frame.ext_freq_valid,
                                s_frame.ext_freq_state,
                                t1,
                                sizeof(t1));
        if (s_frame.ch2_present) {
            hdr_fmt_period(m2.freq_hz, m2.freq_valid ? 1 : 0, t2, sizeof(t2));
        } else {
            (void)snprintf(t2, sizeof(t2), "--");
        }
        UI_FreqFormat_ExtMilliHz(s_frame.ext_freq_millihz,
                                 s_frame.ext_freq_valid,
                                 s_frame.ext_freq_state,
                                 extf,
                                 sizeof(extf));

        {
            const int ch1_on = UI_ChannelCtrl_GetEnabled(DSO_CHANNEL_1) ? 1 : 0;
            const int ch2_on = UI_ChannelCtrl_GetEnabled(DSO_CHANNEL_2) ? 1 : 0;
            if (!ch1_on) {
                (void)snprintf(c1f, sizeof(c1f), "--");
                (void)snprintf(c1p, sizeof(c1p), "--");
                (void)snprintf(c1r, sizeof(c1r), "--");
                (void)snprintf(t1, sizeof(t1), "--");
            }
            if (!ch2_on) {
                (void)snprintf(c2f, sizeof(c2f), "--");
                (void)snprintf(c2p, sizeof(c2p), "--");
                (void)snprintf(c2r, sizeof(c2r), "--");
                (void)snprintf(t2, sizeof(t2), "--");
            }
        }

        GUI_SetFont(&GUI_Font20B_ASCII);
        step = GUI_GetFontDistY() + 2;

        GUI_SetColor(CLR_BG_INFO);
        GUI_FillRect(398, HDR_INFO_TOP + 2, DISP_W - 12, HDR_H - 3);

        y = HDR_INFO_TOP + 4;
        hdr_meas_disp_mode_cal_ext(
            (s_frame.cal_state == DSO_UI_CAL_COLLECTING) ? 1 : 0,
            mode_label,
            (unsigned)s_frame.cal_progress_pct,
            cal_label,
            extf,
            c1a, c1b, y, step,
            CLR_TEXT_MID);
        y += step;

        hdr_meas_disp_label_value("CH1 F: ", c1f, c1a, c1b, y, step, CLR_TEXT_MID);
        hdr_meas_disp_label_value("CH2 F: ", c2f, c2a, c2b, y, step, CLR_TEXT_MID);
        y += step;

        hdr_meas_disp_label_value("CH1 Vpp: ", c1p, c1a, c1b, y, step, CLR_TEXT_MID);
        hdr_meas_disp_label_value("CH2 Vpp: ", c2p, c2a, c2b, y, step, CLR_TEXT_MID);
        y += step;

        hdr_meas_disp_label_value("CH1 Vrms: ", c1r, c1a, c1b, y, step, CLR_TEXT_MID);
        hdr_meas_disp_label_value("CH2 Vrms: ", c2r, c2a, c2b, y, step, CLR_TEXT_MID);
        y += step;

        hdr_meas_disp_label_value("CH1 T: ", t1, c1a, c1b, y, step, CLR_TEXT_MID);
        hdr_meas_disp_label_value("CH2 T: ", t2, c2a, c2b, y, step, CLR_TEXT_MID);
    }
}

/* 宏定义: CLR_MENU_SEL_BG - 界面颜色常量。 */
#define CLR_MENU_SEL_BG   0x1A3550u
/* 宏定义: CLR_MENU_SEL_BR - 界面颜色常量。 */
#define CLR_MENU_SEL_BR   0x2A4060u

/* 侧栏内水平居中（须已 GUI_SetFont）；x0/x1 为含端点的卡片内边界 */
static void scope_menu_str_cx(const char *s, int x0, int x1, int y)
{
    int w;
    if (s == NULL) {
        return;
    }
    w = GUI_GetStringDistX(s);
    GUI_DispStringAt(s, (x0 + x1 + 1 - w) / 2, y);
}

/* 波形区右侧：原主菜单五项（EC1 长按进入菜单档；菜单态 EC1 短按确认） */
static void DrawScopeSideMenu(void)
{
    const int px = SCOPE_PANEL_X;
    const int py = PLOT_Y;
    const int pw = SCOPE_SIDE_PANEL_W;
    const int ph = PLOT_H;
    const int gap = 5;
    /* 顶行 MENU/WAVE 加高后，卡片区高度按底边距 6px 重算 */
    const int title_h = 30;
    const int top = py + title_h;
    const int card_h = (ph - title_h - 6 - 4 * gap) / 5;
    int i;
    int y = top;
    int sel = UI_Nav_GetMenuFocus();
    UI_ScopeUiMode_t uim = UI_Nav_GetScopeUiMode();
    static const struct {
        const char *t1;
        const char *t2;
    } rows[5] = {
        { "Scope",    "Wave" },
        { "Channel",  "V/div  offset" },
        { "Trigger",  "Trig" },
        { "Measure",  "Auto" },
        { "System",   "Setup" }
    };

    GUI_SetTextMode(GUI_TM_TRANS);
    GUI_SetColor(CLR_BG_INFO);
    GUI_FillRect(px, py, px + pw - 1, py + ph - 1);
    GUI_SetColor(CLR_DIVIDER);
    GUI_DrawVLine(px, py, py + ph - 1);

    {
        const char *title = (uim == UI_SCOPE_UI_MENU) ? "MENU" : "WAVE";
        int th;
        GUI_SetFont(&GUI_Font16B_ASCII);
        GUI_SetColor(CLR_TEXT_DIM);
        th = GUI_GetFontDistY();
        scope_menu_str_cx(title, px, px + pw - 1, py + (title_h - th) / 2);
    }

    for (i = 0; i < 5; i++) {
        int x0 = px + 3;
        int x1 = px + pw - 4;
        int ya = y;
        int yb = y + card_h - 1;
        int on = (i == sel) ? 1 : 0;
        int h1, h2, y1, y2, vgap;

        if (on != 0 && uim == UI_SCOPE_UI_MENU) {
            GUI_SetColor(CLR_MENU_SEL_BG);
            GUI_FillRoundedRect(x0, ya, x1, yb, 3);
            GUI_SetColor(CLR_MENU_SEL_BR);
            GUI_DrawRoundedRect(x0, ya, x1, yb, 3);
            GUI_SetColor(CLR_TEXT_HI);
        } else if (on != 0) {
            GUI_SetColor(CLR_BG_PLOT);
            GUI_FillRoundedRect(x0, ya, x1, yb, 3);
            GUI_SetColor(CLR_BORDER_IN);
            GUI_DrawRoundedRect(x0, ya, x1, yb, 3);
            GUI_SetColor(CLR_TEXT_MID);
        } else {
            GUI_SetColor(CLR_BG_PLOT);
            GUI_FillRoundedRect(x0, ya, x1, yb, 3);
            GUI_SetColor(CLR_BORDER_IN);
            GUI_DrawRoundedRect(x0, ya, x1, yb, 3);
            GUI_SetColor(CLR_TEXT_MID);
        }

        GUI_SetFont(&GUI_Font20_ASCII);
        h1 = GUI_GetFontDistY();
        GUI_SetFont(&GUI_Font13_ASCII);
        h2 = GUI_GetFontDistY();
        vgap = 3;
        y1 = ya + (card_h - (h1 + vgap + h2)) / 2;
        y2 = y1 + h1 + vgap;

        GUI_SetFont(&GUI_Font20_ASCII);
        scope_menu_str_cx(rows[i].t1, x0, x1, y1);

        GUI_SetFont(&GUI_Font13_ASCII);
        GUI_SetColor((on != 0 && uim == UI_SCOPE_UI_MENU) ? CLR_TEXT_HI : CLR_TEXT_DIM);
        scope_menu_str_cx(rows[i].t2, x0, x1, y2);

        y += card_h + gap;
    }
}

/*
*********************************************************************************************************
*   函 数 名: DrawTrigBadge
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void DrawTrigBadge(void)
{
    int step, y;
    char buf[32];

    step = hdr_status_line_step();
    y    = hdr_status_y_trig(step);
    (void)snprintf(buf, sizeof(buf), "TRIG %s", UI_Waveform_GetTrigModeLabel());
    GUI_SetFont(&GUI_Font20B_ASCII);
    GUI_SetColor(CLR_TEXT_MID);
    GUI_DispStringAt(buf, 54, y);
}

/** 触发电平（伏）映射到波形区 GUI Y，与 PlotBuf 波形及 MapY 同一垂直约定。 */
static int map_trig_volts_to_plot_y(float trig_v, float scale_min_v, float scale_max_v)
{
    int ty = main_plot_dso_y_to_gui(volts_to_plot_dso_y(trig_v, scale_min_v, scale_max_v));

    if (ty < PLOT_Y) {
        ty = PLOT_Y;
    }
    if (ty > PLOT_Y + PLOT_H - 1) {
        ty = PLOT_Y + PLOT_H - 1;
    }
    return ty;
}

/*
*********************************************************************************************************
*   函 数 名: draw_trig_level_at_y
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void draw_trig_level_at_y(int ty, uint32_t color, int label_dx, const char *tag)
{
    int x;

    GUI_SetColor(color);
    for (x = PLOT_X; x < PLOT_X + PLOT_W; x += 6) {
        int x2 = x + 3;
        if (x2 > PLOT_X + PLOT_W - 1) {
            x2 = PLOT_X + PLOT_W - 1;
        }
        GUI_DrawHLine(ty, x, x2);
    }
    {
        int arr_y = ty;
        if (arr_y < PLOT_Y + 4) {
            arr_y = PLOT_Y + 4;
        }
        if (arr_y > PLOT_Y + PLOT_H - 5) {
            arr_y = PLOT_Y + PLOT_H - 5;
        }
        draw_right_arrow(PLOT_X - 12, arr_y, 3, color);
        GUI_SetFont(&GUI_Font8_ASCII);
        GUI_DispStringAt(tag, PLOT_X + label_dx, arr_y - 4);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DrawTrigLevelLine
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void DrawTrigLevelLine(void)
{
    float trig_v;
    int ty1, ty2, show1, show2;

    if (!s_frame_valid) {
        return;
    }
    if (s_frame.trig_mode == DSO_UI_TRIG_AUTO) {
        return;
    }
    if (s_frame.trig_level_enabled == 0) {
        return;
    }

    trig_v = s_frame.trig_level_v;
    MainTask_ScopeChannelShowFlags(&show1, &show2);

    ty1 = map_trig_volts_to_plot_y(trig_v, s_frame.vert_scale_min_v, s_frame.vert_scale_max_v);
    ty2 = map_trig_volts_to_plot_y(trig_v, s_frame.ch2_vert_scale_min_v, s_frame.ch2_vert_scale_max_v);

    if (show1 != 0) {
        draw_trig_level_at_y(ty1, CLR_TRIG_LINE, -18, "T");
    }
    if (show2 != 0 && s_frame.ch2_present != 0) {
        if (show1 == 0 || ty2 != ty1) {
            uint32_t col = (show1 != 0) ? CLR_CH2 : CLR_TRIG_LINE;
            draw_trig_level_at_y(ty2, col, show1 ? -28 : -18, show1 ? "T2" : "T");
        }
    }
}

/* ── 光标绘制 ────────────────────────────────────────────────── */
#define CLR_CURSOR1  0xFFFF44u
/* 宏定义: CLR_CURSOR2 - 界面颜色常量。 */
#define CLR_CURSOR2  0x44BBFFu

/*
*********************************************************************************************************
*   函 数 名: DrawCursors
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void DrawCursors(void)
{
    UI_CursorMode_t cm = UI_Waveform_GetCursorMode();
    float t1, t2, v1, v2;

    if (cm == UI_CURSOR_OFF) {
        return;
    }

    UI_Waveform_GetTimeCursors(&t1, &t2);
    UI_Waveform_GetVoltCursors(&v1, &v2);

    if (cm == UI_CURSOR_TIME || cm == UI_CURSOR_BOTH) {
        int x1 = PLOT_X + (int)((float)(PLOT_W - 1) * t1 + 0.5f);
        int x2 = PLOT_X + (int)((float)(PLOT_W - 1) * t2 + 0.5f);
        int y;

        GUI_SetColor(CLR_CURSOR1);
        for (y = PLOT_Y; y < PLOT_Y + PLOT_H; y += 4) {
            int y2 = y + 2;
            if (y2 > PLOT_Y + PLOT_H - 1) y2 = PLOT_Y + PLOT_H - 1;
            GUI_DrawVLine(x1, y, y2);
        }
        GUI_SetColor(CLR_CURSOR2);
        for (y = PLOT_Y; y < PLOT_Y + PLOT_H; y += 4) {
            int y2 = y + 2;
            if (y2 > PLOT_Y + PLOT_H - 1) y2 = PLOT_Y + PLOT_H - 1;
            GUI_DrawVLine(x2, y, y2);
        }

        /* delta T readout */
        {
            float ms_div = UI_Waveform_GetTimePerDiv();
            float t_total_ms = (float)GRID_COL_DIV * ms_div;
            float dt_ms = (t2 - t1) * t_total_ms;
            char buf[32];

            if (dt_ms >= 1.0f || dt_ms <= -1.0f)
                (void)snprintf(buf, sizeof(buf), "dT=%.2fms", (double)dt_ms);
            else
                (void)snprintf(buf, sizeof(buf), "dT=%.1fus", (double)(dt_ms * 1000.0f));

            GUI_SetColor(CLR_CURSOR1);
            GUI_SetFont(&GUI_Font8_ASCII);
            GUI_DispStringAt(buf, PLOT_X + PLOT_W - 100, PLOT_Y + 3);
        }
    }

    if (cm == UI_CURSOR_VOLT || cm == UI_CURSOR_BOTH) {
        int dso_y1 = (PLOT_H - 1) - (int)((float)(PLOT_H - 1) * v1 + 0.5f);
        int dso_y2 = (PLOT_H - 1) - (int)((float)(PLOT_H - 1) * v2 + 0.5f);
        int y1 = main_plot_dso_y_to_gui(dso_y1);
        int y2 = main_plot_dso_y_to_gui(dso_y2);
        int x;

        GUI_SetColor(CLR_CURSOR1);
        for (x = PLOT_X; x < PLOT_X + PLOT_W; x += 4) {
            int x2e = x + 2;
            if (x2e > PLOT_X + PLOT_W - 1) x2e = PLOT_X + PLOT_W - 1;
            GUI_DrawHLine(y1, x, x2e);
        }
        GUI_SetColor(CLR_CURSOR2);
        for (x = PLOT_X; x < PLOT_X + PLOT_W; x += 4) {
            int x2e = x + 2;
            if (x2e > PLOT_X + PLOT_W - 1) x2e = PLOT_X + PLOT_W - 1;
            GUI_DrawHLine(y2, x, x2e);
        }

        /* delta V readout（按当前 solo：单 CH2 用 CH2 V/div，双通道取两路平均） */
        {
            float vdiv;
            UI_Waveform_Solo_t sol = UI_Waveform_GetSolo();
            if (sol == UI_WAVEFORM_SOLO_CH2) {
                vdiv = UI_Waveform_GetVoltPerDivCh2();
            } else if (sol == UI_WAVEFORM_SOLO_CH1) {
                vdiv = UI_Waveform_GetVoltPerDiv();
            } else {
                vdiv = 0.5f * (UI_Waveform_GetVoltPerDiv() + UI_Waveform_GetVoltPerDivCh2());
            }
            float v_total = (float)GRID_ROW_DIV * vdiv;
            float dv = (v2 - v1) * v_total;
            char buf[32];

            if (dv >= 1.0f || dv <= -1.0f)
                (void)snprintf(buf, sizeof(buf), "dV=%.2fV", (double)dv);
            else
                (void)snprintf(buf, sizeof(buf), "dV=%.0fmV", (double)(dv * 1000.0f));

            GUI_SetColor(CLR_CURSOR1);
            GUI_SetFont(&GUI_Font8_ASCII);
            GUI_DispStringAt(buf, PLOT_X + PLOT_W - 100, PLOT_Y + 14);
        }
    }
}

/* Header 状态条：静态旋转时仅 RUN/STOP、模式、档位等带状区在「状态未变」时跳过重绘；
 * 测量区/Trig 徽章仍每帧绘制并标记 Header dirty，避免与波形区合成错位、重叠闪烁。 */
#if USE_ROTATE_HEADER_STATIC_SKIP
/*
*********************************************************************************************************
*   函 数 名: MainTask_UpdateHeaderStatusBar
*   功能说明: 执行 emWin 主界面初始化、示波器页面绘制、帧刷新或显示诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void MainTask_UpdateHeaderStatusBar(void)
{
    static uint8_t s_hdr_tracked;
    static int s_prev_run;
    static UI_Waveform_DisplayMode_t s_prev_mode;
    static int s_prev_gui;
    static uint8_t s_prev_trig_mode;
    static uint8_t s_prev_recon_mode;
    static uint8_t s_prev_quality;
    static uint8_t s_prev_cal_state;
    static uint8_t s_prev_cal_progress;
    static uint8_t s_prev_cal_valid;
    static uint32_t s_prev_meas_sig;
    static int s_prev_knob_mode;
    static char s_prev_scale[24];
    static char s_prev_scale_ch2[24];
    static char s_prev_solo[8];
    static char s_prev_fs1[20];
    static char s_prev_fs2[20];
    static uint16_t s_prev_phy_sig_ch1;
    static uint16_t s_prev_phy_sig_ch2;
    static int s_prev_ch1_en;
    static int s_prev_ch2_en;

    char scale[24];
    char scale_ch2[24];
    char fs1[20], fs2[20];
    char mode_solo[20];
    uint32_t meas_sig;
    int run  = UI_Waveform_IsRunning() ? 1 : 0;
    UI_Waveform_DisplayMode_t mode = UI_Waveform_GetDisplayMode();
    int gui  = DMA2D_PlotBuf_IsReady() ? 1 : 0;
    const char *solo_lbl = UI_Waveform_GetSoloLabel();
    int knob_mode = UI_Nav_GetKnobMode();
    int ch1_en = UI_ChannelCtrl_GetEnabled(DSO_CHANNEL_1) ? 1 : 0;
    int ch2_en = UI_ChannelCtrl_GetEnabled(DSO_CHANNEL_2) ? 1 : 0;
    uint16_t phy_sig_ch1 = 0u;
    uint16_t phy_sig_ch2 = 0u;
    {
        DSO_VerticalInfo vi1 = {0};
        DSO_VerticalInfo vi2 = {0};
        DSO_Vertical_GetInfoForChannel(DSO_CHANNEL_1, &vi1);
        DSO_Vertical_GetInfoForChannel(DSO_CHANNEL_2, &vi2);
        phy_sig_ch1 = (uint16_t)(((unsigned)vi1.coupling & 3u) << 8) |
                      (uint16_t)((vi1.probe == DSO_PROBE_X10) ? 2u : 1u);
        phy_sig_ch2 = (uint16_t)(((unsigned)vi2.coupling & 3u) << 8) |
                      (uint16_t)((vi2.probe == DSO_PROBE_X10) ? 2u : 1u);
    }

    UI_Waveform_FormatScaleStr(scale, sizeof(scale));
    UI_Waveform_FormatScaleStrCh2(scale_ch2, sizeof(scale_ch2));
    UI_Waveform_FormatSampleRateHz(UI_Waveform_GetEffectiveSampleRateHz(), fs1, sizeof(fs1));
    UI_Waveform_FormatSampleRateHz(UI_Waveform_GetCh2SampleRateHz(), fs2, sizeof(fs2));
    (void)snprintf(mode_solo, sizeof(mode_solo), "%s %s",
                     mode == UI_WAVEFORM_MODE_ROLL ? "Roll" : "Y-T", solo_lbl);
    {
        meas_sig = ((uint32_t)(s_frame.freq_hz + 0.5f) & 0xFFFFFu) ^
                   ((uint32_t)s_frame.vpp << 3) ^
                   ((uint32_t)(s_frame.vrms + 0.5f) << 11) ^
                   ((uint32_t)(s_frame.ch2_freq_hz + 0.5f) << 1) ^
                   ((uint32_t)s_frame.ch2_vpp << 4) ^
                   ((uint32_t)(s_frame.ch2_vrms + 0.5f) << 13) ^
                   ((uint32_t)s_frame.ch2_vert_mv_per_div << 5) ^
                   ((uint32_t)(s_frame.ext_freq_valid ? 1u : 0u) << 23) ^
                   ((uint32_t)s_frame.ext_freq_state << 24) ^
                   ((uint32_t)s_frame.ext_freq_mode << 28) ^
                   (UI_FreqFormat_FoldMilliHz(s_frame.ext_freq_millihz) << 1);
    }

    if (s_hdr_tracked) {
        if (run == s_prev_run && mode == s_prev_mode && gui == s_prev_gui &&
            knob_mode == s_prev_knob_mode &&
            strcmp(scale, s_prev_scale) == 0 && strcmp(scale_ch2, s_prev_scale_ch2) == 0 &&
            strcmp(solo_lbl, s_prev_solo) == 0 &&
            strcmp(fs1, s_prev_fs1) == 0 && strcmp(fs2, s_prev_fs2) == 0 &&
            s_prev_trig_mode == s_frame.trig_mode &&
            s_prev_recon_mode == s_frame.recon_mode &&
            s_prev_quality == s_frame.display_quality &&
            s_prev_cal_state == s_frame.cal_state &&
            s_prev_cal_progress == s_frame.cal_progress_pct &&
            s_prev_cal_valid == (uint8_t)(s_frame.ch1_interleave_cal_valid ? 1u : 0u) &&
            s_prev_meas_sig == meas_sig &&
            s_prev_phy_sig_ch1 == phy_sig_ch1 &&
            s_prev_phy_sig_ch2 == phy_sig_ch2 &&
            s_prev_ch1_en == ch1_en &&
            s_prev_ch2_en == ch2_en) {
            /* 仍可跳过旋钮/RUN/档位等「带状」重绘，但测量区/Trig 徽章须每帧写入逻辑缓冲并标记 Header dirty，
             * 否则波形区每帧更新而 Header 不显式重载时会与 Ping-Pong+PlotBuf 合成错位，表现为重叠闪烁。 */
            DrawTrigBadge();
            DrawMeasurements();
            LCDConf_MarkHeaderDirty();
            return;
        }
    }
    s_hdr_tracked = 1u;
    s_prev_run      = run;
    s_prev_mode     = mode;
    s_prev_gui      = gui;
    s_prev_knob_mode = knob_mode;
    s_prev_trig_mode = s_frame.trig_mode;
    s_prev_recon_mode = s_frame.recon_mode;
    s_prev_quality = s_frame.display_quality;
    s_prev_cal_state = s_frame.cal_state;
    s_prev_cal_progress = s_frame.cal_progress_pct;
    s_prev_cal_valid = (uint8_t)(s_frame.ch1_interleave_cal_valid ? 1u : 0u);
    s_prev_meas_sig = meas_sig;
    strncpy(s_prev_scale, scale, sizeof(s_prev_scale) - 1u);
    s_prev_scale[sizeof(s_prev_scale) - 1u] = '\0';
    strncpy(s_prev_scale_ch2, scale_ch2, sizeof(s_prev_scale_ch2) - 1u);
    s_prev_scale_ch2[sizeof(s_prev_scale_ch2) - 1u] = '\0';
    strncpy(s_prev_solo, solo_lbl, sizeof(s_prev_solo) - 1u);
    s_prev_solo[sizeof(s_prev_solo) - 1u] = '\0';
    strncpy(s_prev_fs1, fs1, sizeof(s_prev_fs1) - 1u);
    s_prev_fs1[sizeof(s_prev_fs1) - 1u] = '\0';
    strncpy(s_prev_fs2, fs2, sizeof(s_prev_fs2) - 1u);
    s_prev_fs2[sizeof(s_prev_fs2) - 1u] = '\0';
    s_prev_phy_sig_ch1 = phy_sig_ch1;
    s_prev_phy_sig_ch2 = phy_sig_ch2;
    s_prev_ch1_en = ch1_en;
    s_prev_ch2_en = ch2_en;

    /* 旋钮模式 badge（左上，与 DrawHeader 标题条一致） */
    {
        const char *ml = UI_Nav_GetKnobModeLabel();
        uint32_t mbg = (knob_mode == 0) ? 0x183018u : 0x302818u;
        uint32_t mfg = (knob_mode == 0) ? 0x66DD66u : 0xDDAA44u;
        int tmy = (3 + HDR_TITLE_BTN_B - GUI_GetFontDistY()) / 2;
        if (tmy < 3) {
            tmy = 3;
        }
        GUI_SetColor(mbg);
        GUI_FillRoundedRect(8, 3, 88, HDR_TITLE_BTN_B, 4);
        GUI_SetColor(mfg);
        GUI_DrawRoundedRect(8, 3, 88, HDR_TITLE_BTN_B, 4);
        GUI_SetFont(&GUI_Font16_ASCII);
        GUI_DispStringAt(ml, 14, tmy);
    }

    GUI_SetColor(CLR_RUN_BG);
    GUI_FillRoundedRect(DISP_W - 78, 3, DISP_W - 8, HDR_TITLE_BTN_B, 4);
    GUI_SetColor(CLR_BORDER_IN);
    GUI_DrawRoundedRect(DISP_W - 78, 3, DISP_W - 8, HDR_TITLE_BTN_B, 4);
    GUI_SetColor(UI_Waveform_IsRunning() ? CLR_RUN_FG : CLR_STOP_FG);
    {
        int rmy = (3 + HDR_TITLE_BTN_B) / 2;
        GUI_FillCircle(DISP_W - 62, rmy, 4);
    }
    GUI_SetFont(&GUI_Font16_ASCII);
    {
        int rty = (3 + HDR_TITLE_BTN_B - GUI_GetFontDistY()) / 2;
        if (rty < 3) {
            rty = 3;
        }
        GUI_DispStringAt(UI_Waveform_IsRunning() ? "RUN" : "STOP", DISP_W - 54, rty);
    }

    {
        int step, y_ch1, y_ch2;
        DSO_VerticalInfo vi1 = {0};
        DSO_VerticalInfo vi2 = {0};

        step  = hdr_status_line_step();
        y_ch1 = hdr_status_y_ch1();
        y_ch2 = hdr_status_y_ch2(step);
        DSO_Vertical_GetInfoForChannel(DSO_CHANNEL_1, &vi1);
        DSO_Vertical_GetInfoForChannel(DSO_CHANNEL_2, &vi2);
        draw_ch1_badge_with_phy(&vi1, y_ch1);
        draw_ch2_badge_with_phy(&vi2, y_ch2);

        GUI_SetFont(&GUI_Font20B_ASCII);
        GUI_SetColor(CLR_BG_INFO);
        GUI_FillRect(54, y_ch1, 380, HDR_H - 3);
        GUI_SetColor(CLR_TEXT_MID);
        GUI_DispStringAt(scale, 54, y_ch1 + 1);
        {
            int sl = (int)strlen(fs1);
            (void)snprintf(fs1 + sl, sizeof(fs1) - (size_t)sl, "  %s", mode_solo);
        }
        GUI_DispStringAt(fs1, 54, y_ch1 + 1 + step);
        GUI_SetColor(CLR_BG_INFO);
        GUI_FillRect(54, y_ch2, 380, HDR_H - 3);
        GUI_SetColor(CLR_TEXT_MID);
        GUI_DispStringAt(scale_ch2, 54, y_ch2 + 1);
        GUI_DispStringAt(fs2, 54, y_ch2 + 1 + step);
    }

    DrawTrigBadge();
    DrawMeasurements();

    LCDConf_MarkHeaderDirty();
}
#endif

/* ── 示波器整页布局（从其它演示页切回波形页时须重画）──────────────── */
static void MainTask_Scope_RestoreLayout(void)
{
    GUI_SetColor(CLR_BG_SCREEN);
    GUI_FillRect(0, 0, DISP_W - 1, DISP_H - 1);
    DrawHeader();
    if (PLOT_Y > HDR_H + 1) {
        GUI_SetColor(CLR_BG_SCREEN);
        GUI_FillRect(0, HDR_H + 1, DISP_W - 1, PLOT_Y - 1);
    }
    GUI_SetColor(CLR_BG_PLOT);
    GUI_FillRect(0, PLOT_Y, PLOT_X - 1, PLOT_Y + PLOT_H - 1);
    if (PLOT_X + PLOT_W < DISP_W)
        GUI_FillRect(PLOT_X + PLOT_W, PLOT_Y, DISP_W - 1, PLOT_Y + PLOT_H - 1);
    if (PLOT_Y + PLOT_H < DISP_H)
        GUI_FillRect(0, PLOT_Y + PLOT_H, DISP_W - 1, DISP_H - 1);
    DrawYAxisTicks();
    DrawGridFull();
    DrawScopeSideMenu();
    LCDConf_RequestFullScreenRotate();
    ManualRotateToPhysical();
}

/* Body dirty：侧栏、游标、触发/时基/垂直参数变化时才补齐物理后台缓冲。 */
typedef struct {
    UI_ScopeUiMode_t uim;
    int menu_focus;
    UI_CursorMode_t cursor_mode;
    float time_per_div;
    float volt_per_div;
    float volt_per_div_ch2;
    float time_cursor1;
    float time_cursor2;
    float volt_cursor1;
    float volt_cursor2;
    uint8_t trig_mode;
    uint8_t trig_source;
    uint8_t trig_level_enabled;
    float trig_level_v;
    uint8_t ch1_enabled;
    uint8_t ch2_enabled;
    uint8_t solo;
    uint8_t display_mode;
    uint8_t ch2_present;
    float vert_scale_min_v;
    float vert_scale_max_v;
    float ch2_vert_scale_min_v;
    float ch2_vert_scale_max_v;
} ScopeBodyDirtyState;

static ScopeBodyDirtyState s_scope_body_dirty_prev;
static uint8_t            s_scope_body_dirty_prev_valid;

/*
*********************************************************************************************************
*   函 数 名: scope_reset_body_dirty_track
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void scope_reset_body_dirty_track(void)
{
    s_scope_body_dirty_prev_valid = 0u;
    LCDConf_MarkBodyDirty();
}

/*
*********************************************************************************************************
*   函 数 名: scope_mark_body_dirty_if_state_changed
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void scope_mark_body_dirty_if_state_changed(void)
{
    ScopeBodyDirtyState cur = {0};
    float t1, t2, v1, v2;

    cur.uim = UI_Nav_GetScopeUiMode();
    cur.menu_focus = UI_Nav_GetMenuFocus();
    cur.cursor_mode = UI_Waveform_GetCursorMode();
    cur.time_per_div = UI_Waveform_GetTimePerDiv();
    cur.volt_per_div = UI_Waveform_GetVoltPerDiv();
    cur.volt_per_div_ch2 = UI_Waveform_GetVoltPerDivCh2();
    UI_Waveform_GetTimeCursors(&t1, &t2);
    UI_Waveform_GetVoltCursors(&v1, &v2);
    cur.time_cursor1 = t1;
    cur.time_cursor2 = t2;
    cur.volt_cursor1 = v1;
    cur.volt_cursor2 = v2;
    cur.trig_mode = s_frame.trig_mode;
    cur.trig_source = s_frame.trig_source;
    cur.trig_level_enabled = s_frame.trig_level_enabled ? 1U : 0U;
    cur.trig_level_v = s_frame.trig_level_v;
    cur.ch1_enabled = UI_ChannelCtrl_GetEnabled(DSO_CHANNEL_1) ? 1U : 0U;
    cur.ch2_enabled = UI_ChannelCtrl_GetEnabled(DSO_CHANNEL_2) ? 1U : 0U;
    cur.solo = (uint8_t)UI_Waveform_GetSolo();
    cur.display_mode = (uint8_t)UI_Waveform_GetDisplayMode();
    cur.ch2_present = s_frame.ch2_present ? 1U : 0U;
    cur.vert_scale_min_v = s_frame.vert_scale_min_v;
    cur.vert_scale_max_v = s_frame.vert_scale_max_v;
    cur.ch2_vert_scale_min_v = s_frame.ch2_vert_scale_min_v;
    cur.ch2_vert_scale_max_v = s_frame.ch2_vert_scale_max_v;

    if (s_scope_body_dirty_prev_valid == 0U ||
        memcmp(&s_scope_body_dirty_prev, &cur, sizeof(cur)) != 0) {
        LCDConf_MarkBodyDirty();
        s_scope_body_dirty_prev       = cur;
        s_scope_body_dirty_prev_valid = 1u;
    }
}

/** RUN 模式下示波器帧周期：普通时基用 MAINTASK_FRAME_PERIOD_MS，FAST 时基可缩短。 */
static uint32_t MainTask_ScopeFramePeriodMs(void)
{
    uint32_t period_ms = MAINTASK_FRAME_PERIOD_MS;
    uint8_t fast_display = 0u;

    if (UI_Waveform_IsRunning() &&
        s_frame_valid &&
        (s_frame.tb_mode == DSO_UI_TB_MODE_FAST) &&
        (s_frame.tb_sec_div_ns <= MAINTASK_SCOPE_USE_FAST_PERIOD_MAX_SEC_DIV_NS)) {
        period_ms = MAINTASK_FAST_FRAME_PERIOD_MS;
        fast_display = 1u;
    }
    if (period_ms == 0u) {
        period_ms = 1u;
    }
    s_maintask_scope_frame_period_ms = period_ms;
    s_maintask_fast_display_active = fast_display;
    return period_ms;
}

/* ── 示波器单帧渲染（与原先 MainTask while(1) 内主体一致）────────── */
/* 帧内 fu 仍高时：可再做工序级 dirty（头栏/波形区仅脏时重绘），须与缓冲索引一致；当前用 MAINTASK_AB_DISABLE_PLOTBUF_WAVEFORM 做 PlotBuf A/B。 */
#if (Y_T_AB_FREEZE_SCOPE_AFTER_FIRST_FRAME != 0)
static uint8_t s_scope_freeze_frame_done;

#if (Y_T_AB_FREEZE_MONITOR_PHYSICAL_FRAME != 0)
static uint32_t s_scope_freeze_sig_ref;
static uint32_t s_scope_freeze_sig_last;
static uint32_t s_scope_freeze_sig_change_count;
static uint8_t  s_scope_freeze_sig_valid;

/*
*********************************************************************************************************
*   函 数 名: MainTask_ScopeFreeze_PhysSignature
*   功能说明: 执行 emWin 主界面初始化、示波器页面绘制、帧刷新或显示诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t MainTask_ScopeFreeze_PhysSignature(uint32_t phys_addr)
{
    const uint32_t *p = (const uint32_t *)phys_addr;
    uint32_t words = VRAM_PHYS_FRAME_BYTES / sizeof(uint32_t);
    uint32_t step = 257U;
    uint32_t sig = 2166136261UL;
    uint32_t i;

    SCB_InvalidateDCache_by_Addr((uint32_t *)phys_addr, (int32_t)VRAM_PHYS_FRAME_BYTES);
    for (i = 0U; i < words; i += step) {
        sig ^= p[i] + 0x9E3779B9UL + (sig << 6) + (sig >> 2);
    }
    sig ^= p[0];
    sig ^= p[words - 1U] << 1;
    return sig;
}

/*
*********************************************************************************************************
*   函 数 名: MainTask_ScopeFreeze_MonitorPhysicalFrame
*   功能说明: 执行 emWin 主界面初始化、示波器页面绘制、帧刷新或显示诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void MainTask_ScopeFreeze_MonitorPhysicalFrame(void)
{
    static TickType_t s_last_monitor_tick;
    TickType_t now = xTaskGetTickCount();
    uint32_t phys_addr;
    uint32_t sig;

    if ((TickType_t)(now - s_last_monitor_tick) < pdMS_TO_TICKS(2000U)) {
        return;
    }
    s_last_monitor_tick = now;

    phys_addr = LTDC_Layer1->CFBAR;
    if (phys_addr != VRAM_PHYSICAL_0_ADDR && phys_addr != VRAM_PHYSICAL_1_ADDR) {
        printf("[LCD-FREEZE] unexpected CFBAR=0x%08lX\r\n", (unsigned long)phys_addr);
        return;
    }

    sig = MainTask_ScopeFreeze_PhysSignature(phys_addr);
    if (s_scope_freeze_sig_valid == 0U) {
        s_scope_freeze_sig_ref = sig;
        s_scope_freeze_sig_valid = 1U;
    } else if (sig != s_scope_freeze_sig_last) {
        s_scope_freeze_sig_change_count++;
    }
    s_scope_freeze_sig_last = sig;

    printf("[LCD-FREEZE] CFBAR=0x%08lX sig=0x%08lX ref=0x%08lX changes=%lu fu=%lu vsync_to=%lu\r\n",
           (unsigned long)phys_addr,
           (unsigned long)sig,
           (unsigned long)s_scope_freeze_sig_ref,
           (unsigned long)s_scope_freeze_sig_change_count,
           (unsigned long)g_lcdconf_ltdc_fu_cnt,
           (unsigned long)g_lcdconf_vsync_timeout_cnt);
}
#endif

/*
*********************************************************************************************************
*   函 数 名: MainTask_Scope_RenderFrame_FrozenIdle
*   功能说明: 执行 emWin 主界面初始化、示波器页面绘制、帧刷新或显示诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void MainTask_Scope_RenderFrame_FrozenIdle(void)
{
#if (Y_T_AB_FREEZE_MONITOR_PHYSICAL_FRAME != 0)
    MainTask_ScopeFreeze_MonitorPhysicalFrame();
#endif
    mtp_tick_end_of_frame();
}
#endif

/**
 * 示波器单帧完整渲染：拉帧 → emWin 头栏/叠加层 → PlotBuf 或 GUI 波形 → RenderLatch 提交。
 * 由 MainTask 主循环在 UI_SCR_SCOPE（或纯示波器模式）下周期调用。
 */
static void MainTask_Scope_RenderFrame(void)
{
#if (Y_T_AB_FREEZE_SCOPE_AFTER_FIRST_FRAME != 0)
    if (s_scope_freeze_frame_done != 0U) {
        MainTask_Scope_RenderFrame_FrozenIdle();
        return;
    }
#endif
#if (Y_T_AB_CLEAN_DCACHE_BEFORE_SCOPE_FRAME != 0)
    SCB_CleanDCache();
#endif
    mtp_mark_start();
    LCDConf_RenderLatch_MarkScopeFrameFuStart();
    UpdateWaveformData();
    if (s_frame_valid) {
        RollHistory_ApplyRollCoherentPrefill(&s_frame);
    }
    mtp_acc(0);

    GUI_MULTIBUF_Begin();
    GUI_SetTextMode(GUI_TM_TRANS);

#if USE_ROTATE_HEADER_STATIC_SKIP
    MainTask_UpdateHeaderStatusBar();
#else
    {
        GUI_SetColor(CLR_RUN_BG);
        GUI_FillRoundedRect(DISP_W - 78, 3, DISP_W - 8, HDR_TITLE_BTN_B, 4);
        GUI_SetColor(CLR_BORDER_IN);
        GUI_DrawRoundedRect(DISP_W - 78, 3, DISP_W - 8, HDR_TITLE_BTN_B, 4);
        GUI_SetColor(UI_Waveform_IsRunning() ? CLR_RUN_FG : CLR_STOP_FG);
        {
            int rmy = (3 + HDR_TITLE_BTN_B) / 2;
            GUI_FillCircle(DISP_W - 62, rmy, 4);
        }
        GUI_SetFont(&GUI_Font16_ASCII);
        {
            int rty = (3 + HDR_TITLE_BTN_B - GUI_GetFontDistY()) / 2;
            if (rty < 3) {
                rty = 3;
            }
            GUI_DispStringAt(UI_Waveform_IsRunning() ? "RUN" : "STOP", DISP_W - 54, rty);
        }
    }
    {
        int step, y_ch1, y_ch2;
        char scale_str[24];
        char scale_str_ch2[24];
        char fs_ch1[20], fs_ch2[20];
        char mode_solo_nl[20];
        DSO_VerticalInfo vi1 = {0};
        DSO_VerticalInfo vi2 = {0};

        step = hdr_status_line_step();
        y_ch1 = hdr_status_y_ch1();
        y_ch2 = hdr_status_y_ch2(step);
        DSO_Vertical_GetInfoForChannel(DSO_CHANNEL_1, &vi1);
        DSO_Vertical_GetInfoForChannel(DSO_CHANNEL_2, &vi2);
        draw_ch1_badge_with_phy(&vi1, y_ch1);
        draw_ch2_badge_with_phy(&vi2, y_ch2);

        UI_Waveform_FormatScaleStr(scale_str, sizeof(scale_str));
        UI_Waveform_FormatScaleStrCh2(scale_str_ch2, sizeof(scale_str_ch2));
        UI_Waveform_FormatSampleRateHz(UI_Waveform_GetEffectiveSampleRateHz(), fs_ch1, sizeof(fs_ch1));
        UI_Waveform_FormatSampleRateHz(UI_Waveform_GetCh2SampleRateHz(), fs_ch2, sizeof(fs_ch2));
        (void)snprintf(mode_solo_nl, sizeof(mode_solo_nl), "%s %s",
                       UI_Waveform_GetDisplayMode() == UI_WAVEFORM_MODE_ROLL ? "Roll" : "Y-T",
                       UI_Waveform_GetSoloLabel());
        GUI_SetFont(&GUI_Font20B_ASCII);
        GUI_SetColor(CLR_BG_INFO);
        GUI_FillRect(54, y_ch1, 380, HDR_H - 3);
        GUI_SetColor(CLR_TEXT_MID);
        GUI_DispStringAt(scale_str, 54, y_ch1 + 1);
        {
            int sl = (int)strlen(fs_ch1);
            (void)snprintf(fs_ch1 + sl, sizeof(fs_ch1) - (size_t)sl, "  %s", mode_solo_nl);
        }
        GUI_DispStringAt(fs_ch1, 54, y_ch1 + 1 + step);
        GUI_SetColor(CLR_BG_INFO);
        GUI_FillRect(54, y_ch2, 380, HDR_H - 3);
        GUI_SetColor(CLR_TEXT_MID);
        GUI_DispStringAt(scale_str_ch2, 54, y_ch2 + 1);
        GUI_DispStringAt(fs_ch2, 54, y_ch2 + 1 + step);
    }
    DrawTrigBadge();
    DrawMeasurements();
#endif
    mtp_acc(1);

    {
        LCD_RenderLatchFramePlan rl_plan;
        int use_plotbuf;
        /* 每帧只判定一次，绘制路径与 RenderLatch 提交路径共用同一份计划。 */
#if MAINTASK_AB_DISABLE_PLOTBUF_WAVEFORM
        rl_plan.use_plotbuf = 0U;
#else
        rl_plan.use_plotbuf = DMA2D_PlotBuf_IsReady() ? 1U : 0U;
#endif
        use_plotbuf = (rl_plan.use_plotbuf != 0U) ? 1 : 0;

        if (use_plotbuf != 0) {
            DMA2D_PlotBuf_RenderScopeLock();
        }

        if (use_plotbuf != 0) {
            DMA2D_PlotBuf_StartFrame();
        }
        mtp_acc(2);

    if (use_plotbuf != 0) {
        DrawWaveformsToBuf();
    } else {
        GUI_SetColor(CLR_BG_PLOT);
        GUI_FillRect(PLOT_X, PLOT_Y, PLOT_X + PLOT_W - 1, PLOT_Y + PLOT_H - 1);
        DrawGridLines();
        DrawWaveforms();
    }
    mtp_acc(3);

    if (use_plotbuf != 0) {
        GUI_SetColor(CLR_BG_PLOT);
        GUI_FillRect(PLOT_X - 2, PLOT_Y - 2, PLOT_X + PLOT_W + 1, PLOT_Y + PLOT_H + 1);
        DrawYAxisTicks();
    } else {
        GUI_SetFont(&GUI_Font8_ASCII);
        GUI_SetColor(CLR_CH1);
        GUI_DispStringAt("CH1", PLOT_X + PLOT_W - 26,
                         compute_channel_ground_y(s_frame.vert_scale_min_v,
                                                  s_frame.vert_scale_max_v) - 5);
        GUI_SetColor(CLR_CH2);
        GUI_DispStringAt("CH2", PLOT_X + PLOT_W - 26,
                         compute_channel_ground_y(s_frame.ch2_vert_scale_min_v,
                                                  s_frame.ch2_vert_scale_max_v) - 5);
        GUI_SetColor(GUI_MAKE_COLOR(0xFF6600));
        GUI_DispStringAt("GUI", PLOT_X + 4, PLOT_Y + PLOT_H - 12);
    }
    DrawTrigLevelLine();
    DrawCursors();
    DrawGroundRefMarkers();
    DrawScopeSideMenu();
    /* 高速显示降压：默认不每帧旋 Body，避免大块 CPU 旋转继续压 SDRAM。 */
#if MAINTASK_PLOTBUF_ROTATE_BODY_EVERY_FRAME
    if (use_plotbuf != 0) {
        LCDConf_MarkBodyDirty();
    }
#endif
    mtp_acc(4);

    GUI_MULTIBUF_End();

        if (use_plotbuf != 0) {
            DMA2D_PlotBuf_EndFrame();
        }
        mtp_acc(5);

        scope_mark_body_dirty_if_state_changed();
        /* 与 dma2d_wave.h 约定：DMA2D 从 PlotBuf 读到物理屏前，须等待总线、Clean+Invalidate 当前绘制块 */
        if (use_plotbuf != 0) {
            DMA2D_PlotBuf_PreDisplaySync();
        }
#if (Y_T_AB_CLEANINV_LOGICAL_VRAM_BEFORE_LATCH != 0)
        SCB_CleanInvalidateDCache_by_Addr((uint32_t *)VRAM_LOGICAL_ADDR,
                                          (int32_t)VRAM_LOGICAL_SUBFRAME_BYTES);
        __DMB();
#endif
        LCDConf_RenderLatch_PreparePhysical(&rl_plan);
#if LCDCONF_RENDER_LATCH_YIELD_BEFORE_COMMIT
        taskYIELD();
#endif
        LCDConf_RenderLatch_Commit();
        mtp_acc(6);

        if (use_plotbuf != 0) {
            DMA2D_PlotBuf_RenderScopeUnlock();
        }
    }

    mtp_tick_end_of_frame();
#if (Y_T_AB_FREEZE_SCOPE_AFTER_FIRST_FRAME != 0)
    s_scope_freeze_frame_done = 1U;
#endif
}

/* ── MainTask ──────────────────────────────────────────────── */
/**
 * FreeRTOS 入口：GUI_Init 后进入无限循环。
 *
 * @note UI_APP_DEMO_CYCLE_SCREENS
 *   - 1：欢迎/通道/触发/测量/示波器等多页 Demo，按 UI_Nav_GetScreen 分支渲染。
 *   - 0：仅示波器页，while(1) 直接调用 MainTask_Scope_RenderFrame。
 *
 * 调度要点：RUN 用 vTaskDelayUntil 保帧率；STOP 用 vTaskDelay(50) 省电；
 * 非示波器页用 UI_DEMO_IDLE_POLL_MS 事件驱动重绘。
 */
void MainTask(void)
{
    /* vTaskStartScheduler 之后 main 不再打印；此处确认 emWin 任务已调度 */
    APP_INFO_PRINTF("[APP] MainTask started\r\n");

    __HAL_RCC_CRC_CLK_ENABLE();
    GUI_Init();
    APP_INFO_PRINTF("[APP] GUI_Init OK\r\n");
    GUI_SetTextMode(GUI_TM_TRANS);

#if UI_APP_DEMO_CYCLE_SCREENS
    {
        TickType_t t_screen_start   = xTaskGetTickCount();
        UI_ScreenId_t sid           = UI_SCR_WELCOME;
        TickType_t frame_wake_scope = xTaskGetTickCount();
        UI_ScreenId_t prev_sid     = UI_SCR_SYSTEM; /* != SCOPE：首帧从欢迎页开始不触发波形布局 */
        uint8_t plotbuf_attempted   = 0u;

        UI_Waveform_Init();

        /* 与 sid 初值一致；循环内不再每帧 UI_Nav_SetScreen(sid)，否则会覆盖编码器在 UI_Nav_Poll 里更新的界面 */
        UI_Nav_SetScreen(UI_SCR_WELCOME);
        UI_EcPollTask_Start();

        for (;;) {
            TickType_t now = xTaskGetTickCount();
#if (Y_T_AB_DISABLE_MAINTASK_PERIODIC_LOG == 0) && \
    (DSO_VPP_DIAG_ENABLE == 0) && \
    (MAINTASK_STACK_LOG_INTERVAL_MS > 0U) && \
    (INCLUDE_uxTaskGetStackHighWaterMark == 1)
            MainTask_LogStackWatermarkIfDue();
#endif
#if UI_DEMO_AUTO_ADVANCE
            if ((now - t_screen_start) >= pdMS_TO_TICKS(UI_DEMO_SCREEN_MS)) {
                t_screen_start = now;
                sid = (UI_ScreenId_t)(((int)sid + 1) % (int)UI_SCR_COUNT);
                UI_Nav_SetScreen(sid);
            }
#endif

            sid = UI_Nav_GetScreen();

            /* 示波器 + 自动测量页需要帧内测量值；其它菜单页关流以省 CPU */
            UI_Waveform_SetStreamActive((sid == UI_SCR_SCOPE || sid == UI_SCR_MEASURE) ? 1 : 0);

            if (prev_sid == UI_SCR_SCOPE && sid != UI_SCR_SCOPE) {
                scope_reset_body_dirty_track();
            }

            if (sid == UI_SCR_SCOPE) {
                if (prev_sid != UI_SCR_SCOPE) {
                    /* 再次进入示波器：PlotBuf 双缓冲/异步恢复与菜单页 DMA2D 交错后易失步，先同步再强制整屏旋一次 body */
                    scope_reset_body_dirty_track();
                    if (DMA2D_PlotBuf_IsReady()) {
                        DMA2D_PlotBuf_RenderScopeLock();
                    }
                    if (plotbuf_attempted != 0u && DMA2D_PlotBuf_IsReady()) {
                        DMA2D_PlotBuf_ResyncOnScopeEnter();
                    }
                    MainTask_Scope_RestoreLayout();
                    if (DMA2D_PlotBuf_IsReady()) {
                        DMA2D_PlotBuf_RenderScopeUnlock();
                    }
                    frame_wake_scope = xTaskGetTickCount();
                    if (!plotbuf_attempted) {
                        plotbuf_attempted = 1u;
                        if (!DMA2D_PlotBuf_Init()) {
#if !UI_LIGHT_PATH_RELAX
                            Error_Handler(__FILE__, __LINE__);
#endif
                        }
#if MAINTASK_PROFILE_EMWIN_SEGMENTS
                        APP_INFO_PRINTF("\r\n[MTP] MainTask profile ON, print every %u frames, see [6a]/[6b]\r\n",
                                        (unsigned)MAINTASK_PROFILE_ACCUM_FRAMES);
#endif
                    }
                }
                if (!UI_Waveform_IsRunning()) {
                    vTaskDelay(pdMS_TO_TICKS(50));
                    frame_wake_scope = xTaskGetTickCount();
                }
                MainTask_Scope_RenderFrame();
                if (UI_Waveform_IsRunning()) {
                    vTaskDelayUntil(&frame_wake_scope,
                                    pdMS_TO_TICKS(MainTask_ScopeFramePeriodMs()));
                }
            } else {
                int need_redraw = (prev_sid != sid) ? 1 : 0;
                if (UI_Nav_ConsumeRedraw()) {
                    need_redraw = 1;
                }
                /* 测量页不走 Scope 渲染链，须在此拉取 DSO_Frame；否则 s_display_frame 不更新、数值冻结 */
                if (sid == UI_SCR_MEASURE && UpdateWaveformData()) {
                    need_redraw = 1;
                }
                if (need_redraw != 0) {
                    uint32_t tick_ms = (uint32_t)((now - t_screen_start) * portTICK_PERIOD_MS);
                    GUI_MULTIBUF_Begin();
                    GUI_SetTextMode(GUI_TM_TRANS);
                    UI_Screen_Draw(sid, tick_ms);
                    GUI_MULTIBUF_End();
                    LCDConf_MarkFullDirty();
                    if (DMA2D_PlotBuf_IsReady()) {
                        DMA2D_PlotBuf_RenderScopeLock();
                    }
                    ManualRotateToPhysical();
                    if (DMA2D_PlotBuf_IsReady()) {
                        DMA2D_PlotBuf_RenderScopeUnlock();
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(UI_DEMO_IDLE_POLL_MS));
            }
            prev_sid = sid;
        }
    }
#else

    UI_Waveform_Init();
    UI_EcPollTask_Start();

    if (DMA2D_PlotBuf_IsReady()) {
        DMA2D_PlotBuf_RenderScopeLock();
    }
    MainTask_Scope_RestoreLayout();
    if (DMA2D_PlotBuf_IsReady()) {
        DMA2D_PlotBuf_RenderScopeUnlock();
    }

    if (!DMA2D_PlotBuf_Init()) {
#if !UI_LIGHT_PATH_RELAX
        Error_Handler(__FILE__, __LINE__);
#else
        /* UI_LIGHT_PATH_RELAX=1：允许回退 GUI 绘制路径 */
#endif
    }

#if MAINTASK_PROFILE_EMWIN_SEGMENTS
    APP_INFO_PRINTF("\r\n[MTP] MainTask profile ON, print every %u frames, see [6a]/[6b]\r\n",
                    (unsigned)MAINTASK_PROFILE_ACCUM_FRAMES);
#endif

    {
        TickType_t frame_wake = xTaskGetTickCount();

        while (1) {
#if (Y_T_AB_DISABLE_MAINTASK_PERIODIC_LOG == 0) && \
    (DSO_VPP_DIAG_ENABLE == 0) && \
    (MAINTASK_STACK_LOG_INTERVAL_MS > 0U) && \
    (INCLUDE_uxTaskGetStackHighWaterMark == 1)
            MainTask_LogStackWatermarkIfDue();
#endif
            if (!UI_Waveform_IsRunning()) {
                vTaskDelay(pdMS_TO_TICKS(50));
                frame_wake = xTaskGetTickCount();
            }
#if (Y_T_AB_WRAP_SCOPE_RENDER_WITH_GUI_LOCK != 0)
            GUI_LOCK();
#endif
            MainTask_Scope_RenderFrame();
#if (Y_T_AB_WRAP_SCOPE_RENDER_WITH_GUI_LOCK != 0)
            GUI_UNLOCK();
#endif
            if (UI_Waveform_IsRunning()) {
                vTaskDelayUntil(&frame_wake,
                                pdMS_TO_TICKS(MainTask_ScopeFramePeriodMs()));
            }
        }
    }
#endif /* !UI_APP_DEMO_CYCLE_SCREENS */
}
