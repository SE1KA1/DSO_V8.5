/**
 * @file dma2d_nlr.h
 * @brief DMA2D NLR 寄存器打包辅助（宽高 → NLR 域）。
 */

/**
 * STM32H743 DMA2D NLR: bits [31:16] = PL 每行像素数, bits [15:0] = NL 行数
 * 与 HAL DMA2D_SetConfig、CMSIS 头文件位域名一致。用于排查反汇编/寄存器时对照。
 */
#ifndef DMA2D_NLR_H
/* 宏定义: DMA2D_NLR_H - 头文件保护宏。 */
#define DMA2D_NLR_H

#include <stdint.h>
#include "bsp_vram_layout.h"

/* 打包 NLR 寄存器原值 */
#define DMA2D_NLR_PACK_PL_NL(PL, NL)  (((uint32_t)(PL) << 16U) | (uint32_t)(uint16_t)(NL))

/* 从读回的 NLR 解包（与 DMA2D->NLR 读值对照） */
#define DMA2D_NLR_GET_PL(nlr)  ((uint32_t)(nlr) >> 16U)
/* 宏定义: DMA2D_NLR_GET_NL - DMA2D_NLR_GET_NL 的宏定义。 */
#define DMA2D_NLR_GET_NL(nlr)  ((uint32_t)(nlr) & 0xFFFFU)

/* 物理竖屏全幅 front→dst M2M，PL/NL 与 bsp_vram_layout（VRAM_XPHYS×VRAM_YPHYS）一致 */
#define DMA2D_NLR_PHYS_FULLSCR_RGB565  DMA2D_NLR_PACK_PL_NL(VRAM_XPHYS, VRAM_YPHYS)

#endif /* DMA2D_NLR_H */
