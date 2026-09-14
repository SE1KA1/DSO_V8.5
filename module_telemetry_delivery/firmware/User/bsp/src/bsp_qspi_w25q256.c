/**
 * @file bsp_qspi_w25q256.c
 * @brief 安富莱 BSP 通用驱动（W25Q256 QSPI Flash）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : W25Q256 QSPI����ģ��
*	�ļ����� : bsp_qspi_w25q256.c
*	��    �� : V1.0
*	˵    �� : ʹ��CPU��QSPI������������FLASH���ṩ�����Ķ�д����������4�߷�ʽ��MDMA����
*
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2019-02-12  armfly  ��ʽ����
*
*	Copyright (C), 2019-2030, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"
#include "bsp_qspi_w25q256.h"



/* 
    STM32-V7���������

	PG6/QUADSPI_BK1_NCS AF10
	PF10/QUADSPI_CLK	AF9
	PF8/QUADSPI_BK1_IO0 AF10
	PF9/QUADSPI_BK1_IO1 AF10
	PF7/QUADSPI_BK1_IO2 AF9
	PF6/QUADSPI_BK1_IO3 AF9

	W25Q256JV��512�飬ÿ����16��������ÿ������Sector��16ҳ��ÿҳ��256�ֽڣ�����32MB
*/

/* QSPI���ź�ʱ��������ú궨�� */
#define QSPI_CLK_ENABLE()              __HAL_RCC_QSPI_CLK_ENABLE()
/* 宏定义: QSPI_CLK_DISABLE - QSPI_CLK_DISABLE 的宏定义。 */
#define QSPI_CLK_DISABLE()             __HAL_RCC_QSPI_CLK_DISABLE()
/* 宏定义: QSPI_CS_GPIO_CLK_ENABLE - QSPI_CS_GPIO_CLK_ENABLE 的宏定义。 */
#define QSPI_CS_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOG_CLK_ENABLE()
/* 宏定义: QSPI_CLK_GPIO_CLK_ENABLE - QSPI_CLK_GPIO_CLK_ENABLE 的宏定义。 */
#define QSPI_CLK_GPIO_CLK_ENABLE()     __HAL_RCC_GPIOF_CLK_ENABLE()
/* 宏定义: QSPI_BK1_D0_GPIO_CLK_ENABLE - QSPI_BK1_D0_GPIO_CLK_ENABLE 的宏定义。 */
#define QSPI_BK1_D0_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOF_CLK_ENABLE()
/* 宏定义: QSPI_BK1_D1_GPIO_CLK_ENABLE - QSPI_BK1_D1_GPIO_CLK_ENABLE 的宏定义。 */
#define QSPI_BK1_D1_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOF_CLK_ENABLE()
/* 宏定义: QSPI_BK1_D2_GPIO_CLK_ENABLE - QSPI_BK1_D2_GPIO_CLK_ENABLE 的宏定义。 */
#define QSPI_BK1_D2_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOF_CLK_ENABLE()
/* 宏定义: QSPI_BK1_D3_GPIO_CLK_ENABLE - QSPI_BK1_D3_GPIO_CLK_ENABLE 的宏定义。 */
#define QSPI_BK1_D3_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOF_CLK_ENABLE()

/* 宏定义: QSPI_MDMA_CLK_ENABLE - QSPI_MDMA_CLK_ENABLE 的宏定义。 */
#define QSPI_MDMA_CLK_ENABLE()         __HAL_RCC_MDMA_CLK_ENABLE()
/* 宏定义: QSPI_FORCE_RESET - QSPI_FORCE_RESET 的宏定义。 */
#define QSPI_FORCE_RESET()             __HAL_RCC_QSPI_FORCE_RESET()
/* 宏定义: QSPI_RELEASE_RESET - QSPI_RELEASE_RESET 的宏定义。 */
#define QSPI_RELEASE_RESET()           __HAL_RCC_QSPI_RELEASE_RESET()

/* 宏定义: QSPI_CS_PIN - QSPI_CS_PIN 的宏定义。 */
#define QSPI_CS_PIN                    GPIO_PIN_6
/* 宏定义: QSPI_CS_GPIO_PORT - QSPI_CS_GPIO_PORT 的宏定义。 */
#define QSPI_CS_GPIO_PORT              GPIOG

/* 宏定义: QSPI_CLK_PIN - QSPI_CLK_PIN 的宏定义。 */
#define QSPI_CLK_PIN                   GPIO_PIN_10
/* 宏定义: QSPI_CLK_GPIO_PORT - QSPI_CLK_GPIO_PORT 的宏定义。 */
#define QSPI_CLK_GPIO_PORT             GPIOF

/* 宏定义: QSPI_BK1_D0_PIN - QSPI_BK1_D0_PIN 的宏定义。 */
#define QSPI_BK1_D0_PIN                GPIO_PIN_8
/* 宏定义: QSPI_BK1_D0_GPIO_PORT - QSPI_BK1_D0_GPIO_PORT 的宏定义。 */
#define QSPI_BK1_D0_GPIO_PORT          GPIOF

/* 宏定义: QSPI_BK1_D1_PIN - QSPI_BK1_D1_PIN 的宏定义。 */
#define QSPI_BK1_D1_PIN                GPIO_PIN_9
/* 宏定义: QSPI_BK1_D1_GPIO_PORT - QSPI_BK1_D1_GPIO_PORT 的宏定义。 */
#define QSPI_BK1_D1_GPIO_PORT          GPIOF

/* 宏定义: QSPI_BK1_D2_PIN - QSPI_BK1_D2_PIN 的宏定义。 */
#define QSPI_BK1_D2_PIN                GPIO_PIN_7
/* 宏定义: QSPI_BK1_D2_GPIO_PORT - QSPI_BK1_D2_GPIO_PORT 的宏定义。 */
#define QSPI_BK1_D2_GPIO_PORT          GPIOF

/* 宏定义: QSPI_BK1_D3_PIN - QSPI_BK1_D3_PIN 的宏定义。 */
#define QSPI_BK1_D3_PIN                GPIO_PIN_6
/* 宏定义: QSPI_BK1_D3_GPIO_PORT - QSPI_BK1_D3_GPIO_PORT 的宏定义。 */
#define QSPI_BK1_D3_GPIO_PORT          GPIOF


/* �����ļ����õ�ȫ�ֱ��� */
static MDMA_HandleTypeDef hmdma;
static QSPI_HandleTypeDef 	QSPIHandle;
static __IO uint8_t CmdCplt, RxCplt, TxCplt, StatusMatch, TimeOut;

/* �����ļ����õĺ��� */
static void QSPI_WriteEnable(QSPI_HandleTypeDef *hqspi);
/*
*********************************************************************************************************
*   函 数 名: QSPI_AutoPollingMemReady
*   功能说明: QSPI_AutoPollingMemReady 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void QSPI_AutoPollingMemReady(QSPI_HandleTypeDef *hqspi);


/*
*********************************************************************************************************
*	�� �� ��: bsp_InitQSPI_W25Q256
*	����˵��: QSPI FlashӲ����ʼ�������û�������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitQSPI_W25Q256(void)
{
	/* ��λQSPI */
	QSPIHandle.Instance = QUADSPI;
	if (HAL_QSPI_DeInit(&QSPIHandle) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}
	
	/* ����ʱ���ٶȣ�QSPI clock = 200MHz / (ClockPrescaler+1) = 100MHz */
	QSPIHandle.Init.ClockPrescaler  = 1;  
	
	/* ����FIFO��ֵ����Χ1 - 32 */
	QSPIHandle.Init.FifoThreshold   = 32; 
	
	/* 
		QUADSPI��FLASH�����źź�����CLK���ڲŶ�FLASH���������ݲ�����
		���ⲿ�ź��ӳ�ʱ�����������Ƴ����ݲ�����
	*/
	QSPIHandle.Init.SampleShifting  = QSPI_SAMPLE_SHIFTING_HALFCYCLE; 
	
	/*Flash��С��2^(FlashSize + 1) = 2^25 = 32MB */
	QSPIHandle.Init.FlashSize       = QSPI_FLASH_SIZE - 1; 
	
	/* ����֮���CSƬѡ���ٱ���1��ʱ�����ڵĸߵ�ƽ */
	QSPIHandle.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_1_CYCLE;
	
	/*
	   MODE0: ��ʾƬѡ�źſ����ڼ䣬CLKʱ���ź��ǵ͵�ƽ
	   MODE3: ��ʾƬѡ�źſ����ڼ䣬CLKʱ���ź��Ǹߵ�ƽ
	*/
	QSPIHandle.Init.ClockMode = QSPI_CLOCK_MODE_0;
	
	/* QSPI������BANK������ʹ�õ�BANK1 */
	QSPIHandle.Init.FlashID   = QSPI_FLASH_ID_1;
	
	/* V7�������ʹ����BANK1�������ǽ�ֹ˫BANK */
	QSPIHandle.Init.DualFlash = QSPI_DUALFLASH_DISABLE;

	/* ��ʼ������QSPI */
	if (HAL_QSPI_Init(&QSPIHandle) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}	
}	

/*
*********************************************************************************************************
*	�� �� ��: HAL_QSPI_MspInit
*	����˵��: QSPI�ײ��ʼ������HAL_QSPI_Init���õĵײ㺯��
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_QSPI_MspInit(QSPI_HandleTypeDef *hqspi)
{
	GPIO_InitTypeDef GPIO_InitStruct;

	/* ʹ��QPSIʱ��  */
	QSPI_CLK_ENABLE();
	
	/* ��λʱ�ӽӿ� */
	QSPI_FORCE_RESET();
	QSPI_RELEASE_RESET();
	
	/* ʹ��GPIOʱ�� */
	QSPI_CS_GPIO_CLK_ENABLE();
	QSPI_CLK_GPIO_CLK_ENABLE();
	QSPI_BK1_D0_GPIO_CLK_ENABLE();
	QSPI_BK1_D1_GPIO_CLK_ENABLE();
	QSPI_BK1_D2_GPIO_CLK_ENABLE();
	QSPI_BK1_D3_GPIO_CLK_ENABLE(); 

	/* QSPI CS GPIO �������� */
	GPIO_InitStruct.Pin       = QSPI_CS_PIN;
	GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull      = GPIO_PULLUP;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_QUADSPI;
	HAL_GPIO_Init(QSPI_CS_GPIO_PORT, &GPIO_InitStruct);
	
	/* QSPI CLK GPIO �������� */
	GPIO_InitStruct.Pin       = QSPI_CLK_PIN;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
	HAL_GPIO_Init(QSPI_CLK_GPIO_PORT, &GPIO_InitStruct);

	/* QSPI D0 GPIO pin �������� */
	GPIO_InitStruct.Pin       = QSPI_BK1_D0_PIN;
	GPIO_InitStruct.Alternate = GPIO_AF10_QUADSPI;
	HAL_GPIO_Init(QSPI_BK1_D0_GPIO_PORT, &GPIO_InitStruct);

	/* QSPI D1 GPIO �������� */
	GPIO_InitStruct.Pin       = QSPI_BK1_D1_PIN;
	GPIO_InitStruct.Alternate = GPIO_AF10_QUADSPI;
	HAL_GPIO_Init(QSPI_BK1_D1_GPIO_PORT, &GPIO_InitStruct);

	/* QSPI D2 GPIO �������� */
	GPIO_InitStruct.Pin       = QSPI_BK1_D2_PIN;
	GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
	HAL_GPIO_Init(QSPI_BK1_D2_GPIO_PORT, &GPIO_InitStruct);

	/* QSPI D3 GPIO �������� */
	GPIO_InitStruct.Pin       = QSPI_BK1_D3_PIN;
	GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
	HAL_GPIO_Init(QSPI_BK1_D3_GPIO_PORT, &GPIO_InitStruct);

	/* ʹ��QSPI�ж� */
	HAL_NVIC_SetPriority(QUADSPI_IRQn, 0x0F, 0);
	HAL_NVIC_EnableIRQ(QUADSPI_IRQn);
	
	/* ����MDMAʱ�� */
	QSPI_MDMA_CLK_ENABLE();

	hmdma.Instance = MDMA_Channel1;                         /* ʹ��MDMA��ͨ��1 */
	hmdma.Init.Request = MDMA_REQUEST_QUADSPI_FIFO_TH;      /* QSPI��FIFO��ֵ�����ж� */
	hmdma.Init.TransferTriggerMode = MDMA_BUFFER_TRANSFER;  /* ʹ��MDMA��buffer���� */
	hmdma.Init.Priority = MDMA_PRIORITY_HIGH;               /* ���ȼ��� */
	hmdma.Init.Endianness = MDMA_LITTLE_ENDIANNESS_PRESERVE;/* С�˸�ʽ */
	hmdma.Init.SourceInc = MDMA_SRC_INC_BYTE;               /* Դ��ַ�ֽڵ��� */
	hmdma.Init.DestinationInc = MDMA_DEST_INC_DISABLE;      /* Ŀ�ĵ�ַ��Ч���� */
	hmdma.Init.SourceDataSize = MDMA_SRC_DATASIZE_BYTE;     /* Դ��ַ���ݿ����ֽ� */
	hmdma.Init.DestDataSize = MDMA_DEST_DATASIZE_BYTE;      /* Ŀ�ĵ�ַ���ݿ����ֽ� */
	hmdma.Init.DataAlignment = MDMA_DATAALIGN_PACKENABLE;   /* С�ˣ��Ҷ��� */
	hmdma.Init.BufferTransferLength = 128;                  /* ÿ�δ���128���ֽ� */
	hmdma.Init.SourceBurst = MDMA_SOURCE_BURST_SINGLE;      /* Դ���ݵ��δ��� */
	hmdma.Init.DestBurst = MDMA_DEST_BURST_SINGLE;          /* Ŀ�����ݵ��δ��� */
	
	hmdma.Init.SourceBlockAddressOffset = 0;                /* ����block���䣬buffer�����ò��� */
	hmdma.Init.DestBlockAddressOffset = 0;                  /* ����block���䣬buffer�����ò��� */

	/* ����MDMA�����QSPI�������  */
	__HAL_LINKDMA(hqspi, hmdma, hmdma);

	/* �ȸ�λ��Ȼ������MDMA */
	HAL_MDMA_DeInit(&hmdma);  
	HAL_MDMA_Init(&hmdma);

	/* ʹ��MDMA�жϣ����������ȼ� */
	HAL_NVIC_SetPriority(MDMA_IRQn, 0x02, 0);
	HAL_NVIC_EnableIRQ(MDMA_IRQn);
}

/*
*********************************************************************************************************
*	�� �� ��: HAL_QSPI_MspDeInit
*	����˵��: QSPI�ײ㸴λ����HAL_QSPI_Init���õĵײ㺯��
*	��    ��: hqspi QSPI_HandleTypeDef���;��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_QSPI_MspDeInit(QSPI_HandleTypeDef *hqspi)
{
	/* ��λQSPI���� */
	HAL_GPIO_DeInit(QSPI_CS_GPIO_PORT, QSPI_CS_PIN);
	HAL_GPIO_DeInit(QSPI_CLK_GPIO_PORT, QSPI_CLK_PIN);
	HAL_GPIO_DeInit(QSPI_BK1_D0_GPIO_PORT, QSPI_BK1_D0_PIN);
	HAL_GPIO_DeInit(QSPI_BK1_D1_GPIO_PORT, QSPI_BK1_D1_PIN);
	HAL_GPIO_DeInit(QSPI_BK1_D2_GPIO_PORT, QSPI_BK1_D2_PIN);
	HAL_GPIO_DeInit(QSPI_BK1_D3_GPIO_PORT, QSPI_BK1_D3_PIN);

	/* ��λQSPI */
	QSPI_FORCE_RESET();
	QSPI_RELEASE_RESET();

	/* �ر�QSPIʱ�� */
	QSPI_CLK_DISABLE();
}

/*
*********************************************************************************************************
*	�� �� ��: sf_EraseSector
*	����˵��: ����ָ����������������С4KB
*	��    ��: _uiSectorAddr : ������ַ����4KBΪ��λ�ĵ�ַ������0��4096, 8192�ȣ�
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void QSPI_EraseSector(uint32_t address)
{
	QSPI_CommandTypeDef sCommand={0};

	/* �����������ɱ�־ */	
	CmdCplt = 0;

	/* дʹ�� */
	QSPI_WriteEnable(&QSPIHandle);	

	/* �������� */
	sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;    /* 1�߷�ʽ����ָ�� */
	sCommand.AddressSize       = QSPI_ADDRESS_32_BITS;       /* 32λ��ַ */
	sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;  /* �޽����ֽ� */
	sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;      /* W25Q256JV��֧��DDR */
	sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;  /* DDRģʽ����������ӳ� */
	sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;	 /* ÿ�δ��䶼��ָ�� */	
	
	/* �������� */
	sCommand.Instruction = SUBSECTOR_ERASE_4_BYTE_ADDR_CMD;   /* 32bit��ַ��ʽ�������������������С4KB*/       
	sCommand.AddressMode = QSPI_ADDRESS_1_LINE;  /* ��ַ������1�߷�ʽ */       
	sCommand.Address     = address;              /* �����׵�ַ����֤��4KB������ */    
	sCommand.DataMode    = QSPI_DATA_NONE;       /* ���跢������ */  
	sCommand.DummyCycles = 0;                    /* ��������� */  

	if (HAL_QSPI_Command_IT(&QSPIHandle, &sCommand) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}
	
	/* �ȴ��������� */
	while(CmdCplt == 0);
	CmdCplt = 0;
	
	/* �ȴ���̽��� */
	StatusMatch = 0;
	QSPI_AutoPollingMemReady(&QSPIHandle);	
	while(StatusMatch == 0);
	StatusMatch = 0;
}

/*
*********************************************************************************************************
*	�� �� ��: QSPI_WriteBuffer
*	����˵��: ҳ��̣�ҳ��С256�ֽڣ�����ҳ������д��
*	��    ��: _pBuf : ����Դ��������
*			  _uiWrAddr ��Ŀ�������׵�ַ����ҳ�׵�ַ������0�� 256, 512�ȡ�
*			  _usSize �����ݸ��������ܳ���ҳ���С����Χ1 - 256��
*	�� �� ֵ: 1:�ɹ��� 0��ʧ��
*********************************************************************************************************
*/
uint8_t QSPI_WriteBuffer(uint8_t *_pBuf, uint32_t _uiWriteAddr, uint16_t _usWriteSize)
{
	QSPI_CommandTypeDef sCommand={0};
	
	/* ���ڵȴ�������ɱ�־ */
	TxCplt = 0;

	/* дʹ�� */
	QSPI_WriteEnable(&QSPIHandle);	
	
	/* �������� */
	sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;    /* 1�߷�ʽ����ָ�� */
	sCommand.AddressSize       = QSPI_ADDRESS_32_BITS;       /* 32λ��ַ */
	sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;  /* �޽����ֽ� */
	sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;      /* W25Q256JV��֧��DDR */
	sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;  /* DDRģʽ����������ӳ� */
	sCommand.SIOOMode          = QSPI_SIOO_INST_ONLY_FIRST_CMD;	 /* ������һ������ */	
	
	/* д�������� */
	sCommand.Instruction = QUAD_IN_FAST_PROG_4_BYTE_ADDR_CMD; /* 32bit��ַ��4�߿���д������ */
	sCommand.DummyCycles = 0;                    /* ����Ҫ������ */
	sCommand.AddressMode = QSPI_ADDRESS_1_LINE; /* 4�ߵ�ַ��ʽ */
	sCommand.DataMode    = QSPI_DATA_4_LINES;    /* 4�����ݷ�ʽ */
	sCommand.NbData      = _usWriteSize;         /* д���ݴ�С */   
	sCommand.Address     = _uiWriteAddr;         /* д���ַ */
	
	if (HAL_QSPI_Command(&QSPIHandle, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}
	
	/* ����MDMA���� */
	if (HAL_QSPI_Transmit_DMA(&QSPIHandle, _pBuf) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}
	
	/* �ȴ����ݷ������ */
	while(TxCplt == 0);
	TxCplt = 0;
	
	/* �ȴ�Flashҳ������ */
	StatusMatch = 0;
	QSPI_AutoPollingMemReady(&QSPIHandle);	
	while(StatusMatch == 0);
	StatusMatch = 0;	
	
	return 1;
}

/*
*********************************************************************************************************
*	�� �� ��: QSPI_ReadBuffer
*	����˵��: ������ȡ�����ֽڣ��ֽڸ������ܳ���оƬ������
*	��    ��: _pBuf : ����Դ��������
*			  _uiReadAddr ����ʼ��ַ��
*			  _usSize �����ݸ���, ���Դ���PAGE_SIZE, ���ǲ��ܳ���оƬ��������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void QSPI_ReadBuffer(uint8_t * _pBuf, uint32_t _uiReadAddr, uint32_t _uiSize)
{
	
	QSPI_CommandTypeDef sCommand = {0};
	
	/* ���ڵȴ�������ɱ�־ */
	RxCplt = 0;
	
	/* �������� */
	sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;    	/* 1�߷�ʽ����ָ�� */
	sCommand.AddressSize       = QSPI_ADDRESS_32_BITS;      	/* 32λ��ַ */
	sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;  	/* �޽����ֽ� */
	sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;      	/* W25Q256JV��֧��DDR */
	sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;  	/* DDRģʽ����������ӳ� */
	sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;		/* ÿ�δ���Ҫ��ָ�� */	
 
	/* ��ȡ���� */
	sCommand.Instruction = QUAD_INOUT_FAST_READ_4_BYTE_ADDR_CMD; /* 32bit��ַ��4�߿��ٶ�ȡ���� */
	sCommand.DummyCycles = 6;                    /* ������ */
	sCommand.AddressMode = QSPI_ADDRESS_4_LINES; /* 4�ߵ�ַ */
	sCommand.DataMode    = QSPI_DATA_4_LINES;    /* 4������ */ 
	sCommand.NbData      = _uiSize;              /* ��ȡ�����ݴ�С */ 
	sCommand.Address     = _uiReadAddr;          /* ��ȡ���ݵ���ʼ��ַ */ 
	
	if (HAL_QSPI_Command(&QSPIHandle, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}

	/* MDMA��ʽ��ȡ */
	if (HAL_QSPI_Receive_DMA(&QSPIHandle, _pBuf) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}	
	
	/* �Ƚ������ */
	while(RxCplt == 0);
	RxCplt = 0;
}

/*
*********************************************************************************************************
*	�� �� ��: QSPI_WriteEnable
*	����˵��: дʹ��
*	��    ��: hqspi  QSPI_HandleTypeDef�����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void QSPI_WriteEnable(QSPI_HandleTypeDef *hqspi)
{
	QSPI_CommandTypeDef     sCommand = {0};
	
	/* �������� */
	sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;    /* 1�߷�ʽ����ָ�� */
	sCommand.AddressSize       = QSPI_ADDRESS_32_BITS;       /* 32λ��ַ */
	sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;  /* �޽����ֽ� */
	sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;      /* W25Q256JV��֧��DDR */
	sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;  /* DDRģʽ����������ӳ� */
	sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;	 /* ÿ�δ��䶼��ָ�� */
	
	/* дʹ�� */
	sCommand.Instruction       = WRITE_ENABLE_CMD;  /* дʹ��ָ�� */
	sCommand.AddressMode       = QSPI_ADDRESS_NONE; /* �����ַ */
	sCommand.DataMode          = QSPI_DATA_NONE;    /* �������� */
	sCommand.DummyCycles       = 0;                 /* ������  */

	if (HAL_QSPI_Command(&QSPIHandle, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}
	
//	/* �ȴ�дʹ����� */
//	StatusMatch = 0;
//	QSPI_AutoPollingMemReady(&QSPIHandle);	
//	while(StatusMatch == 0);
//	StatusMatch = 0;	
}

/*
*********************************************************************************************************
*	�� �� ��: QSPI_AutoPollingMemReady
*	����˵��: �ȴ�QSPI Flash��������Ҫ����Flash������ҳ���ʱʹ��
*	��    ��: hqspi  QSPI_HandleTypeDef���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void QSPI_AutoPollingMemReady(QSPI_HandleTypeDef *hqspi)
{
	QSPI_CommandTypeDef     sCommand = {0};
	QSPI_AutoPollingTypeDef sConfig = {0};

	
	/* �������� */
	sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;    /* 1�߷�ʽ����ָ�� */
	sCommand.AddressSize       = QSPI_ADDRESS_32_BITS;       /* 32λ��ַ */
	sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;  /* �޽����ֽ� */
	sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;      /* W25Q256JV��֧��DDR */
	sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;  /* DDRģʽ����������ӳ� */
	sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;	 /* ÿ�δ��䶼��ָ�� */
	
	/* ��ȡ״̬*/
	sCommand.Instruction       = READ_STATUS_REG_CMD; /* ��ȡ״̬���� */
	sCommand.AddressMode       = QSPI_ADDRESS_NONE;   /* �����ַ */
	sCommand.DataMode          = QSPI_DATA_1_LINE;    /* 1������ */
	sCommand.DummyCycles       = 0;                   /* ��������� */

	/* ����λ���õ�bit0��ƥ��λ�ȴ�bit0Ϊ0�������ϲ�ѯ״̬�Ĵ���bit0���ȴ���Ϊ0 */
	sConfig.Mask            = 0x01;
	sConfig.Match           = 0x00;
	sConfig.MatchMode       = QSPI_MATCH_MODE_AND;
	sConfig.StatusBytesSize = 1;
	sConfig.Interval        = 0x10;
	sConfig.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;

	if (HAL_QSPI_AutoPolling_IT(&QSPIHandle, &sCommand, &sConfig) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: sf_ReadID
*	����˵��: ��ȡ����ID
*	��    ��: ��
*	�� �� ֵ: 32bit������ID (���8bit��0����ЧIDλ��Ϊ24bit��
*********************************************************************************************************
*/
uint32_t QSPI_ReadID(void)
{
	uint32_t uiID;
	QSPI_CommandTypeDef s_command = {0};
	uint8_t buf[3];

	/* �������� */
	s_command.InstructionMode   = QSPI_INSTRUCTION_1_LINE;    /* 1�߷�ʽ����ָ�� */
	s_command.AddressSize       = QSPI_ADDRESS_32_BITS;       /* 32λ��ַ */
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;  /* �޽����ֽ� */
	s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;      /* W25Q256JV��֧��DDR */
	s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;  /* DDRģʽ����������ӳ� */
	s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;	  /* ÿ�δ��䶼��ָ�� */
	
	/* ��ȡJEDEC ID */
	s_command.Instruction = READ_ID_CMD2;         /* ��ȡID���� */
	s_command.AddressMode = QSPI_ADDRESS_NONE;    /* 1�ߵ�ַ */
	s_command.DataMode = QSPI_DATA_1_LINE;        /* 1�ߵ�ַ */
	s_command.DummyCycles = 0;                    /* �޿����� */
	s_command.NbData = 3;                         /* ��ȡ�������� */

	if (HAL_QSPI_Command(&QSPIHandle, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) 
	{
		ERROR_HANDLER();
	}

	if (HAL_QSPI_Receive(&QSPIHandle, buf, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) 
	{
		ERROR_HANDLER();
	}

	uiID = (buf[0] << 16) | (buf[1] << 8 ) | buf[2];

	return uiID;
}

/*
*********************************************************************************************************
*	�� �� ��: HAL_QSPI_CmdCpltCallback
*	����˵��: QSPI�жϵĻص�������Flash��������QSPI_EraseSectorҪʹ��
*	��    ��: hqspi  QSPI_HandleTypeDef���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_QSPI_CmdCpltCallback(QSPI_HandleTypeDef *hqspi)
{
	CmdCplt++;
}

/*
*********************************************************************************************************
*	�� �� ��: HAL_QSPI_RxCpltCallback
*	����˵��: QSPI�жϵĻص�������Flash������QSPI_ReadBufferҪʹ��
*	��    ��: hqspi  QSPI_HandleTypeDef���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_QSPI_RxCpltCallback(QSPI_HandleTypeDef *hqspi)
{
	RxCplt++;
}

/*
*********************************************************************************************************
*	�� �� ��: HAL_QSPI_TxCpltCallback
*	����˵��: QSPI�жϵĻص�������Flashд����QSPI_ReadBufferҪʹ��
*	��    ��: hqspi  QSPI_HandleTypeDef���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
 void HAL_QSPI_TxCpltCallback(QSPI_HandleTypeDef *hqspi)
{
	TxCplt++; 
}

/*
*********************************************************************************************************
*	�� �� ��: HAL_QSPI_StatusMatchCallback
*	����˵��: QSPI�жϵĻص�������Flash״̬��ѯ����QSPI_AutoPollingMemReadyʹ��
*	��    ��: hqspi  QSPI_HandleTypeDef���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_QSPI_StatusMatchCallback(QSPI_HandleTypeDef *hqspi)
{
	StatusMatch++;
}

/*
*********************************************************************************************************
*	�� �� ��: QUADSPI_IRQHandler
*	����˵��: QSPI�жϷ������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void QUADSPI_IRQHandler(void)
{
	HAL_QSPI_IRQHandler(&QSPIHandle);
}

/*
*********************************************************************************************************
*	�� �� ��: MDMA_IRQHandler
*	����˵��: MDMA�жϷ������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void MDMA_IRQHandler(void)
{
	HAL_MDMA_IRQHandler(QSPIHandle.hmdma);
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
