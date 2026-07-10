/**
 * @file bsp_ir_decode.c
 * @brief 安富莱 BSP 通用驱动（红外解码）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : ����ң�ؽ���������ģ��
*	�ļ����� : bsp_ir_decode.c
*	��    �� : V1.0
*	˵    �� : ����ң�ؽ��յĺ����ź�����CPU�� PB0/TIM3_CH3.  ����������ʹ��TIM3_CH3ͨ�������벶������
*				Э�����롣
*
*	�޸ļ�¼ :
*		�汾��  ����         ����     ˵��
*		V1.0    2014-02-12   armfly  ��ʽ����
*		V1.1	2015-12-09   armfly  ����CPU��Ƶ����TIM��Ƶϵ�������192Mʱ���޷���ȷ�������⡣
*
*	Copyright (C), 2015-2020, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

#define IR_REPEAT_SEND_EN		1	/* ����ʹ�� */
#define IR_REPEAT_FILTER		10	/* ң����108ms ��������������, ��������1��������ط� */

/* ����GPIO�˿� */
#define IRD_CLK_ENABLE() 	__HAL_RCC_GPIOB_CLK_ENABLE()
/* 宏定义: IRD_GPIO - IRD_GPIO 的宏定义。 */
#define IRD_GPIO			GPIOB
/* 宏定义: IRD_PIN - IRD_PIN 的宏定义。 */
#define IRD_PIN				GPIO_PIN_8

/* PB8/TIM4_CH3 �������� */
#define	IRD_TIMx_IRQHandler		TIM4_IRQHandler
/* 宏定义: TIMx - TIMx 的宏定义。 */
#define TIMx					TIM4
/* 宏定义: TIMx_CHANNEL - TIMx_CHANNEL 的宏定义。 */
#define TIMx_CHANNEL			TIM_CHANNEL_3
/* 宏定义: TIMx_ACTIVE_CHANNEL - TIMx_ACTIVE_CHANNEL 的宏定义。 */
#define TIMx_ACTIVE_CHANNEL 	HAL_TIM_ACTIVE_CHANNEL_3

/* Definition for TIMx's NVIC */
#define TIMx_IRQn				TIM4_IRQn

/* 宏定义: TIMx_CLK_ENABLE - TIMx_CLK_ENABLE 的宏定义。 */
#define TIMx_CLK_ENABLE()		__HAL_RCC_TIM4_CLK_ENABLE()
/* 宏定义: TIMx_GPIO_AF_TIMx - TIMx_GPIO_AF_TIMx 的宏定义。 */
#define TIMx_GPIO_AF_TIMx  		GPIO_AF2_TIM4

static TIM_HandleTypeDef    TimHandle_IR;
IRD_T g_tIR;

/*
*********************************************************************************************************
*   函 数 名: IRD_DecodeNec
*   功能说明: IRD_DecodeNec 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void IRD_DecodeNec(uint16_t _width);

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitIRD
*	����˵��: ����STM32��GPIO,���ں���ң��������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitIRD(void)
{
	GPIO_InitTypeDef gpio_init;
	
	IRD_CLK_ENABLE();
	
	/* ����DQΪ��©��� */
	gpio_init.Mode = GPIO_MODE_AF_PP;		/* ���ÿ�©��� */
	gpio_init.Pull = GPIO_NOPULL;			/* ���������費ʹ�� */
	gpio_init.Speed = GPIO_SPEED_FREQ_LOW;  /* GPIO�ٶȵȼ� */	
	gpio_init.Pin = IRD_PIN;	
	HAL_GPIO_Init(IRD_GPIO, &gpio_init);	
}

/*
*********************************************************************************************************
*	�� �� ��: IRD_StartWork
*	����˵��: ����TIM����ʼ����
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void IRD_StartWork(void)
{
	{
		GPIO_InitTypeDef gpio_init;
		
		IRD_CLK_ENABLE();
		
		/* ����DQΪ��©��� */
		gpio_init.Mode = GPIO_MODE_AF_PP;		/* ���ÿ�©��� */
		gpio_init.Pull = GPIO_NOPULL;			/* ���������費ʹ�� */
		gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;  /* GPIO�ٶȵȼ� */
		gpio_init.Alternate = TIMx_GPIO_AF_TIMx;
		gpio_init.Pin = IRD_PIN;	
		HAL_GPIO_Init(IRD_GPIO, &gpio_init);	
	}
  
	{
		TIM_IC_InitTypeDef     sICConfig;
		  
		/* Set TIMx instance */
		TimHandle_IR.Instance = TIMx;

		/* ���÷�ƵΪ 1680/2�� ���������ֵ�ĵ�λ������ 10us, ���������Ƚ� 
			SystemCoreClock ����Ƶ. ����ֵ: 168000000, 180000000,192000000
		*/

		TimHandle_IR.Init.Period            = 0xFFFF;
		TimHandle_IR.Init.Prescaler         = (SystemCoreClock / 100000) / 2;
		TimHandle_IR.Init.ClockDivision     = 0;
		TimHandle_IR.Init.CounterMode       = TIM_COUNTERMODE_UP;
		TimHandle_IR.Init.RepetitionCounter = 0;
		if(HAL_TIM_IC_Init(&TimHandle_IR) != HAL_OK)
		{
			Error_Handler(__FILE__, __LINE__);
		}

		/*##-2- Configure the Input Capture channel ################################*/ 
		/* Configure the Input Capture of channel 2 */
		sICConfig.ICPolarity  = TIM_ICPOLARITY_BOTHEDGE;
		sICConfig.ICSelection = TIM_ICSELECTION_DIRECTTI;
		sICConfig.ICPrescaler = TIM_ICPSC_DIV1;
		sICConfig.ICFilter    = 0;   
		if (HAL_TIM_IC_ConfigChannel(&TimHandle_IR, &sICConfig, TIMx_CHANNEL) != HAL_OK)
		{
			Error_Handler(__FILE__, __LINE__);
		}

		/*##-3- Start the Input Capture in interrupt mode ##########################*/
		if (HAL_TIM_IC_Start_IT(&TimHandle_IR, TIMx_CHANNEL) != HAL_OK)
		{
			Error_Handler(__FILE__, __LINE__);
		}	  
	}
	
	{
		/* TIMx Peripheral clock enable */
		TIMx_CLK_ENABLE();

		HAL_NVIC_SetPriority(TIMx_IRQn, 0, 1);

		/* Enable the TIMx global Interrupt */
		HAL_NVIC_EnableIRQ(TIMx_IRQn);	
	}
	
	g_tIR.LastCapture = 0;	
	g_tIR.Status = 0;
}


/*
*********************************************************************************************************
*	�� �� ��: IRD_TIMx_IRQHandler
*	����˵��: TIM�жϷ������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void IRD_TIMx_IRQHandler(void)
{
	HAL_TIM_IRQHandler(&TimHandle_IR);
}

/*
*********************************************************************************************************
*	�� �� ��: HAL_TIM_IC_CaptureCallback
*	����˵��: TIM�жϷ�������л���ñ��������в����¼�����
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
	uint16_t NowCapture;
	uint16_t Width;
	
	if (htim->Channel == TIMx_ACTIVE_CHANNEL)
	{
		NowCapture = HAL_TIM_ReadCapturedValue(htim, TIMx_CHANNEL);

		if (NowCapture >= g_tIR.LastCapture)
		{
			Width = NowCapture - g_tIR.LastCapture;
		}
		else if (NowCapture < g_tIR.LastCapture)	/* �������ִ���󲢷�ת */
		{
			Width = ((0xFFFF - g_tIR.LastCapture) + NowCapture);
		}			
		
		if ((g_tIR.Status == 0) && (g_tIR.LastCapture == 0))
		{
			g_tIR.LastCapture = NowCapture;
			return;
		}
				
		g_tIR.LastCapture = NowCapture;	/* ���浱ǰ�������������´μ����ֵ */
		
		IRD_DecodeNec(Width);		/* ���� */		
	}
}

/*
*********************************************************************************************************
*	�� �� ��: IRD_StopWork
*	����˵��: ֹͣ�������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void IRD_StopWork(void)
{
	HAL_TIM_IC_DeInit(&TimHandle_IR);		
	
	HAL_TIM_IC_Stop_IT(&TimHandle_IR, TIMx_CHANNEL);
}

/*
*********************************************************************************************************
*	�� �� ��: IRD_DecodeNec
*	����˵��: ����NEC�����ʽʵʱ����
*	��    ��: _width ������ȣ���λ 10us
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void IRD_DecodeNec(uint16_t _width)
{
	static uint16_t s_LowWidth;
	static uint8_t s_Byte;
	static uint8_t s_Bit;
	uint16_t TotalWitdh;
	
	/* NEC ��ʽ ��5�Σ�
		1��������  9ms�� + 4.5ms��
		2����8λ��ַ��  0=1.125ms  1=2.25ms    bit0�ȴ�
		3����8λ��ַ��  0=1.125ms  1=2.25ms
		4��8λ����      0=1.125ms  1=2.25ms
		5��8Ϊ���뷴��  0=1.125ms  1=2.25ms
	*/

loop1:	
	switch (g_tIR.Status)
	{
		case 0:			/* 929 �ȴ���������ź�  7ms - 11ms */
			if ((_width > 700) && (_width < 1100))
			{
				g_tIR.Status = 1;
				s_Byte = 0;
				s_Bit = 0;
			}
			break;

		case 1:			/* 413 �ж���������ź�  3ms - 6ms */
			if ((_width > 313) && (_width < 600))	/* ������ 4.5ms */
			{
				g_tIR.Status = 2;
			}
			else if ((_width > 150) && (_width < 250))	/* 2.25ms */
			{
				#ifdef IR_REPEAT_SEND_EN				
					if (g_tIR.RepeatCount >= IR_REPEAT_FILTER)
					{
						bsp_PutKey(g_tIR.RxBuf[2] + IR_KEY_STRAT);	/* ������ */
					}
					else
					{
						g_tIR.RepeatCount++;
					}
				#endif
				g_tIR.Status = 0;	/* ��λ����״̬ */
			}
			else
			{
				/* �쳣���� */
				g_tIR.Status = 0;	/* ��λ����״̬ */
			}
			break;
		
		case 2:			/* �͵�ƽ�ڼ� 0.56ms */
			if ((_width > 10) && (_width < 100))
			{		
				g_tIR.Status = 3;
				s_LowWidth = _width;	/* ����͵�ƽ���� */
			}
			else	/* �쳣���� */
			{
				/* �쳣���� */
				g_tIR.Status = 0;	/* ��λ������״̬ */	
				goto loop1;		/* �����ж�ͬ���ź� */
			}
			break;

		case 3:			/* 85+25, 64+157 ��ʼ��������32bit */						
			TotalWitdh = s_LowWidth + _width;
			/* 0�Ŀ���Ϊ1.125ms��1�Ŀ���Ϊ2.25ms */				
			s_Byte >>= 1;
			if ((TotalWitdh > 92) && (TotalWitdh < 132))
			{
				;					/* bit = 0 */
			}
			else if ((TotalWitdh > 205) && (TotalWitdh < 245))
			{
				s_Byte += 0x80;		/* bit = 1 */
			}	
			else
			{
				/* �쳣���� */
				g_tIR.Status = 0;	/* ��λ������״̬ */	
				goto loop1;		/* �����ж�ͬ���ź� */
			}
			
			s_Bit++;
			if (s_Bit == 8)	/* ����8λ */
			{
				g_tIR.RxBuf[0] = s_Byte;
				s_Byte = 0;
			}
			else if (s_Bit == 16)	/* ����16λ */
			{
				g_tIR.RxBuf[1] = s_Byte;
				s_Byte = 0;
			}
			else if (s_Bit == 24)	/* ����24λ */
			{
				g_tIR.RxBuf[2] = s_Byte;
				s_Byte = 0;
			}
			else if (s_Bit == 32)	/* ����32λ */
			{
				g_tIR.RxBuf[3] = s_Byte;
								
				if (g_tIR.RxBuf[2] + g_tIR.RxBuf[3] == 255)	/* ���У�� */
				{
					bsp_PutKey(g_tIR.RxBuf[2] + IR_KEY_STRAT);	/* ����ֵ����KEY FIFO */
					
					g_tIR.RepeatCount = 0;	/* �ط������� */										
				}
				
				g_tIR.Status = 0;	/* �ȴ���һ����� */
				break;
			}
			g_tIR.Status = 2;	/* ������һ��bit */
			break;						
	}
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
