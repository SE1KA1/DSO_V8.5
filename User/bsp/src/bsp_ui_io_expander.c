/**
 * @file bsp_ui_io_expander.c
 * @brief IO 扩展器读写与去抖采样。
 */

/*
 * MCP23017 @ I2C 地址 0x26（7-bit），PB8=SCL、PB7=SDA，GPIO 模拟 I2C（开漏 + 外部上拉）。
 */

#include "bsp.h"
#include "bsp_ui_io_expander.h"

/* 宏定义: UI_I2C_GPIO - UI_I2C_GPIO 的宏定义。 */
#define UI_I2C_GPIO     GPIOB
/* 宏定义: UI_I2C_SCL_PIN - UI_I2C_SCL_PIN 的宏定义。 */
#define UI_I2C_SCL_PIN  GPIO_PIN_8
/* 宏定义: UI_I2C_SDA_PIN - UI_I2C_SDA_PIN 的宏定义。 */
#define UI_I2C_SDA_PIN  GPIO_PIN_7

/* 宏定义: UI_I2C_ADDR7 - UI_I2C_ADDR7 的宏定义。 */
#define UI_I2C_ADDR7    0x26u
/* 宏定义: UI_I2C_WR - UI_I2C_WR 的宏定义。 */
#define UI_I2C_WR       0u
/* 宏定义: MCP23017_REG_GPIOB - MCP23017_REG_GPIOB 的宏定义。 */
#define MCP23017_REG_GPIOB  0x13u

/* STM32H7 GPIO_TypeDef 为 BSRRL/BSRRH 分寄存器，与 bsp_i2c_gpio.c 一致 */
#define UI_SCL_1()  (UI_I2C_GPIO->BSRRL = (uint16_t)UI_I2C_SCL_PIN)
/* 宏定义: UI_SCL_0 - UI_SCL_0 的宏定义。 */
#define UI_SCL_0()  (UI_I2C_GPIO->BSRRH = (uint16_t)UI_I2C_SCL_PIN)
/* 宏定义: UI_SDA_1 - UI_SDA_1 的宏定义。 */
#define UI_SDA_1()  (UI_I2C_GPIO->BSRRL = (uint16_t)UI_I2C_SDA_PIN)
/* 宏定义: UI_SDA_0 - UI_SDA_0 的宏定义。 */
#define UI_SDA_0()  (UI_I2C_GPIO->BSRRH = (uint16_t)UI_I2C_SDA_PIN)
/* 宏定义: UI_SDA_READ - UI_SDA_READ 的宏定义。 */
#define UI_SDA_READ()  (((UI_I2C_GPIO->IDR) & UI_I2C_SDA_PIN) != 0U)

/*
*********************************************************************************************************
*   函 数 名: ui_i2c_delay
*   功能说明: ui_i2c_delay 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ui_i2c_delay(void)
{
    bsp_DelayUS(2);
}

/*
*********************************************************************************************************
*   函 数 名: ui_i2c_stop
*   功能说明: ui_i2c_stop 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ui_i2c_stop(void)
{
    UI_SDA_0();
    ui_i2c_delay();
    UI_SCL_1();
    ui_i2c_delay();
    UI_SDA_1();
    ui_i2c_delay();
}

/*
*********************************************************************************************************
*   函 数 名: ui_i2c_start
*   功能说明: ui_i2c_start 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ui_i2c_start(void)
{
    UI_SDA_1();
    UI_SCL_1();
    ui_i2c_delay();
    UI_SDA_0();
    ui_i2c_delay();
    UI_SCL_0();
    ui_i2c_delay();
}

/*
*********************************************************************************************************
*   函 数 名: ui_i2c_send_byte
*   功能说明: ui_i2c_send_byte 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ui_i2c_send_byte(uint8_t b)
{
    uint8_t i;

    for (i = 0U; i < 8U; i++) {
        if ((b & 0x80U) != 0U) {
            UI_SDA_1();
        } else {
            UI_SDA_0();
        }
        ui_i2c_delay();
        UI_SCL_1();
        ui_i2c_delay();
        UI_SCL_0();
        if (i == 7U) {
            UI_SDA_1();
        }
        b <<= 1;
    }
}

/*
*********************************************************************************************************
*   函 数 名: ui_i2c_read_byte
*   功能说明: ui_i2c_read_byte 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t ui_i2c_read_byte(void)
{
    uint8_t i;
    uint8_t v = 0U;

    for (i = 0U; i < 8U; i++) {
        v <<= 1U;
        UI_SCL_1();
        ui_i2c_delay();
        if (UI_SDA_READ()) {
            v++;
        }
        UI_SCL_0();
        ui_i2c_delay();
    }
    return v;
}

/*
*********************************************************************************************************
*   函 数 名: ui_i2c_wait_ack
*   功能说明: ui_i2c_wait_ack 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t ui_i2c_wait_ack(void)
{
    uint8_t re;

    UI_SDA_1();
    ui_i2c_delay();
    UI_SCL_1();
    ui_i2c_delay();
    re = UI_SDA_READ() ? 1U : 0U;
    UI_SCL_0();
    ui_i2c_delay();
    return re;
}

/*
*********************************************************************************************************
*   函 数 名: ui_i2c_nack
*   功能说明: ui_i2c_nack 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ui_i2c_nack(void)
{
    UI_SDA_1();
    ui_i2c_delay();
    UI_SCL_1();
    ui_i2c_delay();
    UI_SCL_0();
    ui_i2c_delay();
}

/*
*********************************************************************************************************
*   函 数 名: BSP_UI_IO_Init
*   功能说明: BSP_UI_IO_Init 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void BSP_UI_IO_Init(void)
{
    GPIO_InitTypeDef gpio;

    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Mode      = GPIO_MODE_OUTPUT_OD;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio.Pin       = UI_I2C_SCL_PIN | UI_I2C_SDA_PIN;
    gpio.Alternate = 0U;
    HAL_GPIO_Init(UI_I2C_GPIO, &gpio);

    ui_i2c_stop();

    /* IODIRA=0 输出(接 LED)，IODIRB=0xFF 输入（编码器/按键） */
    ui_i2c_start();
    ui_i2c_send_byte((uint8_t)((UI_I2C_ADDR7 << 1) | UI_I2C_WR));
    (void)ui_i2c_wait_ack();
    ui_i2c_send_byte(0x00u); /* IODIRA */
    (void)ui_i2c_wait_ack();
    ui_i2c_send_byte(0x00u);
    (void)ui_i2c_wait_ack();
    ui_i2c_send_byte(0xFFu);
    (void)ui_i2c_wait_ack();
    ui_i2c_stop();
}

/*
*********************************************************************************************************
*   函 数 名: BSP_UI_IO_ReadPortB
*   功能说明: BSP_UI_IO_ReadPortB 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
int BSP_UI_IO_ReadPortB(uint8_t *out)
{
    if (out == NULL) {
        return -1;
    }

    ui_i2c_start();
    ui_i2c_send_byte((uint8_t)((UI_I2C_ADDR7 << 1) | UI_I2C_WR));
    if (ui_i2c_wait_ack() != 0U) {
        ui_i2c_stop();
        return -1;
    }
    ui_i2c_send_byte(MCP23017_REG_GPIOB);
    if (ui_i2c_wait_ack() != 0U) {
        ui_i2c_stop();
        return -1;
    }
    ui_i2c_start();
    ui_i2c_send_byte((uint8_t)((UI_I2C_ADDR7 << 1) | 1u)); /* read */
    if (ui_i2c_wait_ack() != 0U) {
        ui_i2c_stop();
        return -1;
    }
    *out = ui_i2c_read_byte();
    ui_i2c_nack();
    ui_i2c_stop();
    return 0;
}
