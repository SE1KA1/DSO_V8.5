/**
  ******************************************************************************
  * @file    bsp_lcd_rgb.c
  * @brief   Minimum panel + LTDC bring-up aligned to the verified v7_h743 path
  ******************************************************************************
  */

#include "bsp_lcd_rgb.h"
#include <string.h>

extern void Error_Handler(void);

LTDC_HandleTypeDef g_hltdc;

typedef struct
{
    uint32_t hsw;
    uint32_t hbp;
    uint32_t hfp;
    uint32_t vsw;
    uint32_t vbp;
    uint32_t vfp;
    uint32_t pll3n;
    uint32_t pll3q;
    uint32_t pll3r;
} LCD_RGB_TimingTypeDef;

#define LCD_PANEL_CS_GPIO_Port     GPIOH
#define LCD_PANEL_CS_Pin           GPIO_PIN_6
#define LCD_PANEL_SCK_GPIO_Port    GPIOH
#define LCD_PANEL_SCK_Pin          GPIO_PIN_7
#define LCD_PANEL_SDA_GPIO_Port    GPIOH
#define LCD_PANEL_SDA_Pin          GPIO_PIN_8

#define LCD_BL_GPIO_Port           GPIOB
#define LCD_BL_Pin                 GPIO_PIN_14

#define LCD_PANEL_CS_HIGH()  HAL_GPIO_WritePin(LCD_PANEL_CS_GPIO_Port, LCD_PANEL_CS_Pin, GPIO_PIN_SET)
#define LCD_PANEL_CS_LOW()   HAL_GPIO_WritePin(LCD_PANEL_CS_GPIO_Port, LCD_PANEL_CS_Pin, GPIO_PIN_RESET)
#define LCD_PANEL_SCK_HIGH() HAL_GPIO_WritePin(LCD_PANEL_SCK_GPIO_Port, LCD_PANEL_SCK_Pin, GPIO_PIN_SET)
#define LCD_PANEL_SCK_LOW()  HAL_GPIO_WritePin(LCD_PANEL_SCK_GPIO_Port, LCD_PANEL_SCK_Pin, GPIO_PIN_RESET)
#define LCD_PANEL_SDA_HIGH() HAL_GPIO_WritePin(LCD_PANEL_SDA_GPIO_Port, LCD_PANEL_SDA_Pin, GPIO_PIN_SET)
#define LCD_PANEL_SDA_LOW()  HAL_GPIO_WritePin(LCD_PANEL_SDA_GPIO_Port, LCD_PANEL_SDA_Pin, GPIO_PIN_RESET)

static void LCD_RGB_GetTiming(LCD_RGB_TimingTypeDef *timing)
{
    timing->hsw = 1U;
    timing->hbp = 1U;
    timing->hfp = 1U;
    timing->vsw = 1U;
    timing->vbp = 1U;
    timing->vfp = 1U;
    timing->pll3n = 88U;
    timing->pll3q = 4U;
    timing->pll3r = 18U;
}

static void LCD_RGB_PanelDelayUS(void)
{
    volatile uint32_t cycles = SystemCoreClock / 5000000U;
    while (cycles-- > 0U)
    {
        __NOP();
    }
}

static void LCD_RGB_PanelInitGPIO(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_GPIOH_CLK_ENABLE();

    gpio_init.Pin = LCD_PANEL_CS_Pin | LCD_PANEL_SCK_Pin | LCD_PANEL_SDA_Pin;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOH, &gpio_init);

    LCD_PANEL_CS_HIGH();
    LCD_PANEL_SCK_HIGH();
    LCD_PANEL_SDA_HIGH();
}

static void LCD_RGB_DisableLegacyCsLine(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_GPIOG_CLK_ENABLE();

    gpio_init.Pin = GPIO_PIN_10;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &gpio_init);
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_10, GPIO_PIN_SET);
}

static void LCD_RGB_PanelWriteByte(uint8_t value)
{
    uint8_t i;

    for (i = 0U; i < 8U; i++)
    {
        LCD_PANEL_SCK_LOW();
        if ((value & 0x80U) != 0U)
        {
            LCD_PANEL_SDA_HIGH();
        }
        else
        {
            LCD_PANEL_SDA_LOW();
        }

        LCD_RGB_PanelDelayUS();
        LCD_PANEL_SCK_HIGH();
        LCD_RGB_PanelDelayUS();
        value <<= 1U;
    }
}

static void LCD_RGB_PanelWriteCmd(uint8_t cmd)
{
    LCD_PANEL_CS_LOW();
    LCD_RGB_PanelWriteByte(0x70U);
    LCD_RGB_PanelWriteByte(cmd);
    LCD_PANEL_CS_HIGH();
}

static void LCD_RGB_PanelWriteData(uint8_t data)
{
    LCD_PANEL_CS_LOW();
    LCD_RGB_PanelWriteByte(0x72U);
    LCD_RGB_PanelWriteByte(data);
    LCD_PANEL_CS_HIGH();
}

static void LCD_RGB_PanelWriteRegData(uint8_t reg, const uint8_t *data, uint32_t len)
{
    uint32_t i;

    LCD_RGB_PanelWriteCmd(reg);
    for (i = 0U; i < len; i++)
    {
        LCD_RGB_PanelWriteData(data[i]);
    }
}

static void LCD_RGB_InitPanelRegisters(void)
{
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

    HAL_Delay(20U);

    LCD_RGB_PanelWriteCmd(0x01U);
    HAL_Delay(10U);

    LCD_RGB_PanelWriteCmd(0x11U);
    HAL_Delay(120U);

    LCD_RGB_PanelWriteRegData(0xB0U, reg_b0, sizeof(reg_b0));
    LCD_RGB_PanelWriteRegData(0xB1U, reg_b1, sizeof(reg_b1));
    LCD_RGB_PanelWriteRegData(0xB2U, reg_b2, sizeof(reg_b2));
    LCD_RGB_PanelWriteRegData(0xB3U, reg_b3, sizeof(reg_b3));
    LCD_RGB_PanelWriteRegData(0xB4U, reg_b4, sizeof(reg_b4));
    LCD_RGB_PanelWriteRegData(0xB5U, reg_b5, sizeof(reg_b5));
    LCD_RGB_PanelWriteRegData(0xB6U, reg_b6, sizeof(reg_b6));
    LCD_RGB_PanelWriteRegData(0xC0U, reg_c0, sizeof(reg_c0));
    LCD_RGB_PanelWriteRegData(0xC3U, reg_c3, sizeof(reg_c3));
    HAL_Delay(10U);

    LCD_RGB_PanelWriteRegData(0xC4U, reg_c4, sizeof(reg_c4));
    HAL_Delay(10U);
    LCD_RGB_PanelWriteRegData(0xC5U, reg_c5, sizeof(reg_c5));
    HAL_Delay(10U);
    LCD_RGB_PanelWriteRegData(0xC6U, reg_c6, sizeof(reg_c6));
    HAL_Delay(10U);

    LCD_RGB_PanelWriteRegData(0xD0U, reg_d0, sizeof(reg_d0));
    LCD_RGB_PanelWriteRegData(0xD1U, reg_d1, sizeof(reg_d1));
    LCD_RGB_PanelWriteRegData(0xD2U, reg_d0, sizeof(reg_d0));
    LCD_RGB_PanelWriteRegData(0xD3U, reg_d1, sizeof(reg_d1));
    LCD_RGB_PanelWriteRegData(0xD4U, reg_d0, sizeof(reg_d0));
    LCD_RGB_PanelWriteRegData(0xD5U, reg_d1, sizeof(reg_d1));

    LCD_RGB_PanelWriteCmd(0x3AU);
    LCD_RGB_PanelWriteData(0x55U);

    LCD_RGB_PanelWriteCmd(0x36U);
    LCD_RGB_PanelWriteData(0x00U);

    HAL_Delay(10U);
    LCD_RGB_PanelWriteCmd(0x29U);
    HAL_Delay(20U);
}

static void LCD_RGB_InitBacklightGPIO(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio_init.Pin = LCD_BL_Pin;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(LCD_BL_GPIO_Port, &gpio_init);

    LCD_RGB_BacklightOff();
}

static void LCD_RGB_InitClock(const LCD_RGB_TimingTypeDef *timing)
{
    RCC_PeriphCLKInitTypeDef periph_clk_init = {0};

    periph_clk_init.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    periph_clk_init.PLL3.PLL3M = 5U;
    periph_clk_init.PLL3.PLL3N = timing->pll3n;
    periph_clk_init.PLL3.PLL3P = 2U;
    periph_clk_init.PLL3.PLL3Q = timing->pll3q;
    periph_clk_init.PLL3.PLL3R = timing->pll3r;
    periph_clk_init.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_2;
    periph_clk_init.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
    periph_clk_init.PLL3.PLL3FRACN = 0U;

    if (HAL_RCCEx_PeriphCLKConfig(&periph_clk_init) != HAL_OK)
    {
        Error_Handler();
    }
}

static void LCD_RGB_InitLTDCGPIO(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();

    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init.Alternate = GPIO_AF14_LTDC;

    gpio_init.Pin = GPIO_PIN_10;
    HAL_GPIO_Init(GPIOF, &gpio_init);

    gpio_init.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOG, &gpio_init);

    gpio_init.Pin = GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 |
                    GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOH, &gpio_init);

    gpio_init.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_4 |
                    GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_9 |
                    GPIO_PIN_10;
    HAL_GPIO_Init(GPIOI, &gpio_init);

    gpio_init.Pin = GPIO_PIN_8;
    HAL_GPIO_Init(GPIOA, &gpio_init);
}

void LCD_RGB_Init(void)
{
    LTDC_LayerCfgTypeDef layer_cfg = {0};
    LCD_RGB_TimingTypeDef timing;

    LCD_RGB_GetTiming(&timing);

    __HAL_RCC_LTDC_CLK_ENABLE();

    LCD_RGB_InitBacklightGPIO();
    LCD_RGB_PanelInitGPIO();
    LCD_RGB_InitPanelRegisters();
    LCD_RGB_DisableLegacyCsLine();
    LCD_RGB_InitClock(&timing);
    LCD_RGB_InitLTDCGPIO();

    memset(&g_hltdc, 0, sizeof(g_hltdc));
    g_hltdc.Instance = LTDC;
    g_hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AH;
    g_hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AH;
    g_hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AH;
    g_hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IIPC;
    g_hltdc.Init.HorizontalSync = timing.hsw - 1U;
    g_hltdc.Init.VerticalSync = timing.vsw - 1U;
    g_hltdc.Init.AccumulatedHBP = timing.hsw + timing.hbp - 1U;
    g_hltdc.Init.AccumulatedVBP = timing.vsw + timing.vbp - 1U;
    g_hltdc.Init.AccumulatedActiveW = timing.hsw + timing.hbp + LCD_RGB_WIDTH - 1U;
    g_hltdc.Init.AccumulatedActiveH = timing.vsw + timing.vbp + LCD_RGB_HEIGHT - 1U;
    g_hltdc.Init.TotalWidth = timing.hsw + timing.hbp + LCD_RGB_WIDTH + timing.hfp - 1U;
    g_hltdc.Init.TotalHeigh = timing.vsw + timing.vbp + LCD_RGB_HEIGHT + timing.vfp - 1U;
    g_hltdc.Init.Backcolor.Red = 0U;
    g_hltdc.Init.Backcolor.Green = 0U;
    g_hltdc.Init.Backcolor.Blue = 0U;

    if (HAL_LTDC_Init(&g_hltdc) != HAL_OK)
    {
        Error_Handler();
    }

    layer_cfg.WindowX0 = 0U;
    layer_cfg.WindowX1 = LCD_RGB_WIDTH;
    layer_cfg.WindowY0 = 0U;
    layer_cfg.WindowY1 = LCD_RGB_HEIGHT;
    layer_cfg.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
    layer_cfg.FBStartAdress = LCD_RGB_FB_ADDR;
    layer_cfg.Alpha = 255U;
    layer_cfg.Alpha0 = 0U;
    layer_cfg.Backcolor.Red = 0U;
    layer_cfg.Backcolor.Green = 0U;
    layer_cfg.Backcolor.Blue = 0U;
    layer_cfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
    layer_cfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
    layer_cfg.ImageWidth = LCD_RGB_WIDTH;
    layer_cfg.ImageHeight = LCD_RGB_HEIGHT;

    if (HAL_LTDC_ConfigLayer(&g_hltdc, &layer_cfg, LTDC_LAYER_1) != HAL_OK)
    {
        Error_Handler();
    }
}

void LCD_RGB_BacklightOn(void)
{
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);
}

void LCD_RGB_BacklightOff(void)
{
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_RESET);
}
