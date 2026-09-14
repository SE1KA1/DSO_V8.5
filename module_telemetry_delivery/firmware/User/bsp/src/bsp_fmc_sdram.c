/**
 * @file bsp_fmc_sdram.c
 * @brief SDRAM 时序配置、读写自检、GUI/Record 堆基址依赖。
 */

#include "bsp.h"
#include "bsp_fmc_sdram.h"

/* 宏定义: SDRAM_TIMEOUT - SDRAM_TIMEOUT 的宏定义。 */
#define SDRAM_TIMEOUT          ((uint32_t)0xFFFFU)
/* 宏定义: SDRAM_REFRESH_COUNT - SDRAM_REFRESH_COUNT 的宏定义。 */
#define SDRAM_REFRESH_COUNT    ((uint32_t)761U)

/* 宏定义: SDRAM_MODEREG_BURST_LENGTH_1 - SDRAM_MODEREG_BURST_LENGTH_1 的宏定义。 */
#define SDRAM_MODEREG_BURST_LENGTH_1             ((uint16_t)0x0000U)
/* 宏定义: SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL - SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL 的宏定义。 */
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL      ((uint16_t)0x0000U)
/* 宏定义: SDRAM_MODEREG_CAS_LATENCY_3 - SDRAM_MODEREG_CAS_LATENCY_3 的宏定义。 */
#define SDRAM_MODEREG_CAS_LATENCY_3              ((uint16_t)0x0030U)
/* 宏定义: SDRAM_MODEREG_OPERATING_MODE_STANDARD - SDRAM_MODEREG_OPERATING_MODE_STANDARD 的宏定义。 */
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD    ((uint16_t)0x0000U)
/* 宏定义: SDRAM_MODEREG_WRITEBURST_MODE_SINGLE - SDRAM_MODEREG_WRITEBURST_MODE_SINGLE 的宏定义。 */
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE     ((uint16_t)0x0200U)

static SDRAM_HandleTypeDef g_hsdram;

/*
*********************************************************************************************************
*   函 数 名: SDRAM_GPIOConfig
*   功能说明: SDRAM_GPIOConfig 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void SDRAM_GPIOConfig(void)
{
    GPIO_InitTypeDef gpio_init;

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

/*
*********************************************************************************************************
*   函 数 名: SDRAM_Initialization_Sequence
*   功能说明: SDRAM_Initialization_Sequence 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void SDRAM_Initialization_Sequence(SDRAM_HandleTypeDef *hsdram)
{
    FMC_SDRAM_CommandTypeDef command;
    uint32_t tmpmrd;

    memset(&command, 0, sizeof(command));

    command.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
    command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
    command.AutoRefreshNumber = 1;
    command.ModeRegisterDefinition = 0;
    HAL_SDRAM_SendCommand(hsdram, &command, SDRAM_TIMEOUT);

    HAL_Delay(1);

    command.CommandMode = FMC_SDRAM_CMD_PALL;
    command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
    command.AutoRefreshNumber = 1;
    command.ModeRegisterDefinition = 0;
    HAL_SDRAM_SendCommand(hsdram, &command, SDRAM_TIMEOUT);

    command.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
    command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
    command.AutoRefreshNumber = 8;
    command.ModeRegisterDefinition = 0;
    HAL_SDRAM_SendCommand(hsdram, &command, SDRAM_TIMEOUT);

    tmpmrd = (uint32_t)SDRAM_MODEREG_BURST_LENGTH_1 |
             SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL |
             SDRAM_MODEREG_CAS_LATENCY_3 |
             SDRAM_MODEREG_OPERATING_MODE_STANDARD |
             SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;

    command.CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
    command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
    command.AutoRefreshNumber = 1;
    command.ModeRegisterDefinition = tmpmrd;
    HAL_SDRAM_SendCommand(hsdram, &command, SDRAM_TIMEOUT);

    HAL_SDRAM_ProgramRefreshRate(hsdram, SDRAM_REFRESH_COUNT);
}

/*
*********************************************************************************************************
*   函 数 名: bsp_InitExtSDRAM
*   功能说明: bsp_InitExtSDRAM 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void bsp_InitExtSDRAM(void)
{
    FMC_SDRAM_TimingTypeDef timing;

    SDRAM_GPIOConfig();

    memset(&g_hsdram, 0, sizeof(g_hsdram));
    memset(&timing, 0, sizeof(timing));

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

    timing.LoadToActiveDelay = 2;
    timing.ExitSelfRefreshDelay = 8;
    timing.SelfRefreshTime = 6;
    timing.RowCycleDelay = 6;
    timing.WriteRecoveryTime = 2;
    timing.RPDelay = 2;
    timing.RCDDelay = 2;

    if (HAL_SDRAM_Init(&g_hsdram, &timing) != HAL_OK)
    {
        Error_Handler(__FILE__, __LINE__);
    }

    SDRAM_Initialization_Sequence(&g_hsdram);
}

/*
*********************************************************************************************************
*   函 数 名: bsp_TestExtSDRAM1
*   功能说明: bsp_TestExtSDRAM1 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t bsp_TestExtSDRAM1(void)
{
    uint32_t i;
    uint32_t *p32;
    uint8_t *p8;
    uint32_t err;
    const uint8_t byte_buf[4] = {0x55U, 0xA5U, 0x5AU, 0xAAU};

    p32 = (uint32_t *)EXT_SDRAM_ADDR;
    for (i = 0; i < (EXT_SDRAM_SIZE / 4U); i++)
    {
        p32[i] = i;
    }

    err = 0;
    for (i = 0; i < (EXT_SDRAM_SIZE / 4U); i++)
    {
        if (p32[i] != i)
        {
            err++;
        }
    }
    if (err > 0U)
    {
        return (err * 4U);
    }

    for (i = 0; i < (EXT_SDRAM_SIZE / 4U); i++)
    {
        p32[i] = ~p32[i];
    }

    err = 0;
    for (i = 0; i < (EXT_SDRAM_SIZE / 4U); i++)
    {
        if (p32[i] != (~i))
        {
            err++;
        }
    }
    if (err > 0U)
    {
        return (err * 4U);
    }

    p8 = (uint8_t *)EXT_SDRAM_ADDR;
    for (i = 0; i < sizeof(byte_buf); i++)
    {
        p8[i] = byte_buf[i];
    }

    err = 0;
    for (i = 0; i < sizeof(byte_buf); i++)
    {
        if (p8[i] != byte_buf[i])
        {
            err++;
        }
    }

    return err;
}

/*
*********************************************************************************************************
*   函 数 名: bsp_TestExtSDRAM2
*   功能说明: bsp_TestExtSDRAM2 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t bsp_TestExtSDRAM2(void)
{
    uint32_t i;
    uint32_t *p32;
    uint8_t *p8;
    uint32_t err;
    const uint8_t byte_buf[4] = {0x55U, 0xA5U, 0x5AU, 0xAAU};

    p32 = (uint32_t *)SDRAM_APP_BUF;
    for (i = 0; i < (SDRAM_APP_SIZE / 4U); i++)
    {
        p32[i] = i;
    }

    err = 0;
    for (i = 0; i < (SDRAM_APP_SIZE / 4U); i++)
    {
        if (p32[i] != i)
        {
            err++;
        }
    }
    if (err > 0U)
    {
        return (err * 4U);
    }

    p8 = (uint8_t *)SDRAM_APP_BUF;
    for (i = 0; i < sizeof(byte_buf); i++)
    {
        p8[i] = byte_buf[i];
    }

    err = 0;
    for (i = 0; i < sizeof(byte_buf); i++)
    {
        if (p8[i] != byte_buf[i])
        {
            err++;
        }
    }

    return err;
}

/*
*********************************************************************************************************
*   函 数 名: bsp_TestExtSDRAM_Block
*   功能说明: bsp_TestExtSDRAM_Block 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t bsp_TestExtSDRAM_Block(uint32_t addr, uint32_t size_bytes)
{
    uint32_t i;
    uint32_t n;
    uint32_t *p32;
    uint32_t err;

    if (size_bytes < 4U || (size_bytes & 3U) != 0U) {
        return 1U;
    }
    n = size_bytes / 4U;
    p32 = (uint32_t *)addr;

    for (i = 0U; i < n; i++) {
        p32[i] = i;
    }
    err = 0U;
    for (i = 0U; i < n; i++) {
        if (p32[i] != i) {
            err++;
        }
    }
    if (err > 0U) {
        return err * 4U;
    }
    return 0U;
}
