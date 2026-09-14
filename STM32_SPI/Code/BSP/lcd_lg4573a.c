/**
  ******************************************************************************
  * @file    lcd_lg4573a.c
  * @brief   LG4573A serial initialization driver for TK032F8004
  *          Uses the verified 0x70/0x72 3-wire serial write protocol
  ******************************************************************************
  */

#include "lcd_lg4573a.h"

/* Private macros */
#define LCD_SDI_HIGH()      HAL_GPIO_WritePin(LCD_SDI_GPIO_Port, LCD_SDI_Pin, GPIO_PIN_SET)
#define LCD_SDI_LOW()       HAL_GPIO_WritePin(LCD_SDI_GPIO_Port, LCD_SDI_Pin, GPIO_PIN_RESET)
#define LCD_SCL_HIGH()      HAL_GPIO_WritePin(LCD_SCL_GPIO_Port, LCD_SCL_Pin, GPIO_PIN_SET)
#define LCD_SCL_LOW()       HAL_GPIO_WritePin(LCD_SCL_GPIO_Port, LCD_SCL_Pin, GPIO_PIN_RESET)
#define LCD_CS_HIGH()       HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET)
#define LCD_CS_LOW()        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET)

static void LCD_WriteByteSPI(uint8_t byte);
static void SPI_WriteComm(uint8_t cmd);
static void SPI_WriteData(uint8_t data);
static void SPI_WriteRegData(uint8_t reg, const uint8_t *data, uint32_t len);

/* Short software delay to cap the bit-bang SCL frequency well below the
 * LG4573A ~15 MHz serial-interface limit. On H743 @ 400MHz each NOP is
 * ~2.5ns; 60 NOPs ≈ 150ns → SCL ≈ 3 MHz. Matches the colleague's
 * bsp_DelayUS(1) cadence in bsp_lcd_rgb.c in order of magnitude. */
/* Approximate the verified v7_h743 bit-bang cadence for the PH6/PH7/PH8
 * panel-init path without pulling in the full BSP delay layer. */
static void LCD_SPI_ShortDelay(void)
{
    volatile uint32_t cycles = SystemCoreClock / 5000000U;
    while (cycles-- > 0U)
    {
        __NOP();
    }
}

static void LCD_WriteByteSPI(uint8_t byte)
{
    uint8_t bit_index;

    for (bit_index = 0U; bit_index < 8U; bit_index++)
    {
        LCD_SCL_LOW();

        if ((byte & 0x80U) != 0U)
        {
            LCD_SDI_HIGH();
        }
        else
        {
            LCD_SDI_LOW();
        }

        LCD_SPI_ShortDelay();
        LCD_SCL_HIGH();
        LCD_SPI_ShortDelay();

        byte <<= 1;
    }
}

static void SPI_WriteComm(uint8_t cmd)
{
    LCD_CS_LOW();
    LCD_WriteByteSPI(0x70U);
    LCD_WriteByteSPI(cmd);
    LCD_CS_HIGH();
}

static void SPI_WriteData(uint8_t data)
{
    LCD_CS_LOW();
    LCD_WriteByteSPI(0x72U);
    LCD_WriteByteSPI(data);
    LCD_CS_HIGH();
}

static void SPI_WriteRegData(uint8_t reg, const uint8_t *data, uint32_t len)
{
    uint32_t i;

    SPI_WriteComm(reg);
    for (i = 0U; i < len; i++)
    {
        SPI_WriteData(data[i]);
    }
}

void LG4573A_Reset(void)
{
    SPI_WriteComm(0x01U);
    HAL_Delay(120U);
}

void LG4573A_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    static const uint8_t reg_b0[] = {0x05U};
    static const uint8_t reg_b1[] = {0x00U, 0x14U, 0x06U};
    static const uint8_t reg_b2[] = {0x10U, 0xC8U};
    static const uint8_t reg_b3[] = {0x00U};
    static const uint8_t reg_b4[] = {0x04U};
    static const uint8_t reg_b5[] = {0x10U, 0x30U, 0x30U, 0x00U, 0x00U};
    static const uint8_t reg_b6[] = {0x01U, 0x18U, 0x02U, 0x40U, 0x10U, 0x00U};
    static const uint8_t reg_c0[] = {0x01U, 0x18U};
    static const uint8_t reg_c3[] = {0x03U, 0x04U, 0x03U, 0x03U, 0x03U};
    static const uint8_t reg_c4[] = {0x02U, 0x23U, 0x11U, 0x12U, 0x02U, 0x77U};
    static const uint8_t reg_c5[] = {0x73U};
    static const uint8_t reg_c6[] = {0x24U, 0x60U, 0x00U};
    static const uint8_t reg_d0[] = {0x14U, 0x01U, 0x53U, 0x25U, 0x02U, 0x02U, 0x66U, 0x14U, 0x03U};
    static const uint8_t reg_d1[] = {0x14U, 0x01U, 0x53U, 0x07U, 0x02U, 0x02U, 0x66U, 0x14U, 0x03U};

    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    GPIO_InitStruct.Pin = LCD_SDI_Pin;
    HAL_GPIO_Init(LCD_SDI_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LCD_SCL_Pin;
    HAL_GPIO_Init(LCD_SCL_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LCD_CS_Pin;
    HAL_GPIO_Init(LCD_CS_GPIO_Port, &GPIO_InitStruct);

    LCD_CS_HIGH();
    LCD_SCL_HIGH();
    LCD_SDI_HIGH();
    HAL_Delay(20U);

    SPI_WriteComm(0x01U);
    HAL_Delay(10U);

    SPI_WriteComm(0x11U);
    HAL_Delay(120U);

    SPI_WriteRegData(0xB0U, reg_b0, sizeof(reg_b0));
    SPI_WriteRegData(0xB1U, reg_b1, sizeof(reg_b1));
    SPI_WriteRegData(0xB2U, reg_b2, sizeof(reg_b2));
    SPI_WriteRegData(0xB3U, reg_b3, sizeof(reg_b3));
    SPI_WriteRegData(0xB4U, reg_b4, sizeof(reg_b4));
    SPI_WriteRegData(0xB5U, reg_b5, sizeof(reg_b5));
    SPI_WriteRegData(0xB6U, reg_b6, sizeof(reg_b6));
    SPI_WriteRegData(0xC0U, reg_c0, sizeof(reg_c0));
    SPI_WriteRegData(0xC3U, reg_c3, sizeof(reg_c3));
    HAL_Delay(10U);

    SPI_WriteRegData(0xC4U, reg_c4, sizeof(reg_c4));
    HAL_Delay(10U);
    SPI_WriteRegData(0xC5U, reg_c5, sizeof(reg_c5));
    HAL_Delay(10U);
    SPI_WriteRegData(0xC6U, reg_c6, sizeof(reg_c6));
    HAL_Delay(10U);

    SPI_WriteRegData(0xD0U, reg_d0, sizeof(reg_d0));
    SPI_WriteRegData(0xD1U, reg_d1, sizeof(reg_d1));
    SPI_WriteRegData(0xD2U, reg_d0, sizeof(reg_d0));
    SPI_WriteRegData(0xD3U, reg_d1, sizeof(reg_d1));
    SPI_WriteRegData(0xD4U, reg_d0, sizeof(reg_d0));
    SPI_WriteRegData(0xD5U, reg_d1, sizeof(reg_d1));

    SPI_WriteComm(0x3AU);
    SPI_WriteData(0x55U);

    SPI_WriteComm(0x36U);
    SPI_WriteData(0x00U);

    SPI_WriteComm(0x29U);
    HAL_Delay(20U);
}
