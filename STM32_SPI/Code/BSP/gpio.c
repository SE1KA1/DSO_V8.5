/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   GPIO driver for STM32H743
  *          LED control and extended GPIO for FPGA communication
  ******************************************************************************
  */

#include "gpio.h"
#include "bsp_ui_io_expander.h"

/**
  * @brief  Initialize GPIO
  * @param  None
  * @retval None
  */
HAL_StatusTypeDef MX_GPIO_Init(void)
{
    /* Enable GPIO clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();

    /* The two MCP23017 devices are initialized independently by main().
     * A NACK from one device must not prevent the other device from starting. */
    return HAL_OK;
}

/**
  * @brief  Initialize STM32 local key indicator LEDs
  * @param  None
  * @retval None
  */
HAL_StatusTypeDef KEY_LED_Init(void)
{
    return BSP_UI_IO_InitKeyLeds();
}

HAL_StatusTypeDef KEY_LED_WriteMask(uint8_t mask)
{
    return BSP_UI_IO_WriteKeyLeds(mask);
}

HAL_StatusTypeDef FMCU_PI_Init(void)
{
    /* BSP init already preloads the complete safe 0x8000 word before IODIR. */
    return BSP_UI_IO_Init();
}

HAL_StatusTypeDef FMCU_PI_Write(uint16_t value)
{
    return BSP_UI_IO_Write16(value);
}
