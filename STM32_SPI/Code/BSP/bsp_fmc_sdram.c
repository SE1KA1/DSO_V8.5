/**
  ******************************************************************************
  * @file    bsp_fmc_sdram.c
  * @brief   External SDRAM bring-up aligned to the verified v7_h743 path
  ******************************************************************************
  */

#include "bsp_fmc_sdram.h"
#include <string.h>

extern void Error_Handler(void);

#define SDRAM_TIMEOUT          ((uint32_t)0xFFFFU)
#define SDRAM_REFRESH_COUNT    ((uint32_t)761U)

#define SDRAM_MODEREG_BURST_LENGTH_1             ((uint16_t)0x0000U)
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL      ((uint16_t)0x0000U)
#define SDRAM_MODEREG_CAS_LATENCY_3              ((uint16_t)0x0030U)
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD    ((uint16_t)0x0000U)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE     ((uint16_t)0x0200U)

static SDRAM_HandleTypeDef g_hsdram;

static void SDRAM_GPIOConfig(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_FMC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init.Alternate = GPIO_AF12_FMC;

    gpio_init.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_9 |
                    GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &gpio_init);

    gpio_init.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_7 | GPIO_PIN_8 |
                    GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 |
                    GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOE, &gpio_init);

    gpio_init.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                    GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_11 | GPIO_PIN_12 |
                    GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOF, &gpio_init);

    gpio_init.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 |
                    GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOG, &gpio_init);

    gpio_init.Pin = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOH, &gpio_init);
}

static void SDRAM_Initialization_Sequence(SDRAM_HandleTypeDef *hsdram)
{
    FMC_SDRAM_CommandTypeDef command = {0};
    uint32_t mode_register;

    command.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
    command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
    command.AutoRefreshNumber = 1U;
    command.ModeRegisterDefinition = 0U;
    if (HAL_SDRAM_SendCommand(hsdram, &command, SDRAM_TIMEOUT) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_Delay(1U);

    command.CommandMode = FMC_SDRAM_CMD_PALL;
    if (HAL_SDRAM_SendCommand(hsdram, &command, SDRAM_TIMEOUT) != HAL_OK)
    {
        Error_Handler();
    }

    command.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
    command.AutoRefreshNumber = 8U;
    if (HAL_SDRAM_SendCommand(hsdram, &command, SDRAM_TIMEOUT) != HAL_OK)
    {
        Error_Handler();
    }

    mode_register = (uint32_t)SDRAM_MODEREG_BURST_LENGTH_1 |
                    SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL |
                    SDRAM_MODEREG_CAS_LATENCY_3 |
                    SDRAM_MODEREG_OPERATING_MODE_STANDARD |
                    SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;

    command.CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
    command.AutoRefreshNumber = 1U;
    command.ModeRegisterDefinition = mode_register;
    if (HAL_SDRAM_SendCommand(hsdram, &command, SDRAM_TIMEOUT) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_SDRAM_ProgramRefreshRate(hsdram, SDRAM_REFRESH_COUNT) != HAL_OK)
    {
        Error_Handler();
    }
}

void bsp_InitExtSDRAM(void)
{
    FMC_SDRAM_TimingTypeDef timing = {0};

    SDRAM_GPIOConfig();

    memset(&g_hsdram, 0, sizeof(g_hsdram));
    g_hsdram.Instance = FMC_SDRAM_DEVICE;
    g_hsdram.Init.SDBank = FMC_SDRAM_BANK1;
    g_hsdram.Init.ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_9;
    g_hsdram.Init.RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_13;
    g_hsdram.Init.MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_16;
    g_hsdram.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
    g_hsdram.Init.CASLatency = FMC_SDRAM_CAS_LATENCY_3;
    g_hsdram.Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
    g_hsdram.Init.SDClockPeriod = FMC_SDRAM_CLOCK_PERIOD_2;
    g_hsdram.Init.ReadBurst = FMC_SDRAM_RBURST_ENABLE;
    g_hsdram.Init.ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_1;

    timing.LoadToActiveDelay = 2U;
    timing.ExitSelfRefreshDelay = 8U;
    timing.SelfRefreshTime = 6U;
    timing.RowCycleDelay = 6U;
    timing.WriteRecoveryTime = 2U;
    timing.RPDelay = 2U;
    timing.RCDDelay = 2U;

    if (HAL_SDRAM_Init(&g_hsdram, &timing) != HAL_OK)
    {
        Error_Handler();
    }

    SDRAM_Initialization_Sequence(&g_hsdram);
}

uint32_t bsp_TestExtSDRAM_Block(uint32_t addr, uint32_t size_bytes)
{
    uint32_t i;
    uint32_t count;
    uint32_t *p32;
    uint32_t errors = 0U;

    if ((size_bytes < 4U) || ((size_bytes & 3U) != 0U))
    {
        return 1U;
    }

    count = size_bytes / 4U;
    p32 = (uint32_t *)addr;

    for (i = 0U; i < count; i++)
    {
        p32[i] = i;
    }

    for (i = 0U; i < count; i++)
    {
        if (p32[i] != i)
        {
            errors++;
        }
    }

    if (errors != 0U)
    {
        return errors * 4U;
    }

    return 0U;
}
