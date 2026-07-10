/**
 * @file bsp_key.c
 * @brief 安富莱 BSP 通用驱动（按键）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : ������������ģ�� (�ⲿ����IO)
*	�ļ����� : bsp_key.c
*	��    �� : V1.3
*	˵    �� : ɨ��������������������˲����ƣ����а���FIFO�����Լ�������¼���
*				(1) ��������
*				(2) ��������
*				(3) ������
*				(4) ����ʱ�Զ�����
*
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2013-02-01 armfly  ��ʽ����
*		V1.1    2013-06-29 armfly  ����1����ָ�룬����bsp_Idle() ������ȡϵͳ������ϼ���������
*								   ���� K1 K2 ��ϼ� �� K2 K3 ��ϼ�������ϵͳ����
*		V1.2	2016-01-25 armfly  ���P02���ذ����. ����gpio���巽ʽ�����Ӽ��
*		V1.3	2018-11-26 armfly  s_tBtn�ṹ����ֵ0
*
*	Copyright (C), 2016-2020, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

#define HARD_KEY_NUM	    8	   						/* ʵ�尴������ */
#define KEY_COUNT   	 	(HARD_KEY_NUM + 2)	/* 8�������� + 2����ϰ��� */

/* ʹ��GPIOʱ�� */
#define ALL_KEY_GPIO_CLK_ENABLE() {	\
		__HAL_RCC_GPIOB_CLK_ENABLE();	\
		__HAL_RCC_GPIOC_CLK_ENABLE();	\
		__HAL_RCC_GPIOG_CLK_ENABLE();	\
		__HAL_RCC_GPIOH_CLK_ENABLE();	\
		__HAL_RCC_GPIOI_CLK_ENABLE();	\
	};

/* ���ζ���GPIO */
typedef struct
{
	GPIO_TypeDef* gpio;
	uint16_t pin;
	uint8_t ActiveLevel;	/* �����ƽ */
}X_GPIO_T;

/* GPIO��PIN���� */
static const X_GPIO_T s_gpio_list[HARD_KEY_NUM] = {
	{GPIOI, GPIO_PIN_8, 0},		/* K1 */
	{GPIOC, GPIO_PIN_13, 0},	/* K2 */
	{GPIOH, GPIO_PIN_4, 0},		/* K3 */
	{GPIOG, GPIO_PIN_2, 0},		/* JOY_U */	
	{GPIOB, GPIO_PIN_0, 0},		/* JOY_D */
	{GPIOG, GPIO_PIN_3, 0},		/* JOY_L */	
	{GPIOG, GPIO_PIN_7, 0},		/* JOY_R */	
	{GPIOI, GPIO_PIN_11, 0},	/* JOY_OK */
};	

/* ����һ���꺯���򻯺������� 
	�ж�GPIO�����Ƿ���Ч����
*/
static KEY_T s_tBtn[KEY_COUNT] = {0};
static KEY_FIFO_T s_tKey;		/* ����FIFO����,�ṹ�� */

static void bsp_InitKeyVar(void);
/*
*********************************************************************************************************
*   函 数 名: bsp_InitKeyHard
*   功能说明: bsp_InitKeyHard 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void bsp_InitKeyHard(void);
/*
*********************************************************************************************************
*   函 数 名: bsp_DetectKey
*   功能说明: bsp_DetectKey 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void bsp_DetectKey(uint8_t i);

/* 宏定义: KEY_PIN_ACTIVE - KEY_PIN_ACTIVE 的宏定义。 */
#define KEY_PIN_ACTIVE(id)	

/*
*********************************************************************************************************
*	�� �� ��: KeyPinActive
*	����˵��: �жϰ����Ƿ���
*	��    ��: ��
*	�� �� ֵ: ����ֵ1 ��ʾ����(��ͨ����0��ʾδ���£��ͷţ�
*********************************************************************************************************
*/
static uint8_t KeyPinActive(uint8_t _id)
{
	uint8_t level;
	
	if ((s_gpio_list[_id].gpio->IDR & s_gpio_list[_id].pin) == 0)
	{
		level = 0;
	}
	else
	{
		level = 1;
	}

	if (level == s_gpio_list[_id].ActiveLevel)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: IsKeyDownFunc
*	����˵��: �жϰ����Ƿ��¡���������ϼ����֡������¼������������������¡�
*	��    ��: ��
*	�� �� ֵ: ����ֵ1 ��ʾ����(��ͨ����0��ʾδ���£��ͷţ�
*********************************************************************************************************
*/
static uint8_t IsKeyDownFunc(uint8_t _id)
{
	/* ʵ�嵥�� */
	if (_id < HARD_KEY_NUM)
	{
		uint8_t i;
		uint8_t count = 0;
		uint8_t save = 255;
		
		/* �ж��м��������� */
		for (i = 0; i < HARD_KEY_NUM; i++)
		{
			if (KeyPinActive(i)) 
			{
				count++;
				save = i;
			}
		}
		
		if (count == 1 && save == _id)
		{
			return 1;	/* ֻ��1��������ʱ����Ч */
		}		

		return 0;
	}
	
	/* ��ϼ� K1K2 */
	if (_id == HARD_KEY_NUM + 0)
	{
		if (KeyPinActive(KID_K1) && KeyPinActive(KID_K2))
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}

	/* ��ϼ� K2K3 */
	if (_id == HARD_KEY_NUM + 1)
	{
		if (KeyPinActive(KID_K2) && KeyPinActive(KID_K3))
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}

	return 0;
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitKey
*	����˵��: ��ʼ������. �ú����� bsp_Init() ���á�
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitKey(void)
{
	bsp_InitKeyVar();		/* ��ʼ���������� */
	bsp_InitKeyHard();		/* ��ʼ������Ӳ�� */
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitKeyHard
*	����˵��: ���ð�����Ӧ��GPIO
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void bsp_InitKeyHard(void)
{	
	GPIO_InitTypeDef gpio_init;
	uint8_t i;

	/* ��1������GPIOʱ�� */
	ALL_KEY_GPIO_CLK_ENABLE();
	
	/* ��2�����������еİ���GPIOΪ��������ģʽ(ʵ����CPU��λ���������״̬) */
	gpio_init.Mode = GPIO_MODE_INPUT;   			/* �������� */
	gpio_init.Pull = GPIO_NOPULL;                 /* ���������費ʹ�� */
	gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;  /* GPIO�ٶȵȼ� */
	
	for (i = 0; i < HARD_KEY_NUM; i++)
	{
		gpio_init.Pin = s_gpio_list[i].pin;
		HAL_GPIO_Init(s_gpio_list[i].gpio, &gpio_init);	
	}
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitKeyVar
*	����˵��: ��ʼ����������
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void bsp_InitKeyVar(void)
{
	uint8_t i;

	/* �԰���FIFO��дָ������ */
	s_tKey.Read = 0;
	s_tKey.Write = 0;
	s_tKey.Read2 = 0;

	/* ��ÿ�������ṹ���Ա������һ��ȱʡֵ */
	for (i = 0; i < KEY_COUNT; i++)
	{
		s_tBtn[i].LongTime = KEY_LONG_TIME;			/* ����ʱ�� 0 ��ʾ����ⳤ�����¼� */
		s_tBtn[i].Count = KEY_FILTER_TIME / 2;		/* ����������Ϊ�˲�ʱ���һ�� */
		s_tBtn[i].State = 0;							/* ����ȱʡ״̬��0Ϊδ���� */
		s_tBtn[i].RepeatSpeed = 0;						/* �����������ٶȣ�0��ʾ��֧������ */
		s_tBtn[i].RepeatCount = 0;						/* ���������� */
	}

	/* �����Ҫ��������ĳ�������Ĳ����������ڴ˵������¸�ֵ */
	
	/* ҡ���������ң�֧�ֳ���1����Զ����� */
	bsp_SetKeyParam(KID_JOY_U, 100, 6);
	bsp_SetKeyParam(KID_JOY_D, 100, 6);
	bsp_SetKeyParam(KID_JOY_L, 100, 6);
	bsp_SetKeyParam(KID_JOY_R, 100, 6);
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_PutKey
*	����˵��: ��1����ֵѹ�밴��FIFO��������������ģ��һ��������
*	��    ��:  _KeyCode : ��������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_PutKey(uint8_t _KeyCode)
{
	s_tKey.Buf[s_tKey.Write] = _KeyCode;

	if (++s_tKey.Write  >= KEY_FIFO_SIZE)
	{
		s_tKey.Write = 0;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_GetKey
*	����˵��: �Ӱ���FIFO��������ȡһ����ֵ��
*	��    ��: ��
*	�� �� ֵ: ��������
*********************************************************************************************************
*/
uint8_t bsp_GetKey(void)
{
	uint8_t ret;

	if (s_tKey.Read == s_tKey.Write)
	{
		return KEY_NONE;
	}
	else
	{
		ret = s_tKey.Buf[s_tKey.Read];

		if (++s_tKey.Read >= KEY_FIFO_SIZE)
		{
			s_tKey.Read = 0;
		}
		return ret;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_GetKey2
*	����˵��: �Ӱ���FIFO��������ȡһ����ֵ�������Ķ�ָ�롣
*	��    ��:  ��
*	�� �� ֵ: ��������
*********************************************************************************************************
*/
uint8_t bsp_GetKey2(void)
{
	uint8_t ret;

	if (s_tKey.Read2 == s_tKey.Write)
	{
		return KEY_NONE;
	}
	else
	{
		ret = s_tKey.Buf[s_tKey.Read2];

		if (++s_tKey.Read2 >= KEY_FIFO_SIZE)
		{
			s_tKey.Read2 = 0;
		}
		return ret;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_GetKeyState
*	����˵��: ��ȡ������״̬
*	��    ��:  _ucKeyID : ����ID����0��ʼ
*	�� �� ֵ: 1 ��ʾ���£� 0 ��ʾδ����
*********************************************************************************************************
*/
uint8_t bsp_GetKeyState(KEY_ID_E _ucKeyID)
{
	return s_tBtn[_ucKeyID].State;
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_SetKeyParam
*	����˵��: ���ð�������
*	��    �Σ�_ucKeyID : ����ID����0��ʼ
*			_LongTime : �����¼�ʱ��
*			 _RepeatSpeed : �����ٶ�
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_SetKeyParam(uint8_t _ucKeyID, uint16_t _LongTime, uint8_t  _RepeatSpeed)
{
	s_tBtn[_ucKeyID].LongTime = _LongTime;			/* ����ʱ�� 0 ��ʾ����ⳤ�����¼� */
	s_tBtn[_ucKeyID].RepeatSpeed = _RepeatSpeed;			/* �����������ٶȣ�0��ʾ��֧������ */
	s_tBtn[_ucKeyID].RepeatCount = 0;						/* ���������� */
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_ClearKey
*	����˵��: ��հ���FIFO������
*	��    �Σ���
*	�� �� ֵ: ��������
*********************************************************************************************************
*/
void bsp_ClearKey(void)
{
	s_tKey.Read = s_tKey.Write;
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_DetectKey
*	����˵��: ���һ��������������״̬�����뱻�����Եĵ��á�
*	��    ��: IO��id�� ��0��ʼ����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void bsp_DetectKey(uint8_t i)
{
	KEY_T *pBtn;

	pBtn = &s_tBtn[i];
	if (IsKeyDownFunc(i))
	{
		if (pBtn->Count < KEY_FILTER_TIME)
		{
			pBtn->Count = KEY_FILTER_TIME;
		}
		else if(pBtn->Count < 2 * KEY_FILTER_TIME)
		{
			pBtn->Count++;
		}
		else
		{
			if (pBtn->State == 0)
			{
				pBtn->State = 1;

				/* ���Ͱ�ť���µ���Ϣ */
				bsp_PutKey((uint8_t)(3 * i + 1));
			}

			if (pBtn->LongTime > 0)
			{
				if (pBtn->LongCount < pBtn->LongTime)
				{
					/* ���Ͱ�ť�������µ���Ϣ */
					if (++pBtn->LongCount == pBtn->LongTime)
					{
						/* ��ֵ���밴��FIFO */
						bsp_PutKey((uint8_t)(3 * i + 3));
					}
				}
				else
				{
					if (pBtn->RepeatSpeed > 0)
					{
						if (++pBtn->RepeatCount >= pBtn->RepeatSpeed)
						{
							pBtn->RepeatCount = 0;
							/* ��������ÿ��10ms����1������ */
							bsp_PutKey((uint8_t)(3 * i + 1));
						}
					}
				}
			}
		}
	}
	else
	{
		if(pBtn->Count > KEY_FILTER_TIME)
		{
			pBtn->Count = KEY_FILTER_TIME;
		}
		else if(pBtn->Count != 0)
		{
			pBtn->Count--;
		}
		else
		{
			if (pBtn->State == 1)
			{
				pBtn->State = 0;

				/* ���Ͱ�ť�������Ϣ */
				bsp_PutKey((uint8_t)(3 * i + 2));
			}
		}

		pBtn->LongCount = 0;
		pBtn->RepeatCount = 0;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_DetectFastIO
*	����˵��: �����ٵ�����IO. 1msˢ��һ��
*	��    ��: IO��id�� ��0��ʼ����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void bsp_DetectFastIO(uint8_t i)
{
	KEY_T *pBtn;

	pBtn = &s_tBtn[i];
	if (IsKeyDownFunc(i))
	{
		if (pBtn->State == 0)
		{
			pBtn->State = 1;

			/* ���Ͱ�ť���µ���Ϣ */
			bsp_PutKey((uint8_t)(3 * i + 1));
		}

		if (pBtn->LongTime > 0)
		{
			if (pBtn->LongCount < pBtn->LongTime)
			{
				/* ���Ͱ�ť�������µ���Ϣ */
				if (++pBtn->LongCount == pBtn->LongTime)
				{
					/* ��ֵ���밴��FIFO */
					bsp_PutKey((uint8_t)(3 * i + 3));
				}
			}
			else
			{
				if (pBtn->RepeatSpeed > 0)
				{
					if (++pBtn->RepeatCount >= pBtn->RepeatSpeed)
					{
						pBtn->RepeatCount = 0;
						/* ��������ÿ��10ms����1������ */
						bsp_PutKey((uint8_t)(3 * i + 1));
					}
				}
			}
		}
	}
	else
	{
		if (pBtn->State == 1)
		{
			pBtn->State = 0;

			/* ���Ͱ�ť�������Ϣ */
			bsp_PutKey((uint8_t)(3 * i + 2));
		}

		pBtn->LongCount = 0;
		pBtn->RepeatCount = 0;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_KeyScan10ms
*	����˵��: ɨ�����а���������������systick�ж������Եĵ��ã�10msһ��
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_KeyScan10ms(void)
{
	uint8_t i;

	for (i = 0; i < KEY_COUNT; i++)
	{
		bsp_DetectKey(i);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_KeyScan1ms
*	����˵��: ɨ�����а���������������systick�ж������Եĵ��ã�1msһ��.
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_KeyScan1ms(void)
{
	uint8_t i;

	for (i = 0; i < KEY_COUNT; i++)
	{
		bsp_DetectFastIO(i);
	}
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
