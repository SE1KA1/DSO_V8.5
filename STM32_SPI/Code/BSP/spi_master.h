/**
  ******************************************************************************
  * @file    spi_master.h
  * @brief   SPI master driver header for STM32H743
  *          Compatible with XO2-4000HC SPI slave communication
  ******************************************************************************
  */

#ifndef __SPI_MASTER_H
#define __SPI_MASTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"

/* SPI Frame Size: 96 bits = 12 bytes.
 * Bytes 0..5 keep the legacy control payload; bytes 6..11 carry XC7A PO/PIO. */
#define SPI_FRAME_SIZE      12
#define SPI1_DMA_BUFFER_SIZE 32U
#define SPI1_DMA_DEADLINE_US 75U
#define SPI1_CS_SETUP_US     1U
#define SPI1_CS_HOLD_US      1U

#if defined(__GNUC__)
#define SPI1_DMA_ALIGNED __attribute__((aligned(32)))
#else
#define SPI1_DMA_ALIGNED
#endif

/* SPI1 kernel clock comes from PLL in SystemClock_Config() (~200 MHz).
 * 25 MHz is the ONLY stable SCLK on current hardware.
 * 50/100/200MHz presets are not usable because hardware errors above 25MHz. */
#define SPI1_BAUD_PRESCALER_25MHZ       SPI_BAUDRATEPRESCALER_8
/* Faster presets are compile-time diagnostics only; do not use them on hardware. */
#define SPI1_BAUD_PRESCALER_50MHZ       SPI_BAUDRATEPRESCALER_4   /* unsupported */
#define SPI1_BAUD_PRESCALER_100MHZ      SPI_BAUDRATEPRESCALER_2   /* unsupported */
#define SPI1_BAUD_PRESCALER_200MHZ      SPI_BAUDRATEPRESCALER_2   /* unattainable */

#ifndef SPI1_BAUD_PRESCALER_TEST
#define SPI1_BAUD_PRESCALER_TEST        SPI1_BAUD_PRESCALER_25MHZ
#endif

#ifndef SPI1_BAUD_LABEL
#if (SPI1_BAUD_PRESCALER_TEST == SPI1_BAUD_PRESCALER_25MHZ)
#define SPI1_BAUD_LABEL                 "25MHz"
#elif (SPI1_BAUD_PRESCALER_TEST == SPI1_BAUD_PRESCALER_50MHZ)
#define SPI1_BAUD_LABEL                 "50MHz"
#elif (SPI1_BAUD_PRESCALER_TEST == SPI1_BAUD_PRESCALER_100MHZ)
#define SPI1_BAUD_LABEL                 "100MHz"
#else
#define SPI1_BAUD_LABEL                 "CUSTOM"
#endif
#endif

/* XO2 SPI link on the current board:
 *   PA15 = CS   (software-controlled GPIO)
 *   PB3  = SCK  (AF5_SPI1)
 *   PG9  = MISO (AF5_SPI1)
 *   PD7  = MOSI (AF5_SPI1)
 * Keep the SPI1_* names so main.c stays unchanged. */
#define SPI1_CS_GPIO_Port   GPIOA
#define SPI1_CS_Pin         GPIO_PIN_15
#define SPI1_SCK_GPIO_Port  GPIOB
#define SPI1_SCK_Pin        GPIO_PIN_3
#define SPI1_MISO_GPIO_Port GPIOG
#define SPI1_MISO_Pin       GPIO_PIN_9
#define SPI1_MOSI_GPIO_Port GPIOD
#define SPI1_MOSI_Pin       GPIO_PIN_7

void SPI1_Master_Init(void);

typedef enum
{
    SPI1_TRANSFER_IDLE = 0,
    SPI1_TRANSFER_BUSY,
    SPI1_TRANSFER_COMPLETE,
    SPI1_TRANSFER_ERROR,
    SPI1_TRANSFER_TIMEOUT
} SPI1_TransferResult;

HAL_StatusTypeDef SPI1_TransferStart(uint8_t *tx_buf, uint8_t *rx_buf, uint16_t size);
SPI1_TransferResult SPI1_TransferPoll(void);
uint8_t SPI1_TransferIsBusy(void);
uint32_t SPI1_GetLastError(void);

extern SPI_HandleTypeDef hspi1;

#ifdef __cplusplus
}
#endif

#endif /* __SPI_MASTER_H */
