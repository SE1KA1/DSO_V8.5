/**
 * @file bsp_led.c
 * @brief LED1-3 初始化；PB15/LED4 默认关闭（TIM12 测频占用）。
 */

/*
*********************************************************************************************************
*
*    Module: LED driver
*    Notes : Adapted for BIDT_GPEP core board.
*            LED1 -> PA0
*            LED2 -> PB4
*            LED3 -> PB9
*            LED4 -> PB15
*
*********************************************************************************************************
*/

#include "bsp.h"

/* 宏定义: LED_ACTIVE_LEVEL - LED_ACTIVE_LEVEL 的宏定义。 */
#define LED_ACTIVE_LEVEL    GPIO_PIN_SET
/* 宏定义: LED_INACTIVE_LEVEL - LED_INACTIVE_LEVEL 的宏定义。 */
#define LED_INACTIVE_LEVEL  GPIO_PIN_RESET

/*
 * PB15 is now reserved for TIM12_CH2 external frequency measurement.
 * Keep LED4 disabled so boot-time board init never drives the measurement input.
 */
#ifndef BSP_LED4_PB15_ENABLE
/* 宏定义: BSP_LED4_PB15_ENABLE - BSP_LED4_PB15_ENABLE 的宏定义。 */
#define BSP_LED4_PB15_ENABLE  0u
#endif

/*
*********************************************************************************************************
*   函 数 名: LedToPortPin
*   功能说明: LedToPortPin 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t LedToPortPin(uint8_t _no, GPIO_TypeDef **_port, uint16_t *_pin)
{
    switch (_no)
    {
    case 1:
        *_port = GPIOA;
        *_pin = GPIO_PIN_0;
        return 1;

    case 2:
        *_port = GPIOB;
        *_pin = GPIO_PIN_4;
        return 1;

    case 3:
        *_port = GPIOB;
        *_pin = GPIO_PIN_9;
        return 1;

#if (BSP_LED4_PB15_ENABLE != 0u)
    case 4:
        *_port = GPIOB;
        *_pin = GPIO_PIN_15;
        return 1;
#endif

    default:
        return 0;
    }
}

/*
*********************************************************************************************************
*   函 数 名: bsp_InitLed
*   功能说明: bsp_InitLed 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void bsp_InitLed(void)
{
    GPIO_InitTypeDef gpio_init;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    gpio_init.Pin = GPIO_PIN_0;
    HAL_GPIO_Init(GPIOA, &gpio_init);

    gpio_init.Pin = GPIO_PIN_4 | GPIO_PIN_9;
#if (BSP_LED4_PB15_ENABLE != 0u)
    gpio_init.Pin |= GPIO_PIN_15;
#endif
    HAL_GPIO_Init(GPIOB, &gpio_init);

    bsp_LedOff(1);
    bsp_LedOff(2);
    bsp_LedOff(3);
    bsp_LedOff(4);
}

/*
*********************************************************************************************************
*   函 数 名: bsp_LedOn
*   功能说明: bsp_LedOn 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void bsp_LedOn(uint8_t _no)
{
    GPIO_TypeDef *port;
    uint16_t pin;

    if (LedToPortPin(_no, &port, &pin) == 0)
    {
        return;
    }

    HAL_GPIO_WritePin(port, pin, LED_ACTIVE_LEVEL);
}

/*
*********************************************************************************************************
*   函 数 名: bsp_LedOff
*   功能说明: bsp_LedOff 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void bsp_LedOff(uint8_t _no)
{
    GPIO_TypeDef *port;
    uint16_t pin;

    if (LedToPortPin(_no, &port, &pin) == 0)
    {
        return;
    }

    HAL_GPIO_WritePin(port, pin, LED_INACTIVE_LEVEL);
}

/*
*********************************************************************************************************
*   函 数 名: bsp_LedToggle
*   功能说明: bsp_LedToggle 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void bsp_LedToggle(uint8_t _no)
{
    GPIO_TypeDef *port;
    uint16_t pin;

    if (LedToPortPin(_no, &port, &pin) == 0)
    {
        return;
    }

    HAL_GPIO_TogglePin(port, pin);
}

/*
*********************************************************************************************************
*   函 数 名: bsp_IsLedOn
*   功能说明: bsp_IsLedOn 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t bsp_IsLedOn(uint8_t _no)
{
    GPIO_TypeDef *port;
    uint16_t pin;

    if (LedToPortPin(_no, &port, &pin) == 0)
    {
        return 0;
    }

    return (HAL_GPIO_ReadPin(port, pin) == LED_ACTIVE_LEVEL) ? 1 : 0;
}

/***************************** END OF FILE *********************************/
