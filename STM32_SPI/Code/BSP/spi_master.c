/**
  ******************************************************************************
  * @file    spi_master.c
  * @brief   SPI1 master driver for STM32H743 (XO2 link on PA15/PB3/PG9/PD7).
  *          STM32 actively drives CS / SCK / MOSI.
  *          XO2-4000HC FPGA acts as SPI slave and returns 12 bytes per frame.
  *          Frame: 96 bits, CPOL=0 / CPHA=0, MSB-first.
  *          Pin map:
  *              PB3  = SPI1_SCK  (AF5)
  *              PG9  = SPI1_MISO (AF5)
  *              PD7  = SPI1_MOSI (AF5)
  *              PA15 = SPI1_CS   (software CS, GPIO output)
  ******************************************************************************
  */

#include "spi_master.h"
extern void Error_Handler(void);

/* SPI Handle */
SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_rx;
DMA_HandleTypeDef hdma_spi1_tx;

#define SPI1_DCACHE_LINE_SIZE 32U

typedef enum
{
    SPI1_DMA_EVENT_NONE = 0,
    SPI1_DMA_EVENT_COMPLETE,
    SPI1_DMA_EVENT_ERROR
} SPI1_DmaEvent;

static volatile SPI1_DmaEvent spi1_dma_event = SPI1_DMA_EVENT_NONE;
static uint8_t spi1_transfer_busy = 0U;
static uint8_t *spi1_rx_address = NULL;
static uint16_t spi1_transfer_size = 0U;
static uint32_t spi1_deadline_cycles = 0U;
static uint32_t spi1_last_error = HAL_SPI_ERROR_NONE;

static void SPI1_DMA_Init(void);
static void SPI1_DWT_Init(void);
static void SPI1_DelayCycles(uint32_t cycles);
static void SPI1_ReleaseCs(void);
static void SPI1_CleanDCacheRegion(const void *addr, uint32_t size);
static void SPI1_InvalidateDCacheRegion(const void *addr, uint32_t size);

/**
  * @brief  Initialize SPI1 as Master
  *         CPOL=0, CPHA=0, 8-bit, MSB first, software NSS
  */
void SPI1_Master_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable clocks */
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /* SCK = PB3 (AF5_SPI1) */
    GPIO_InitStruct.Pin = SPI1_SCK_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(SPI1_SCK_GPIO_Port, &GPIO_InitStruct);

    /* MISO = PG9 (AF5_SPI1) */
    GPIO_InitStruct.Pin = SPI1_MISO_Pin;
    HAL_GPIO_Init(SPI1_MISO_GPIO_Port, &GPIO_InitStruct);

    /* MOSI = PD7 (AF5_SPI1) */
    GPIO_InitStruct.Pin = SPI1_MOSI_Pin;
    HAL_GPIO_Init(SPI1_MOSI_GPIO_Port, &GPIO_InitStruct);

    /* CS = PA15, software-controlled GPIO output */
    GPIO_InitStruct.Pin = SPI1_CS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = 0;
    HAL_GPIO_Init(SPI1_CS_GPIO_Port, &GPIO_InitStruct);

    /* Idle CS high */
    HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET);

    /* Configure SPI1 as Master */
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI1_BAUD_PRESCALER_TEST;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 0x0;
    hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
    hspi1.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
    hspi1.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
    hspi1.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    hspi1.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    hspi1.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
    hspi1.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
    hspi1.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
    hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
    hspi1.Init.IOSwap = SPI_IO_SWAP_DISABLE;

    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        Error_Handler();
    }

    SPI1_DMA_Init();
    SPI1_DWT_Init();
}

HAL_StatusTypeDef SPI1_TransferStart(uint8_t *tx_buf, uint8_t *rx_buf, uint16_t size)
{
    HAL_StatusTypeDef status;
    uint32_t cycles_per_us;

    if ((tx_buf == NULL) || (rx_buf == NULL) || (size == 0U))
    {
        return HAL_ERROR;
    }

    if (spi1_transfer_busy != 0U)
    {
        return HAL_BUSY;
    }

    spi1_dma_event = SPI1_DMA_EVENT_NONE;
    spi1_last_error = HAL_SPI_ERROR_NONE;
    spi1_rx_address = rx_buf;
    spi1_transfer_size = size;

    SPI1_CleanDCacheRegion(tx_buf, size);
    SPI1_InvalidateDCacheRegion(rx_buf, size);

    HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_RESET);

    /* XO2 synchronizes CS into its 50 MHz domain and snapshots the complete
     * status packet before the first SCLK edge. Keep this interval explicit
     * and independent of compiler optimization. */
    cycles_per_us = SystemCoreClock / 1000000U;
    SPI1_DelayCycles(cycles_per_us * SPI1_CS_SETUP_US);

    status = HAL_SPI_TransmitReceive_DMA(&hspi1, tx_buf, rx_buf, size);
    if (status != HAL_OK)
    {
        spi1_last_error = HAL_SPI_GetError(&hspi1);
        (void)HAL_SPI_Abort(&hspi1);
        spi1_last_error |= HAL_SPI_GetError(&hspi1);
        SPI1_ReleaseCs();
        spi1_rx_address = NULL;
        spi1_transfer_size = 0U;
        return status;
    }

    spi1_deadline_cycles = DWT->CYCCNT + (cycles_per_us * SPI1_DMA_DEADLINE_US);
    spi1_transfer_busy = 1U;
    return HAL_OK;
}

SPI1_TransferResult SPI1_TransferPoll(void)
{
    SPI1_DmaEvent event;

    if (spi1_transfer_busy == 0U)
    {
        return SPI1_TRANSFER_IDLE;
    }

    event = spi1_dma_event;
    if (event == SPI1_DMA_EVENT_COMPLETE)
    {
        spi1_dma_event = SPI1_DMA_EVENT_NONE;
        spi1_transfer_busy = 0U;
        SPI1_InvalidateDCacheRegion(spi1_rx_address, spi1_transfer_size);
        SPI1_ReleaseCs();
        spi1_rx_address = NULL;
        spi1_transfer_size = 0U;
        spi1_last_error = HAL_SPI_ERROR_NONE;
        return SPI1_TRANSFER_COMPLETE;
    }

    if (event == SPI1_DMA_EVENT_ERROR)
    {
        spi1_dma_event = SPI1_DMA_EVENT_NONE;
        spi1_last_error = HAL_SPI_GetError(&hspi1);
        (void)HAL_SPI_Abort(&hspi1);
        spi1_last_error |= HAL_SPI_GetError(&hspi1);
        spi1_transfer_busy = 0U;
        SPI1_ReleaseCs();
        spi1_rx_address = NULL;
        spi1_transfer_size = 0U;
        return SPI1_TRANSFER_ERROR;
    }

    if ((int32_t)(DWT->CYCCNT - spi1_deadline_cycles) >= 0)
    {
        spi1_last_error = HAL_SPI_GetError(&hspi1);
        (void)HAL_SPI_Abort(&hspi1);
        spi1_last_error |= HAL_SPI_GetError(&hspi1);
        spi1_dma_event = SPI1_DMA_EVENT_NONE;
        spi1_transfer_busy = 0U;
        SPI1_ReleaseCs();
        spi1_rx_address = NULL;
        spi1_transfer_size = 0U;
        return SPI1_TRANSFER_TIMEOUT;
    }

    return SPI1_TRANSFER_BUSY;
}

uint8_t SPI1_TransferIsBusy(void)
{
    return spi1_transfer_busy;
}

uint32_t SPI1_GetLastError(void)
{
    return spi1_last_error;
}

static void SPI1_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    hdma_spi1_rx.Instance = DMA1_Stream0;
    hdma_spi1_rx.Init.Request = DMA_REQUEST_SPI1_RX;
    hdma_spi1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_spi1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_spi1_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_spi1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_spi1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_spi1_rx.Init.Mode = DMA_NORMAL;
    hdma_spi1_rx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_spi1_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_spi1_rx) != HAL_OK)
    {
        Error_Handler();
    }
    __HAL_LINKDMA(&hspi1, hdmarx, hdma_spi1_rx);

    hdma_spi1_tx.Instance = DMA1_Stream1;
    hdma_spi1_tx.Init.Request = DMA_REQUEST_SPI1_TX;
    hdma_spi1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_spi1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_spi1_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_spi1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_spi1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_spi1_tx.Init.Mode = DMA_NORMAL;
    hdma_spi1_tx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_spi1_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_spi1_tx) != HAL_OK)
    {
        Error_Handler();
    }
    __HAL_LINKDMA(&hspi1, hdmatx, hdma_spi1_tx);

    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
    HAL_NVIC_SetPriority(SPI1_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(SPI1_IRQn);
}

static void SPI1_DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void SPI1_DelayCycles(uint32_t cycles)
{
    const uint32_t start = DWT->CYCCNT;

    while ((uint32_t)(DWT->CYCCNT - start) < cycles)
    {
    }
}

static void SPI1_ReleaseCs(void)
{
    const uint32_t cycles_per_us = SystemCoreClock / 1000000U;

    SPI1_DelayCycles(cycles_per_us * SPI1_CS_HOLD_US);
    HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET);
}

static void SPI1_CleanDCacheRegion(const void *addr, uint32_t size)
{
    uintptr_t start;
    uintptr_t end;

    if ((addr == NULL) || (size == 0U))
    {
        return;
    }

    start = ((uintptr_t)addr) & ~(uintptr_t)(SPI1_DCACHE_LINE_SIZE - 1U);
    end = ((uintptr_t)addr + size + (SPI1_DCACHE_LINE_SIZE - 1U)) &
          ~(uintptr_t)(SPI1_DCACHE_LINE_SIZE - 1U);
    SCB_CleanDCache_by_Addr((uint32_t *)start, (int32_t)(end - start));
}

static void SPI1_InvalidateDCacheRegion(const void *addr, uint32_t size)
{
    uintptr_t start;
    uintptr_t end;

    if ((addr == NULL) || (size == 0U))
    {
        return;
    }

    start = ((uintptr_t)addr) & ~(uintptr_t)(SPI1_DCACHE_LINE_SIZE - 1U);
    end = ((uintptr_t)addr + size + (SPI1_DCACHE_LINE_SIZE - 1U)) &
          ~(uintptr_t)(SPI1_DCACHE_LINE_SIZE - 1U);
    SCB_InvalidateDCache_by_Addr((uint32_t *)start, (int32_t)(end - start));
}

void DMA1_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_spi1_rx);
}

void DMA1_Stream1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_spi1_tx);
}

void SPI1_IRQHandler(void)
{
    HAL_SPI_IRQHandler(&hspi1);
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
    {
        spi1_dma_event = SPI1_DMA_EVENT_COMPLETE;
    }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
    {
        spi1_dma_event = SPI1_DMA_EVENT_ERROR;
    }
}
