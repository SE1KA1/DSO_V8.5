/**
  ******************************************************************************
  * @file    lcd_ltdc.h
  * @brief   Thin framebuffer drawing helpers for the SPI monitor page
  ******************************************************************************
  */

#ifndef __LCD_LTDC_H
#define __LCD_LTDC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include "bsp_lcd_rgb.h"
#include <stdint.h>

/* Logical monitor page stays landscape while LTDC scans a portrait panel. */
#define LCD_WIDTH           800U
#define LCD_HEIGHT          480U
#define LCD_PHYS_WIDTH      LCD_RGB_WIDTH
#define LCD_PHYS_HEIGHT     LCD_RGB_HEIGHT

#define LCD_COLOR_BLACK     0x000000U
#define LCD_COLOR_WHITE     0xFFFFFFU
#define LCD_COLOR_RED       0xFF0000U
#define LCD_COLOR_GREEN     0x00FF00U
#define LCD_COLOR_BLUE      0x0000FFU
#define LCD_COLOR_YELLOW    0xFFFF00U
#define LCD_COLOR_CYAN      0x00FFFFU
#define LCD_COLOR_MAGENTA   0xFF00FFU
#define LCD_COLOR_GRAY      0x808080U
#define LCD_COLOR_ORANGE    0xFFA500U

void LCD_Clear(uint32_t color);
void LCD_DrawPoint(uint16_t x, uint16_t y, uint32_t color);
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t color);
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t color);
void LCD_Fill(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t color);
void LCD_FillRounded(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t r, uint32_t color);
void LCD_DrawRoundedRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t r, uint32_t color);
void LCD_ShowChar(uint16_t x, uint16_t y, char chr, uint8_t size, uint32_t fc, uint32_t bc);
void LCD_ShowString(uint16_t x, uint16_t y, char *str, uint8_t size, uint32_t fc, uint32_t bc);
void LCD_ShowChinese(uint16_t x, uint16_t y, const uint8_t *str, uint8_t size, uint32_t fc, uint32_t bc);
void LCD_DisplayStringLine(uint16_t y, uint8_t *str);
void LCD_SetBackColor(uint32_t color);
void LCD_SetTextColor(uint32_t color);
void LCD_RefreshRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_Refresh(void);
uint32_t LCD_GetDma2dErrorCount(void);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_LTDC_H */
