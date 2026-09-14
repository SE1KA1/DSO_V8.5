/**
  ******************************************************************************
  * @file    main.h
  * @brief   Header for main.c module
  * @author  BIDT Team
  ******************************************************************************
  */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"
#include "bsp_fmc_sdram.h"
#include "bsp_lcd_rgb.h"
#include "spi_master.h"
#include "lcd_ltdc.h"
#include "gpio.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define DEBUG_LOG(fmt, ...)  printf("[%s:%d] " fmt "\r\n", __FILE__, __LINE__, ##__VA_ARGS__)

/* LED 定义已包含在 gpio.h 中，无需重复定义 */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);
void SystemClock_Config(void);
void MPU_Config(void);
void CPU_CACHE_Enable(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
