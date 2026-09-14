/**
  ******************************************************************************
  * @file    bsp_lcd_rgb.h
  * @brief   Minimum panel + LTDC bring-up aligned to the verified v7_h743 path
  ******************************************************************************
  */

#ifndef __BSP_LCD_RGB_H
#define __BSP_LCD_RGB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"

#define LCD_RGB_WIDTH     480U
#define LCD_RGB_HEIGHT    800U
#define LCD_RGB_FB_ADDR   ((uint32_t)0xC0200000U)

void LCD_RGB_Init(void);
void LCD_RGB_BacklightOn(void);
void LCD_RGB_BacklightOff(void);

extern LTDC_HandleTypeDef g_hltdc;

#ifdef __cplusplus
}
#endif

#endif /* __BSP_LCD_RGB_H */
