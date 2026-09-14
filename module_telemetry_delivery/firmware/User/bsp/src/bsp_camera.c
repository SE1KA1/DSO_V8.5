/**
 * @file bsp_camera.c
 * @brief 安富莱 BSP 通用驱动（摄像头）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : ����ͷ����BSPģ��(For OV7670)
*	�ļ����� : bsp_camera.c
*	��    �� : V1.0
*	˵    �� : OV7670�������򡣱����������� guanfu_wang  ��OV7670����ͷ������FIFO,����LDO������24M����)
*			  ������STM32-V5�����弯����3.0V LDO��OV7670���磬���弯����24M��Դ�����ṩ������ͷ��
*
*			  ������ο��� guanfu_wang �ṩ�����ӡ�http://mcudiy.taobao.com/
*
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2013-03-01 armfly  ��ʽ����
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/*
	������STM32-V7����������ͷ�ӿ�GPIO���壺 ��DCIM�豸�� ����ͷ  �ʹ���6, AD7606 ģ�鲻��ͬʱʹ�á�
	PA6/DCMI_PIXCLK
	PA4/DCMI_HSYNC/DAC_OUT1
	PC6/DCMI_D0/AD7606_CONVST
	PC7/DCMI_D1/USART6_RX
	PG10/DCMI_D2/NRF24L01_CSN
	PG11/DCMI_D3/ETH_RMII_TX_EN
	PE4/DCMI_D4/NRF24L01_IRQ
	PD3/DCMI_D5
	PE5/DCMI_D6/AD7606_BUSY
	PE6/DCMI_D7/NRF905_CD
	PB7/DCMI_VSYNC
	
	--- I2C���߿�������ͷ
	PB6/I2C2_SCL
	PB9/I2C2_SDA
*/

#define DCMI_DR_ADDRESS       	0x50050028
#define OV_REG_NUM  	116  //OV7670


/*��DMAͨ������,��ѡ���� DMA2_Stream1 +  DMA_Channel_1�� DMA2_Stream7 +  DMA_Channel_1  */

#define DMA_CLOCK              RCC_AHB1Periph_DMA2
/* 宏定义: DMA_STREAM - DMA_STREAM 的宏定义。 */
#define DMA_STREAM             DMA2_Stream7
/* 宏定义: DMA_CHANNEL - DMA_CHANNEL 的宏定义。 */
#define DMA_CHANNEL            DMA_Channel_1
/* 宏定义: DMA_IRQ - DMA_IRQ 的宏定义。 */
#define DMA_IRQ                DMA2_Stream7_IRQn
/* 宏定义: DMA_IT_TCIF - DMA_IT_TCIF 的宏定义。 */
#define DMA_IT_TCIF            DMA_IT_TCIF7
/* 宏定义: DMA_IRQHandler - DMA_IRQHandler 的宏定义。 */
#define DMA_IRQHandler         DMA2_Stream7_IRQHandler


/* ���ֱ���

12 17 18 19 1A 03������

*/

/*
	����ΪOV7670 QVGA RGB565����  (by guanfu_wang)  http://mcudiy.taobao.com

	����RA8875ͼ��ģʽ�£�ɨ�跽��Ϊ�����ң����ϵ��¡�
	��wang_guanfu�ṩ��ȱʡֵ��ͬ����������ʵ��ĵ�����
*/
static const unsigned char  OV_reg[OV_REG_NUM][2] =
{
	{0x3a, 0x0c},
	{0x67, 0xc0},
	{0x68, 0x80},

	{0x40, 0xd0}, //RGB565
	//{0x40, 0x10}, //RGB565
	{0x12, 0x14}, //Output format, QVGA,RGB

	{0x32, 0x80},
	{0x17, 0x16},
	{0x18, 0x04},
	{0x19, 0x02},
	{0x1a, 0x7a},//0x7a,

	{0x03, 0x05},//0x0a,
	{0x0c, 0x00},
	{0x3e, 0x00},//

	{0x70, 0x00},
	{0x71, 0x01},
	{0x72, 0x11},
	{0x73, 0x00},//
	{0xa2, 0x02},
	{0x11, 0x01},

	{0x7a,  0x2C},
	{0x7b,  0x11},
	{0x7c,  0x1a},
	{0x7d,  0x2a},
	{0x7e,  0x42},
	{0x7f,  0x4c},
	{0x80,  0x56},
	{0x81,  0x5f},
	{0x82,  0x67},
	{0x83,  0x70},
	{0x84,  0x78},
	{0x85,  0x87},
	{0x86,  0x95},
	{0x87,  0xaf},
	{0x88,  0xc8},
	{0x89,  0xdf},

	////////////////
	{0x13, 0xe0},
	{0x00, 0x00},//AGC

	{0x10, 0x00},
	{0x0d, 0x00},
	{0x14, 0x10},//0x38, limit the max gain
	{0xa5, 0x05},
	{0xab, 0x07},

	{0x24, 0x75},//40
	{0x25, 0x63},
	{0x26, 0xA5},
	{0x9f, 0x78},
	{0xa0, 0x68},

	{0xa1, 0x03},//0x0b,
	{0xa6, 0xdf},//0xd8,
	{0xa7, 0xdf},//0xd8,
	{0xa8, 0xf0},
	{0xa9, 0x90},

	{0xaa, 0x94},//50
	{0x13, 0xe5},
	{0x0e, 0x61},
	{0x0f, 0x4b},
	{0x16, 0x02},

#if 1
	{0x1e, 0x37},//0x07, 0x17, 0x27, 0x37 ѡ��1��������ɨ�跽��. ��Ҫ��LCD��ɨ�跽��ƥ�䡣
#else
	{0x1e, 0x27},//0x07,
#endif


	{0x21, 0x02},
	{0x22, 0x91},
	{0x29, 0x07},
	{0x33, 0x0b},

	{0x35, 0x0b},//60
	{0x37, 0x1d},
	{0x38, 0x71},
	{0x39, 0x2a},
	{0x3c, 0x78},

	{0x4d, 0x40},
	{0x4e, 0x20},
	{0x69, 0x5d},
	{0x6b, 0x0a},//PLL
	{0x74, 0x19},
	{0x8d, 0x4f},

	{0x8e, 0x00},//70
	{0x8f, 0x00},
	{0x90, 0x00},
	{0x91, 0x00},
	{0x92, 0x00},//0x19,//0x66

	{0x96, 0x00},
	{0x9a, 0x80},
	{0xb0, 0x84},
	{0xb1, 0x0c},
	{0xb2, 0x0e},

	{0xb3, 0x82},//80
	{0xb8, 0x0a},
	{0x43, 0x14},
	{0x44, 0xf0},
	{0x45, 0x34},

	{0x46, 0x58},
	{0x47, 0x28},
	{0x48, 0x3a},
	{0x59, 0x88},
	{0x5a, 0x88},

	{0x5b, 0x44},//90
	{0x5c, 0x67},
	{0x5d, 0x49},
	{0x5e, 0x0e},
	{0x64, 0x04},
	{0x65, 0x20},

	{0x66, 0x05},
	{0x94, 0x04},
	{0x95, 0x08},
	{0x6c, 0x0a},
	{0x6d, 0x55},


	{0x4f, 0x80},
	{0x50, 0x80},
	{0x51, 0x00},
	{0x52, 0x22},
	{0x53, 0x5e},
	{0x54, 0x80},

	{0x76, 0xe1},

	{0x6e, 0x11},//100
	{0x6f, 0x9f},//0x9e for advance AWB
	{0x55, 0x00},//����
	{0x56, 0x40},//�Աȶ�
	{0x57, 0x80},//0x40,
};

/*
*********************************************************************************************************
*   函 数 名: CAM_ConfigCPU
*   功能说明: CAM_ConfigCPU 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void CAM_ConfigCPU(void);
/*
*********************************************************************************************************
*   函 数 名: OV_InitReg
*   功能说明: OV_InitReg 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t OV_InitReg(void);
/*
*********************************************************************************************************
*   函 数 名: OV_WriteReg
*   功能说明: OV_WriteReg 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void OV_WriteReg(uint8_t _ucRegAddr, uint8_t _ucRegValue);
/*
*********************************************************************************************************
*   函 数 名: OV_ReadReg
*   功能说明: OV_ReadReg 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t OV_ReadReg(uint8_t _ucRegAddr);

CAM_T g_tCam;

DCMI_HandleTypeDef hdcmi;
DMA_HandleTypeDef  hdma_dcmi;

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitCamera
*	����˵��: ��������ͷGPIO��CAMERA�豸.
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitCamera(void)
{
	CAM_ConfigCPU();

	#if 1	/* ����Ĵ��룬��֤��д�Ĵ����Ƿ���ȷ */
	{
		uint8_t read;

		read = OV_ReadReg(0x3A);

		OV_WriteReg(0x3A, read + 1);

		read = OV_ReadReg(0x3A);

		OV_WriteReg(0x3A, read + 1);

		read = OV_ReadReg(0x3A);
	}
	#endif

	OV_InitReg();
}

/*
*********************************************************************************************************
*	�� �� ��: CAM_ConfigCPU
*	����˵��: ��������ͷGPIO��CAMERA�豸��0V7670��I2C�ӿ������� bsp_gpio_i2c.c �ļ�ʵ�֡�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void CAM_ConfigCPU(void)
{
 	/* ������STM32-V7�����������Դ�����ṩ24Mʱ�ӣ���˲���PA8����ʱ�� */

	/*
		��DCMI��ص�GPIO����Ϊ����ģʽ  - STM32-V7
			PA4/DCMI_HSYNC/DAC_OUT1		
			PA6/DCMI_PIXCLK
			PB7/DCMI_VSYNC			
			PC6/DCMI_D0/AD7606_CONVST
			PC7/DCMI_D1/USART6_RX
			PD3/DCMI_D5
			PE4/DCMI_D4/NRF24L01_IRQ
			PE5/DCMI_D6/AD7606_BUSY
			PE6/DCMI_D7/NRF905_CD			
			PG10/DCMI_D2/NRF24L01_CSN
			PG11/DCMI_D3/ETH_RMII_TX_EN
	*/
	{  	
		GPIO_InitTypeDef GPIO_InitStruct;
		
		/* Peripheral clock enable */
		__HAL_RCC_DCMI_CLK_ENABLE();
		
		__HAL_RCC_GPIOA_CLK_ENABLE();
		__HAL_RCC_GPIOB_CLK_ENABLE();
		__HAL_RCC_GPIOC_CLK_ENABLE();
		__HAL_RCC_GPIOD_CLK_ENABLE();
		__HAL_RCC_GPIOE_CLK_ENABLE();
		__HAL_RCC_GPIOG_CLK_ENABLE();
		
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
		
		GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_6;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);		
		
		GPIO_InitStruct.Pin = GPIO_PIN_7;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
		HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);	

		GPIO_InitStruct.Pin = GPIO_PIN_3;
		HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

		GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6;
		HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);	
		
		GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11;
		HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);	
	}

	/* ���� DCMIC ���� */
	{
		/* DCMI INIT */
		hdcmi.Instance = DCMI;
		hdcmi.Init.SynchroMode = DCMI_SYNCHRO_HARDWARE;
		hdcmi.Init.PCKPolarity = DCMI_PCKPOLARITY_RISING;   
		hdcmi.Init.VSPolarity = DCMI_VSPOLARITY_HIGH;              
		hdcmi.Init.HSPolarity = DCMI_HSPOLARITY_LOW;    
		hdcmi.Init.CaptureRate = DCMI_CR_ALTERNATE_2_FRAME;
		hdcmi.Init.ExtendedDataMode = DCMI_EXTEND_DATA_8B;                  
		hdcmi.Init.JPEGMode = DCMI_JPEG_DISABLE;  
		hdcmi.Init.ByteSelectMode = DCMI_BSM_ALL;
		hdcmi.Init.ByteSelectStart = DCMI_OEBS_ODD;
		hdcmi.Init.LineSelectMode = DCMI_LSM_ALL;
		hdcmi.Init.LineSelectStart = DCMI_OELS_ODD; 
		if (HAL_DCMI_Init(&hdcmi) != HAL_OK)
		{
			Error_Handler(__FILE__, __LINE__);
		}

		/* DCMI interrupt Init */
		HAL_NVIC_SetPriority(DCMI_IRQn, 0, 0);
		HAL_NVIC_EnableIRQ(DCMI_IRQn);			
	}
}

/*
*********************************************************************************************************
*	�� �� ��: OV_InitReg
*	����˵��: ��λOV7670, ����OV7670�ļĴ�����QVGA
*	��    ��: ��
*	�� �� ֵ: 0 ��ʾ��ȷ��1��ʾʧ��
*********************************************************************************************************
*/
static uint8_t OV_InitReg(void)
{
  	uint8_t i;

  	CAM_ConfigCPU();
	//bsp_InitI2C();		/* ����I2C����, �� bsp.c �ļ�ִ���� */

	OV_WriteReg(0x12, 0x80); 	/* Reset SCCB */

	bsp_DelayMS(5);

	/* ���� OV7670�Ĵ��� */
  	for (i = 0; i < OV_REG_NUM; i++)
  	{
		OV_WriteReg(OV_reg[i][0], OV_reg[i][1]);
  	}
	return 0;
}

/*
*********************************************************************************************************
*	�� �� ��: CAM_Start
*	����˵��: ����DMA��DCMI����ʼ����ͼ�����ݵ�LCD�Դ�
*	��    ��: _uiDispMemAddr �Դ��ַ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void CAM_Start(uint32_t _uiDispMemAddr)
{	
	memset((char *)_uiDispMemAddr, 0,  2000);
	/* DCMI DMA Init */
	{
		hdma_dcmi.Instance = DMA1_Stream7;
		hdma_dcmi.Init.Request = DMA_REQUEST_DCMI;
		hdma_dcmi.Init.Direction = DMA_PERIPH_TO_MEMORY;
		hdma_dcmi.Init.PeriphInc = DMA_PINC_DISABLE;
		hdma_dcmi.Init.MemInc = DMA_MINC_ENABLE;
		hdma_dcmi.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
		hdma_dcmi.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
		hdma_dcmi.Init.Mode = DMA_CIRCULAR;
		hdma_dcmi.Init.Priority = DMA_PRIORITY_LOW;
		hdma_dcmi.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
		if (HAL_DMA_Init(&hdma_dcmi) != HAL_OK)
		{
			Error_Handler(__FILE__, __LINE__);
		}

		__HAL_LINKDMA(&hdcmi, DMA_Handle, hdma_dcmi);

		  /* DMA controller clock enable */
		__HAL_RCC_DMA1_CLK_ENABLE();

		/* DMA interrupt init */
		/* DMA1_Stream7_IRQn interrupt configuration */
		HAL_NVIC_SetPriority(DMA1_Stream7_IRQn, 0, 0);
		HAL_NVIC_EnableIRQ(DMA1_Stream7_IRQn);		
	}
	
	//HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_CONTINUOUS, (uint32_t)buff, 128);
	HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_SNAPSHOT, (uint32_t)_uiDispMemAddr, 265*320*2);	

	g_tCam.CaptureOk = 0;		/* ȫ�ֱ�־ */
}

/*
*********************************************************************************************************
*	�� �� ��: CAM_Stop
*	����˵��: ֹͣDMA��DCMI
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void CAM_Stop(void)
{
	HAL_DCMI_Stop(&hdcmi);
}

/*
*********************************************************************************************************
*   函 数 名: DMA1_Stream7_IRQHandler
*   功能说明: DMA1_Stream7_IRQHandler 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DMA1_Stream7_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Stream7_IRQn 0 */

  /* USER CODE END DMA1_Stream7_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_dcmi);
  /* USER CODE BEGIN DMA1_Stream7_IRQn 1 */

  /* USER CODE END DMA1_Stream7_IRQn 1 */
}

/*
*********************************************************************************************************
*	�� �� ��: DMA2_Stream1_IRQHandler
*	����˵��: DMA��������жϷ������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void DCMI_IRQHandler(void)
{
	HAL_DCMI_IRQHandler(&hdcmi);
	
	/* �ر����� */
	CAM_Stop();
	g_tCam.CaptureOk = 1;		/* ��ʾDMA������� */
}

/*
*********************************************************************************************************
*	�� �� ��: OV_WriteReg
*	����˵��: д0V7670�Ĵ���
*	��    ��: _ucRegAddr  : �Ĵ�����ַ
*			  _ucRegValue : �Ĵ���ֵ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void OV_WriteReg(uint8_t _ucRegAddr, uint8_t _ucRegValue)
{
    i2c_Start();							/* ���߿�ʼ�ź� */

    i2c_SendByte(OV7670_SLAVE_ADDRESS);		/* �����豸��ַ+д�ź� */
	i2c_WaitAck();

    i2c_SendByte(_ucRegAddr);				/* ���ͼĴ�����ַ */
	i2c_WaitAck();

    i2c_SendByte(_ucRegValue);				/* ���ͼĴ�����ֵ */
	i2c_WaitAck();

    i2c_Stop();                   			/* ����ֹͣ�ź� */
}

/*
*********************************************************************************************************
*	�� �� ��: OV_ReadReg
*	����˵��: ��0V7670�Ĵ���
*	��    ��: _ucRegAddr  : �Ĵ�����ַ
*	�� �� ֵ: �Ĵ���ֵ
*********************************************************************************************************
*/
static uint8_t OV_ReadReg(uint8_t _ucRegAddr)
{
	uint16_t usRegValue;

	i2c_Start();                  			/* ���߿�ʼ�ź� */
	i2c_SendByte(OV7670_SLAVE_ADDRESS);		/* �����豸��ַ+д�ź� */
	i2c_WaitAck();
	i2c_SendByte(_ucRegAddr);				/* ���͵�ַ */
	i2c_WaitAck();

	i2c_Stop();			/* 0V7670 ��Ҫ���� stop, �����ȡ�Ĵ���ʧ�� */

	i2c_Start();                  			/* ���߿�ʼ�ź� */
	i2c_SendByte(OV7670_SLAVE_ADDRESS + 1);/* �����豸��ַ+���ź� */
	i2c_WaitAck();

	usRegValue = i2c_ReadByte();       		/* �������ֽ����� */
	i2c_NAck();
	i2c_Stop();                  			/* ����ֹͣ�ź� */

	return usRegValue;
}

/*
*********************************************************************************************************
*	�� �� ��: OV_ReadID
*	����˵��: ��0V7670��оƬID
*	��    ��: ��
*	�� �� ֵ: оƬID. ����Ӧ�÷��� 0x7673
*********************************************************************************************************
*/
uint16_t OV_ReadID(void)
{
	uint8_t idh,idl;

	idh = OV_ReadReg(0x0A);
	idl = OV_ReadReg(0x0B);
	return (idh << 8) + idl;
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
