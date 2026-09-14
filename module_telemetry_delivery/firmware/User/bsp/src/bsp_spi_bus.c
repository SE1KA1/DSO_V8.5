/**
 * @file bsp_spi_bus.c
 * @brief 安富莱 BSP 通用驱动（SPI 总线）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : SPI��������
*	�ļ����� : bsp_spi_bus1.h
*	��    �� : V1.2
*	˵    �� : SPI���ߵײ��������ṩSPI���á��շ����ݡ����豸����SPI����.
*	�޸ļ�¼ :
*		�汾��  ����        ����    ˵��
*       v1.0    2014-10-24 armfly  �װ档������FLASH��TSC2046��VS1053��AD7705��ADS1256��SPI�豸������
*									���շ����ݵĺ������л��ܷ��ࡣ�������ͬ�ٶȵ��豸��Ĺ������⡣
*		V1.1	2015-02-25 armfly  Ӳ��SPIʱ��û�п���GPIOBʱ�ӣ��ѽ����
*		V1.2	2015-07-23 armfly  �޸� bsp_SPI_Init() ���������ӿ���SPIʱ�ӵ���䡣�淶Ӳ��SPI������SPI�ĺ궨��.
*
*	Copyright (C), 2015-2016, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/*
	������STM32-V7��������߷���
	PB3/SPI3_SCK/SPI1_SCK
	PB4/SPI3_MISO/SPI1_MISO
	PB5/SPI3_MOSI/SPI1_MOSI	
*/

/* 
	armfly �� 2018-10-03  SPI ����������� (����HAL�����ȱ�ݣ�
	1. DMAģʽ�����ͻ����������� const ��λ��flash�����������䡣��λ��SRAM����DMA�����쳣��
	2. INTģʽ��32�ֽ�һ����������������48�ֽ������쳣��ֻ�ܷ���һ�Ρ���2�λ���;��ֹ��
	3. Ŀǰ������ͨ��ѯģʽ��δ�������⡣
	
	���Դ������£�
	bsp_InitSPIBus();
	bsp_InitSPIParam(SPI_BAUDRATEPRESCALER_8, SPI_PHASE_1EDGE, SPI_POLARITY_LOW);
	while (1)
	{
		g_spiLen = 32;		// ���ݳ���
		bsp_spiTransfer();
		
		bsp_DelayUS(1000);
	}
*/
//#define USE_SPI_DMA
//#define USE_SPI_INT
#define USE_SPI_POLL

/* 宏定义: SPIx - SPIx 的宏定义。 */
#define SPIx						SPI1

/* 宏定义: SPIx_CLK_ENABLE - SPIx_CLK_ENABLE 的宏定义。 */
#define SPIx_CLK_ENABLE()			__HAL_RCC_SPI1_CLK_ENABLE()

/* 宏定义: DMAx_CLK_ENABLE - DMAx_CLK_ENABLE 的宏定义。 */
#define DMAx_CLK_ENABLE()			__HAL_RCC_DMA2_CLK_ENABLE()

/* 宏定义: SPIx_FORCE_RESET - SPIx_FORCE_RESET 的宏定义。 */
#define SPIx_FORCE_RESET()			__HAL_RCC_SPI1_FORCE_RESET()
/* 宏定义: SPIx_RELEASE_RESET - SPIx_RELEASE_RESET 的宏定义。 */
#define SPIx_RELEASE_RESET()		__HAL_RCC_SPI1_RELEASE_RESET()

/* 宏定义: SPIx_SCK_CLK_ENABLE - SPIx_SCK_CLK_ENABLE 的宏定义。 */
#define SPIx_SCK_CLK_ENABLE()		__HAL_RCC_GPIOB_CLK_ENABLE()
/* 宏定义: SPIx_SCK_GPIO - SPIx_SCK_GPIO 的宏定义。 */
#define SPIx_SCK_GPIO				GPIOB
/* 宏定义: SPIx_SCK_PIN - SPIx_SCK_PIN 的宏定义。 */
#define SPIx_SCK_PIN				GPIO_PIN_3
/* 宏定义: SPIx_SCK_AF - SPIx_SCK_AF 的宏定义。 */
#define SPIx_SCK_AF					GPIO_AF5_SPI1

/* 宏定义: SPIx_MISO_CLK_ENABLE - SPIx_MISO_CLK_ENABLE 的宏定义。 */
#define SPIx_MISO_CLK_ENABLE()		__HAL_RCC_GPIOB_CLK_ENABLE()
/* 宏定义: SPIx_MISO_GPIO - SPIx_MISO_GPIO 的宏定义。 */
#define SPIx_MISO_GPIO				GPIOB
/* 宏定义: SPIx_MISO_PIN - SPIx_MISO_PIN 的宏定义。 */
#define SPIx_MISO_PIN 				GPIO_PIN_4
/* 宏定义: SPIx_MISO_AF - SPIx_MISO_AF 的宏定义。 */
#define SPIx_MISO_AF				GPIO_AF5_SPI1

/* 宏定义: SPIx_MOSI_CLK_ENABLE - SPIx_MOSI_CLK_ENABLE 的宏定义。 */
#define SPIx_MOSI_CLK_ENABLE()		__HAL_RCC_GPIOB_CLK_ENABLE()
/* 宏定义: SPIx_MOSI_GPIO - SPIx_MOSI_GPIO 的宏定义。 */
#define SPIx_MOSI_GPIO				GPIOB
/* 宏定义: SPIx_MOSI_PIN - SPIx_MOSI_PIN 的宏定义。 */
#define SPIx_MOSI_PIN 				GPIO_PIN_5
/* 宏定义: SPIx_MOSI_AF - SPIx_MOSI_AF 的宏定义。 */
#define SPIx_MOSI_AF				GPIO_AF5_SPI1

/* Definition for SPIx's DMA */
#define SPIx_TX_DMA_STREAM               DMA2_Stream3
/* 宏定义: SPIx_RX_DMA_STREAM - SPIx_RX_DMA_STREAM 的宏定义。 */
#define SPIx_RX_DMA_STREAM               DMA2_Stream2

/* 宏定义: SPIx_TX_DMA_REQUEST - SPIx_TX_DMA_REQUEST 的宏定义。 */
#define SPIx_TX_DMA_REQUEST              DMA_REQUEST_SPI1_TX
/* 宏定义: SPIx_RX_DMA_REQUEST - SPIx_RX_DMA_REQUEST 的宏定义。 */
#define SPIx_RX_DMA_REQUEST              DMA_REQUEST_SPI1_RX

/* Definition for SPIx's NVIC */
#define SPIx_DMA_TX_IRQn                 DMA2_Stream3_IRQn
/* 宏定义: SPIx_DMA_RX_IRQn - SPIx_DMA_RX_IRQn 的宏定义。 */
#define SPIx_DMA_RX_IRQn                 DMA2_Stream2_IRQn

/* 宏定义: SPIx_DMA_TX_IRQHandler - SPIx_DMA_TX_IRQHandler 的宏定义。 */
#define SPIx_DMA_TX_IRQHandler           DMA2_Stream3_IRQHandler
/* 宏定义: SPIx_DMA_RX_IRQHandler - SPIx_DMA_RX_IRQHandler 的宏定义。 */
#define SPIx_DMA_RX_IRQHandler           DMA2_Stream2_IRQHandler

/* 宏定义: SPIx_IRQn - SPIx_IRQn 的宏定义。 */
#define SPIx_IRQn                        SPI1_IRQn
/* 宏定义: SPIx_IRQHandler - SPIx_IRQHandler 的宏定义。 */
#define SPIx_IRQHandler                  SPI1_IRQHandler

enum {
	TRANSFER_WAIT,
	TRANSFER_COMPLETE,
	TRANSFER_ERROR
};

static SPI_HandleTypeDef hspi = {0};
static DMA_HandleTypeDef hdma_tx;
static DMA_HandleTypeDef hdma_rx;


//const uint8_t g_spiTxBuf[] = "****SPI - Two Boards communication based on DMA **** SPI Message ********* SPI Message *********";
uint8_t g_spiTxBuf[SPI_BUFFER_SIZE];

ALIGN_32BYTES(uint8_t g_spiRxBuf[SPI_BUFFER_SIZE]);	/* ����32�ֽڶ��� */
uint32_t g_spiLen;		/* �շ������ݳ��� */

/* transfer state */
__IO uint32_t wTransferState = TRANSFER_WAIT;

/* ����SPI�����ؼ���������������ʣ���λ������. �����ͬ�����л�����Ҫ����Init SPI���� */
static uint32_t s_BaudRatePrescaler;
static uint32_t s_CLKPhase;
static uint32_t s_CLKPolarity;

uint8_t g_spi_busy;

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitSPIBus1
*	����˵��: ����SPI���ߡ� ֻ���� SCK�� MOSI�� MISO���ߵ����á�������ƬѡCS��Ҳ����������оƬ���е�INT��BUSY��
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitSPIBus(void)
{	
	g_spi_busy = 0;
	
	bsp_InitSPIParam(SPI_BAUDRATEPRESCALER_8, SPI_PHASE_1EDGE, SPI_POLARITY_LOW);
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitSPIParam
*	����˵��: ����SPI���߲����������ʡ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitSPIParam(uint32_t _BaudRatePrescaler, uint32_t _CLKPhase, uint32_t _CLKPolarity)
{
	/* ���ִ��Ч�ʣ�ֻ����SPIӲ�����������仯ʱ����ִ��HAL_Init */
	if (s_BaudRatePrescaler == _BaudRatePrescaler && s_CLKPhase == _CLKPhase && s_CLKPolarity == _CLKPolarity)
	{		
		return;
	}

	s_BaudRatePrescaler = _BaudRatePrescaler;	
	s_CLKPhase = _CLKPhase;
	s_CLKPolarity = _CLKPolarity;
	
	/*##-1- Configure the SPI peripheral #######################################*/
	/* Set the SPI parameters */
	hspi.Instance               = SPIx;
	hspi.Init.BaudRatePrescaler = _BaudRatePrescaler;
	hspi.Init.Direction         = SPI_DIRECTION_2LINES;
	hspi.Init.CLKPhase          = _CLKPhase;
	hspi.Init.CLKPolarity       = _CLKPolarity;
	hspi.Init.DataSize          = SPI_DATASIZE_8BIT;
	hspi.Init.FirstBit          = SPI_FIRSTBIT_MSB;
	hspi.Init.TIMode            = SPI_TIMODE_DISABLE;
	hspi.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
	hspi.Init.CRCPolynomial     = 7;
	hspi.Init.CRCLength         = SPI_CRC_LENGTH_8BIT;
	hspi.Init.NSS               = SPI_NSS_SOFT;
	hspi.Init.FifoThreshold     = SPI_FIFO_THRESHOLD_01DATA;
	hspi.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;
	hspi.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;  /* Recommanded setting to avoid glitches */
	hspi.Init.Mode 			 = SPI_MODE_MASTER;

	if (HAL_SPI_Init(&hspi) != HAL_OK)
	{
		/* Initialization Error */
		Error_Handler(__FILE__, __LINE__);
	}	
}

/*
*********************************************************************************************************
*   函 数 名: HAL_SPI_MspInit
*   功能说明: HAL_SPI_MspInit 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void HAL_SPI_MspInit(SPI_HandleTypeDef *_hspi)
{
	/* ���� SPI����GPIO : SCK MOSI MISO */
	{
		GPIO_InitTypeDef  GPIO_InitStruct;
			
		/*##-1- Enable peripherals and GPIO Clocks #################################*/
		SPIx_SCK_CLK_ENABLE();
		SPIx_MISO_CLK_ENABLE();
		SPIx_MOSI_CLK_ENABLE();

		/* Enable SPI1 clock */
		SPIx_CLK_ENABLE();

		/*##-2- Configure peripheral GPIO ##########################################*/  
		/* SPI SCK GPIO pin configuration  */
		GPIO_InitStruct.Pin       = SPIx_SCK_PIN;
		GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull      = GPIO_PULLDOWN;		/* ���� */
		GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;
		GPIO_InitStruct.Alternate = SPIx_SCK_AF;
		HAL_GPIO_Init(SPIx_SCK_GPIO, &GPIO_InitStruct);

		/* SPI MISO GPIO pin configuration  */
		GPIO_InitStruct.Pin = SPIx_MISO_PIN;
		GPIO_InitStruct.Alternate = SPIx_MISO_AF;
		HAL_GPIO_Init(SPIx_MISO_GPIO, &GPIO_InitStruct);

		/* SPI MOSI GPIO pin configuration  */
		GPIO_InitStruct.Pin = SPIx_MOSI_PIN;
		GPIO_InitStruct.Alternate = SPIx_MOSI_AF;
		HAL_GPIO_Init(SPIx_MOSI_GPIO, &GPIO_InitStruct);
	}

	/* ����DMA��NVIC */
	#ifdef USE_SPI_DMA
	{
		/* Enable DMA clock */
		DMAx_CLK_ENABLE();
		
		/*##-3- Configure the DMA ##################################################*/
		/* Configure the DMA handler for Transmission process */
		hdma_tx.Instance                 = SPIx_TX_DMA_STREAM;
		hdma_tx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
		hdma_tx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
		hdma_tx.Init.MemBurst            = DMA_MBURST_INC4;	// DMA_MBURST_INC4; DMA_MBURST_SINGLE
		hdma_tx.Init.PeriphBurst         = DMA_PBURST_INC4;	// DMA_PBURST_INC4; DMA_PBURST_SINGLE
		hdma_tx.Init.Request             = SPIx_TX_DMA_REQUEST;
		hdma_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
		hdma_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
		hdma_tx.Init.MemInc              = DMA_MINC_ENABLE;
		hdma_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
		hdma_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
		hdma_tx.Init.Mode                = DMA_NORMAL;
		hdma_tx.Init.Priority            = DMA_PRIORITY_LOW;
		HAL_DMA_Init(&hdma_tx);
		__HAL_LINKDMA(_hspi, hdmatx, hdma_tx);	 /* Associate the initialized DMA handle to the the SPI handle */

		/* Configure the DMA handler for Transmission process */
		hdma_rx.Instance                 = SPIx_RX_DMA_STREAM;
		hdma_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
		hdma_rx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
		hdma_rx.Init.MemBurst            = DMA_MBURST_INC4;	// DMA_MBURST_INC4;  DMA_MBURST_SINGLE
		hdma_rx.Init.PeriphBurst         = DMA_PBURST_INC4;	// DMA_PBURST_INC4;  DMA_PBURST_SINGLE
		hdma_rx.Init.Request             = SPIx_RX_DMA_REQUEST;
		hdma_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
		hdma_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
		hdma_rx.Init.MemInc              = DMA_MINC_ENABLE;
		hdma_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
		hdma_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
		hdma_rx.Init.Mode                = DMA_NORMAL;
		hdma_rx.Init.Priority            = DMA_PRIORITY_HIGH;
		HAL_DMA_Init(&hdma_rx);    
	   __HAL_LINKDMA(_hspi, hdmarx, hdma_rx);	/* Associate the initialized DMA handle to the the SPI handle */

		/* NVIC configuration for DMA transfer complete interrupt (SPI1_TX) */
		HAL_NVIC_SetPriority(SPIx_DMA_TX_IRQn, 1, 1);
		HAL_NVIC_EnableIRQ(SPIx_DMA_TX_IRQn);
		
		/* NVIC configuration for DMA transfer complete interrupt (SPI1_RX) */
		HAL_NVIC_SetPriority(SPIx_DMA_RX_IRQn, 1, 0);
		HAL_NVIC_EnableIRQ(SPIx_DMA_RX_IRQn);
		
		/* NVIC configuration for SPI transfer complete interrupt (SPI1) */
		HAL_NVIC_SetPriority(SPIx_IRQn, 1, 0);
		HAL_NVIC_EnableIRQ(SPIx_IRQn);
	}
	#endif

	#ifdef USE_SPI_INT
		/* NVIC configuration for SPI transfer complete interrupt (SPI1) */
		HAL_NVIC_SetPriority(SPIx_IRQn, 1, 0);
		HAL_NVIC_EnableIRQ(SPIx_IRQn);
	#endif
}
	
/*
*********************************************************************************************************
*	�� �� ��: bsp_spiTransfer
*	����˵��: ��SPI���߷���1�������ֽ�
*	��    ��:  g_spi_TxBuf : ���͵�����
*		      g_spi_RxBuf  ���շ������ݳ���
*			  _TxRxLen �����յ����ݻ�����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_spiTransfer(void)
{
	if (g_spiLen > SPI_BUFFER_SIZE)
	{
		return;
	}
	
#ifdef USE_SPI_DMA
	wTransferState == TRANSFER_WAIT;
	
	while (hspi.State != HAL_SPI_STATE_READY);

	if(HAL_SPI_TransmitReceive_DMA(&hspi, (uint8_t*)g_spiTxBuf, (uint8_t *)g_spiRxBuf, g_spiLen) != HAL_OK)	
	{
		Error_Handler(__FILE__, __LINE__);
	}
	
	while (wTransferState == TRANSFER_WAIT)
	{
		;
	}
	
	/* Invalidate cache prior to access by CPU */
	SCB_InvalidateDCache_by_Addr ((uint32_t *)g_spiRxBuf, SPI_BUFFER_SIZE);
#endif
	
#ifdef USE_SPI_INT
	wTransferState = TRANSFER_WAIT;
	
	while (hspi.State != HAL_SPI_STATE_READY);

	if(HAL_SPI_TransmitReceive_IT(&hspi, (uint8_t*)g_spiTxBuf, (uint8_t *)g_spiRxBuf, g_spiLen) != HAL_OK)	
	{
		Error_Handler(__FILE__, __LINE__);
	}
	
	while (wTransferState == TRANSFER_WAIT)
	{
		;
	}
#endif
	
#ifdef USE_SPI_POLL
	if(HAL_SPI_TransmitReceive(&hspi, (uint8_t*)g_spiTxBuf, (uint8_t *)g_spiRxBuf, g_spiLen, 1000000) != HAL_OK)	
	{
		Error_Handler(__FILE__, __LINE__);
	}	
#endif
}

/**
  * @brief  TxRx Transfer completed callback.
  * @param  hspi: SPI handle
  * @note   This example shows a simple way to report end of DMA TxRx transfer, and 
  *         you can add your own implementation. 
  * @retval None
  */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
	wTransferState = TRANSFER_COMPLETE;
}

/**
  * @brief  SPI error callbacks.
  * @param  hspi: SPI handle
  * @note   This example shows a simple way to report transfer error, and you can
  *         add your own implementation.
  * @retval None
  */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
	wTransferState = TRANSFER_ERROR;
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_SpiBusEnter
*	����˵��: ռ��SPI����
*	��    ��: ��
*	�� �� ֵ: 0 ��ʾ��æ  1��ʾæ
*********************************************************************************************************
*/
void bsp_SpiBusEnter(void)
{
	g_spi_busy = 1;
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_SpiBusExit
*	����˵��: �ͷ�ռ�õ�SPI����
*	��    ��: ��
*	�� �� ֵ: 0 ��ʾ��æ  1��ʾæ
*********************************************************************************************************
*/
void bsp_SpiBusExit(void)
{
	g_spi_busy = 0;
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_SpiBusBusy
*	����˵��: �ж�SPI����æ�������Ǽ������SPIоƬ��Ƭѡ�ź��Ƿ�Ϊ1
*	��    ��: ��
*	�� �� ֵ: 0 ��ʾ��æ  1��ʾæ
*********************************************************************************************************
*/
uint8_t bsp_SpiBusBusy(void)
{
	return g_spi_busy;
}


#ifdef USE_SPI_INT
	/**
	  * @brief  This function handles SPIx interrupt request.
	  * @param  None
	  * @retval None
	  */
	void SPIx_IRQHandler(void)
	{
		HAL_SPI_IRQHandler(&hspi);
	}	
#endif

#ifdef USE_SPI_DMA
	/**
	  * @brief  This function handles DMA Rx interrupt request.
	  * @param  None
	  * @retval None
	  */
	void SPIx_DMA_RX_IRQHandler(void)
	{
		HAL_DMA_IRQHandler(hspi.hdmarx);
	}

	/**
	  * @brief  This function handles DMA Tx interrupt request.
	  * @param  None
	  * @retval None
	  */
	void SPIx_DMA_TX_IRQHandler(void)
	{
		HAL_DMA_IRQHandler(hspi.hdmatx);
	}
	
	/**
	  * @brief  This function handles SPIx interrupt request.
	  * @param  None
	  * @retval None
	  */
	void SPIx_IRQHandler(void)
	{
		HAL_SPI_IRQHandler(&hspi);
	}	
#endif
	
/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
