/**
  ******************************************************************************
  * @file    bsp_ui_io_expander.c
  * @brief   MCP23017 software-I2C output driver for FMCU_PI[15:0] and key LEDs
  ******************************************************************************
  */

#include "bsp_ui_io_expander.h"

#define UI_I2C_GPIO         GPIOB
#define UI_I2C_SCL_PIN      GPIO_PIN_8
#define UI_I2C_SDA_PIN      GPIO_PIN_7

#define KEY_LED_MCP23017_ADDR7 0x26u
#define FMCU_PI_MCP23017_ADDR7 0x27u

#define MCP23017_REG_IODIRA 0x00u
#define MCP23017_REG_IODIRB 0x01u
#define MCP23017_REG_IOCON_BANK1 0x05u
#define MCP23017_REG_IOCON  0x0Au
#define MCP23017_REG_GPIOA  0x12u
#define MCP23017_REG_GPIOB  0x13u
#define MCP23017_IOCON_BANK_BIT 0x80u
#define MCP23017_IOCON_SEQOP_BIT 0x20u
#define MCP23017_IOCON_BANK0_SEQ_ENABLE 0x00u

static void ui_i2c_delay(void)
{
    volatile uint32_t i;

    for (i = 0U; i < 64U; ++i)
    {
        __NOP();
    }
}

static void ui_scl_write(GPIO_PinState state)
{
    UI_I2C_GPIO->BSRR = (state == GPIO_PIN_SET) ? UI_I2C_SCL_PIN : ((uint32_t)UI_I2C_SCL_PIN << 16U);
}

static void ui_sda_write(GPIO_PinState state)
{
    UI_I2C_GPIO->BSRR = (state == GPIO_PIN_SET) ? UI_I2C_SDA_PIN : ((uint32_t)UI_I2C_SDA_PIN << 16U);
}

static GPIO_PinState ui_sda_read(void)
{
    return ((UI_I2C_GPIO->IDR & UI_I2C_SDA_PIN) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

static void ui_i2c_start(void)
{
    ui_sda_write(GPIO_PIN_SET);
    ui_scl_write(GPIO_PIN_SET);
    ui_i2c_delay();
    ui_sda_write(GPIO_PIN_RESET);
    ui_i2c_delay();
    ui_scl_write(GPIO_PIN_RESET);
    ui_i2c_delay();
}

static void ui_i2c_stop(void)
{
    ui_sda_write(GPIO_PIN_RESET);
    ui_i2c_delay();
    ui_scl_write(GPIO_PIN_SET);
    ui_i2c_delay();
    ui_sda_write(GPIO_PIN_SET);
    ui_i2c_delay();
}

static void ui_i2c_send_byte(uint8_t value)
{
    uint8_t bit;

    for (bit = 0U; bit < 8U; ++bit)
    {
        ui_sda_write(((value & 0x80U) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        ui_i2c_delay();
        ui_scl_write(GPIO_PIN_SET);
        ui_i2c_delay();
        ui_scl_write(GPIO_PIN_RESET);
        ui_i2c_delay();
        value <<= 1;
    }

    ui_sda_write(GPIO_PIN_SET);
}

static uint8_t ui_i2c_wait_ack(void)
{
    uint8_t nack;

    ui_sda_write(GPIO_PIN_SET);
    ui_i2c_delay();
    ui_scl_write(GPIO_PIN_SET);
    ui_i2c_delay();
    nack = (ui_sda_read() == GPIO_PIN_SET) ? 1U : 0U;
    ui_scl_write(GPIO_PIN_RESET);
    ui_i2c_delay();
    return nack;
}

static uint8_t ui_i2c_read_byte_nack(void)
{
    uint8_t bit;
    uint8_t value = 0U;

    ui_sda_write(GPIO_PIN_SET);
    for (bit = 0U; bit < 8U; ++bit)
    {
        value <<= 1;
        ui_scl_write(GPIO_PIN_SET);
        ui_i2c_delay();
        if (ui_sda_read() == GPIO_PIN_SET)
        {
            value |= 1U;
        }
        ui_scl_write(GPIO_PIN_RESET);
        ui_i2c_delay();
    }

    /* NACK the final byte. */
    ui_sda_write(GPIO_PIN_SET);
    ui_scl_write(GPIO_PIN_SET);
    ui_i2c_delay();
    ui_scl_write(GPIO_PIN_RESET);
    ui_i2c_delay();
    return value;
}

static void ui_i2c_recover_bus(void)
{
    uint8_t pulse;

    ui_sda_write(GPIO_PIN_SET);
    for (pulse = 0U; pulse < 9U; ++pulse)
    {
        ui_scl_write(GPIO_PIN_RESET);
        ui_i2c_delay();
        ui_scl_write(GPIO_PIN_SET);
        ui_i2c_delay();
    }
    ui_i2c_stop();
}

static HAL_StatusTypeDef ui_i2c_fail(void)
{
    ui_i2c_stop();
    ui_i2c_recover_bus();
    return HAL_ERROR;
}

static HAL_StatusTypeDef ui_i2c_write_bytes(uint8_t addr7, uint8_t reg,
                                            const uint8_t *data, uint8_t length)
{
    uint8_t index;

    if ((data == NULL) || (length == 0U))
    {
        return HAL_ERROR;
    }

    ui_i2c_start();
    ui_i2c_send_byte((uint8_t)(addr7 << 1));
    if (ui_i2c_wait_ack() != 0U)
    {
        return ui_i2c_fail();
    }

    ui_i2c_send_byte(reg);
    if (ui_i2c_wait_ack() != 0U)
    {
        return ui_i2c_fail();
    }

    for (index = 0U; index < length; ++index)
    {
        ui_i2c_send_byte(data[index]);
        if (ui_i2c_wait_ack() != 0U)
        {
            return ui_i2c_fail();
        }
    }

    ui_i2c_stop();
    return HAL_OK;
}

static HAL_StatusTypeDef ui_i2c_write_reg(uint8_t addr7, uint8_t reg, uint8_t value)
{
    return ui_i2c_write_bytes(addr7, reg, &value, 1U);
}

static HAL_StatusTypeDef ui_i2c_read_reg(uint8_t addr7, uint8_t reg, uint8_t *value)
{
    if (value == NULL)
    {
        return HAL_ERROR;
    }

    ui_i2c_start();
    ui_i2c_send_byte((uint8_t)(addr7 << 1));
    if (ui_i2c_wait_ack() != 0U)
    {
        return ui_i2c_fail();
    }

    ui_i2c_send_byte(reg);
    if (ui_i2c_wait_ack() != 0U)
    {
        return ui_i2c_fail();
    }

    ui_i2c_start();
    ui_i2c_send_byte((uint8_t)((addr7 << 1) | 1U));
    if (ui_i2c_wait_ack() != 0U)
    {
        return ui_i2c_fail();
    }

    *value = ui_i2c_read_byte_nack();
    ui_i2c_stop();
    return HAL_OK;
}

static void BSP_UI_IO_InitBus(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio_init.Pin = UI_I2C_SCL_PIN | UI_I2C_SDA_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_OD;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(UI_I2C_GPIO, &gpio_init);

    ui_i2c_stop();
}

static HAL_StatusTypeDef ui_mcp23017_force_bank0(uint8_t address7)
{
    uint8_t iocon = 0xFFU;

    /* If BANK=1, IOCON is at 0x05.  If BANK=0, 0x05 is GPINTENB and
     * clearing it is harmless for these output-only expanders. */
    if (ui_i2c_write_reg(address7, MCP23017_REG_IOCON_BANK1,
                         MCP23017_IOCON_BANK0_SEQ_ENABLE) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (ui_i2c_write_reg(address7, MCP23017_REG_IOCON,
                         MCP23017_IOCON_BANK0_SEQ_ENABLE) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (ui_i2c_read_reg(address7, MCP23017_REG_IOCON, &iocon) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if ((iocon & (MCP23017_IOCON_BANK_BIT | MCP23017_IOCON_SEQOP_BIT)) != 0U)
    {
        ui_i2c_recover_bus();
        return HAL_ERROR;
    }

    return HAL_OK;
}

HAL_StatusTypeDef BSP_UI_IO_Init(void)
{
    const uint8_t directions[2] = {0x00U, 0x00U};
    const uint8_t initial_output[2] = {0x00U, 0x80U};

    BSP_UI_IO_InitBus();

    if (ui_mcp23017_force_bank0(FMCU_PI_MCP23017_ADDR7) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Load the safe PI value while both ports are still inputs, avoiding a
     * transient all-low output when IODIR is changed. */
    if (ui_i2c_write_bytes(FMCU_PI_MCP23017_ADDR7,
                           MCP23017_REG_GPIOA, initial_output, 2U) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return ui_i2c_write_bytes(FMCU_PI_MCP23017_ADDR7,
                              MCP23017_REG_IODIRA, directions, 2U);
}

HAL_StatusTypeDef BSP_UI_IO_Write16(uint16_t value)
{
    const uint8_t data[2] = {
        (uint8_t)(value & 0x00FFU),
        (uint8_t)(value >> 8)
    };

    /* GPIOA carries PI[7:0], then sequential GPIOB carries PI[15:8]. */
    return ui_i2c_write_bytes(FMCU_PI_MCP23017_ADDR7,
                              MCP23017_REG_GPIOA, data, 2U);
}

HAL_StatusTypeDef BSP_UI_IO_InitKeyLeds(void)
{
    BSP_UI_IO_InitBus();

    if (ui_mcp23017_force_bank0(KEY_LED_MCP23017_ADDR7) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* LEDs are active-low: preload all-high while GPIOA is still an input so
     * changing IODIRA cannot flash every LED on. */
    if (ui_i2c_write_reg(KEY_LED_MCP23017_ADDR7, MCP23017_REG_GPIOA, 0xFFU) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (ui_i2c_write_reg(KEY_LED_MCP23017_ADDR7, MCP23017_REG_IODIRA, 0x00U) != HAL_OK)
    {
        return HAL_ERROR;
    }
    return HAL_OK;
}

HAL_StatusTypeDef BSP_UI_IO_WriteKeyLeds(uint8_t mask)
{
    return ui_i2c_write_reg(KEY_LED_MCP23017_ADDR7,
                            MCP23017_REG_GPIOA, (uint8_t)~mask);
}
