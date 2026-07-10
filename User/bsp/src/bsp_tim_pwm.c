/**
 * @file bsp_tim_pwm.c
 * @brief 安富莱 BSP 通用驱动（TIM PWM）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : TIM������ʱ�жϺ�PWM����ģ��
*	�ļ����� : bsp_tim_pwm.c
*	��    �� : V1.6
*	˵    �� : ����STM32H7�ڲ�TIM���PWM�źţ� ��ʵ�ֻ����Ķ�ʱ�ж�
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2013-08-16 armfly  ��ʽ����
*		V1.1	2014-06-15 armfly  ���� bsp_SetTIMOutPWM����ռ�ձ�=0��100%ʱ���رն�ʱ����GPIO����Ϊ���
*		V1.2	2015-05-08 armfly  ���TIM8�������PWM�����⡣
*		V1.3	2015-07-23 armfly  ��ʼ����ʱ������������ TIM_TimeBaseInitStruct.TIM_RepetitionCounter = 0x0000;		
*								   TIM1 �� TIM8 �������á�����������Ŀ��Ʋ�������
*		V1.4	2015-07-30 armfly  ���ӷ����������PWM���� bsp_SetTIMOutPWM_N();
*		V1.5	2016-02-01 armfly  ȥ�� TIM_OC1PreloadConfig(TIMx, TIM_OCPreload_Enable);
*		V1.6	2016-02-27 armfly  ���TIM14�޷��жϵ�BUG, TIM8_TRG_COM_TIM14_IRQn
*
*	Copyright (C), 2018-2030, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"


/*
    ע�⣬STM32H7��TIM1 �C TIM17��û��TIM9��TIM10��TIM11������14����ʱ����
	���������GPIO��TIMͨ��:
    TIM1_CH1,  PA8   PE9   PK1
    TIM1_CH2,  PA9   PE11
    TIM1_CH3,  PA10  PE13  PJ9
    TIM1_CH4,  PA11  PE14  PJ11

    TIM2_CH1,  PA0   PA5   PA15
    TIM2_CH2,  PA1   PB3  
    TIM2_CH3,  PA2
    TIM2_CH4,  PA3   PB11

    TIM3_CH1,  PA6   PC6  PB4
    TIM3_CH2,  PA7   PC7  PB5   
    TIM3_CH3,  PB0   PC8  
    TIM3_CH4,  PB1   PC9  

    TIM4_CH1,  PB6   PD12
    TIM4_CH2,  PB7   PD13
    TIM4_CH3,  PB8   PD14
    TIM4_CH4,  PB9   PD15

    TIM5_CH1,  PA0   PH10
    TIM5_CH2,  PA1   PH11
    TIM5_CH3,  PA2   PH12
    TIM5_CH4,  PA3   PI0

    TIM8_CH1,  PC6   PI5  PJ8
    TIM8_CH2,  PC7   PI6  PJ10
    TIM8_CH3,  PC8   PI7  PK0
    TIM8_CH4,  PC9   

    TIM12_CH1,  PB14  PH6
    TIM12_CH2,  PB15  PH9

    TIM13_CH1,  PF8

    TIM14_CH1,  PF9

    TIM15_CH1,  PE5 
    TIM15_CH2,  PE6

    TIM16_CH1,  PB8   PF6
    TIM16_CH2,  PF7

    TIM17_CH1,  PB9
*/

/*
*********************************************************************************************************
*	�� �� ��: bsp_RCC_GPIO_Enable
*	����˵��: ʹ��GPIOʱ��
*	��    ��: GPIOx GPIOA - GPIOK
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_RCC_GPIO_Enable(GPIO_TypeDef* GPIOx)
{
	if (GPIOx == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
	else if (GPIOx == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
	else if (GPIOx == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
	else if (GPIOx == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();
	else if (GPIOx == GPIOE) __HAL_RCC_GPIOE_CLK_ENABLE();
	else if (GPIOx == GPIOF) __HAL_RCC_GPIOF_CLK_ENABLE();
	else if (GPIOx == GPIOG) __HAL_RCC_GPIOG_CLK_ENABLE();
	else if (GPIOx == GPIOH) __HAL_RCC_GPIOH_CLK_ENABLE();
	else if (GPIOx == GPIOI) __HAL_RCC_GPIOI_CLK_ENABLE();
	else if (GPIOx == GPIOJ) __HAL_RCC_GPIOJ_CLK_ENABLE();
	else if (GPIOx == GPIOK) __HAL_RCC_GPIOK_CLK_ENABLE();
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_RCC_TIM_Enable
*	����˵��: ʹ��TIM RCC ʱ��
*	��    ��: TIMx TIM1 - TIM17
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_RCC_TIM_Enable(TIM_TypeDef* TIMx)
{
	if (TIMx == TIM1) __HAL_RCC_TIM1_CLK_ENABLE();
	else if (TIMx == TIM2) __HAL_RCC_TIM2_CLK_ENABLE();
	else if (TIMx == TIM3) __HAL_RCC_TIM3_CLK_ENABLE();
	else if (TIMx == TIM4) __HAL_RCC_TIM4_CLK_ENABLE();
	else if (TIMx == TIM5) __HAL_RCC_TIM5_CLK_ENABLE();
	else if (TIMx == TIM6) __HAL_RCC_TIM6_CLK_ENABLE();
	else if (TIMx == TIM7) __HAL_RCC_TIM7_CLK_ENABLE();
	else if (TIMx == TIM8) __HAL_RCC_TIM8_CLK_ENABLE();
//	else if (TIMx == TIM9) __HAL_RCC_TIM9_CLK_ENABLE();
//	else if (TIMx == TIM10) __HAL_RCC_TIM10_CLK_ENABLE();
//	else if (TIMx == TIM11) __HAL_RCC_TIM11_CLK_ENABLE();
	else if (TIMx == TIM12) __HAL_RCC_TIM12_CLK_ENABLE();
	else if (TIMx == TIM13) __HAL_RCC_TIM13_CLK_ENABLE();
	else if (TIMx == TIM14) __HAL_RCC_TIM14_CLK_ENABLE();
	else if (TIMx == TIM15) __HAL_RCC_TIM15_CLK_ENABLE();
	else if (TIMx == TIM16) __HAL_RCC_TIM16_CLK_ENABLE();
	else if (TIMx == TIM17) __HAL_RCC_TIM17_CLK_ENABLE();	
	else
	{
		Error_Handler(__FILE__, __LINE__);
	}	
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_RCC_TIM_Disable
*	����˵��: �ر�TIM RCC ʱ��
*	��    ��: TIMx TIM1 - TIM17
*	�� �� ֵ: TIM����ʱ����
*********************************************************************************************************
*/
void bsp_RCC_TIM_Disable(TIM_TypeDef* TIMx)
{
	/*
        APB1 ��ʱ���� TIM2, TIM3 ,TIM4, TIM5, TIM6, TIM7, TIM12, TIM13, TIM14
        APB2 ��ʱ���� TIM1, TIM8 , TIM15, TIM16��TIM17
	*/
	if (TIMx == TIM1) __HAL_RCC_TIM3_CLK_DISABLE();
	else if (TIMx == TIM2) __HAL_RCC_TIM2_CLK_DISABLE();
	else if (TIMx == TIM3) __HAL_RCC_TIM3_CLK_DISABLE();
	else if (TIMx == TIM4) __HAL_RCC_TIM4_CLK_DISABLE();
	else if (TIMx == TIM5) __HAL_RCC_TIM5_CLK_DISABLE();
	else if (TIMx == TIM6) __HAL_RCC_TIM6_CLK_DISABLE();
	else if (TIMx == TIM7) __HAL_RCC_TIM7_CLK_DISABLE();
	else if (TIMx == TIM8) __HAL_RCC_TIM8_CLK_DISABLE();
//	else if (TIMx == TIM9) __HAL_RCC_TIM9_CLK_DISABLE();
//	else if (TIMx == TIM10) __HAL_RCC_TIM10_CLK_DISABLE();
//	else if (TIMx == TIM11) __HAL_RCC_TIM11_CLK_DISABLE();
	else if (TIMx == TIM12) __HAL_RCC_TIM12_CLK_DISABLE();
	else if (TIMx == TIM13) __HAL_RCC_TIM13_CLK_DISABLE();
	else if (TIMx == TIM14) __HAL_RCC_TIM14_CLK_DISABLE();
	else if (TIMx == TIM15) __HAL_RCC_TIM15_CLK_DISABLE();
	else if (TIMx == TIM16) __HAL_RCC_TIM16_CLK_DISABLE();
	else if (TIMx == TIM17) __HAL_RCC_TIM17_CLK_DISABLE();
	else
	{
		Error_Handler(__FILE__, __LINE__);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_GetAFofTIM
*	����˵��: ����TIM �õ�AF�Ĵ�������
*	��    ��: TIMx TIM1 - TIM17
*	�� �� ֵ: AF�Ĵ�������
*********************************************************************************************************
*/
uint8_t bsp_GetAFofTIM(TIM_TypeDef* TIMx)
{
	uint8_t ret = 0;

	if (TIMx == TIM1) ret = GPIO_AF1_TIM1;
	else if (TIMx == TIM2) ret = GPIO_AF1_TIM2;
	else if (TIMx == TIM3) ret = GPIO_AF2_TIM3;
	else if (TIMx == TIM4) ret = GPIO_AF2_TIM4;
	else if (TIMx == TIM5) ret = GPIO_AF2_TIM5;
	else if (TIMx == TIM8) ret = GPIO_AF3_TIM8;
	else if (TIMx == TIM12) ret = GPIO_AF2_TIM12;
	else if (TIMx == TIM13) ret = GPIO_AF9_TIM13;
	else if (TIMx == TIM14) ret = GPIO_AF9_TIM14;
	else if (TIMx == TIM15) ret = GPIO_AF4_TIM15;
	else if (TIMx == TIM16) ret = GPIO_AF1_TIM16;
	else if (TIMx == TIM17) ret = GPIO_AF1_TIM17;
	else
	{
		Error_Handler(__FILE__, __LINE__);
	}
	
	return ret;
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_ConfigTimGpio
*	����˵��: ����GPIO��TIMʱ�ӣ� GPIO���ӵ�TIM���ͨ��
*	��    ��: GPIOx : GPIOA - GPIOK
*			  GPIO_PinX : GPIO_PIN_0 - GPIO__PIN_15
*			  TIMx : TIM1 - TIM17
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_ConfigTimGpio(GPIO_TypeDef* GPIOx, uint16_t GPIO_PinX, TIM_TypeDef* TIMx)
{
	GPIO_InitTypeDef   GPIO_InitStruct;

	/* ʹ��GPIOʱ�� */
	bsp_RCC_GPIO_Enable(GPIOx);

  	/* ʹ��TIMʱ�� */
	bsp_RCC_TIM_Enable(TIMx);

	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = bsp_GetAFofTIM(TIMx);
	GPIO_InitStruct.Pin = GPIO_PinX;
	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_ConfigGpioOut
*	����˵��: ����GPIOΪ�����������Ҫ����PWM�����ռ�ձ�Ϊ0��100�������
*	��    ��: GPIOx : GPIOA - GPIOK
*			  GPIO_PinX : GPIO_PIN_0 - GPIO__PIN_15
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_ConfigGpioOut(GPIO_TypeDef* GPIOx, uint16_t GPIO_PinX)
{
	GPIO_InitTypeDef   GPIO_InitStruct;

	bsp_RCC_GPIO_Enable(GPIOx);		/* ʹ��GPIOʱ�� */

	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Pin = GPIO_PinX;
	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_SetTIMOutPWM
*	����˵��: �������������PWM�źŵ�Ƶ�ʺ�ռ�ձ�.  ��Ƶ��Ϊ0������ռ��Ϊ0ʱ���رն�ʱ����GPIO���0��
*			  ��Ƶ��Ϊ0��ռ�ձ�Ϊ100%ʱ��GPIO���1.
*	��    ��: GPIOx : GPIOA - GPIOK
*			  GPIO_Pin : GPIO_PIN_0 - GPIO__PIN_15
*			  TIMx : TIM1 - TIM17
*             _ucChannel��ʹ�õĶ�ʱ��ͨ������Χ1 - 4
*			  _ulFreq : PWM�ź�Ƶ�ʣ���λHz (ʵ�ʲ��ԣ��������100MHz����0 ��ʾ��ֹ���
*			  _ulDutyCycle : PWM�ź�ռ�ձȣ���λ: ���֮һ����5000����ʾ50.00%��ռ�ձ�
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_SetTIMOutPWM(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, TIM_TypeDef* TIMx, uint8_t _ucChannel,
	 uint32_t _ulFreq, uint32_t _ulDutyCycle)
{
	TIM_HandleTypeDef  TimHandle = {0};
	TIM_OC_InitTypeDef sConfig = {0};	
	uint16_t usPeriod;
	uint16_t usPrescaler;
	uint32_t pulse;
	uint32_t uiTIMxCLK;
	const uint16_t TimChannel[6+1] = {0, TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_4, TIM_CHANNEL_5, TIM_CHANNEL_6};

	if (_ucChannel > 6)
	{
		Error_Handler(__FILE__, __LINE__);
	}
	
	if (_ulDutyCycle == 0)
	{		
		//bsp_RCC_TIM_Disable(TIMx);		/* �ر�TIMʱ��, ����Ӱ������ͨ�� */		
		bsp_ConfigGpioOut(GPIOx, GPIO_Pin);	/* ����GPIOΪ������� */			
		GPIOx->BSRRH = GPIO_Pin;		/* PWM = 0 */		
		return;
	}
	else if (_ulDutyCycle == 10000)
	{
		//bsp_RCC_TIM_Disable(TIMx);		/* �ر�TIMʱ��, ����Ӱ������ͨ�� */
		bsp_ConfigGpioOut(GPIOx, GPIO_Pin);	/* ����GPIOΪ������� */		
		GPIOx->BSRRL = GPIO_Pin;		/* PWM = 1*/	
		return;
	}
	
	/* ������PWM��� */
	
	bsp_ConfigTimGpio(GPIOx, GPIO_Pin, TIMx);	/* ʹ��GPIO��TIMʱ�ӣ�������TIMͨ����GPIO */
	
    /*-----------------------------------------------------------------------
		bsp.c �ļ��� void SystemClock_Config(void) ������ʱ�ӵ���������: 

        System Clock source       = PLL (HSE)
        SYSCLK(Hz)                = 400000000 (CPU Clock)
        HCLK(Hz)                  = 200000000 (AXI and AHBs Clock)
        AHB Prescaler             = 2
        D1 APB3 Prescaler         = 2 (APB3 Clock  100MHz)
        D2 APB1 Prescaler         = 2 (APB1 Clock  100MHz)
        D2 APB2 Prescaler         = 2 (APB2 Clock  100MHz)
        D3 APB4 Prescaler         = 2 (APB4 Clock  100MHz)

        ��ΪAPB1 prescaler != 1, ���� APB1�ϵ�TIMxCLK = APB1 x 2 = 200MHz;
        ��ΪAPB2 prescaler != 1, ���� APB2�ϵ�TIMxCLK = APB2 x 2 = 200MHz;
        APB4�����TIMxCLKû�з�Ƶ�����Ծ���100MHz;

        APB1 ��ʱ���� TIM2, TIM3 ,TIM4, TIM5, TIM6, TIM7, TIM12, TIM13, TIM14��LPTIM1
        APB2 ��ʱ���� TIM1, TIM8 , TIM15, TIM16��TIM17

        APB4 ��ʱ���� LPTIM2��LPTIM3��LPTIM4��LPTIM5

	----------------------------------------------------------------------- */
	if ((TIMx == TIM1) || (TIMx == TIM8) || (TIMx == TIM15) || (TIMx == TIM16) || (TIMx == TIM17))
	{
		/* APB2 ��ʱ��ʱ�� = 200M */
		uiTIMxCLK = SystemCoreClock / 2;
	}
	else	
	{
		/* APB1 ��ʱ�� = 200M */
		uiTIMxCLK = SystemCoreClock / 2;
	}

	if (_ulFreq < 100)
	{
		usPrescaler = 10000 - 1;					/* ��Ƶ�� = 10000 */
		usPeriod =  (uiTIMxCLK / 10000) / _ulFreq  - 1;		/* �Զ���װ��ֵ */
	}
	else if (_ulFreq < 3000)
	{
		usPrescaler = 100 - 1;					/* ��Ƶ�� = 100 */
		usPeriod =  (uiTIMxCLK / 100) / _ulFreq  - 1;		/* �Զ���װ��ֵ */
	}
	else	/* ����4K��Ƶ�ʣ������Ƶ */
	{
		usPrescaler = 0;					/* ��Ƶ�� = 1 */
		usPeriod = uiTIMxCLK / _ulFreq - 1;	/* �Զ���װ��ֵ */
	}
	pulse = (_ulDutyCycle * usPeriod) / 10000;

	
	HAL_TIM_PWM_DeInit(&TimHandle);
    
	/*  PWMƵ�� = TIMxCLK / usPrescaler + 1��/usPeriod + 1��*/
	TimHandle.Instance = TIMx;
	TimHandle.Init.Prescaler         = usPrescaler;
	TimHandle.Init.Period            = usPeriod;
	TimHandle.Init.ClockDivision     = 0;
	TimHandle.Init.CounterMode       = TIM_COUNTERMODE_UP;
	TimHandle.Init.RepetitionCounter = 0;
	TimHandle.Init.AutoReloadPreload = 0;
	if (HAL_TIM_PWM_Init(&TimHandle) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}

	/* ���ö�ʱ��PWM���ͨ�� */
	sConfig.OCMode       = TIM_OCMODE_PWM1;
	sConfig.OCPolarity   = TIM_OCPOLARITY_HIGH;
	sConfig.OCFastMode   = TIM_OCFAST_DISABLE;
	sConfig.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
	sConfig.OCNIdleState = TIM_OCNIDLESTATE_RESET;
	sConfig.OCIdleState  = TIM_OCIDLESTATE_RESET;

	/* ռ�ձ� */
	sConfig.Pulse = pulse;
	if (HAL_TIM_PWM_ConfigChannel(&TimHandle, &sConfig, TimChannel[_ucChannel]) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}
	
	/* ����PWM��� */
	if (HAL_TIM_PWM_Start(&TimHandle, TimChannel[_ucChannel]) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_SetTIMforInt
*	����˵��: ����TIM��NVIC�����ڼ򵥵Ķ�ʱ�жϣ�������ʱ�жϡ�����ע���жϷ��������Ҫ���û�Ӧ�ó���ʵ�֡�
*	��    ��: TIMx : ��ʱ��
*			  _ulFreq : ��ʱƵ�� ��Hz���� 0 ��ʾ�رա�
*			  _PreemptionPriority : ��ռ���ȼ�
*			  _SubPriority : �����ȼ�
*	�� �� ֵ: ��
*********************************************************************************************************
*/
/*	
TIM��ʱ�жϷ�����������������жϱ�־
void TIM6_DAC_IRQHandler(void)
{
	if((TIM6->SR & TIM_FLAG_UPDATE) != RESET)
	{
		TIM6->SR = ~ TIM_FLAG_UPDATE;
		//�����û�����
	}
}
*/
void bsp_SetTIMforInt(TIM_TypeDef* TIMx, uint32_t _ulFreq, uint8_t _PreemptionPriority, uint8_t _SubPriority)
{
	TIM_HandleTypeDef   TimHandle = {0};
	uint16_t usPeriod;
	uint16_t usPrescaler;
	uint32_t uiTIMxCLK;
	
	/* ʹ��TIMʱ�� */
	bsp_RCC_TIM_Enable(TIMx);
	
    /*-----------------------------------------------------------------------
		bsp.c �ļ��� void SystemClock_Config(void) ������ʱ�ӵ���������: 

        System Clock source       = PLL (HSE)
        SYSCLK(Hz)                = 400000000 (CPU Clock)
        HCLK(Hz)                  = 200000000 (AXI and AHBs Clock)
        AHB Prescaler             = 2
        D1 APB3 Prescaler         = 2 (APB3 Clock  100MHz)
        D2 APB1 Prescaler         = 2 (APB1 Clock  100MHz)
        D2 APB2 Prescaler         = 2 (APB2 Clock  100MHz)
        D3 APB4 Prescaler         = 2 (APB4 Clock  100MHz)

        ��ΪAPB1 prescaler != 1, ���� APB1�ϵ�TIMxCLK = APB1 x 2 = 200MHz;
        ��ΪAPB2 prescaler != 1, ���� APB2�ϵ�TIMxCLK = APB2 x 2 = 200MHz;
        APB4�����TIMxCLKû�з�Ƶ�����Ծ���100MHz;

        APB1 ��ʱ���� TIM2, TIM3 ,TIM4, TIM5, TIM6, TIM7, TIM12, TIM13, TIM14��LPTIM1
        APB2 ��ʱ���� TIM1, TIM8 , TIM15, TIM16��TIM17

        APB4 ��ʱ���� LPTIM2��LPTIM3��LPTIM4��LPTIM5
	----------------------------------------------------------------------- */
	if ((TIMx == TIM1) || (TIMx == TIM8) || (TIMx == TIM15) || (TIMx == TIM16) || (TIMx == TIM17))
	{
		/* APB2 ��ʱ��ʱ�� = 200M */
		uiTIMxCLK = SystemCoreClock / 2;
	}
	else	
	{
		/* APB1 ��ʱ�� = 200M */
		uiTIMxCLK = SystemCoreClock / 2;
	}

	if (_ulFreq < 100)
	{
		usPrescaler = 10000 - 1;					/* ��Ƶ�� = 10000 */
		usPeriod =  (uiTIMxCLK / 10000) / _ulFreq  - 1;		/* �Զ���װ��ֵ */
	}
	else if (_ulFreq < 3000)
	{
		usPrescaler = 100 - 1;					/* ��Ƶ�� = 100 */
		usPeriod =  (uiTIMxCLK / 100) / _ulFreq  - 1;		/* �Զ���װ��ֵ */
	}
	else	/* ����4K��Ƶ�ʣ������Ƶ */
	{
		usPrescaler = 0;					/* ��Ƶ�� = 1 */
		usPeriod = uiTIMxCLK / _ulFreq - 1;	/* �Զ���װ��ֵ */
	}

	/* 
       ��ʱ���жϸ������� = TIMxCLK / usPrescaler + 1��/usPeriod + 1��
	*/
	TimHandle.Instance = TIMx;
	TimHandle.Init.Prescaler         = usPrescaler;
	TimHandle.Init.Period            = usPeriod;	
	TimHandle.Init.ClockDivision     = 0;
	TimHandle.Init.CounterMode       = TIM_COUNTERMODE_UP;
	TimHandle.Init.RepetitionCounter = 0;
	TimHandle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_Base_Init(&TimHandle) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}

	/* ʹ�ܶ�ʱ���ж�  */
	__HAL_TIM_ENABLE_IT(&TimHandle, TIM_IT_UPDATE);
	

	/* ����TIM��ʱ�����ж� (Update) */
	{
        uint8_t irq = 0;	/* �жϺ�, ������ stm32h7xx.h */

        if (TIMx == TIM1) irq = TIM1_UP_IRQn;
        else if (TIMx == TIM2) irq = TIM2_IRQn;
        else if (TIMx == TIM3) irq = TIM3_IRQn;
        else if (TIMx == TIM4) irq = TIM4_IRQn;
        else if (TIMx == TIM5) irq = TIM5_IRQn;
        else if (TIMx == TIM6) irq = TIM6_DAC_IRQn;
        else if (TIMx == TIM7) irq = TIM7_IRQn;
        else if (TIMx == TIM8) irq = TIM8_UP_TIM13_IRQn;
        else if (TIMx == TIM12) irq = TIM8_BRK_TIM12_IRQn;
        else if (TIMx == TIM13) irq = TIM8_UP_TIM13_IRQn;
        else if (TIMx == TIM14) irq = TIM8_TRG_COM_TIM14_IRQn;
        else if (TIMx == TIM15) irq = TIM15_IRQn;
        else if (TIMx == TIM16) irq = TIM16_IRQn;
        else if (TIMx == TIM17) irq = TIM17_IRQn;
        else
        {
            Error_Handler(__FILE__, __LINE__);
        }	
        HAL_NVIC_SetPriority((IRQn_Type)irq, _PreemptionPriority, _SubPriority);
        HAL_NVIC_EnableIRQ((IRQn_Type)irq);		
	}
	
	HAL_TIM_Base_Start(&TimHandle);
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
