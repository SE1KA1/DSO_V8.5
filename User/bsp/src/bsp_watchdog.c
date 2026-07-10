/**
 * @file bsp_watchdog.c
 * @brief 安富莱 BSP 通用驱动（独立看门狗）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*	                                  
*	ģ������ : STM32�ڲ����Ź�
*	�ļ����� : bsp_watchdog.c
*	��    �� : V1.0
*	˵    �� : STM32�ڲ��������Ź�IWDG�ʹ��ڿ��Ź�WWDG��������
*	�޸ļ�¼ :
*		�汾��   ����        ����     ˵��
*		V1.0    2015-10-06  armfly   ��ʽ����
*
*	Copyright (C), 2015-2020, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/*
*********************************************************************************************************
*   函 数 名: bsp_InitIwdg
*   功能说明: bsp_InitIwdg 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void bsp_InitIwdg(uint32_t _ulIWDGTime);

/*
*********************************************************************************************************
*	�� �� ��: bsp_StartDog
*	����˵��: �������Ź��� �������Ź�����Ϊ 1000ms, ���ڿ��Ź���ʹ�ܡ�
*	��    ��: ��
*	�� �� ֵ: ��		        
*********************************************************************************************************
*/
void bsp_StartDog(void)
{
	/* �����������Ź�, ��ʱ1000ms */
	bsp_InitIwdg(1000);
	
	
	/*
		���ڿ��Ź���ʼ��
		����ˢ��ʱ���: ~780 * (0x7f-0x50) = 36.6ms < refresh window < ~780 * 64 = 49.9ms
		
		�������ü�����ֵ�� 0x7f������ֵ�� 0x50�����ǲ��ܹ��ڼ������ݼ��� 0x50 ֮ǰ��ˢ��		
	*/
	//bsp_InitWwdg(0x7f, 0x50, WWDG_Prescaler_8);
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_FeedDog
*	����˵��: ι��������Զ������Ź�
*	��    ��: ��
*	�� �� ֵ: ��		        
*********************************************************************************************************
*/
void bsp_FeedDog(void)
{
	IWDG_ReloadCounter();
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitIwdg
*	����˵��: �������Ź�ʱ�����ú���
*	��    ��: IWDGTime: 0 ---- 0x0FFF
*			  �������Ź�ʱ������,��λΪms,IWDGTime = 1000 ��Լ����һ���ʱ��
*			  LSI = 34000����
*	�� �� ֵ: ��		        
*********************************************************************************************************
*/
static void bsp_InitIwdg(uint32_t _ulIWDGTime)
{
		
	/* ���ϵͳ�Ƿ�Ӷ������Ź���λ�лָ� */
	if (RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET)
	{		
		/* �����λ��־ */
		RCC_ClearFlag();
	}
	else
	{
		/* ��־û������ */
	}
	
	/* ʹ��LSI */
	RCC_LSICmd(ENABLE);
	
	/* �ȴ�ֱ��LSI���� */
	while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
	{}
	
	/* д��0x5555��ʾ��������IWDG_PR ��IWDG_RLR�Ĵ��� */
	IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
	
	/*  LSI/32 ��Ƶ�� ֮�����ڲ��1ms */
	IWDG_SetPrescaler(IWDG_Prescaler_32);
	
	/*
		�ر�ע�⣬��������_ulIWDGTime����С��λ��ms, ����������װ�����ļ���ʱ ��Ҫ����1000
	 	Counter Reload Value = (_ulIWDGTime / 1000) /(1 / IWDG counter clock period)
	                      = (_ulIWDGTime / 1000) / (32/LSI)
	                      = (_ulIWDGTime / 1000) / (32/LsiFreq)
	                      = LsiFreq * _ulIWDGTime / 32000
	 	ʵ�ʲ���LsiFreq = 34000����������ȡ1��ʱ�� ��ž���1ms 
	*/
	IWDG_SetReload(_ulIWDGTime);
	
	/* ����IWDG���� */
	IWDG_ReloadCounter();
	
	/* ʹ�� IWDG (LSI oscillator ��Ӳ��ʹ��) */
	IWDG_Enable();		
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitWwdg
*	����˵��: ���ڿ��Ź����� 
*	��    �Σ�
*             _ucTreg       : T[6:0],������ֵ 	��Χ0x40 �� 0x7F                                               
*             _ucWreg       : W[6:0],����ֵ     ����С�� 0x80
*            WWDG_Prescaler : ���ڿ��Ź���Ƶ	PCLK1 = 42MHz
*                             WWDG_Prescaler_1: WWDG counter clock = (PCLK1/4096)/1
*							  WWDG_Prescaler_2: WWDG counter clock = (PCLK1/4096)/2
*							  WWDG_Prescaler_4: WWDG counter clock = (PCLK1/4096)/4
*							  WWDG_Prescaler_8: WWDG counter clock = (PCLK1/4096)/8 
*	�� �� ֵ: ��		        
*********************************************************************************************************
*/
void bsp_InitWwdg(uint8_t _ucTreg, uint8_t _ucWreg, uint32_t WWDG_Prescaler)
{
	NVIC_InitTypeDef NVIC_InitStructure;
	
	/* ���ϵͳ�Ƿ�Ӵ��ڿ��Ź���λ�лָ� */
	if (RCC_GetFlagStatus(RCC_FLAG_WWDGRST) != RESET)
	{ 	
		/* �����λ��־ */
		RCC_ClearFlag();
	}
	else
	{
		/* WWDGRST ��־û������ */
	}
	
	/* ʹ��WWDGʱ�� */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_WWDG, ENABLE);
	
	/* 
	   ���ڿ��Ź���Ƶ���ã�
	   ����ѡ��WWDG_Prescaler_8
	   (PCLK1 (42MHz)/4096)/8 = 1281 Hz (~780 us)  
	*/
	WWDG_SetPrescaler(WWDG_Prescaler);
	
	/* 
	 ���ô���ֵ��_ucWreg���û�������С��_ucWreg�Ҵ���0x40ʱˢ�¼���
	 ����Ҫ�������ϵͳ��λ��
    */
	WWDG_SetWindowValue(_ucWreg);
	
	/* 
	 ʹ��WWDG�����ü�����
	 ��������_ucTreg=127 8��Ƶʱ����ô���ʱ�����= ~780 us * 64 = 49.92 ms 
	 ���ڿ��Ź���ˢ��ʱ�����: ~780 * (127-80) = 36.6ms < ˢ�´��ڿ��Ź� < ~780 * 64 = 49.9ms
	*/
	WWDG_Enable(_ucTreg);
	
	/* ���EWI�жϱ�־ */
	WWDG_ClearFlag();	

	/* ʹ��EW�ж� */
	WWDG_EnableIT();

    /* ���� WWDG ��NVIC */
	NVIC_InitStructure.NVIC_IRQChannel = WWDG_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);	
}


/*
*********************************************************************************************************
*	�� �� ��: WWDG_IRQHandler
*	����˵��: ���ڿ��Ź������жϷ������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void WWDG_IRQHandler(void)
{
	/*�ر�ע�⣬���������ִ��˳���ܷ��ˣ����˾ͳ�������*/
	/* ���EWI��־ */
	if (WWDG_GetFlagStatus != RESET)
	{
		/* �����λ��־ */
		WWDG_ClearFlag();
		
		/* ���´��ڿ��Ź������� */
		WWDG_SetCounter(127);
	}	
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
