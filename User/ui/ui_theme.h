/**
 * @file ui_theme.h
 * @brief 全应用 UI 配色与布局常量（子页与 MainTask 示波器页对齐）。
 *
 * 波形页顶栏高 UI_HDR_H=160；子页用 UI_HDR_H_COMPACT 紧凑头。
 * 颜色 0x00RRGGBB，与 GUI_SetColor / DMA2D RGB565 转换约定一致。
 */
#ifndef UI_THEME_H
/* 宏定义: UI_THEME_H - 头文件保护宏。 */
#define UI_THEME_H

/* 宏定义: UI_DISP_W - 显示尺寸或布局尺寸常量。 */
#define UI_DISP_W       800
/* 宏定义: UI_DISP_H - 头文件保护宏。 */
#define UI_DISP_H       480
/* 与 MainTask HDR_H / DrawHeader / LCD ROTATE_HEADER_H 一致（仅波形页） */
#define UI_HDR_H        160
/* 与 MainTask 标题条（CLR_BG_HEADER 上段）等高：为原比例顶条高度的一半 */
#define UI_HDR_TOP      (((UI_HDR_H * 33) / 80) / 2)
/* 菜单/侧页顶栏总高：仅标题带 + 窄底带，无示波器 CH/测量详细信息区 */
#define UI_HDR_H_COMPACT  ((UI_HDR_TOP) + 12)
/* 与 LCDConf ROTATE_HEADER_H 一致 */
#define UI_ROT_HEADER_H 160
/* 正文区紧接头栏下沿（子页用 UI_HDR_H_COMPACT；波形页仍用 MainTask HDR_H=160） */
#define UI_CONTENT_TOP    ((UI_HDR_H_COMPACT) + 2)
/* 与 MainTask PLOT_X、右侧信息栏及底留白一致（底留白 = DISP_H - PLOT_Y - PLOT_H） */
#define UI_MARGIN_X        24
/* 宏定义: UI_MARGIN_XR - 当前模块使用的配置常量。 */
#define UI_MARGIN_XR       4
/* 宏定义: UI_SCOPE_SIDE_W - 显示尺寸或布局尺寸常量。 */
#define UI_SCOPE_SIDE_W    156
/* 宏定义: UI_SCOPE_SIDE_GAP - 当前模块使用的配置常量。 */
#define UI_SCOPE_SIDE_GAP  8
/* 宏定义: UI_BOTTOM_PAD - 当前模块使用的配置常量。 */
#define UI_BOTTOM_PAD      36
/* 宏定义: UI_FOOTER_H - 头文件保护宏。 */
#define UI_FOOTER_H     24

/* 宏定义: UI_CLR_BG - 当前模块使用的配置常量。 */
#define UI_CLR_BG       0x080C10u
/* 宏定义: UI_CLR_HDR_TOP - 当前模块使用的配置常量。 */
#define UI_CLR_HDR_TOP  0x10161Eu
/* 宏定义: UI_CLR_HDR_BOT - 当前模块使用的配置常量。 */
#define UI_CLR_HDR_BOT  0x0C1018u
/* 宏定义: UI_CLR_PANEL - 当前模块使用的配置常量。 */
#define UI_CLR_PANEL    0x0C1018u
/* 宏定义: UI_CLR_ACCENT - 当前模块使用的配置常量。 */
#define UI_CLR_ACCENT   0x203858u
/* 宏定义: UI_CLR_GRID_MJ - 当前模块使用的配置常量。 */
#define UI_CLR_GRID_MJ  0x1A2E46u
/* 宏定义: UI_CLR_GRID_MN - 当前模块使用的配置常量。 */
#define UI_CLR_GRID_MN  0x0D1828u
/* 宏定义: UI_CLR_AXIS - 当前模块使用的配置常量。 */
#define UI_CLR_AXIS     0x2C4A78u
/* 宏定义: UI_CLR_CH1 - 当前模块使用的配置常量。 */
#define UI_CLR_CH1      0xFF3355u
/* 宏定义: UI_CLR_CH2 - 当前模块使用的配置常量。 */
#define UI_CLR_CH2      0x22EE88u
/* 宏定义: UI_CLR_CH1_BADGE - 当前模块使用的配置常量。 */
#define UI_CLR_CH1_BADGE 0x380010u
/* 宏定义: UI_CLR_CH2_BADGE - 当前模块使用的配置常量。 */
#define UI_CLR_CH2_BADGE 0x003820u
/* 与 dma2d_wave.h CLR_BG_PLOT 一致，卡片区贴近波形区底色 */
#define UI_CLR_BG_PLOT  0x0A1218u
/* 宏定义: UI_CLR_TEXT_HI - 当前模块使用的配置常量。 */
#define UI_CLR_TEXT_HI  0xD0E4F8u
/* 宏定义: UI_CLR_TEXT_MID - 当前模块使用的配置常量。 */
#define UI_CLR_TEXT_MID 0x6888A8u
/* 宏定义: UI_CLR_TEXT_DIM - 当前模块使用的配置常量。 */
#define UI_CLR_TEXT_DIM 0x304050u
/* 宏定义: UI_CLR_DIV - 当前模块使用的配置常量。 */
#define UI_CLR_DIV      0x1A2A40u
/* 宏定义: UI_CLR_RUN_BG - 当前模块使用的配置常量。 */
#define UI_CLR_RUN_BG   0x143220u
/* 宏定义: UI_CLR_RUN_FG - 当前模块使用的配置常量。 */
#define UI_CLR_RUN_FG   0x44FF66u
/* 宏定义: UI_CLR_CARD - 当前模块使用的配置常量。 */
#define UI_CLR_CARD     0x101820u
/* 宏定义: UI_CLR_CARD_BR - 当前模块使用的配置常量。 */
#define UI_CLR_CARD_BR  0x2A4060u
/* 宏定义: UI_CLR_SEL - 当前模块使用的配置常量。 */
#define UI_CLR_SEL      0x1A3550u

#endif /* UI_THEME_H */
