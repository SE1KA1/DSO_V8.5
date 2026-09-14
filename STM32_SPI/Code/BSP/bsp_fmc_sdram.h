/**
  ******************************************************************************
  * @file    bsp_fmc_sdram.h
  * @brief   External SDRAM bring-up aligned to the verified v7_h743 path
  ******************************************************************************
  */

#ifndef __BSP_FMC_SDRAM_H
#define __BSP_FMC_SDRAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"

#define EXT_SDRAM_ADDR   ((uint32_t)0xC0000000U)
#define EXT_SDRAM_SIZE   (32U * 1024U * 1024U)

#define SDRAM_LCD_SIZE   (2U * 1024U * 1024U)
#define SDRAM_LCD_LAYER  2U
#define SDRAM_LCD_BUF1   EXT_SDRAM_ADDR
#define SDRAM_LCD_BUF2   (EXT_SDRAM_ADDR + SDRAM_LCD_SIZE)
#define SDRAM_APP_BUF    (EXT_SDRAM_ADDR + SDRAM_LCD_SIZE * SDRAM_LCD_LAYER)
#define SDRAM_APP_SIZE   (EXT_SDRAM_SIZE - SDRAM_LCD_SIZE * SDRAM_LCD_LAYER)

void bsp_InitExtSDRAM(void);
uint32_t bsp_TestExtSDRAM_Block(uint32_t addr, uint32_t size_bytes);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_FMC_SDRAM_H */
