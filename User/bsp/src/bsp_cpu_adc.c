/**
 * @file bsp_cpu_adc.c
 * @brief 安富莱 BSP 通用驱动（CPU 内置 ADC）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : ʾ����ģ��ADC�ײ������
*	�ļ����� : bsp_adc_dso.c
*	��    �� : V1.0
*	˵    �� : ʹ��STM32�ڲ�ADC��ͬ���ɼ���·���Ρ�ռ���˲���GPIO����ʾ����ģ����������Ϸ�ʽ��
*				ʹ�� ADC_EXTERNALTRIG_T3_TRGO ��ΪADC����Դ
*
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2018-10-25  armfly  ��ʽ����
*
*	Copyright (C), 2018-2030, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/*
	STM32-V7�����壬ʾ����ģ��GPIO����
			
	---------------------------------------------------------
	D112-2 �汾��ʾ����ģ��  ���°棩
		ʾ����ͨ��1
			PC3  = ADC123_IN13
			AC1  = PB7    ����AC-DC�л�, 1��ʾDC, 0��ʾAC
			G1A  = PC6    ����˥����
			G1B  = PC7    ����˥����

		ʾ����ͨ��2
			PC0  = ADC123_IN10
			AC2  = PA6   ����AC-DC�л�, 1��ʾDC, 0��ʾAC
			G2A  = PG10  ����˥����
			G2B  = PA5   ����˥����	
			
		DAC = PA4
*/	
#define AC1_CLK_ENABLE()	__HAL_RCC_GPIOB_CLK_ENABLE()
/* 宏定义: AC1_GPIO - AC1_GPIO 的宏定义。 */
#define AC1_GPIO			GPIOB
/* 宏定义: AC1_PIN - AC1_PIN 的宏定义。 */
#define AC1_PIN				GPIO_PIN_7
/* 宏定义: AC1_0 - AC1_0 的宏定义。 */
#define AC1_0()				AC1_GPIO->BSRRH = AC1_PIN
/* 宏定义: AC1_1 - AC1_1 的宏定义。 */
#define AC1_1()				AC1_GPIO->BSRRL = AC1_PIN

/* 宏定义: AC2_CLK_ENABLE - AC2_CLK_ENABLE 的宏定义。 */
#define AC2_CLK_ENABLE()	__HAL_RCC_GPIOA_CLK_ENABLE()
/* 宏定义: AC2_GPIO - AC2_GPIO 的宏定义。 */
#define AC2_GPIO			GPIOA
/* 宏定义: AC2_PIN - AC2_PIN 的宏定义。 */
#define AC2_PIN				GPIO_PIN_6
/* 宏定义: AC2_0 - AC2_0 的宏定义。 */
#define AC2_0()				AC2_GPIO->BSRRH = AC2_PIN
/* 宏定义: AC2_1 - AC2_1 的宏定义。 */
#define AC2_1()				AC2_GPIO->BSRRL = AC2_PIN

/* 宏定义: G1A_CLK_ENABLE - G1A_CLK_ENABLE 的宏定义。 */
#define G1A_CLK_ENABLE()	__HAL_RCC_GPIOC_CLK_ENABLE()
/* 宏定义: G1A_GPIO - G1A_GPIO 的宏定义。 */
#define G1A_GPIO			GPIOC
/* 宏定义: G1A_PIN - G1A_PIN 的宏定义。 */
#define G1A_PIN				GPIO_PIN_6
/* 宏定义: G1A_0 - G1A_0 的宏定义。 */
#define G1A_0()				G1A_GPIO->BSRRH = G1A_PIN
/* 宏定义: G1A_1 - G1A_1 的宏定义。 */
#define G1A_1()				G1A_GPIO->BSRRL = G1A_PIN

/* 宏定义: G1B_CLK_ENABLE - G1B_CLK_ENABLE 的宏定义。 */
#define G1B_CLK_ENABLE()	__HAL_RCC_GPIOC_CLK_ENABLE()
/* 宏定义: G1B_GPIO - G1B_GPIO 的宏定义。 */
#define G1B_GPIO			GPIOC
/* 宏定义: G1B_PIN - G1B_PIN 的宏定义。 */
#define G1B_PIN				GPIO_PIN_7
/* 宏定义: G1B_0 - G1B_0 的宏定义。 */
#define G1B_0()				G1B_GPIO->BSRRH = G1B_PIN
/* 宏定义: G1B_1 - G1B_1 的宏定义。 */
#define G1B_1()				G1B_GPIO->BSRRL = G1B_PIN

/* 宏定义: G2A_CLK_ENABLE - G2A_CLK_ENABLE 的宏定义。 */
#define G2A_CLK_ENABLE()	__HAL_RCC_GPIOG_CLK_ENABLE()
/* 宏定义: G2A_GPIO - G2A_GPIO 的宏定义。 */
#define G2A_GPIO			GPIOG
/* 宏定义: G2A_PIN - G2A_PIN 的宏定义。 */
#define G2A_PIN				GPIO_PIN_10
/* 宏定义: G2A_0 - G2A_0 的宏定义。 */
#define G2A_0()				G2A_GPIO->BSRRH = G2A_PIN
/* 宏定义: G2A_1 - G2A_1 的宏定义。 */
#define G2A_1()				G2A_GPIO->BSRRL = G2A_PIN

/* 宏定义: G2B_CLK_ENABLE - G2B_CLK_ENABLE 的宏定义。 */
#define G2B_CLK_ENABLE()	__HAL_RCC_GPIOA_CLK_ENABLE()
/* 宏定义: G2B_GPIO - G2B_GPIO 的宏定义。 */
#define G2B_GPIO			GPIOA
/* 宏定义: G2B_PIN - G2B_PIN 的宏定义。 */
#define G2B_PIN				GPIO_PIN_5
/* 宏定义: G2B_0 - G2B_0 的宏定义。 */
#define G2B_0()				G2B_GPIO->BSRRH = G2B_PIN
/* 宏定义: G2B_1 - G2B_1 的宏定义。 */
#define G2B_1()				G2B_GPIO->BSRRL = G2B_PIN

    
/* ADC CH1ͨ��GPIO, ADCͨ����DMA���� */
#define ADCH1                            ADC1
/* 宏定义: ADCH1_CLK_ENABLE - ADCH1_CLK_ENABLE 的宏定义。 */
#define ADCH1_CLK_ENABLE()               __HAL_RCC_ADC12_CLK_ENABLE()

/* 宏定义: ADCH1_FORCE_RESET - ADCH1_FORCE_RESET 的宏定义。 */
#define ADCH1_FORCE_RESET()              __HAL_RCC_ADC12_FORCE_RESET()
/* 宏定义: ADCH1_RELEASE_RESET - ADCH1_RELEASE_RESET 的宏定义。 */
#define ADCH1_RELEASE_RESET()            __HAL_RCC_ADC12_RELEASE_RESET()

/* 宏定义: ADCH1_CHANNEL_GPIO_CLK_ENABLE - ADCH1_CHANNEL_GPIO_CLK_ENABLE 的宏定义。 */
#define ADCH1_CHANNEL_GPIO_CLK_ENABLE()   __HAL_RCC_GPIOC_CLK_ENABLE()
/* 宏定义: ADCH1_CHANNEL_PIN - ADCH1_CHANNEL_PIN 的宏定义。 */
#define ADCH1_CHANNEL_PIN                GPIO_PIN_3
/* 宏定义: ADCH1_CHANNEL_GPIO_PORT - ADCH1_CHANNEL_GPIO_PORT 的宏定义。 */
#define ADCH1_CHANNEL_GPIO_PORT          GPIOC

/* 宏定义: ADCH1_CHANNEL - ADCH1_CHANNEL 的宏定义。 */
#define ADCH1_CHANNEL                    ADC_CHANNEL_13

/* 宏定义: CH1_DMA_CLK_ENABLE - CH1_DMA_CLK_ENABLE 的宏定义。 */
#define CH1_DMA_CLK_ENABLE()           __HAL_RCC_DMA1_CLK_ENABLE()
/* 宏定义: CH1_DMA_Stream - CH1_DMA_Stream 的宏定义。 */
#define CH1_DMA_Stream					 DMA1_Stream1
/* 宏定义: CH1_DMA_Stream_IRQn - CH1_DMA_Stream_IRQn 的宏定义。 */
#define CH1_DMA_Stream_IRQn				 DMA1_Stream1_IRQn
/* 宏定义: CH1_DMA_Stream_IRQHandle - CH1_DMA_Stream_IRQHandle 的宏定义。 */
#define CH1_DMA_Stream_IRQHandle         DMA1_Stream1_IRQHandler
/* 宏定义: CH1_DMA_REQUEST_ADC - CH1_DMA_REQUEST_ADC 的宏定义。 */
#define CH1_DMA_REQUEST_ADC				 DMA_REQUEST_ADC1

/* ADC CH2ͨ��GPIO, ADCͨ����DMA���� */
#if 1
/* 宏定义: ADCH2 - ADCH2 的宏定义。 */
	#define ADCH2                            ADC2
/* 宏定义: ADCH2_CLK_ENABLE - ADCH2_CLK_ENABLE 的宏定义。 */
	#define ADCH2_CLK_ENABLE()               __HAL_RCC_ADC12_CLK_ENABLE()

/* 宏定义: ADCH2_FORCE_RESET - ADCH2_FORCE_RESET 的宏定义。 */
	#define ADCH2_FORCE_RESET()              __HAL_RCC_ADC12_FORCE_RESET()
/* 宏定义: ADCH2_RELEASE_RESET - ADCH2_RELEASE_RESET 的宏定义。 */
	#define ADCH2_RELEASE_RESET()            __HAL_RCC_ADC12_RELEASE_RESET()

/* 宏定义: ADCH2_CHANNEL_GPIO_CLK_ENABLE - ADCH2_CHANNEL_GPIO_CLK_ENABLE 的宏定义。 */
	#define ADCH2_CHANNEL_GPIO_CLK_ENABLE()   __HAL_RCC_GPIOC_CLK_ENABLE()
/* 宏定义: ADCH2_CHANNEL_PIN - ADCH2_CHANNEL_PIN 的宏定义。 */
	#define ADCH2_CHANNEL_PIN                GPIO_PIN_0
/* 宏定义: ADCH2_CHANNEL_GPIO_PORT - ADCH2_CHANNEL_GPIO_PORT 的宏定义。 */
	#define ADCH2_CHANNEL_GPIO_PORT          GPIOC

/* 宏定义: ADCH2_CHANNEL - ADCH2_CHANNEL 的宏定义。 */
	#define ADCH2_CHANNEL                    ADC_CHANNEL_10


/* 宏定义: CH2_DMA_CLK_ENABLE - CH2_DMA_CLK_ENABLE 的宏定义。 */
	#define CH2_DMA_CLK_ENABLE()           __HAL_RCC_DMA2_CLK_ENABLE()
/* 宏定义: CH2_DMA_Stream - CH2_DMA_Stream 的宏定义。 */
	#define CH2_DMA_Stream					 DMA2_Stream1
/* 宏定义: CH2_DMA_Stream_IRQn - CH2_DMA_Stream_IRQn 的宏定义。 */
	#define CH2_DMA_Stream_IRQn				 DMA2_Stream1_IRQn
/* 宏定义: CH2_DMA_Stream_IRQHandle - CH2_DMA_Stream_IRQHandle 的宏定义。 */
	#define CH2_DMA_Stream_IRQHandle         DMA2_Stream1_IRQHandler
/* 宏定义: CH2_DMA_REQUEST_ADC - CH2_DMA_REQUEST_ADC 的宏定义。 */
	#define CH2_DMA_REQUEST_ADC				 DMA_REQUEST_ADC2
#else
/* 宏定义: ADCH2 - ADCH2 的宏定义。 */
	#define ADCH2                            ADC3
/* 宏定义: ADCH2_CLK_ENABLE - ADCH2_CLK_ENABLE 的宏定义。 */
	#define ADCH2_CLK_ENABLE()               __HAL_RCC_ADC3_CLK_ENABLE()

/* 宏定义: ADCH2_FORCE_RESET - ADCH2_FORCE_RESET 的宏定义。 */
	#define ADCH2_FORCE_RESET()              __HAL_RCC_ADC3_FORCE_RESET()
/* 宏定义: ADCH2_RELEASE_RESET - ADCH2_RELEASE_RESET 的宏定义。 */
	#define ADCH2_RELEASE_RESET()            __HAL_RCC_ADC3_RELEASE_RESET()

/* 宏定义: ADCH2_CHANNEL_GPIO_CLK_ENABLE - ADCH2_CHANNEL_GPIO_CLK_ENABLE 的宏定义。 */
	#define ADCH2_CHANNEL_GPIO_CLK_ENABLE()   __HAL_RCC_GPIOC_CLK_ENABLE()
/* 宏定义: ADCH2_CHANNEL_PIN - ADCH2_CHANNEL_PIN 的宏定义。 */
	#define ADCH2_CHANNEL_PIN                GPIO_PIN_3
/* 宏定义: ADCH2_CHANNEL_GPIO_PORT - ADCH2_CHANNEL_GPIO_PORT 的宏定义。 */
	#define ADCH2_CHANNEL_GPIO_PORT          GPIOC

/* 宏定义: ADCH2_CHANNEL - ADCH2_CHANNEL 的宏定义。 */
	#define ADCH2_CHANNEL                    ADC_CHANNEL_13


/* 宏定义: CH2_DMA_CLK_ENABLE - CH2_DMA_CLK_ENABLE 的宏定义。 */
	#define CH2_DMA_CLK_ENABLE()           __HAL_RCC_DMA2_CLK_ENABLE()
/* 宏定义: CH2_DMA_Stream - CH2_DMA_Stream 的宏定义。 */
	#define CH2_DMA_Stream					 DMA2_Stream1
/* 宏定义: CH2_DMA_Stream_IRQn - CH2_DMA_Stream_IRQn 的宏定义。 */
	#define CH2_DMA_Stream_IRQn				 DMA2_Stream1_IRQn
/* 宏定义: CH2_DMA_Stream_IRQHandle - CH2_DMA_Stream_IRQHandle 的宏定义。 */
	#define CH2_DMA_Stream_IRQHandle         DMA2_Stream1_IRQHandler
/* 宏定义: CH2_DMA_REQUEST_ADC - CH2_DMA_REQUEST_ADC 的宏定义。 */
	#define CH2_DMA_REQUEST_ADC				 DMA_REQUEST_ADC3
#endif

/* Definition of ADCH1 conversions data table size */
#define ADC_BUFFER_SIZE   ((uint32_t)  1024)   /* Size of array aADCH1ConvertedData[], Aligned on cache line size */

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* ADC handle declaration */
ADC_HandleTypeDef             AdcHandle1 = {0};
ADC_HandleTypeDef             AdcHandle2 = {0};

/* ADC channel configuration structure declaration */
ADC_ChannelConfTypeDef        sConfig1 = {0};
ADC_ChannelConfTypeDef        sConfig2 = {0};

/* Variable containing ADC conversions data */
ALIGN_32BYTES (uint16_t   aADCH1ConvertedData[ADC_BUFFER_SIZE]);

ALIGN_32BYTES (uint16_t   aADCH2ConvertedData[ADC_BUFFER_SIZE]);

/*
*********************************************************************************************************
*   函 数 名: TIM3_Config
*   功能说明: TIM3_Config 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void TIM3_Config(uint32_t _freq);

/*
*********************************************************************************************************
*	�� �� ��: DSO_InitHard
*	����˵��: ���ÿ�����ͨ����Ϻ������GPIO, ����ADC
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void DSO_InitHard(void)
{
	DSO_ConfigCtrlGPIO();
}

/*
*********************************************************************************************************
*	�� �� ��: DSO_ConfigCtrlGPIO
*	����˵��: ���ÿ�����ͨ����Ϻ������GPIO
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void DSO_ConfigCtrlGPIO(void)
{
	/* ���ÿ����������ϵ�GPIO */
	GPIO_InitTypeDef gpio_init;

	/* ��GPIOʱ�� */
	AC1_CLK_ENABLE();
	AC2_CLK_ENABLE();
	G1A_CLK_ENABLE();
	G1B_CLK_ENABLE();
	G2A_CLK_ENABLE();
	G2B_CLK_ENABLE();
	
	gpio_init.Mode = GPIO_MODE_OUTPUT_PP;		/* ����������� */
	gpio_init.Pull = GPIO_NOPULL;				/* ���������費ʹ�� */
	gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;  	/* GPIO�ٶȵȼ� */	
	
	gpio_init.Pin = AC1_PIN;	
	HAL_GPIO_Init(AC1_GPIO, &gpio_init);	
	
	gpio_init.Pin = AC2_PIN;	
	HAL_GPIO_Init(AC2_GPIO, &gpio_init);	

	gpio_init.Pin = G1A_PIN;	
	HAL_GPIO_Init(G1A_GPIO, &gpio_init);	
	
	gpio_init.Pin = G1B_PIN;	
	HAL_GPIO_Init(G1B_GPIO, &gpio_init);	
	
	gpio_init.Pin = G2A_PIN;	
	HAL_GPIO_Init(G2A_GPIO, &gpio_init);	
	
	gpio_init.Pin = G2B_PIN;	
	HAL_GPIO_Init(G2B_GPIO, &gpio_init);	

	DSO_SetDC(1, 0);	/* CH1ѡ��AC��� */
	DSO_SetDC(2, 0);	/* CH1ѡ��AC��� */
	DSO_SetGain(1, 0);	/* CH1ѡ��С���� ˥��1/5, ��2������1��ʾ��˥��1;1 */
	DSO_SetGain(2, 0);	/* CH2ѡ��С���� ˥��1/5, ��2������1��ʾ��˥��1;1 */		
}

/*
*********************************************************************************************************
*	�� �� ��: DSO_SetDC
*	����˵��: ����AC DC���ģʽ
*	��    ��: _ch : ͨ��1��2
*			  _mode : 0��1.  1��ʾDC��� 0��ʾAC���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void DSO_SetDC(uint8_t _ch, uint8_t _mode)
{	
	if (_ch == 1)
	{
		if (_mode == 1)
		{
			AC1_1();
		}
		else
		{
			AC1_0();
		}
	}
	else
	{
		if (_mode == 1)
		{
			AC2_1();
		}
		else
		{
			AC2_0();
		}
	}	
}

/*
*********************************************************************************************************
*	�� �� ��: SetGainHigh
*	����˵��: ��������ģʽ
*	��    ��: _ch : ͨ��1��2
*			  _gain : 0��1.  1��ʾ1:1�� 0��ʾ˥��1/5
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void DSO_SetGain(uint8_t _ch, uint8_t _gain)
{	
	if (_ch == 1)
	{
		if (_gain == 0)
		{
			G1A_0();
			G1B_0();
		}
		else if (_gain == 1)
		{
			G1A_1();
			G1B_0();
		}		
		else if (_gain == 2)
		{
			G1A_0();
			G1B_1();
		}
		else if (_gain == 3)
		{
			G1A_1();
			G1B_1();
		}			
	}
	else
	{
		if (_gain == 0)
		{
			G2A_0();
			G2B_0();
		}
		else if (_gain == 1)
		{
			G2A_1();
			G2B_0();
		}		
		else if (_gain == 2)
		{
			G2A_0();
			G2B_1();
		}
		else if (_gain == 3)
		{
			G2A_1();
			G2B_1();
		}	
	}
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_StartAdcCH1
*	����˵��: ����CH1ͨ����GPIO, ADC, DMA
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_StartAdcCH1(void)
{
	/* ### - 1 - Initialize ADC peripheral #################################### */
	AdcHandle1.Instance          = ADCH1;
	if (HAL_ADC_DeInit(&AdcHandle1) != HAL_OK)
	{
		/* ADC de-initialization Error */
		Error_Handler(__FILE__, __LINE__);
	}

	AdcHandle1.Init.ClockPrescaler           = ADC_CLOCK_SYNC_PCLK_DIV4;        /* Synchronous clock mode, input ADC clock divided by 4*/
	AdcHandle1.Init.Resolution               = ADC_RESOLUTION_12B;              /* 16-bit resolution for converted data */
	AdcHandle1.Init.ScanConvMode             = DISABLE;                         /* Sequencer disabled (ADC conversion on only 1 channel: channel set on rank 1) */
	AdcHandle1.Init.EOCSelection             = ADC_EOC_SINGLE_CONV;             /* EOC flag picked-up to indicate conversion end */
	AdcHandle1.Init.LowPowerAutoWait         = DISABLE;                         /* Auto-delayed conversion feature disabled */
	AdcHandle1.Init.ContinuousConvMode       = DISABLE;                          /* Continuous mode enabled (automatic conversion restart after each conversion) */
	AdcHandle1.Init.NbrOfConversion          = 1;                               /* Parameter discarded because sequencer is disabled */
	AdcHandle1.Init.DiscontinuousConvMode    = DISABLE;                         /* Parameter discarded because sequencer is disabled */
	AdcHandle1.Init.NbrOfDiscConversion      = 1;                               /* Parameter discarded because sequencer is disabled */
	AdcHandle1.Init.ExternalTrigConv         = ADC_EXTERNALTRIG_T3_TRGO;              /* Software start to trig the 1st conversion manually, without external event */
	AdcHandle1.Init.ExternalTrigConvEdge     = ADC_EXTERNALTRIGCONVEDGE_RISING;   /* Parameter discarded because software trigger chosen */
	AdcHandle1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR; /* ADC DMA circular requested */
	AdcHandle1.Init.Overrun                  = ADC_OVR_DATA_OVERWRITTEN;        /* DR register is overwritten with the last conversion result in case of overrun */
	AdcHandle1.Init.OversamplingMode         = DISABLE;                         /* No oversampling */
	AdcHandle1.Init.BoostMode                = ENABLE;                          /* Enable Boost mode as ADC clock frequency is bigger than 20 MHz */
	/* Initialize ADC peripheral according to the passed parameters */
	if (HAL_ADC_Init(&AdcHandle1) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}


	/* ### - 2 - Start calibration ############################################ */
	if (HAL_ADCEx_Calibration_Start(&AdcHandle1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}

	/* ### - 3 - Channel configuration ######################################## */
	sConfig1.Channel      = ADCH1_CHANNEL;                /* Sampled channel number */
	sConfig1.Rank         = ADC_REGULAR_RANK_1;          /* Rank of sampled channel number ADCH1_CHANNEL */
	sConfig1.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;   /* Sampling time (number of clock cycles unit) */
	sConfig1.SingleDiff   = ADC_SINGLE_ENDED;            /* Single-ended input channel */
	sConfig1.OffsetNumber = ADC_OFFSET_NONE;             /* No offset subtraction */ 
	sConfig1.Offset = 0;                                 /* Parameter discarded because offset correction is disabled */
	if (HAL_ADC_ConfigChannel(&AdcHandle1, &sConfig1) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}

	/* ### - 4 - Start conversion in DMA mode ################################# */
	if (HAL_ADC_Start_DMA(&AdcHandle1,
						(uint32_t *)aADCH1ConvertedData,
						ADC_BUFFER_SIZE
					   ) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_StartAdcCH2
*	����˵��: ����CH2ͨ����GPIO, ADC, DMA
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_StartAdcCH2(void)
{
	/* ### - 1 - Initialize ADC peripheral #################################### */
	AdcHandle2.Instance          = ADCH2;
	if (HAL_ADC_DeInit(&AdcHandle2) != HAL_OK)
	{
		/* ADC de-initialization Error */
		Error_Handler(__FILE__, __LINE__);
	}

	AdcHandle2.Init.ClockPrescaler           = ADC_CLOCK_SYNC_PCLK_DIV4;        /* Synchronous clock mode, input ADC clock divided by 4*/
	AdcHandle2.Init.Resolution               = ADC_RESOLUTION_12B;              /* 16-bit resolution for converted data */
	AdcHandle2.Init.ScanConvMode             = DISABLE;                         /* Sequencer disabled (ADC conversion on only 1 channel: channel set on rank 1) */
	AdcHandle2.Init.EOCSelection             = ADC_EOC_SINGLE_CONV;             /* EOC flag picked-up to indicate conversion end */
	AdcHandle2.Init.LowPowerAutoWait         = DISABLE;                         /* Auto-delayed conversion feature disabled */
	AdcHandle2.Init.ContinuousConvMode       = DISABLE;                          /* Continuous mode enabled (automatic conversion restart after each conversion) */
	AdcHandle2.Init.NbrOfConversion          = 1;                               /* Parameter discarded because sequencer is disabled */
	AdcHandle2.Init.DiscontinuousConvMode    = DISABLE;                         /* Parameter discarded because sequencer is disabled */
	AdcHandle2.Init.NbrOfDiscConversion      = 1;                               /* Parameter discarded because sequencer is disabled */
	AdcHandle2.Init.ExternalTrigConv         = ADC_EXTERNALTRIG_T3_TRGO;              /* Software start to trig the 1st conversion manually, without external event */
	AdcHandle2.Init.ExternalTrigConvEdge     = ADC_EXTERNALTRIGCONVEDGE_RISING;   /* Parameter discarded because software trigger chosen */
	AdcHandle2.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR; /* ADC DMA circular requested */
	AdcHandle2.Init.Overrun                  = ADC_OVR_DATA_OVERWRITTEN;        /* DR register is overwritten with the last conversion result in case of overrun */
	AdcHandle2.Init.OversamplingMode         = DISABLE;                         /* No oversampling */
	AdcHandle2.Init.BoostMode                = ENABLE;                          /* Enable Boost mode as ADC clock frequency is bigger than 20 MHz */
	/* Initialize ADC peripheral according to the passed parameters */
	if (HAL_ADC_Init(&AdcHandle2) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}


	/* ### - 2 - Start calibration ############################################ */
	if (HAL_ADCEx_Calibration_Start(&AdcHandle2, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}

	/* ### - 3 - Channel configuration ######################################## */
	sConfig2.Channel      = ADCH2_CHANNEL;                /* Sampled channel number */
	sConfig2.Rank         = ADC_REGULAR_RANK_1;          /* Rank of sampled channel number ADCH1_CHANNEL */
	sConfig2.SamplingTime = ADC_SAMPLETIME_1CYCLE_5  ;   /* Sampling time (number of clock cycles unit) */
	sConfig2.SingleDiff   = ADC_SINGLE_ENDED;            /* Single-ended input channel */
	sConfig2.OffsetNumber = ADC_OFFSET_NONE;             /* No offset subtraction */ 
	sConfig2.Offset = 0;                                 /* Parameter discarded because offset correction is disabled */
	if (HAL_ADC_ConfigChannel(&AdcHandle2, &sConfig2) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}

	/* ### - 4 - Start conversion in DMA mode ################################# */
	if (HAL_ADC_Start_DMA(&AdcHandle2,
						(uint32_t *)aADCH2ConvertedData,
						ADC_BUFFER_SIZE
					   ) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_StopAdcCH1
*	����˵��: ��λCH1ͨ�����ã�ֹͣADC�ɼ�.
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_StopAdcCH1(void)
{
	/* ### - 1 - Initialize ADC peripheral #################################### */
	AdcHandle1.Instance          = ADCH2;
	if (HAL_ADC_DeInit(&AdcHandle2) != HAL_OK)
	{
		/* ADC de-initialization Error */
		Error_Handler(__FILE__, __LINE__);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_StopAdcCH2
*	����˵��: ��λCH2ͨ�����ã�ֹͣADC�ɼ�.
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_StopAdcCH2(void)
{
	/* ### - 1 - Initialize ADC peripheral #################################### */
	AdcHandle2.Instance          = ADCH2;
	if (HAL_ADC_DeInit(&AdcHandle2) != HAL_OK)
	{
		/* ADC de-initialization Error */
		Error_Handler(__FILE__, __LINE__);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TIM3_Config
*	����˵��: ����TIM3��ΪADC����Դ
*	��    ��: _freq : ����Ƶ�ʣ���λHz
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void TIM3_Config(uint32_t _freq)
{
	static TIM_HandleTypeDef  htim = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};

	htim.Instance = TIM3;
	if (_freq == 0)
	{
		__HAL_RCC_TIM3_CLK_DISABLE();
		HAL_TIM_Base_Stop(&htim);
	}
	else
	{
		__HAL_RCC_TIM3_CLK_ENABLE();
		
		/*##-1- Configure the TIM peripheral #######################################*/
		/* Time base configuration */

		htim.Init.Period            = (SystemCoreClock / 2) / _freq - 1;
		htim.Init.Prescaler         = 0;
		htim.Init.ClockDivision     = 0;
		htim.Init.CounterMode       = TIM_COUNTERMODE_UP;
		htim.Init.RepetitionCounter = 0;
		HAL_TIM_Base_Init(&htim);

		/* TIM6 TRGO selection */
		sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
		sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

		HAL_TIMEx_MasterConfigSynchronization(&htim, &sMasterConfig);

		/*##-2- Enable TIM peripheral counter ######################################*/
		HAL_TIM_Base_Start(&htim);		
	}
}

/*
*********************************************************************************************************
*	�� �� ��: DSO_StartADC
*	����˵��: ��Ҫ���ģ����GPIO�����á�ADC�����á���ʱ���������Լ�DMA�����á�
*			- ����ADC�����ڶ���ģʽ
*			- ������ͬ���ⲿ������ ADC_EXTERNALTRIG_T4_CC4
*			- TIM1��CC1Ƶ�ʵľ����˲���Ƶ��
*
*	��    ��: _uiBufAddr1 : DMAĿ���ַ��CH1���ݴ�ŵĻ�������ַ
*			  _uiBufAddr2 : DMAĿ���ַ��CH2���ݴ�ŵĻ�������ַ
*			  _uiCount : ���������������� (�����ֽ���)����ͨ��ͬ���ɼ�.
*			  _uiFreq : ����Ƶ�ʣ� Hz
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void DSO_StartADC(uint16_t **_AdcBuf1, uint16_t **_AdcBuf2, uint32_t _uiFreq)
{			
	*(uint32_t *)_AdcBuf1 = (uint32_t)aADCH1ConvertedData;
	*(uint32_t *)_AdcBuf2 = (uint32_t)aADCH2ConvertedData;
	
	bsp_StartAdcCH1();
	bsp_StartAdcCH2();
	
	/* ���ò���������ʱ����ʹ��TIM1 CC1 */
	DSO_SetSampRate(_uiFreq);	/* �޸Ĳ���Ƶ�ʣ�������TIM */
}

/*
*********************************************************************************************************
*	�� �� ��: DSO_StopADC
*	����˵��: �ر�ADC�������е����衣ADC, DMA, TIM
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void DSO_StopADC(void)
{	
	TIM3_Config(0);
	
	bsp_StopAdcCH1();
	bsp_StopAdcCH1();
}

/*
*********************************************************************************************************
*	�� �� ��: PauseADC
*	����˵��: ��ͣADC������׼���������ݡ���֤�´�DMA����������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void DSO_PauseADC(void)
{
	TIM3_Config(0);
}

/**
* @brief  ADC MSP Init
* @param  hadc : ADC handle
* @retval None
*/
void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
	if (hadc->Instance == ADCH1)
	{	
	  GPIO_InitTypeDef          GPIO_InitStruct;
	  static DMA_HandleTypeDef  DmaHandle1 = {0};
	  
	  /*##-1- Enable peripherals and GPIO Clocks #################################*/
	  /* Enable GPIO clock ****************************************/
	  ADCH1_CHANNEL_GPIO_CLK_ENABLE();
	  /* ADC Periph clock enable */
	  ADCH1_CLK_ENABLE();
	  /* ADC Periph interface clock configuration */
	  __HAL_RCC_ADC_CONFIG(RCC_ADCCLKSOURCE_CLKP);
	  /* Enable DMA clock */
	  CH1_DMA_CLK_ENABLE();
	  
	  /*##- 2- Configure peripheral GPIO #########################################*/
	  /* ADC Channel GPIO pin configuration */
	  GPIO_InitStruct.Pin = ADCH1_CHANNEL_PIN;
	  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	  GPIO_InitStruct.Pull = GPIO_NOPULL;
	  HAL_GPIO_Init(ADCH1_CHANNEL_GPIO_PORT, &GPIO_InitStruct);
	  /*##- 3- Configure DMA #####################################################*/ 

	  /*********************** Configure DMA parameters ***************************/
	  DmaHandle1.Instance                 = CH1_DMA_Stream;
	  DmaHandle1.Init.Request             = CH1_DMA_REQUEST_ADC;
	  DmaHandle1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
	  DmaHandle1.Init.PeriphInc           = DMA_PINC_DISABLE;
	  DmaHandle1.Init.MemInc              = DMA_MINC_ENABLE;
	  DmaHandle1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
	  DmaHandle1.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
	  DmaHandle1.Init.Mode                = DMA_NORMAL;	// DMA_CIRCULAR;
	  DmaHandle1.Init.Priority            = DMA_PRIORITY_MEDIUM;
	  /* Deinitialize  & Initialize the DMA for new transfer */
	  HAL_DMA_DeInit(&DmaHandle1);
	  HAL_DMA_Init(&DmaHandle1);
	  
	  /* Associate the DMA handle */
	  __HAL_LINKDMA(hadc, DMA_Handle, DmaHandle1);

	  /* NVIC configuration for DMA Input data interrupt */
	  HAL_NVIC_SetPriority(CH1_DMA_Stream_IRQn, 1, 0);
	  HAL_NVIC_EnableIRQ(CH1_DMA_Stream_IRQn);  
  }
 else if (hadc->Instance == ADCH2)
 {
	  GPIO_InitTypeDef          GPIO_InitStruct;
	  static DMA_HandleTypeDef  DmaHandle2 = {0};
	  
	  /*##-1- Enable peripherals and GPIO Clocks #################################*/
	  /* Enable GPIO clock ****************************************/
	  ADCH2_CHANNEL_GPIO_CLK_ENABLE();
	  /* ADC Periph clock enable */
	  ADCH2_CLK_ENABLE();
	  /* ADC Periph interface clock configuration */
	  __HAL_RCC_ADC_CONFIG(RCC_ADCCLKSOURCE_CLKP);
	  /* Enable DMA clock */
	  CH2_DMA_CLK_ENABLE();
	  
	  /*##- 2- Configure peripheral GPIO #########################################*/
	  /* ADC Channel GPIO pin configuration */
	  GPIO_InitStruct.Pin = ADCH2_CHANNEL_PIN;
	  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	  GPIO_InitStruct.Pull = GPIO_NOPULL;
	  HAL_GPIO_Init(ADCH2_CHANNEL_GPIO_PORT, &GPIO_InitStruct);
	  /*##- 3- Configure DMA #####################################################*/ 

	  /*********************** Configure DMA parameters ***************************/
	  DmaHandle2.Instance                 = CH2_DMA_Stream;
	  DmaHandle2.Init.Request             = CH2_DMA_REQUEST_ADC;
	  DmaHandle2.Init.Direction           = DMA_PERIPH_TO_MEMORY;
	  DmaHandle2.Init.PeriphInc           = DMA_PINC_DISABLE;
	  DmaHandle2.Init.MemInc              = DMA_MINC_ENABLE;
	  DmaHandle2.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
	  DmaHandle2.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
	  DmaHandle2.Init.Mode                = DMA_NORMAL; // DMA_CIRCULAR;
	  DmaHandle2.Init.Priority            = DMA_PRIORITY_MEDIUM;
	  /* Deinitialize  & Initialize the DMA for new transfer */
	  HAL_DMA_DeInit(&DmaHandle2);
	  HAL_DMA_Init(&DmaHandle2);
	  
	  /* Associate the DMA handle */
	  __HAL_LINKDMA(hadc, DMA_Handle, DmaHandle2);

	  /* NVIC configuration for DMA Input data interrupt */
	  HAL_NVIC_SetPriority(CH2_DMA_Stream_IRQn, 1, 0);
	  HAL_NVIC_EnableIRQ(CH2_DMA_Stream_IRQn);  	 
 }		
}

/**
  * @brief ADC MSP De-Initialization
  *        This function frees the hardware resources used in this example:
  *          - Disable the Peripheral's clock
  *          - Revert GPIO to their default state
  * @param hadc: ADC handle pointer
  * @retval None
  */
void HAL_ADC_MspDeInit(ADC_HandleTypeDef *hadc)
{

	if (hadc->Instance == ADCH1)
	{
	  /*##-1- Reset peripherals ##################################################*/
	  ADCH1_FORCE_RESET();
	  ADCH1_RELEASE_RESET();
	  /* ADC Periph clock disable
	   (automatically reset all ADC instances of the ADC common group) */
	  __HAL_RCC_ADC12_CLK_DISABLE();

	  /*##-2- Disable peripherals and GPIO Clocks ################################*/
	  /* De-initialize the ADC Channel GPIO pin */
	  HAL_GPIO_DeInit(ADCH1_CHANNEL_GPIO_PORT, ADCH1_CHANNEL_PIN);
	}
	else if (hadc->Instance == ADCH2)
	{
	  /*##-1- Reset peripherals ##################################################*/
//	  ADCH2_FORCE_RESET();
//	  ADCH2_RELEASE_RESET();
	  /* ADC Periph clock disable
	   (automatically reset all ADC instances of the ADC common group) */
//	  __HAL_RCC_ADC12_CLK_DISABLE();

	  /*##-2- Disable peripherals and GPIO Clocks ################################*/
	  /* De-initialize the ADC Channel GPIO pin */
	  HAL_GPIO_DeInit(ADCH2_CHANNEL_GPIO_PORT, ADCH2_CHANNEL_PIN);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: DSO_SetSampRate
*	����˵��: �޸Ĳ���Ƶ��. ʹ��TIM4_CC4����
*	��    ��: freq : ����Ƶ�� ��λHz
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void DSO_SetSampRate(uint32_t _ulFreq)
{
	TIM3_Config(_ulFreq);
}

/*
*********************************************************************************************************
*	�� �� ��: CH1_DMA_STREAM_IRQHANDLER
*	����˵��: CH1 DAM�жϷ������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void CH1_DMA_Stream_IRQHandle(void)
{
	HAL_DMA_IRQHandler(AdcHandle1.DMA_Handle);
}

/*
*********************************************************************************************************
*	�� �� ��: CH1_DMA_STREAM_IRQHANDLER
*	����˵��: CH2 DAM�жϷ������,
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void CH2_DMA_Stream_IRQHandle(void)
{
	HAL_DMA_IRQHandler(AdcHandle2.DMA_Handle);
}

/*
*********************************************************************************************************
*	�� �� ��: HAL_ADC_ConvHalfCpltCallback
*	����˵��: DAM�жϷ������ص�����������cache����ˢ�µ��ڴ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{
	if (hadc->Instance == ADCH1)
	{
		/* Invalidate Data Cache to get the updated content of the SRAM on the first half of the ADC converted data buffer: 32 bytes */ 
		SCB_InvalidateDCache_by_Addr((uint32_t *)aADCH1ConvertedData,  ADC_BUFFER_SIZE);
	}
	else if (hadc->Instance == ADCH2)
	{
		/* Invalidate Data Cache to get the updated content of the SRAM on the first half of the ADC converted data buffer: 32 bytes */ 
		SCB_InvalidateDCache_by_Addr((uint32_t *)aADCH2ConvertedData,  ADC_BUFFER_SIZE);
	}	
}

/*
*********************************************************************************************************
*	�� �� ��: HAL_ADC_ConvCpltCallback
*	����˵��: DAM�жϷ������ص�����������cache����ˢ�µ��ڴ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
	if (hadc->Instance == ADCH1)
	{
		/* Invalidate Data Cache to get the updated content of the SRAM on the first half of the ADC converted data buffer: 32 bytes */ 
		SCB_InvalidateDCache_by_Addr((uint32_t *)aADCH1ConvertedData + ADC_BUFFER_SIZE,  ADC_BUFFER_SIZE);
	}
	else if (hadc->Instance == ADCH2)
	{
		/* Invalidate Data Cache to get the updated content of the SRAM on the first half of the ADC converted data buffer: 32 bytes */ 
		SCB_InvalidateDCache_by_Addr((uint32_t *)(aADCH2ConvertedData + ADC_BUFFER_SIZE),  ADC_BUFFER_SIZE);
	}	
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
