/**
  ******************************************************************************
  * @file    bsp_ui_io_expander.h
  * @brief   MCP23017 software-I2C output driver for FMCU_PI[15:0]
  ******************************************************************************
  */

#ifndef BSP_UI_IO_EXPANDER_H
#define BSP_UI_IO_EXPANDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"

HAL_StatusTypeDef BSP_UI_IO_Init(void);
HAL_StatusTypeDef BSP_UI_IO_Write16(uint16_t value);
HAL_StatusTypeDef BSP_UI_IO_InitKeyLeds(void);
HAL_StatusTypeDef BSP_UI_IO_WriteKeyLeds(uint8_t mask);

#ifdef __cplusplus
}
#endif

#endif /* BSP_UI_IO_EXPANDER_H */
