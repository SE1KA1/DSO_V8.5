/**
 * @file stm32h7xx_hal_timebase_tim.c
 * @brief HAL 时基：SysTick 替代 TIM 实现，配合 FreeRTOS。
 */

/*
*********************************************************************************************************
*
*	ģ������ : HAL��ʱ��
*	�ļ����� : stm32h7xx_hal_timbase_tim.c
*	��    �� : V1.0
*	˵    �� : ����ΪHAL���ṩʱ���׼
*	�޸ļ�¼ :
*		�汾��  ����         ����       ˵��
*		V1.0    2018-07-29  Eric2013   ��ʽ����
*
*	Copyright (C), 2018-2030, ���������� www.armfly.com
*
*********************************************************************************************************
*/
#include "includes.h"


/* ����TIM7��������RTX5�ں�ʱ�ӷ��� */
#if 0

TIM_HandleTypeDef   TimHandle = {0};
	
/*
*********************************************************************************************************
*	�� �� ��: System_Init
*	����˵��: ΪHAL������1ms��ʱ���׼���˺����ᱻHAL_Init��HAL_RCC_ClockConfig����
*	��    �Σ�TickPriority  ��ʱ�����ȼ�
*	�� �� ֵ: ��
*********************************************************************************************************
*/
HAL_StatusTypeDef HAL_InitTick (uint32_t TickPriority)
{

	RCC_ClkInitTypeDef    clkconfig;
	uint32_t              uwTimclock, uwAPB1Prescaler = 0U;
	uint32_t              uwPrescalerValue = 0U;
	uint32_t              pFLatency;

	
	/* ��λ��ʱ��7 */
	TimHandle.Instance = TIM7;
	if(HAL_TIM_Base_DeInit(&TimHandle) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}
	
	/* ����TIM7���ж����ȼ���ʹ�� */
	HAL_NVIC_SetPriority(TIM7_IRQn, TickPriority ,0U);
	HAL_NVIC_EnableIRQ(TIM7_IRQn);

	/* ʹ��TIM7ʱ�� */
	__HAL_RCC_TIM7_CLK_ENABLE();

	/*-----------------------------------------------------------------------
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
	/* ��ȡʱ������ */
	HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);

	/* ��ȡAPB1ʱ�� */
	uwAPB1Prescaler = clkconfig.APB1CLKDivider;

	/* ���TIM7ʱ�� */
	if (uwAPB1Prescaler == RCC_HCLK_DIV1) 
	{
		uwTimclock = HAL_RCC_GetPCLK1Freq();
	}
	else
	{
		uwTimclock = 2*HAL_RCC_GetPCLK1Freq();
	}

	/* TIM7��Ƶ��1MHz */
	uwPrescalerValue = (uint32_t) ((uwTimclock / 1000000U) - 1U);

	/* TIM7CLK = uwTimclock / (Period + 1) / (Prescaler + 1) = 1KHz */
	TimHandle.Init.Period = (1000000U / 1000U) - 1U;
	TimHandle.Init.Prescaler = uwPrescalerValue;
	TimHandle.Init.ClockDivision = 0;
	TimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
	if(HAL_TIM_Base_Init(&TimHandle) == HAL_OK)
	{
		/* ����TIM7 */
		return HAL_TIM_Base_Start_IT(&TimHandle);
	}

	/* ���ش��� */
	return HAL_ERROR;
}

/*
*********************************************************************************************************
*	�� �� ��: HAL_SuspendTick
*	����˵��: �ر�TIM7
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_SuspendTick(void)
{
	__HAL_TIM_DISABLE_IT(&TimHandle, TIM_IT_UPDATE);
}

/*
*********************************************************************************************************
*	�� �� ��: HAL_ResumeTick
*	����˵��: ʹ��TIM7
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_ResumeTick(void)
{
	__HAL_TIM_ENABLE_IT(&TimHandle, TIM_IT_UPDATE);
}

/*
*********************************************************************************************************
*	�� �� ��: TIM7_IRQHandler
*	����˵��: TIM7��ʱ���жϷ������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TIM7_IRQHandler(void)
{
	if((TIM7->SR & TIM_FLAG_UPDATE) != RESET)
	{
		 TIM7->SR = ~ TIM_FLAG_UPDATE;
		 HAL_IncTick();
	}
}

#else
/*
*********************************************************************************************************
*	�� �� ��: HAL_Delay
*	����˵��: �ض�������ӳٺ������滻HAL�еĺ�������ΪHAL�е�ȱʡ����������Systick�жϣ������USB��SD��
*             �ж������ӳٺ��������������Ҳ����ͨ������HAL_NVIC_SetPriority����Systick�ж�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_Delay(uint32_t Delay)
{
	bsp_DelayMS(Delay);
}

/*
*********************************************************************************************************
*   函 数 名: HAL_InitTick
*   功能说明: HAL_InitTick 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
HAL_StatusTypeDef HAL_InitTick (uint32_t TickPriority)
{
	return HAL_OK;
}

/*
*********************************************************************************************************
*   函 数 名: HAL_GetTick
*   功能说明: HAL_GetTick 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t HAL_GetTick (void) 
{
	static uint32_t ticks = 0U;
	uint32_t i;

	if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
	{
		if(__get_IPSR() != 0U)
		{
			return ((uint32_t)xTaskGetTickCountFromISR());
		}
		else
		{
			return ((uint32_t)xTaskGetTickCount());
		}
	}

	/* ���RTX5��û�����У��������淽ʽ */
	for (i = (SystemCoreClock >> 14U); i > 0U; i--) 
	{
		__NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
		__NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
	}
	
	return ++ticks;
}
#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
