/**
 * @file bsp_fmc_ad7606.c
 * @brief 安富莱 BSP 通用驱动（AD7606 FMC ADC）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : AD7606���ݲɼ�ģ��
*	�ļ����� : bsp_ad7606.c
*	��    �� : V1.0
*	˵    �� : AD7606����STM32��FMC�����ϡ�
*
*			������ʹ���� TIM3 ��ΪӲ����ʱ������ʱ����ADCת��
*
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2015-10-11 armfly  ��ʽ����
*
*	Copyright (C), 2015-2020, ���������� www.armfly.com
*
*********************************************************************************************************
*/

/*
	STM32-V7������ + AD7606ģ�飬 ���Ʋɼ���IO:
	
	PC6/TIM3_CH1/TIM8_CH1     ----> AD7606_CONVST  (������ͷ����),  ���PWM��������ΪADC�����ź�
	PE5/DCMI_D6/AD7606_BUSY   <---- AD7606_BUSY    , CPU��BUSY�жϷ�������ж�ȡ�ɼ����
*/

#include "bsp.h"

/* CONVST ����ADCת����GPIO = PC6 */
#define CONVST_RCC_GPIO_CLK_ENABLE	__HAL_RCC_GPIOC_CLK_ENABLE
/* 宏定义: CONVST_GPIO - CONVST_GPIO 的宏定义。 */
#define CONVST_GPIO		GPIOC
/* 宏定义: CONVST_PIN - CONVST_PIN 的宏定义。 */
#define CONVST_PIN		GPIO_PIN_6
/* 宏定义: CONVST_TIMX - CONVST_TIMX 的宏定义。 */
#define CONVST_TIMX		TIM8
/* 宏定义: CONVST_TIMCH - CONVST_TIMCH 的宏定义。 */
#define CONVST_TIMCH	1

/* BUSY ת������ź� = PE5 */
#define BUSY_RCC_GPIO_CLK_ENABLE	__HAL_RCC_GPIOE_CLK_ENABLE
/* 宏定义: BUSY_GPIO - BUSY_GPIO 的宏定义。 */
#define BUSY_GPIO		GPIOE
/* 宏定义: BUSY_PIN - BUSY_PIN 的宏定义。 */
#define BUSY_PIN		GPIO_PIN_5
/* 宏定义: BUSY_IRQn - BUSY_IRQn 的宏定义。 */
#define BUSY_IRQn		EXTI9_5_IRQn
/* 宏定义: BUSY_IRQHandler - BUSY_IRQHandler 的宏定义。 */
#define BUSY_IRQHandler	EXTI9_5_IRQHandler

/* ���ù�������IO, ����չ��74HC574�� */
#define OS0_1()		HC574_SetPin(AD7606_OS0, 1)
/* 宏定义: OS0_0 - OS0_0 的宏定义。 */
#define OS0_0()		HC574_SetPin(AD7606_OS0, 0)
/* 宏定义: OS1_1 - OS1_1 的宏定义。 */
#define OS1_1()		HC574_SetPin(AD7606_OS1, 1)
/* 宏定义: OS1_0 - OS1_0 的宏定义。 */
#define OS1_0()		HC574_SetPin(AD7606_OS1, 0)
/* 宏定义: OS2_1 - OS2_1 的宏定义。 */
#define OS2_1()		HC574_SetPin(AD7606_OS2, 1)
/* 宏定义: OS2_0 - OS2_0 的宏定义。 */
#define OS2_0()		HC574_SetPin(AD7606_OS2, 0)

/* ����ADת����GPIO : PC6 */
#define CONVST_1()	CONVST_GPIO->BSRRL = CONVST_PIN
/* 宏定义: CONVST_0 - CONVST_0 的宏定义。 */
#define CONVST_0()	CONVST_GPIO->BSRRH = CONVST_PIN

/* �����������̵�GPIO, ����չ��74HC574�� */
#define RANGE_1()	HC574_SetPin(AD7606_RANGE, 1)
/* 宏定义: RANGE_0 - RANGE_0 的宏定义。 */
#define RANGE_0()	HC574_SetPin(AD7606_RANGE, 0)

/* AD7606��λ����, ����չ��74HC574�� */
#define RESET_1()	HC574_SetPin(AD7606_RESET, 1)
/* 宏定义: RESET_0 - RESET_0 的宏定义。 */
#define RESET_0()	HC574_SetPin(AD7606_RESET, 0)

/* AD7606 FSMC���ߵ�ַ��ֻ�ܶ�������д */
#define AD7606_RESULT()	*(__IO uint16_t *)0x60003000

AD7606_VAR_T g_tAD7606;		/* ����1��ȫ�ֱ���������һЩ���� */
AD7606_FIFO_T g_tAdcFifo;	/* ����FIFO�ṹ����� */

static void AD7606_CtrlLinesConfig(void);
/*
*********************************************************************************************************
*   函 数 名: AD7606_FSMCConfig
*   功能说明: AD7606_FSMCConfig 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void AD7606_FSMCConfig(void);

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitAD7606
*	����˵��: ���������ⲿSRAM��GPIO��FSMC
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitAD7606(void)
{
	AD7606_CtrlLinesConfig();
	AD7606_FSMCConfig();

	AD7606_SetOS(AD_OS_NO);		/* �޹����� */
	AD7606_SetInputRange(0);	/* 0��ʾ��������Ϊ����5V, 1��ʾ����10V */

	AD7606_Reset();

	CONVST_1();					/* ����ת����GPIOƽʱ����Ϊ�� */
}

/*
*********************************************************************************************************
*	�� �� ��: AD7606_CtrlLinesConfig
*	����˵��: ����GPIO���ߣ�FMC�ܽ�����Ϊ���ù���
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
/*
	������STM32-V7��������߷�����
	PD0/FMC_D2
	PD1/FMC_D3
	PD4/FMC_NOE		---- �������źţ�OE = Output Enable �� N ��ʾ����Ч
	PD5/FMC_NWE		-XX- д�����źţ�AD7606 ֻ�ж�����д�ź�
	PD8/FMC_D13
	PD9/FMC_D14
	PD10/FMC_D15
	PD14/FMC_D0
	PD15/FMC_D1

	PE7/FMC_D4
	PE8/FMC_D5
	PE9/FMC_D6
	PE10/FMC_D7
	PE11/FMC_D8
	PE12/FMC_D9
	PE13/FMC_D10
	PE14/FMC_D11
	PE15/FMC_D12
	
	PG0/FMC_A10		--- ����ƬѡFMC_NE2һ������
	PG1/FMC_A11		--- ����ƬѡFMC_NE2һ������
	PG9/FMC_NE2		--- ��Ƭѡ��TFT, OLED �� AD7606��	
*/

/* 
	����AD7606����������IO��������չ��74HC574��
	X13 - AD7606_OS0
	X14 - AD7606_OS1
	X15 - AD7606_OS2
	X24 - AD7606_RESET
	X25 - AD7606_RAGE	
	
	PE5 - AD7606_BUSY
*/
static void AD7606_CtrlLinesConfig(void)
{
	/* bsp_fm_io ������fmc�� �˴������ظ����� 
		bsp_InitExtIO();
	*/
	
	/*

	PD0/FMC_D2
	PD1/FMC_D3
	PD4/FMC_NOE		---- �������źţ�OE = Output Enable �� N ��ʾ����Ч
	PD5/FMC_NWE		-XX- д�����źţ�AD7606 ֻ�ж�����д�ź�
	PD8/FMC_D13
	PD9/FMC_D14
	PD10/FMC_D15
	PD14/FMC_D0
	PD15/FMC_D1

	PE7/FMC_D4
	PE8/FMC_D5
	PE9/FMC_D6
	PE10/FMC_D7
	PE11/FMC_D8
	PE12/FMC_D9
	PE13/FMC_D10
	PE14/FMC_D11
	PE15/FMC_D12
	
	PG0/FMC_A10		--- ����ƬѡFMC_NE2һ������
	PG1/FMC_A11		--- ����ƬѡFMC_NE2һ������
	PG9/FMC_NE2		--- ��Ƭѡ��OLED, 74HC574, DM9000, AD7606��	
	
//	PF0/FMC_A0      ---- RS
	*/	

	GPIO_InitTypeDef gpio_init_structure;

	/* ʹ�� GPIOʱ�� */
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOF_CLK_ENABLE();
	__HAL_RCC_GPIOG_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOI_CLK_ENABLE();

	/* ʹ��FMCʱ�� */
	__HAL_RCC_FMC_CLK_ENABLE();

	/* ���� GPIOD ��ص�IOΪ����������� */
	gpio_init_structure.Mode = GPIO_MODE_AF_PP;
	gpio_init_structure.Pull = GPIO_PULLUP;
	gpio_init_structure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	gpio_init_structure.Alternate = GPIO_AF12_FMC;
	
	/* ����GPIOD */
	gpio_init_structure.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 |
	                            GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_14 |
	                            GPIO_PIN_15;
	HAL_GPIO_Init(GPIOD, &gpio_init_structure);

	/* ����GPIOE */
	gpio_init_structure.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
	                            GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 |
	                            GPIO_PIN_15;
	HAL_GPIO_Init(GPIOE, &gpio_init_structure);

	/* ����GPIOG */
	gpio_init_structure.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_9;
	HAL_GPIO_Init(GPIOG, &gpio_init_structure);
	
	/* ����GPIOH */
	gpio_init_structure.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12
						| GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
	HAL_GPIO_Init(GPIOH, &gpio_init_structure);

	/* ����GPIOI */
	gpio_init_structure.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_6
						| GPIO_PIN_7 | GPIO_PIN_9 | GPIO_PIN_10;
	HAL_GPIO_Init(GPIOI, &gpio_init_structure);
}

/*
*********************************************************************************************************
*	�� �� ��: AD7606_FSMCConfig
*	����˵��: ����FSMC���ڷ���ʱ��
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void AD7606_FSMCConfig(void)
{
	/* bsp_fm_io ������fmc�� �˴������ظ����� 
		bsp_InitExtIO();
	*/
	/*
		AD7606�����Ҫ��(3.3Vʱ)��RD���źŵ͵�ƽ����������21ns���ߵ�ƽ������̿���15ns��

		������������ ������������Ϊ�˺�ͬBANK��LCD������ͬ��ѡ��3-0-6-1-0-0
		3-0-5-1-0-0  : RD�߳���75ns�� �͵�ƽ����50ns.  1us���ڿɶ�ȡ8·�������ݵ��ڴ档
		1-0-1-1-0-0  : RD��75ns���͵�ƽִ��12ns���ң��½��ز��Ҳ12ns.  ���ݶ�ȡ��ȷ��
	*/
	SRAM_HandleTypeDef hsram = {0};
	FMC_NORSRAM_TimingTypeDef SRAM_Timing = {0};
		
	/*
		AD7606�����Ҫ��(3.3Vʱ)��RD���źŵ͵�ƽ����������21ns���ߵ�ƽ������̿���15ns��

		������������ ������������Ϊ�˺�ͬBANK��LCD������ͬ��ѡ��3-0-6-1-0-0
		3-0-5-1-0-0  : RD�߳���75ns�� �͵�ƽ����50ns.  1us���ڿɶ�ȡ8·�������ݵ��ڴ档
		1-0-1-1-0-0  : RD��75ns���͵�ƽִ��12ns���ң��½��ز��Ҳ12ns.  ���ݶ�ȡ��ȷ��
	*/
	hsram.Instance  = FMC_NORSRAM_DEVICE;
	hsram.Extended  = FMC_NORSRAM_EXTENDED_DEVICE;

	/* SRAM ����ʱ������ */  
	SRAM_Timing.AddressSetupTime       = 4;
	SRAM_Timing.AddressHoldTime        = 1;
	SRAM_Timing.DataSetupTime          = 5;
	SRAM_Timing.BusTurnAroundDuration  = 3;
	SRAM_Timing.CLKDivision            = 2;
	SRAM_Timing.DataLatency            = 2;
	SRAM_Timing.AccessMode             = FMC_ACCESS_MODE_A;

	hsram.Init.NSBank             = FMC_NORSRAM_BANK1;
	hsram.Init.DataAddressMux     = FMC_DATA_ADDRESS_MUX_DISABLE;
	hsram.Init.MemoryType         = FMC_MEMORY_TYPE_SRAM;
	hsram.Init.MemoryDataWidth    = FMC_NORSRAM_MEM_BUS_WIDTH_32;	/* 32λ���߿��� */
	hsram.Init.BurstAccessMode    = FMC_BURST_ACCESS_MODE_DISABLE;
	hsram.Init.WaitSignalPolarity = FMC_WAIT_SIGNAL_POLARITY_LOW;
	hsram.Init.WaitSignalActive   = FMC_WAIT_TIMING_BEFORE_WS;
	hsram.Init.WriteOperation     = FMC_WRITE_OPERATION_ENABLE;
	hsram.Init.WaitSignal         = FMC_WAIT_SIGNAL_DISABLE;
	hsram.Init.ExtendedMode       = FMC_EXTENDED_MODE_DISABLE;
	hsram.Init.AsynchronousWait   = FMC_ASYNCHRONOUS_WAIT_DISABLE;
	hsram.Init.WriteBurst         = FMC_WRITE_BURST_DISABLE;
	hsram.Init.ContinuousClock    = FMC_CONTINUOUS_CLOCK_SYNC_ONLY;

	/* ��ʼ��SRAM������ */
	if (HAL_SRAM_Init(&hsram, &SRAM_Timing, &SRAM_Timing) != HAL_OK)
	{
		/* ��ʼ������ */
		Error_Handler(__FILE__, __LINE__);
	}	
}

/*
*********************************************************************************************************
*	�� �� ��: AD7606_SetOS
*	����˵��: ����AD7606�����˲�����Ҳ�����ù��������ʡ�
*			 ͨ������ AD7606_OS0��OS1��OS2���ߵĵ�ƽ���״̬�������������ʡ�
*			 ����ADת��֮��AD7606�ڲ��Զ�ʵ��ʣ�������Ĳɼ���Ȼ����ƽ��ֵ�����
*
*			 ����������Խ�ߣ�ת��ʱ��Խ����
*			 �޹�����ʱ��ADת��ʱ�� 4us;
*				2��������ʱ = 8.7us;
*				4��������ʱ = 16us
*			 	64��������ʱ = 286us
*
*	��    ��: _ucOS : ����������, 0 - 6
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void AD7606_SetOS(uint8_t _ucOS)
{
	g_tAD7606.ucOS = _ucOS;
	switch (_ucOS)
	{
		case AD_OS_X2:
			OS2_0();
			OS1_0();
			OS0_1();
			break;

		case AD_OS_X4:
			OS2_0();
			OS1_1();
			OS0_0();
			break;

		case AD_OS_X8:
			OS2_0();
			OS1_1();
			OS0_1();
			break;

		case AD_OS_X16:
			OS2_1();
			OS1_0();
			OS0_0();
			break;

		case AD_OS_X32:
			OS2_1();
			OS1_0();
			OS0_1();
			break;

		case AD_OS_X64:
			OS2_1();
			OS1_1();
			OS0_0();
			break;

		case AD_OS_NO:
		default:
			g_tAD7606.ucOS = AD_OS_NO;
			OS2_0();
			OS1_0();
			OS0_0();
			break;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: AD7606_SetInputRange
*	����˵��: ����AD7606ģ���ź��������̡�
*	��    ��: _ucRange : 0 ��ʾ����5V   1��ʾ����10V
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void AD7606_SetInputRange(uint8_t _ucRange)
{
	if (_ucRange == 0)
	{
		g_tAD7606.ucRange = 0;
		RANGE_0();	/* ����Ϊ����5V */
	}
	else
	{
		g_tAD7606.ucRange = 1;
		RANGE_1();	/* ����Ϊ����10V */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: AD7606_Reset
*	����˵��: Ӳ����λAD7606����λ֮��ָ�����������״̬��
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void AD7606_Reset(void)
{
	RESET_0();	/* �˳���λ״̬ */

	RESET_1();	/* ���븴λ״̬ */
	RESET_1();	/* �������ӳ١� RESET��λ�ߵ�ƽ���������С50ns�� */
	RESET_1();
	RESET_1();

	RESET_0();	/* �˳���λ״̬ */
}

/*
*********************************************************************************************************
*	�� �� ��: AD7606_StartConvst
*	����˵��: ����1��ADCת��
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void AD7606_StartConvst(void)
{
	/* page 7��  CONVST �ߵ�ƽ������Ⱥ͵͵�ƽ���������� 25ns */
	/* CONVSTƽʱΪ�� */
	CONVST_0();
	CONVST_0();
	CONVST_0();

	CONVST_1();
}

/*
*********************************************************************************************************
*	�� �� ��: AD7606_ReadNowAdc
*	����˵��: ��ȡ8·�������������洢��ȫ�ֱ��� g_tAD7606
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void AD7606_ReadNowAdc(void)
{
	g_tAD7606.sNowAdc[0] = AD7606_RESULT();	/* ����1·���� */
	g_tAD7606.sNowAdc[1] = AD7606_RESULT();	/* ����2·���� */
	g_tAD7606.sNowAdc[2] = AD7606_RESULT();	/* ����3·���� */
	g_tAD7606.sNowAdc[3] = AD7606_RESULT();	/* ����4·���� */
	g_tAD7606.sNowAdc[4] = AD7606_RESULT();	/* ����5·���� */
	g_tAD7606.sNowAdc[5] = AD7606_RESULT();	/* ����6·���� */
	g_tAD7606.sNowAdc[6] = AD7606_RESULT();	/* ����7·���� */
	g_tAD7606.sNowAdc[7] = AD7606_RESULT();	/* ����8·���� */
}

/*
*********************************************************************************************************
*		����ĺ������ڶ�ʱ�ɼ�ģʽ�� TIM5Ӳ����ʱ�ж��ж�ȡADC���������ȫ��FIFO
*
*
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*	�� �� ��: AD7606_HasNewData
*	����˵��: �ж�FIFO���Ƿ���������
*	��    ��:  _usReadAdc : ���ADC����ı���ָ��
*	�� �� ֵ: 1 ��ʾ�У�0��ʾ��������
*********************************************************************************************************
*/
uint8_t AD7606_HasNewData(void)
{
	if (g_tAdcFifo.usCount > 0)
	{
		return 1;
	}
	return 0;
}

/*
*********************************************************************************************************
*	�� �� ��: AD7606_FifoFull
*	����˵��: �ж�FIFO�Ƿ���
*	��    ��:  _usReadAdc : ���ADC����ı���ָ��
*	�� �� ֵ: 1 ��ʾ����0��ʾδ��
*********************************************************************************************************
*/
uint8_t AD7606_FifoFull(void)
{
	return g_tAdcFifo.ucFull;
}

/*
*********************************************************************************************************
*	�� �� ��: AD7606_ReadFifo
*	����˵��: ��FIFO�ж�ȡһ��ADCֵ
*	��    ��:  _usReadAdc : ���ADC����ı���ָ��
*	�� �� ֵ: 1 ��ʾOK��0��ʾ��������
*********************************************************************************************************
*/
uint8_t AD7606_ReadFifo(uint16_t *_usReadAdc)
{
	if (AD7606_HasNewData())
	{
		*_usReadAdc = g_tAdcFifo.sBuf[g_tAdcFifo.usRead];
		if (++g_tAdcFifo.usRead >= ADC_FIFO_SIZE)
		{
			g_tAdcFifo.usRead = 0;
		}

		DISABLE_INT();
		if (g_tAdcFifo.usCount > 0)
		{
			g_tAdcFifo.usCount--;
		}
		ENABLE_INT();
		return 1;
	}
	return 0;
}

/*
*********************************************************************************************************
*	�� �� ��: AD7606_StartRecord
*	����˵��: ��ʼ�ɼ�
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void AD7606_StartRecord(uint32_t _ulFreq)
{
	AD7606_StopRecord();

	AD7606_Reset();					/* ��λӲ�� */
	AD7606_StartConvst();			/* ���������������1������ȫ0������ */

	g_tAdcFifo.usRead = 0;			/* �����ڿ���TIM2֮ǰ��0 */
	g_tAdcFifo.usWrite = 0;
	g_tAdcFifo.usCount = 0;
	g_tAdcFifo.ucFull = 0;

	AD7606_EnterAutoMode(_ulFreq);
}


/*
*********************************************************************************************************
*	�� �� ��: AD7606_EnterAutoMode
*	����˵��: ����Ӳ���������Զ��ɼ�ģʽ������洢��FIFO��������
*	��    ��:  _ulFreq : ����Ƶ�ʣ���λHz��	1k��2k��5k��10k��20K��50k��100k��200k
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void AD7606_EnterAutoMode(uint32_t _ulFreq)
{
	/* ����PC6ΪTIM1_CH1���ܣ����ռ�ձ�50%�ķ��� */
	bsp_SetTIMOutPWM(CONVST_GPIO, CONVST_PIN, CONVST_TIMX,  CONVST_TIMCH, _ulFreq, 5000);
	
	/* ����PE5, BUSY ��Ϊ�ж�����ڣ��½��ش��� */
	{
		GPIO_InitTypeDef   GPIO_InitStructure;
		
		CONVST_RCC_GPIO_CLK_ENABLE();	/* ��GPIOʱ�� */
		__HAL_RCC_SYSCFG_CLK_ENABLE();

		GPIO_InitStructure.Mode = GPIO_MODE_IT_FALLING;	/* �ж��½��ش��� */
		GPIO_InitStructure.Pull = GPIO_NOPULL;
		GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;		
		GPIO_InitStructure.Pin = BUSY_PIN;
		HAL_GPIO_Init(BUSY_GPIO, &GPIO_InitStructure);	

		HAL_NVIC_SetPriority(BUSY_IRQn, 2, 0);
		HAL_NVIC_EnableIRQ(BUSY_IRQn);		
	}		
}

/*
*********************************************************************************************************
*	�� �� ��: AD7606_StopRecord
*	����˵��: ֹͣ�ɼ���ʱ��
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void AD7606_StopRecord(void)
{
	/* ����PC6 ����͵�ƽ���ر�TIM */
	bsp_SetTIMOutPWM(CONVST_GPIO, CONVST_PIN, CONVST_TIMX,  CONVST_TIMCH, 1000, 10000);
	CONVST_1();					/* ����ת����GPIOƽʱ����Ϊ�� */	

	/* ���� BUSY ��Ϊ�ж�����ڣ��½��ش��� */
	{
		GPIO_InitTypeDef   GPIO_InitStructure;
		
		CONVST_RCC_GPIO_CLK_ENABLE();		/* ��GPIOʱ�� */

		/* Configure PC.13 pin as input floating */
		GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
		GPIO_InitStructure.Pull = GPIO_NOPULL;
		GPIO_InitStructure.Pin = CONVST_PIN;
		HAL_GPIO_Init(CONVST_GPIO, &GPIO_InitStructure);	

		HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);		
	}
}

/*
*********************************************************************************************************
*	�� �� ��: AD7606_ISR
*	����˵��: ��ʱ�ɼ��жϷ������
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void AD7606_ISR(void)
{
	uint8_t i;

	AD7606_ReadNowAdc();

	for (i = 0; i < 8; i++)
	{
		g_tAdcFifo.sBuf[g_tAdcFifo.usWrite] = g_tAD7606.sNowAdc[i];
		if (++g_tAdcFifo.usWrite >= ADC_FIFO_SIZE)
		{
			g_tAdcFifo.usWrite = 0;
		}
		if (g_tAdcFifo.usCount < ADC_FIFO_SIZE)
		{
			g_tAdcFifo.usCount++;
		}
		else
		{
			g_tAdcFifo.ucFull = 1;		/* FIFO ������������������������ */
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: EXTI9_5_IRQHandler
*	����˵��: �ⲿ�жϷ��������ڡ�PE5 / AD7606_BUSY �½����жϴ���
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
#ifndef EXTI9_5_ISR_MOVE_OUT		/* bsp.h �ж�����У���ʾ�������Ƶ� stam32h7xx_it.c�� �����ظ����� */
void BUSY_IRQHandler(void)
{
	HAL_GPIO_EXTI_IRQHandler(BUSY_PIN);
}

/*
*********************************************************************************************************
*	�� �� ��: EXTI9_5_IRQHandler
*	����˵��: �ⲿ�жϷ��������ڡ�PI6 / AD7606_BUSY �½����жϴ���
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == BUSY_PIN)
	{
		AD7606_ISR();
	}
}

#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
