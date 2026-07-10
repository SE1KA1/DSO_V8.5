/**
 * @file vram_sentinels.h
 * @brief 物理帧缓冲后方哨兵：检测 SDRAM 显示/DMA2D 越界。
 */

/**
 * 物理 / 中间 SDRAM 与显存计划之间的“缝隙”中放置哨兵字，供 DMA2D 越界场测
 * 与 DTCM 栈区破坏区分（哨兵在 0xC0 段，栈在 0x24 段时）。
 * 基址与间隙见 bsp_vram_layout.h（与 main / LCDConf 同源头）。
 */
#ifndef VRAM_SENTINELS_H
/* 宏定义: VRAM_SENTINELS_H - 头文件保护宏。 */
#define VRAM_SENTINELS_H

#include <stdint.h>
#include "bsp_vram_layout.h"
/* 宏定义: VRAM_SENTINEL_WORDS - VRAM_SENTINEL_WORDS 的宏定义。 */
#define VRAM_SENTINEL_WORDS  256U
/* 宏定义: VRAM_SENTINEL_PATTERN - VRAM_SENTINEL_PATTERN 的宏定义。 */
#define VRAM_SENTINEL_PATTERN 0x5E11E1C5U

/*
*********************************************************************************************************
*   函 数 名: VRAM_Sentinels_Init
*   功能说明: VRAM_Sentinels_Init 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void VRAM_Sentinels_Init(void);
/*
*********************************************************************************************************
*   函 数 名: VRAM_Sentinels_Check
*   功能说明: VRAM_Sentinels_Check 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
int  VRAM_Sentinels_Check(void);
/*
*********************************************************************************************************
*   函 数 名: VRAM_Sentinels_FailCount
*   功能说明: VRAM_Sentinels_FailCount 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t VRAM_Sentinels_FailCount(void);

#endif
