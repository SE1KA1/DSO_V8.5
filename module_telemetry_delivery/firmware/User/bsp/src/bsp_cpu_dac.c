/**
 * @file bsp_cpu_dac.c
 * @brief 安富莱 BSP 通用驱动（CPU 内置 DAC）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : DAC���η�����
*	�ļ����� : bsp_dac_wave.c
*	��    �� : V1.0
*	˵    �� : ʹ��STM32�ڲ�DAC������Ρ�֧��DAC1��DAC2�����ͬ�Ĳ��Ρ�
*
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2015-10-06  armfly  ��ʽ����
*
*	Copyright (C), 2015-2020, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/*
	PA4���� DAC_OUT1
	PA5���� DAC_OUT2   �����õ�2·�������ſ���G2B˥����

	DAC1ʹ����TIM6��Ϊ��ʱ������ DMAͨ��: DMA1_Stream5
//	DAC2ʹ����TIM7��Ϊ��ʱ������ DMAͨ��: DMA2_Stream6	
	
	DAC_InitStructure.DAC_OutputBuffer = DAC_OutputBuffer_Enable ������DAC������壬������������,
	���˻���֮�󣬿���0V�Ͳο���Դʱ��ʧ�����������50mV
	�������岨�νϺã���0VĿ�ⲻ������ʧ�档
	
	���ܣ�
	1��������Ҳ������Ⱥ�Ƶ�ʿɵ���
	2���������������ƫ�ƿɵ��ڣ�Ƶ�ʿɵ��ڣ�ռ�ձȿ��Ե���
	3��������ǲ������ȿɵ��ڣ�Ƶ�ʿɵ��ڣ�������ռ�ȿɵ���
	4��������DAC���ֱ����ƽ�ĺ���
*/

#define DACx                            DAC1
/* 宏定义: DACx_CHANNEL_GPIO_CLK_ENABLE - DACx_CHANNEL_GPIO_CLK_ENABLE 的宏定义。 */
#define DACx_CHANNEL_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOA_CLK_ENABLE()
/* 宏定义: DMAx_CLK_ENABLE - DMAx_CLK_ENABLE 的宏定义。 */
#define DMAx_CLK_ENABLE()               __HAL_RCC_DMA1_CLK_ENABLE()

/* 宏定义: DACx_CLK_ENABLE - DACx_CLK_ENABLE 的宏定义。 */
#define DACx_CLK_ENABLE()               __HAL_RCC_DAC12_CLK_ENABLE()
/* 宏定义: DACx_FORCE_RESET - DACx_FORCE_RESET 的宏定义。 */
#define DACx_FORCE_RESET()              __HAL_RCC_DAC12_FORCE_RESET()
/* 宏定义: DACx_RELEASE_RESET - DACx_RELEASE_RESET 的宏定义。 */
#define DACx_RELEASE_RESET()            __HAL_RCC_DAC12_RELEASE_RESET()
					  
					  /* Definition for DACx Channel Pin */
#define DACx_CHANNEL_PIN                GPIO_PIN_4
/* 宏定义: DACx_CHANNEL_GPIO_PORT - DACx_CHANNEL_GPIO_PORT 的宏定义。 */
#define DACx_CHANNEL_GPIO_PORT          GPIOA
					  
/* Definition for DACx's DMA_STREAM */
#define DACx_CHANNEL                    DAC_CHANNEL_1

/* Definition for DACx's DMA_STREAM */
#define DACx_DMA_INSTANCE               DMA1_Stream5

/* Definition for DACx's NVIC */
#define DACx_DMA_IRQn                   DMA1_Stream5_IRQn
/* 宏定义: DACx_DMA_IRQHandler - DACx_DMA_IRQHandler 的宏定义。 */
#define DACx_DMA_IRQHandler             DMA1_Stream5_IRQHandler


/*  ���Ҳ����ݣ�12bit��1������128����, 0-4095֮��仯 */
const uint16_t g_SineWave128[] = {
	2047 ,
	2147 ,
	2248 ,
	2347 ,
	2446 ,
	2544 ,
	2641 ,
	2737 ,
	2830 ,
	2922 ,
	3012 ,
	3099 ,
	3184 ,
	3266 ,
	3346 ,
	3422 ,
	3494 ,
	3564 ,
	3629 ,
	3691 ,
	3749 ,
	3803 ,
	3852 ,
	3897 ,
	3938 ,
	3974 ,
	4006 ,
	4033 ,
	4055 ,
	4072 ,
	4084 ,
	4092 ,
	4094 ,
	4092 ,
	4084 ,
	4072 ,
	4055 ,
	4033 ,
	4006 ,
	3974 ,
	3938 ,
	3897 ,
	3852 ,
	3803 ,
	3749 ,
	3691 ,
	3629 ,
	3564 ,
	3494 ,
	3422 ,
	3346 ,
	3266 ,
	3184 ,
	3099 ,
	3012 ,
	2922 ,
	2830 ,
	2737 ,
	2641 ,
	2544 ,
	2446 ,
	2347 ,
	2248 ,
	2147 ,
	2047 ,
	1947 ,
	1846 ,
	1747 ,
	1648 ,
	1550 ,
	1453 ,
	1357 ,
	1264 ,
	1172 ,
	1082 ,
	995  ,
	910  ,
	828  ,
	748  ,
	672  ,
	600  ,
	530  ,
	465  ,
	403  ,
	345  ,
	291  ,
	242  ,
	197  ,
	156  ,
	120  ,
	88   ,
	61   ,
	39   ,
	22   ,
	10   ,
	2    ,
	0    ,
	2    ,
	10   ,
	22   ,
	39   ,
	61   ,
	88   ,
	120  ,
	156  ,
	197  ,
	242  ,
	291  ,
	345  ,
	403  ,
	465  ,
	530  ,
	600  ,
	672  ,
	748  ,
	828  ,
	910  ,
	995  ,
	1082 ,
	1172 ,
	1264 ,
	1357 ,
	1453 ,
	1550 ,
	1648 ,
	1747 ,
	1846 ,
	1947
};

/* ���Ҳ� (32�������ʺϸ�Ƶ�� */
const uint16_t g_SineWave32[32] = {
                      2047, 2447, 2831, 3185, 3498, 3750, 3939, 4056, 4095, 4056,
                      3939, 3750, 3495, 3185, 2831, 2447, 2047, 1647, 1263, 909,
                      599, 344, 155, 38, 0, 38, 155, 344, 599, 909, 1263, 1647};

/* DMA���λ����� */
uint16_t g_Wave1[128];
//uint16_t g_Wave2[128];
					  
DAC_HandleTypeDef    DacHandle;
static DAC_ChannelConfTypeDef sConfig;

	
/*
*********************************************************************************************************
*	�� �� ��: bsp_InitDAC1
*	����˵��: ����PA4/DAC1�� ������DMA������ bsp_SetDAC1()�����޸����DACֵ
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitDAC1(void)
{	
	/* ����DAC, �޴���������DMA */
	{		
		DacHandle.Instance = DACx;

		HAL_DAC_DeInit(&DacHandle);
		
		  /*##-1- Initialize the DAC peripheral ######################################*/
		if (HAL_DAC_Init(&DacHandle) != HAL_OK)
		{
			Error_Handler(__FILE__, __LINE__);
		}

		/*##-1- DAC channel1 Configuration #########################################*/
		sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
		sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;

		if (HAL_DAC_ConfigChannel(&DacHandle, &sConfig, DAC_CHANNEL_1) != HAL_OK)
		{
			Error_Handler(__FILE__, __LINE__);
		}

		/*##-2- Enable DAC selected channel and associated DMA #############################*/
		if (HAL_DAC_Start(&DacHandle, DAC_CHANNEL_1) != HAL_OK)  
		{
			Error_Handler(__FILE__, __LINE__);
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_SetDAC1
*	����˵��: ����DAC1������ݼĴ������ı������ѹ
*	��    ��: _dac : DAC���ݣ�0-4095
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_SetDAC1(uint16_t _dac)
{
	DAC1->DHR12R1 = _dac;
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_SetDAC2
*	����˵��: ����DAC2������ݼĴ������ı������ѹ
*	��    ��: _dac : DAC���ݣ�0-4095
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_SetDAC2(uint16_t _dac)
{
	DAC1->DHR12R2 = _dac;
}

/*
*********************************************************************************************************
*	�� �� ��: HAL_DAC_MspInit
*	����˵��: ����DAC�õ���ʱ�ӣ����ź�DMAͨ��
*	��    ��: hdac  DAC_HandleTypeDef���ͽṹ��ָ�����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_DAC_MspInit(DAC_HandleTypeDef *hdac)
{
	GPIO_InitTypeDef          GPIO_InitStruct;

	/*##-1- ʹ��ʱ�� #################################*/
	/* ʹ��GPIOʱ�� */
	DACx_CHANNEL_GPIO_CLK_ENABLE();
	/* ʹ��DAC����ʱ�� */
	DACx_CLK_ENABLE();

	/*##-2- ����GPIO ##########################################*/
	/* DAC Channel1 GPIO ���� */
	GPIO_InitStruct.Pin = DACx_CHANNEL_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(DACx_CHANNEL_GPIO_PORT, &GPIO_InitStruct);
}

/*
*********************************************************************************************************
*	�� �� ��: HAL_DAC_MspDeInit
*	����˵��: ��λDAC
*	��    ��: hdac  DAC_HandleTypeDef���ͽṹ��ָ�����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_DAC_MspDeInit(DAC_HandleTypeDef *hdac)
{
	/*##-1- ��λDAC���� ##################################################*/
	DACx_FORCE_RESET();
	DACx_RELEASE_RESET();

	/*##-2- ��λDAC��ӦGPIO ################################*/
	HAL_GPIO_DeInit(DACx_CHANNEL_GPIO_PORT, DACx_CHANNEL_PIN);

	/*##-3- �ر�DAC�õ�DMA Stream ############################################*/
	HAL_DMA_DeInit(hdac->DMA_Handle1);

	/*##-4- �ر�DMA�ж� ###########################################*/
	HAL_NVIC_DisableIRQ(DACx_DMA_IRQn);
}

/*
*********************************************************************************************************
*	�� �� ��: HAL_TIM_Base_MspInit
*	����˵��: ��ʼ����ʱ��ʱ��
*	��    ��: htim  TIM_HandleTypeDef���ͽṹ��ָ�����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
	/* TIM6 ʱ��ʹ�� */
	if (htim->Instance == TIM6)
	{
		/* TIM6 ʱ��ʹ�� */
		__HAL_RCC_TIM6_CLK_ENABLE();
	}
}

/*
*********************************************************************************************************
*	�� �� ��: HAL_TIM_Base_MspDeInit
*	����˵��: ��λ��ʱ��ʱ��
*	��    ��: htim  TIM_HandleTypeDef���ͽṹ��ָ�����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef *htim)
{
	/*##-1- ��λ���� ##################################################*/
	if (htim->Instance == TIM6)
	{
		/*##-1- ��λTIM6 ##################################################*/
		__HAL_RCC_TIM6_FORCE_RESET();
		__HAL_RCC_TIM6_RELEASE_RESET();
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TIM6_Config
*	����˵��: ����TIM6��ΪDAC����Դ
*	��    ��: _freq : ����Ƶ�ʣ���λHz
*	�� �� ֵ: ��
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*	�� �� ��: bsp_StartDAC1_DMA
*	����˵��: ����PA4 ΪDAC_OUT1, ����DMA, ��ʼ�����������
*	��    ��: _BufAddr : DMA���ݻ�������ַ�� ���붨λ��0x24000000 RAM����flash����
*			  _Count  : ��������������
*			 _DacFreq : DAC��������Ƶ��, Hz,оƬ��������1MHz�� ʵ����Ե�10MHz.
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_StartDAC1_DMA(uint32_t _BufAddr, uint32_t _Count, uint32_t _DacFreq)
{		
	(void)_DacFreq;  /* Ƶ����TIM8 TRGO����������������Ϊ���ݾɽӿ� */
		
	/* ����DAC, TIM8 TRGO������DMA���� */
	{		
		DacHandle.Instance = DACx;

		HAL_DAC_DeInit(&DacHandle);
		
		  /*##-1- Initialize the DAC peripheral ######################################*/
		if (HAL_DAC_Init(&DacHandle) != HAL_OK)
		{
			Error_Handler(__FILE__, __LINE__);
		}

		/* ����DMA */
		{
			static DMA_HandleTypeDef  hdma_dac1;
			
			/* ʹ��DMA1ʱ�� */
			DMAx_CLK_ENABLE();
			
			/* ���� DACx_DMA_STREAM  */
			hdma_dac1.Instance = DACx_DMA_INSTANCE;

			hdma_dac1.Init.Request  = DMA_REQUEST_DAC1;

			hdma_dac1.Init.Direction = DMA_MEMORY_TO_PERIPH;
			hdma_dac1.Init.PeriphInc = DMA_PINC_DISABLE;
			hdma_dac1.Init.MemInc = DMA_MINC_ENABLE;
			hdma_dac1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
			hdma_dac1.Init.MemDataAlignment = DMA_PDATAALIGN_HALFWORD;
			hdma_dac1.Init.Mode = DMA_CIRCULAR;
			hdma_dac1.Init.Priority = DMA_PRIORITY_HIGH;

			HAL_DMA_Init(&hdma_dac1);

			/* ����DMA�����DAC����� */
			__HAL_LINKDMA(&DacHandle, DMA_Handle1, hdma_dac1);
		}		

		/*##-1- DAC channel1 Configuration #########################################*/
		sConfig.DAC_Trigger = DAC_TRIGGER_T8_TRGO;
		sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;

		if (HAL_DAC_ConfigChannel(&DacHandle, &sConfig, DAC_CHANNEL_1) != HAL_OK)
		{
			Error_Handler(__FILE__, __LINE__);
		}

		/*##-2- Enable DAC selected channel and associated DMA #############################*/
		if (HAL_DAC_Start_DMA(&DacHandle, DAC_CHANNEL_1, (uint32_t *)_BufAddr, _Count, DAC_ALIGN_12B_R) != HAL_OK)  
		{
			Error_Handler(__FILE__, __LINE__);
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: dac1_SetSinWave
*	����˵��: DAC1������Ҳ�
*	��    ��: _vpp : ���� 0-2047;
*			  _freq : Ƶ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void dac1_SetSinWave(uint16_t _vpp, uint32_t _freq)
{	
	uint32_t i;
	uint32_t dac;
		
	/* �������Ҳ����� */		
	for (i = 0; i < 128; i++)
	{
		dac = (g_SineWave128[i] * _vpp) / 2047;
		if (dac > 4095)
		{
			dac = 4095;	
		}
		g_Wave1[i] = dac;
	}
	
	bsp_StartDAC1_DMA((uint32_t)&g_Wave1, 128, _freq * 128);
}

/*
*********************************************************************************************************
*	�� �� ��: dac1_SetRectWave
*	����˵��: DAC1�������
*	��    ��: _low : �͵�ƽʱDAC, 
*			  _high : �ߵ�ƽʱDAC
*			  _freq : Ƶ�� Hz
*			  _duty : ռ�ձ� 2% - 98%, ���ڲ��� 1%
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void dac1_SetRectWave(uint16_t _low, uint16_t _high, uint32_t _freq, uint16_t _duty)
{	
	uint16_t i;
	
	for (i = 0; i < (_duty * 128) / 100; i++)
	{
		g_Wave1[i] = _high;
	}
	for (; i < 128; i++)
	{
		g_Wave1[i] = _low;
	}
	
	bsp_StartDAC1_DMA((uint32_t)&g_Wave1, 128, _freq * 128);
}

/*
*********************************************************************************************************
*	�� �� ��: dac1_SetTriWave
*	����˵��: DAC1������ǲ�
*	��    ��: _low : �͵�ƽʱDAC, 
*			  _high : �ߵ�ƽʱDAC
*			  _freq : Ƶ�� Hz
*			  _duty : ռ�ձ�
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void dac1_SetTriWave(uint16_t _low, uint16_t _high, uint32_t _freq, uint16_t _duty)
{	
	uint32_t i;
	uint16_t dac;
	uint16_t m;
		
	/* �������ǲ����飬128���������� _low �� _high */		
	m = (_duty * 128) / 100;
	
	if (m == 0)
	{
		m = 1;
	}
	
	if (m > 127)
	{
		m = 127;
	}
	for (i = 0; i < m; i++)
	{
		dac = _low + ((_high - _low) * i) / m;
		g_Wave1[i] = dac;
	}
	for (; i < 128; i++)
	{
		dac = _high - ((_high - _low) * (i - m)) / (128 - m);
		g_Wave1[i] = dac;
	}	
	
	bsp_StartDAC1_DMA((uint32_t)&g_Wave1, 128, _freq * 128);
}

/*
*********************************************************************************************************
*	�� �� ��: dac1_StopWave
*	����˵��: ֹͣDAC1���
*	��    ��: ��
*			  _freq : Ƶ�� 0-5Hz
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void dac1_StopWave(void)
{	
	__HAL_RCC_DAC12_FORCE_RESET();
	__HAL_RCC_DAC12_RELEASE_RESET();
	
	HAL_DMA_DeInit(DacHandle.DMA_Handle1);
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/



