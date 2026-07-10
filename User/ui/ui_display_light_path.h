/**
 * @file ui_display_light_path.h
 * @brief 轻显示路径编译约束：PlotBuf + Header 静态跳过。
 */

/*
 * 轻显示路径（部分旋转 + PlotBuf + Header/Body 静态跳过）
 * ─────────────────────────────────────────────────────────────
 * 思路一：编译期强制宏组合；运行期强制 PlotBuf 初始化成功（可关）。
 *
 * 排障时（临时关掉 PlotBuf / 整屏旋转对比）：在工程预处理器增加
 *   UI_LIGHT_PATH_RELAX=1
 * 将关闭本头文件内的 #error 以及 MainTask 里 Init 失败的 Error_Handler。
 */
#ifndef UI_DISPLAY_LIGHT_PATH_H
/* 宏定义: UI_DISPLAY_LIGHT_PATH_H - 头文件保护宏。 */
#define UI_DISPLAY_LIGHT_PATH_H

#ifndef UI_LIGHT_PATH_RELAX
/* 宏定义: UI_LIGHT_PATH_RELAX - 当前模块使用的配置常量。 */
#define UI_LIGHT_PATH_RELAX  0
#endif

#include "dma2d_wave.h"
#include "lcd_rotate_request.h"

#if !UI_LIGHT_PATH_RELAX

#if USE_DMA2D_PLOTBUF != 1
#error "轻路径要求 USE_DMA2D_PLOTBUF==1（dma2d_wave.h）；排障请定义 UI_LIGHT_PATH_RELAX=1"
#endif

#if USE_ROTATE_HEADER_STATIC_SKIP != 1
#error "轻路径要求 USE_ROTATE_HEADER_STATIC_SKIP==1（lcd_rotate_request.h）；排障请定义 UI_LIGHT_PATH_RELAX=1"
#endif

#endif /* !UI_LIGHT_PATH_RELAX */

#endif /* UI_DISPLAY_LIGHT_PATH_H */
