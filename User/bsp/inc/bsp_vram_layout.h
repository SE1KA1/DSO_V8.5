/**
 * @file bsp_vram_layout.h
 * @brief 显存布局单一来源：逻辑横屏与物理竖屏 ping/pong 地址。
 */

/**
 * SDRAM 显存与哨兵地址 — 单一来源，须与 main.c / LCDConf / LTDC 一致。
 * 修改帧布局时只改本头文件，并同步 Keil/IAR 工程与 vram_sentinels 说明。
 */
#ifndef BSP_VRAM_LAYOUT_H
/* 宏定义: BSP_VRAM_LAYOUT_H - 头文件保护宏。 */
#define BSP_VRAM_LAYOUT_H

#include <stdint.h>
#include "bsp_fmc_sdram.h"

/* 逻辑横屏 emWin；物理竖屏 LTDC 扫描 */
#define VRAM_XLOG                 800U
/* 宏定义: VRAM_YLOG - VRAM_YLOG 的宏定义。 */
#define VRAM_YLOG                 480U
/* 宏定义: VRAM_XPHYS - VRAM_XPHYS 的宏定义。 */
#define VRAM_XPHYS                480U
/* 宏定义: VRAM_YPHYS - VRAM_YPHYS 的宏定义。 */
#define VRAM_YPHYS                800U
/* 宏定义: VRAM_BPP - VRAM_BPP 的宏定义。 */
#define VRAM_BPP                  2U

/* 宏定义: VRAM_LOGICAL_ADDR - VRAM_LOGICAL_ADDR 的宏定义。 */
#define VRAM_LOGICAL_ADDR         0xC0000000U
/* 宏定义: VRAM_PHYSICAL_0_ADDR - VRAM_PHYSICAL_0_ADDR 的宏定义。 */
#define VRAM_PHYSICAL_0_ADDR      0xC0200000U
/* 宏定义: VRAM_PHYSICAL_1_ADDR - VRAM_PHYSICAL_1_ADDR 的宏定义。 */
#define VRAM_PHYSICAL_1_ADDR      0xC0300000U

/* 宏定义: VRAM_LOGICAL_SUBFRAME_BYTES - VRAM_LOGICAL_SUBFRAME_BYTES 的宏定义。 */
#define VRAM_LOGICAL_SUBFRAME_BYTES  ((uint32_t)VRAM_XLOG * (uint32_t)VRAM_YLOG * (uint32_t)VRAM_BPP)
/* 宏定义: VRAM_PHYS_FRAME_BYTES - VRAM_PHYS_FRAME_BYTES 的宏定义。 */
#define VRAM_PHYS_FRAME_BYTES       ((uint32_t)VRAM_XPHYS * (uint32_t)VRAM_YPHYS * (uint32_t)VRAM_BPP)

/*
 * Phys0 末尾之后、Phys1 之前未作 LTDC 的缝隙起始处放哨兵 0（与历史地址 0xC02BB800 一致）。
 * Phys1 之后、SDRAM_APP_BUF(0xC0400000) 前再放哨兵 1；Phys1 尾后留 0x800 字节再置哨兵块。
 */
#define VRAM_GAP_AFTER_PHYS1_BYTES  0x800U

/* 宏定义: VRAM_SENTINEL0_ADDR - VRAM_SENTINEL0_ADDR 的宏定义。 */
#define VRAM_SENTINEL0_ADDR  (VRAM_PHYSICAL_0_ADDR + VRAM_PHYS_FRAME_BYTES)
/* 宏定义: VRAM_SENTINEL1_ADDR - VRAM_SENTINEL1_ADDR 的宏定义。 */
#define VRAM_SENTINEL1_ADDR  (VRAM_PHYSICAL_1_ADDR + VRAM_PHYS_FRAME_BYTES + VRAM_GAP_AFTER_PHYS1_BYTES)

#endif /* BSP_VRAM_LAYOUT_H */
