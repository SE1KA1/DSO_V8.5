/**
  ******************************************************************************
  * @file    lcd_lg4573a.h
 * @brief   LG4573A serial initialization driver
 *          Uses the verified 0x70/0x72 3-wire serial protocol
  ******************************************************************************
  */

#ifndef __LCD_LG4573A_H
#define __LCD_LG4573A_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"

/* Compile-time selectable panel-init pinsets. Default stays on the current
 * PG10/PH7/PH8 mapping so existing behavior does not change unexpectedly. */
#define LCD_PANEL_PINSET_PH6_PH7_PH8    0U
#define LCD_PANEL_PINSET_PG10_PH7_PH8   1U
#define LCD_PANEL_PINSET_PG10_PH7_PG3   2U

#ifndef LCD_PANEL_PINSET
#define LCD_PANEL_PINSET LCD_PANEL_PINSET_PG10_PH7_PH8
#endif

#if (LCD_PANEL_PINSET == LCD_PANEL_PINSET_PH6_PH7_PH8)
#define LCD_CS_GPIO_Port    GPIOH
#define LCD_CS_Pin          GPIO_PIN_6
#define LCD_SCL_GPIO_Port   GPIOH
#define LCD_SCL_Pin         GPIO_PIN_7
#define LCD_SDI_GPIO_Port   GPIOH
#define LCD_SDI_Pin         GPIO_PIN_8
#elif (LCD_PANEL_PINSET == LCD_PANEL_PINSET_PG10_PH7_PH8)
#define LCD_CS_GPIO_Port    GPIOG
#define LCD_CS_Pin          GPIO_PIN_10
#define LCD_SCL_GPIO_Port   GPIOH
#define LCD_SCL_Pin         GPIO_PIN_7
#define LCD_SDI_GPIO_Port   GPIOH
#define LCD_SDI_Pin         GPIO_PIN_8
#elif (LCD_PANEL_PINSET == LCD_PANEL_PINSET_PG10_PH7_PG3)
#define LCD_CS_GPIO_Port    GPIOG
#define LCD_CS_Pin          GPIO_PIN_10
#define LCD_SCL_GPIO_Port   GPIOH
#define LCD_SCL_Pin         GPIO_PIN_7
#define LCD_SDI_GPIO_Port   GPIOG
#define LCD_SDI_Pin         GPIO_PIN_3
#else
#error "Invalid LCD_PANEL_PINSET"
#endif

/* Function Prototypes */
void LG4573A_Init(void);
void LG4573A_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_LG4573A_H */
