/**
 * @file cloud_bridge.c
 * @brief Cloud bridge FPGA PS-mode configuration.
 *
 * Pin mapping (shared with main XC7A100 config bus):
 *   PC6  = nCONFIG (PROGRAM_B, active LOW)
 *   PC13 = CONF_DONE (input with pull-up, HIGH = config complete)
 *   PG12 = DCLK    (configuration clock, bit-banged GPIO)
 *   PI8  = DATA0   (configuration data, MSB-first per byte)
 */
#include "cloud_bridge.h"
#include "bsp_qspi_w25q256.h"
#include "stm32h7xx_hal.h"
#include <string.h>

/* ---- internal helpers -------------------------------------------------- */

static void dclk_pulse(void)
{
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_12, GPIO_PIN_SET);
    for (volatile uint32_t d = 0; d < CB_CFG_DCLK_HALF_US * 50; d++) {}
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_12, GPIO_PIN_RESET);
    for (volatile uint32_t d = 0; d < CB_CFG_DCLK_HALF_US * 50; d++) {}
}

static void send_byte(uint8_t b)
{
    for (int i = 7; i >= 0; i--) {
        HAL_GPIO_WritePin(GPIOI, GPIO_PIN_8, (b >> i) & 1);
        dclk_pulse();
    }
}

static void config_pins_output(void)
{
    /* Clocks already enabled in System_Init() */
    GPIO_InitTypeDef g = {0};

    /* PC6  = nCONFIG output */
    g.Pin = GPIO_PIN_6;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &g);

    /* PG12 = DCLK output */
    g.Pin = GPIO_PIN_12;
    HAL_GPIO_Init(GPIOG, &g);

    /* PI8  = DATA0 output */
    g.Pin = GPIO_PIN_8;
    HAL_GPIO_Init(GPIOI, &g);

    /* Start with nCONFIG HIGH, DCLK LOW, DATA0 LOW */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_12, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_8, GPIO_PIN_RESET);
}

/* ---- public API -------------------------------------------------------- */

void CloudBridge_Isolate(void)
{
    /* Already handled in System_Init(): PC6 LOW, DCLK LOW, DATA0 tristate.
     * This function is a placeholder for any additional early-stage
     * isolation needed before FMC/SDRAM access. */
}

CloudBridgeStatus CloudBridge_Configure(void)
{
    CloudBridgeStatus ret = CB_OK;
    static uint8_t chunk[4096];  /* 4KB chunk buffer in .bss */

    config_pins_output();

    /* 1. Pulse nCONFIG LOW >= 2us to initiate configuration */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
    for (volatile uint32_t d = 0; d < 200; d++) {}
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
    for (volatile uint32_t d = 0; d < 200; d++) {}

    HAL_Delay(1);  /* 1ms settling */

#if 0  /* TODO: enable after QSPI Flash is programmed with FPGA bitstream */
    /* 3. Init QSPI and stream bitstream */
    bsp_InitQSPI_W25Q256();
    uint32_t offset = CLOUD_BRIDGE_BITSTREAM_OFFSET;
    uint32_t remaining = CLOUD_BRIDGE_BITSTREAM_MAX_SIZE;
    printf("[CB] streaming %lu bytes @ 0x%lX...\r\n",
           (unsigned long)remaining, (unsigned long)offset);
    while (remaining > 0) {
        uint32_t n = (remaining > sizeof(chunk)) ? sizeof(chunk) : remaining;
        QSPI_ReadBuffer(chunk, offset, n);
        for (uint32_t i = 0; i < n; i++) send_byte(chunk[i]);
        offset += n;
        remaining -= n;
    }
#endif

    /* 5. Wait for CONF_DONE (PC13) to go HIGH */
    uint32_t timeout = 0;
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_13;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &g);

    while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {
        timeout++;
        if (timeout > CB_CFG_T_DONE_MAX / 10) {
            printf("[CB] CONF_DONE timeout\r\n");
            ret = CB_ERR_CONF_DONE;
            goto done;
        }
        HAL_Delay(10);
    }

    /* 6. Send extra DCLK cycles to complete initialization */
    for (int i = 0; i < 8; i++) {
        dclk_pulse();
    }

    printf("[CB] FPGA configured OK\r\n");

done:
    /* PC6: restore as OUTPUT HIGH (FPGA nCONFIG released) */
    g.Pin = GPIO_PIN_6;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &g);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
    /* PG12/PI8: keep as-is to avoid GPIO port glitch conflicts */

    return ret;
}

int CloudBridge_IsConfigured(void)
{
    return (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET) ? 1 : 0;
}
