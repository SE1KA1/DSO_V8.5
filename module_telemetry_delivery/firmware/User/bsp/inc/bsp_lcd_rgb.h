/**
 * @file bsp_lcd_rgb.h
 * @brief RGB LCD / LTDC 层配置、背光、帧缓冲地址接口。
 */

#ifndef BSP_LCD_RGB_H
/* 宏定义: BSP_LCD_RGB_H - 头文件保护宏。 */
#define BSP_LCD_RGB_H

#include "stm32h7xx_hal.h"
#include "bsp_vram_layout.h"

/*
 * Panel control pin mappings:
 * - H743 schematic: T_CS=PG10, T_SCK=PH7, T_MOSI=PH8
 * - F429 image     : T_CS=PI8,  T_SCK=PH6, T_MOSI=PI3
 */
#define LCD_RGB_PANEL_PINSET_H743_SCHEMATIC   0U
/* 宏定义: LCD_RGB_PANEL_PINSET_F429_IMAGE - LCD_RGB_PANEL_PINSET_F429_IMAGE 的宏定义。 */
#define LCD_RGB_PANEL_PINSET_F429_IMAGE       1U
/* 宏定义: LCD_RGB_PANEL_PINSET - LCD_RGB_PANEL_PINSET 的宏定义。 */
#define LCD_RGB_PANEL_PINSET                  LCD_RGB_PANEL_PINSET_H743_SCHEMATIC

/*
 * LTDC timings: 物理面板按 480x800 输出（避免 1.5 屏错行）
 */
#define LCD_RGB_TIMING_800X480_LANDSCAPE      0U
/* 宏定义: LCD_RGB_TIMING_480X800_PORTRAIT - LCD_RGB_TIMING_480X800_PORTRAIT 的宏定义。 */
#define LCD_RGB_TIMING_480X800_PORTRAIT       1U
/* 宏定义: LCD_RGB_TIMING_MODE - LCD_RGB_TIMING_MODE 的宏定义。 */
#define LCD_RGB_TIMING_MODE                   LCD_RGB_TIMING_480X800_PORTRAIT

/*
 * LTDC 极性配置 (根据面板调整)
 * 如果屏幕显示异常，尝试不同的极性组合
 * ILI9806E 典型配置: HSYNC=AL, VSYNC=AL, DE=AH, PCLK=IPC 或 IIPC
 */
#define LCD_RGB_POLARITY_HSYNC_AL             0U  /* Active Low */
#define LCD_RGB_POLARITY_VSYNC_AL             0U  /* Active Low */
#define LCD_RGB_POLARITY_DE_AH                1U  /* Active High (尝试 0=AL 如果有问题) */
#define LCD_RGB_POLARITY_PCLK_IIPC            1U  /* 下降沿采样 (尝试 0=IPC 如果有问题) */

/* Panel register options */
#define LCD_RGB_PANEL_PIXFMT_666              0x66U
/* 宏定义: LCD_RGB_PANEL_PIXFMT_565 - LCD_RGB_PANEL_PIXFMT_565 的宏定义。 */
#define LCD_RGB_PANEL_PIXFMT_565              0x55U
/* 宏定义: LCD_RGB_PANEL_PIXFMT - LCD_RGB_PANEL_PIXFMT 的宏定义。 */
#define LCD_RGB_PANEL_PIXFMT                  LCD_RGB_PANEL_PIXFMT_565
/* 宏定义: LCD_RGB_PANEL_MADCTL_RGB - LCD_RGB_PANEL_MADCTL_RGB 的宏定义。 */
#define LCD_RGB_PANEL_MADCTL_RGB              0x00U
/* 宏定义: LCD_RGB_PANEL_MADCTL_BGR - LCD_RGB_PANEL_MADCTL_BGR 的宏定义。 */
#define LCD_RGB_PANEL_MADCTL_BGR              0x08U
/* 红蓝颠倒修复：使用 RGB 顺序 (如果红蓝颠倒，改为 LCD_RGB_PANEL_MADCTL_RGB) */
#define LCD_RGB_PANEL_MADCTL                  LCD_RGB_PANEL_MADCTL_RGB
/* 关闭自动探测，避免把像素格式切回 666 导致 emWin565 颜色/背景异常 */
#define LCD_RGB_PANEL_AUTOPROBE               0U

/* 宏定义: LCD_RGB_WIDTH - LCD_RGB_WIDTH 的宏定义。 */
#define LCD_RGB_WIDTH                         480U
/* 宏定义: LCD_RGB_HEIGHT - LCD_RGB_HEIGHT 的宏定义。 */
#define LCD_RGB_HEIGHT                        800U

/* 须与 bsp_vram_layout 中物理竖屏缓冲 0（LTDC 扫描）一致；勿用 0xC0100000（落在 emWin 逻辑子帧 1 内） */
#define LCD_RGB_FB_ADDR          ((uint32_t)VRAM_PHYSICAL_0_ADDR)

/* sRGB888 -> RGB565 */
#define LCD_RGB565(_r, _g, _b) \
    ((uint16_t)((((uint16_t)((_r) & 0xF8U)) << 8) | \
                (((uint16_t)((_g) & 0xFCU)) << 3) | \
                (((uint16_t)(_b)) >> 3)))

/*
*********************************************************************************************************
*   函 数 名: LCD_RGB_Init
*   功能说明: LCD_RGB_Init 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void LCD_RGB_Init(void);
/*
*********************************************************************************************************
*   函 数 名: LCD_RGB_BacklightOn
*   功能说明: LCD_RGB_BacklightOn 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void LCD_RGB_BacklightOn(void);
/*
*********************************************************************************************************
*   函 数 名: LCD_RGB_BacklightOff
*   功能说明: LCD_RGB_BacklightOff 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void LCD_RGB_BacklightOff(void);
/*
*********************************************************************************************************
*   函 数 名: LCD_RGB_Fill
*   功能说明: LCD_RGB_Fill 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void LCD_RGB_Fill(uint16_t color);

/* BSP 初始化的 LTDC 句柄，供 emWin LCDConf 使用 */
extern LTDC_HandleTypeDef g_hltdc;

#endif
