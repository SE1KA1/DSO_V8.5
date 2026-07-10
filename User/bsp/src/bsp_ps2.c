/**
 * @file bsp_ps2.c
 * @brief 安富莱 BSP 通用驱动（PS/2 键盘）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : PS/2�����������ģ��
*	�ļ����� : bsp_ps2.c
*	��    �� : V1.0
*	˵    �� :
*
*	�޸ļ�¼ :
*		�汾��  ����         ����     ˵��
*		V1.0    2014-02-12   armfly  ��ʽ����
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

/*
	ʹ��˵��:
	1. ����ִ��1�� bsp_InitPS2() ����GPIO
	2. ִ�� PS2_StartWork() ��PS2�ж�
	3. ÿms���� PS2_Timer().  ��bsp.c ��bsp_RunPer10ms() ���������ӵ��ü��ɡ�
	4. ����ҪPS2����ʱ, ���� PS2_StopWork() �ر��жϡ�	
*/

#include "bsp.h"

/* ��ӡ������� */
#define ps2_printf printf
//#define ps2_printf(...)

/* ����GPIO�˿�  PG7 = PS2_CLK   PG3 = PS2_DATA */
#define RCC_PS2_CLK_ENABLE()   __HAL_RCC_GPIOG_CLK_ENABLE();
/* 宏定义: PORT_PS2_CLK - PORT_PS2_CLK 的宏定义。 */
#define PORT_PS2_CLK  GPIOG
/* 宏定义: PIN_PS2_CLK - PIN_PS2_CLK 的宏定义。 */
#define PIN_PS2_CLK   GPIO_PIN_7

/* 宏定义: RCC_PS2_DATA_ENABLE - RCC_PS2_DATA_ENABLE 的宏定义。 */
#define RCC_PS2_DATA_ENABLE()   __HAL_RCC_GPIOG_CLK_ENABLE();
/* 宏定义: PORT_PS2_DATA - PORT_PS2_DATA 的宏定义。 */
#define PORT_PS2_DATA GPIOG
/* 宏定义: PIN_PS2_DATA - PIN_PS2_DATA 的宏定义。 */
#define PIN_PS2_DATA  GPIO_PIN_3

/* 宏定义: PS2_DATA_IS_HIGH - PS2_DATA_IS_HIGH 的宏定义。 */
#define PS2_DATA_IS_HIGH()	((PORT_PS2_DATA->IDR & PIN_PS2_DATA) != 0)

#define PS2_CLK_1()  PORT_PS2_CLK->BSRRL = PIN_PS2_CLK				/* CLK = 1 */
#define PS2_CLK_0()  PORT_PS2_CLK->BSRRH = PIN_PS2_CLK				/* CLK = 0 */

#define PS2_DATA_1() PORT_PS2_DATA->BSRRL = PIN_PS2_DATA			/* DATA = 1 */
#define PS2_DATA_0() PORT_PS2_DATA->BSRRH = PIN_PS2_DATA			/* DATA = 0 */


typedef struct
{
	uint32_t code;
	char * str;
}KB_STR_T;

static const KB_STR_T s_KeyNameTab[] = 
{	
	{0xEEEEEEEE, ""},

    {KB_A, "A"},
    {KB_B, "B"},
    {KB_C, "C"},
    {KB_D, "D"},
    {KB_E, "E"},
    {KB_F, "F"},
    {KB_G, "G"},
    {KB_H, "H"},
    {KB_I, "I"},
    {KB_J, "J"},
    {KB_K, "K"},
    {KB_L, "L"},
    {KB_M, "M"},
    {KB_N, "N"},
    {KB_O, "O"},
    {KB_P, "P"},
    {KB_Q, "Q"},
    {KB_R, "R"},
    {KB_S, "S"},
    {KB_T, "T"},
    {KB_U, "U"},
    {KB_V, "V"},
    {KB_W, "W"},
    {KB_X, "X"},
    {KB_Y, "Y"},
    {KB_Z, "Z"},
    {KB_0, "0"},
    {KB_1, "1"},
    {KB_2, "2"},
    {KB_3, "3"},
    {KB_4, "4"},
    {KB_5, "5"},
    {KB_6, "6"},
    {KB_7, "7"},
    {KB_8, "8"},
    {KB_9, "9"},
    {KB_PIE, "`"},	/* Ʋ���������Ͻ� */
    {KB_SUB, "-"},	/* �иܣ����� */
    {KB_EQU, "="},	/* �Ⱥ� */
    {KB_FXG, "\\"},	/* ��б�� */
    {KB_BKSP,"Backspace"},
    {KB_SPACE, "Space"},
    {KB_TAB, "Tab"},
    {KB_CAPS, "CapsLk"},
    {KB_L_SHFT, "Shift Left"},
    {KB_L_CTRL, "Ctrl Left"},
    {KB_L_GUI, "GUI Left"},
    {KB_L_ALT, "Alt Left"},
    {KB_R_SHFT,"Shift Right"},
    {KB_R_CTRL, "Ctrl Right"},
    {KB_R_GUI, "Gui Right"},
    {KB_R_ALT, "Alt Right"},
    {KB_APPS, "Apps"},
    {KB_ENTER, "Enter"},
    {KB_ESC, "ESC"},
    {KB_F1, "F1"},
    {KB_F2, "F2"},
    {KB_F3, "F3"},
    {KB_F4, "F4"},
    {KB_F5, "F5"},
    {KB_F6, "F6"},
    {KB_F7, "F7"},
    {KB_F8, "F8"},
    {KB_F9, "F9"},
    {KB_F10, "F10"},
    {KB_F11, "F11"},
    {KB_F12, "F12"},
    {KB_PRNT_SCRN, "Print Screen/SysRq"},
    {KB_SCROLL, "Scroll Lock"},
    {KB_PAUSE, "Pause/Break"},
    {KB_ZZKH, "["},
    {KB_INSERT, "Insert"},
    {KB_HOME, "Home"},
    {KB_PGUP, "Page Up"},
    {KB_DELETE, "Delete"},
    {KB_END, "End"},
    {KB_PGDN, "Page Down"},
    {KB_U_ARROW, "Up Arrow"},
    {KB_L_ARROW, "Left Arrow"},
    {KB_D_ARROW, "Down Arrow"},
    {KB_R_ARROW, "Right Arrow"},
    {KB_NUM, "Num Lock"},
    {KB_KP_DIV, "KP /"},    /* С���̳���  KP ��ʾС���� */
    {KB_KP_MULT, "KP *"},	/* С���̳˺� */
    {KB_KP_SUB, "KP -"},	/* - */
    {KB_KP_ADD, "KP +"},
    {KB_KP_ENTER, "KP Enter"},
    {KB_KP_DOT, "KP ."},	/* С���� */
    {KB_KP_0, "KP 0"},
    {KB_KP_1, "KP 1"},
    {KB_KP_2, "KP 2"},
    {KB_KP_3, "KP 3"},
    {KB_KP_4, "KP 4"},
    {KB_KP_5, "KP 5"},
    {KB_KP_6, "KP 6"},
    {KB_KP_7, "KP 7"},
    {KB_KP_8, "KP 8"},
    {KB_KP_9, "KP 9"},
    {KB_YZKH, "]"}, 	/* ] �������� */
    {KB_SEMICOLON, ";"},/* ; �ֺ� */
    {KB_QUOTES, "'"},	/* ������ */
    {KB_COMMA, ","},	/* ���� */
    {KB_DOT, "."},		/* С���� */
    {KB_DIV, "/"},		/* ���� */
	
	{0, ""}		/* ���������־ */
};

PS2_T g_tPS2;

/*
*********************************************************************************************************
*   函 数 名: PS2_SendCmd
*   功能说明: PS2_SendCmd 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void PS2_SendCmd(uint8_t _byte);
/*
*********************************************************************************************************
*   函 数 名: PS2_WaitMsg
*   功能说明: PS2_WaitMsg 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t PS2_WaitMsg(uint32_t *_rsp, uint16_t _timeout);
/*
*********************************************************************************************************
*   函 数 名: PS2_HookKeyboard
*   功能说明: PS2_HookKeyboard 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t PS2_HookKeyboard(uint32_t _msg);

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitPS2
*	����˵��: ����STM32��GPIO,����PS2
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitPS2(void)
{
	GPIO_InitTypeDef gpio_init;

	/* ��GPIOʱ�� */
	RCC_PS2_CLK_ENABLE();
	RCC_PS2_DATA_ENABLE();

	PS2_CLK_1();
	PS2_DATA_1();

	/* ����CLK �� DATA Ϊ��©������� */	
	gpio_init.Mode = GPIO_MODE_OUTPUT_OD;	/* ����������� */
	gpio_init.Pull = GPIO_NOPULL;			/* ���������費ʹ�� */
	gpio_init.Speed = GPIO_SPEED_FREQ_LOW;  /* GPIO�ٶȵȼ� */
	
	gpio_init.Pin = PIN_PS2_CLK;	
	HAL_GPIO_Init(PORT_PS2_CLK, &gpio_init);	
	
	gpio_init.Pin = PIN_PS2_DATA;	
	HAL_GPIO_Init(PORT_PS2_DATA, &gpio_init);		
	
	g_tPS2.TxTimeOut = 0;
	g_tPS2.RxTimeOut = 0;
}

/*
*********************************************************************************************************
*	�� �� ��: PS2_StartWork
*	����˵��: �����жϣ���ʼ����
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void PS2_StartWork(void)
{
	/* ���� PS2_CLK ��Ϊ�ж�����ڣ��½��ش��� */
	{
		GPIO_InitTypeDef   GPIO_InitStructure;
		
		RCC_PS2_CLK_ENABLE();		/* ��GPIOʱ�� */
		
		GPIO_InitStructure.Mode = GPIO_MODE_IT_FALLING;		/* �ж��½��ش��� */;
		GPIO_InitStructure.Pull = GPIO_NOPULL;
		GPIO_InitStructure.Pin = PIN_PS2_CLK;
		HAL_GPIO_Init(PORT_PS2_CLK, &GPIO_InitStructure);	

		HAL_NVIC_SetPriority(EXTI9_5_IRQn, 2, 0);
		HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);		
	}

	PS2_CLK_1();
	PS2_DATA_1();

	g_tPS2.Status = 0;

	PS2_ClearBuf();		/* ��λFIFO����������� */
}

/*
*********************************************************************************************************
*	�� �� ��: PS2_StopWork
*	����˵��: ֹͣ����
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void PS2_StopWork(void)
{
	/* ���� PS2_CLK ��Ϊ�ж�����ڣ��½��ش��� */
	{
		GPIO_InitTypeDef   GPIO_InitStructure;
		
//		RCC_PS2_CLK_ENABLE();		/* ��GPIOʱ�� */

		/* Configure PC.13 pin as input floating */
		GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_OD;	/* ���ж� */;
		GPIO_InitStructure.Pull = GPIO_NOPULL;
		GPIO_InitStructure.Pin = PIN_PS2_CLK;
		HAL_GPIO_Init(PORT_PS2_CLK, &GPIO_InitStructure);	

		HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);		
	}

	PS2_ClearBuf();
}

/*
*********************************************************************************************************
*	�� �� ��: PS2_Timer
*	����˵��: �˺�������ÿms��ִ��1�Σ� ���뵽 bsp.c �е� bsp_RunPer1ms() ����
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void PS2_Timer(void)
{
	if (g_tPS2.TxTimeOut > 0)
	{
		if (--g_tPS2.TxTimeOut == 0)
		{
			/* �ͷ� DATA�� */
			PS2_DATA_1();
		}
	}
	if (g_tPS2.RxTimeOut > 0)
	{
		g_tPS2.RxTimeOut--;

		if (g_tPS2.RxTimeOut == 0)
		{
			if (g_tPS2.Len > 0)
			{
				uint32_t value;

				if (g_tPS2.Len == 1)
				{
					value = g_tPS2.CodeBuf[0];
				}
				else if (g_tPS2.Len == 2)
				{
					value = (g_tPS2.CodeBuf[0] << 8) + g_tPS2.CodeBuf[1];
				}
				else if (g_tPS2.Len == 3)
				{
					value = (g_tPS2.CodeBuf[0] << 16) + (g_tPS2.CodeBuf[1] << 8) + g_tPS2.CodeBuf[2];
				}
				else
				{
					value = (g_tPS2.CodeBuf[0] << 24) + (g_tPS2.CodeBuf[1] << 16) +
							(g_tPS2.CodeBuf[2] << 8) + g_tPS2.CodeBuf[3];
				}

				g_tPS2.Len = 0;

				if (PS2_HookKeyboard(value))
				{
					if (g_tPS2.LedReq < 2)	/* ���������̷�������׶� �������̵�Ӧ����Ϣ����Ӧ�ò� */
					{					
						PS2_PutMsg(value);	/* �������������FIFO, ����Ӧ�ò㴦�� */
					}
				}
			}
		}
	}
	
	/* �����̷���������Ƽ��̵�ָʾ�� */
	if (g_tPS2.LedReq > 0)
	{
		g_tPS2.LedReq++;
		if (g_tPS2.LedReq == 2)
		{			
			PS2_SendCmd(0xED);				/* ����ָ���1�ֽ� -- �޸�LED״̬  */
		}
		else if (g_tPS2.LedReq == 7)
		{
			PS2_SendCmd(g_tPS2.LedData);	/* �ӳ�5ms ����ָ���2�ֽ� -- ָʾ��״̬ */
		}		
		else if (g_tPS2.LedReq == 17)		/* ���ӳ�10ms, �ȼ��̵�Ӧ����ɺ��˳� */
		{
			g_tPS2.LedReq = 0;
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: PS2_HookKeyboard
*	����˵��: ���̹��Ӻ���������Ӧ�ó���֮ǰ����Ԥ������
*	��    ��: _msg  ��ֵ����
*	�� �� ֵ: 0 ��ʾ�ڲ�������������Ӧ�ò㡣1��ʾ����Ҫ����Ӧ�ò㴦����
*********************************************************************************************************
*/
static uint8_t PS2_HookKeyboard(uint32_t _msg)
{
	uint8_t reput = 1;	/* ����ʹ��ʱ��������Ϊ0. CTRL , ALT ,SHIFT �����¼������ϱ�Ӧ�ò㴦�� */

	/* ����PS2����״̬ */
	switch (_msg)
	{
		case KB_L_SHFT:		/* Shift ������ */
		case KB_R_SHFT:
			g_tPS2.ksShift = 1;
			break;

		case BREAK_L_SHFT:	/* Shift ���ɿ� */
		case BREAK_R_SHFT:
			g_tPS2.ksShift = 0;
			break;

		case KB_L_CTRL:		/* Ctrl ������ */
		case KB_R_CTRL:
			g_tPS2.ksCtrl = 1;
			break;

		case BREAK_L_CTRL:	/* Ctrl ���ɿ� */
		case BREAK_R_CTRL:
			g_tPS2.ksCtrl = 0;
			break;

		case KB_L_ALT:		/* Alt ������ */
		case KB_R_ALT:
			g_tPS2.ksAlt = 1;
			break;

		case BREAK_L_ALT:	/* Alt ���ɿ� */
		case BREAK_R_ALT:
			g_tPS2.ksAlt = 0;
			break;

		case KB_CAPS:		/* ��Сд�л� */
			if (g_tPS2.ksCapsLock == 0)
			{
				g_tPS2.ksCapsLock = 1;
				//ps2_printf(" ��Caps Lock��\r\n");
			}
			else
			{
				g_tPS2.ksCapsLock = 0;
				//ps2_printf(" ��Caps Lock��\r\n");
			}
			PS2_SetKeyboardLed(LED_CapsLock, g_tPS2.ksCapsLock);
			break;

		case KB_NUM:		/* ����С����ʹ�� */
			if (g_tPS2.KsNumLock == 0)
			{
				g_tPS2.KsNumLock = 1;
				//ps2_printf(" ��Num Lock��\r\n");
			}
			else
			{
				g_tPS2.KsNumLock = 0;
				//ps2_printf(" ��Num Lock��\r\n");
			}
			PS2_SetKeyboardLed(LED_NumLock, g_tPS2.KsNumLock);
			break;

		case KB_SCROLL:		/* */
			if (g_tPS2.KsScrollLock == 0)
			{
				g_tPS2.KsScrollLock = 1;
				//ps2_printf(" ��Scroll Lock��\r\n");
			}
			else
			{
				g_tPS2.KsScrollLock = 0;
				//ps2_printf(" ��Scroll Lock��\r\n");
			}
			PS2_SetKeyboardLed(LED_ScrollLock, g_tPS2.KsScrollLock);
			break;

		default:
			reput = 1;
			break;
	}

	return reput;
}

/*
*********************************************************************************************************
*	�� �� ��: GetNameOfKey
*	����˵��: ��ð�������
*	��    ��: ��
*	�� �� ֵ: �ַ���ָ��
*********************************************************************************************************
*/
const char * GetNameOfKey(uint32_t _code)
{
	uint16_t i = 0;

	
	while (1)
	{		
		if (s_KeyNameTab[i].code == 0)
		{
			break;
		}
		
		if (_code == s_KeyNameTab[i].code)
		{
			return s_KeyNameTab[i].str;
		}
		i++;
	}
	
	return "";
}


/*
*********************************************************************************************************
*	�� �� ��: PS2_PutMsg
*	����˵��: ��1����ֵѹ�밴��FIFO��������������ģ��һ��������
*	��    ��:  _KeyCode : ��������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void PS2_PutMsg(uint32_t _KeyCode)
{
	g_tPS2.Buf[g_tPS2.Write] = _KeyCode;

	if (++g_tPS2.Write  >= KEY_FIFO_SIZE)
	{
		g_tPS2.Write = 0;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_GetMsg
*	����˵��: �Ӱ���FIFO��������ȡһ����ֵ��
*	��    ��:  ��
*	�� �� ֵ: ��������
*********************************************************************************************************
*/
uint32_t PS2_GetMsg(void)
{
	uint32_t ret;

	if (g_tPS2.Read == g_tPS2.Write)
	{
		return PS2_NONE;
	}
	else
	{
		ret = g_tPS2.Buf[g_tPS2.Read];

		if (++g_tPS2.Read >= KEY_FIFO_SIZE)
		{
			g_tPS2.Read = 0;
		}
		return ret;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: PS2_ClearBuf
*	����˵��: �������������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void PS2_ClearBuf(void)
{
	/* �԰���FIFO��дָ������ */
	g_tPS2.Read = 0;
	g_tPS2.Write = 0;

	g_tPS2.Len = 0;
	g_tPS2.TxTimeOut = 0;
	g_tPS2.RxTimeOut = 0;
}

/*
*********************************************************************************************************
*	�� �� ��: PS2_SendCmd
*	����˵��: ��������̷���1�ֽ�����
*	��    ��: _byte : �����͵�����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void PS2_SendCmd(uint8_t _byte)
{
	/*
		�������밴����Ĳ��跢�����ݵ�PS/2�豸��
		1)  ��Clock����������100us��
		2)  ��Data�����ͣ�
		3)  �ͷ�Clock�ߣ�
		4)  �ȴ�PS/2�豸��Clock�����ͣ�
		5)  ����/��λData�߷��͵�һ������λ��
		6)  �ȴ�PS/2�豸��ʱ�����ߣ�
		7)  �ȴ�PS/2�豸��ʱ�����ͣ�
		8)  �ظ�5-7������ʣ�µ�7������λ��У��λ��
		9)  �ͷ�Data�ߣ�������ֹͣλ(1)��
		10) �ȴ�PS/2�豸��Clock�����ߣ� //�˲���ʡ��,��Ϊ��һ��PS/2�豸���ǻ��Data�����͵�
		11) �ȴ�PS/2�豸��Data�����ͣ�
		12) �ȴ�PS/2�豸��Clock�����ͣ�
		13) �ȴ�PS/2�豸�ͷ�Clock�ߺ�Data�ߡ�
	*/

	g_tPS2.Sending = 1;
	g_tPS2.Cmd = _byte;
	g_tPS2.Status = 0;	/*  */

	PS2_CLK_0();		/* ��Clock����������100us --- ���õͺ�ᴥ��CLK�½����жϣ��жϷ������Ӧ�ö�������¼��� */
	bsp_DelayUS(100);

	PS2_DATA_0();		/* ��Data������ */
	g_tPS2.TxTimeOut = 2;		/* ����û��PS2�豸ʱ��DATA��һֱ������ */

	bsp_DelayUS(50);

	PS2_CLK_1();		/* �ͷ�Clock�� */

	/* ����Ĺ�����CLK�½����жϷ��������� */
	
}

/*
*********************************************************************************************************
*	�� �� ��: PS2_WaitMsg
*	����˵��: �ȴ�Ӧ��.
*	��    ��: _rsp : ��Ž��
*			 _timeout : ��ʱʱ�䣬�������0����λ ms
*	�� �� ֵ: 0 ��ʾ�����ݣ� 1��ʾ�ɹ�
*********************************************************************************************************
*/
static uint8_t PS2_WaitMsg(uint32_t *_rsp, uint16_t _timeout)
{
	uint32_t read;
	uint16_t i;

	for (i = 0; i < _timeout; i++)
	{
		read = PS2_GetMsg();
		if (read != PS2_NONE)
		{
			*_rsp = read;
			break;
		}
		bsp_DelayUS(1000);
	}

	g_tPS2.Sending = 0;

	if (i == _timeout)
	{
		return 0;	/* ��ʱδ�������� */
	}
	return 1;	/* �ɹ��������� */
}


/*
*********************************************************************************************************
*	�� �� ��: PS2_GetDevceType
*	����˵��: �Զ�ʶ���豸���ͣ���껹�Ǽ��̣�
*	��    ��: ��
*	�� �� ֵ: 0 ��ʾʧ�ܣ� 1��ʾ�ɹ�
*********************************************************************************************************
*/
uint8_t PS2_GetDevceType(void)
{
	uint32_t Rsp;
	uint8_t dev;
	uint8_t i;

	/* ʶ�𷽷�Ϊ������0xFFָ�Ӧ��FA    AA ���Ǽ��̣�Ӧ�� FA     AA 00 ������ꡣ */

	dev = PS2_UNKNOW_DEVICE;

	for (i = 0; i < 3; i++)
	{
		/* �����PS2�豸ͬʱ�ϵ�ʱ���豸�ڲ��������ڳ�ʼ���������Ҫ�ȴ� */		
		PS2_SendCmd(0xFF);
		ps2_printf("   Host   : %X\r\n", 0xFF);
		if (PS2_WaitMsg(&Rsp, 20))	/* 20ms ��ʱ */
		{
			ps2_printf("   Device : %X\r\n", Rsp);			
			if (Rsp == 0xFA)
			{
				if (PS2_WaitMsg(&Rsp, 1000))
				{
					ps2_printf("   Device : %X\r\n", Rsp);

					if (Rsp == 0xAA00)
					{
						dev = PS2_MOUSE;	/* ��� */
						break;
					}
					else if (Rsp == 0xAA)
					{
						dev = PS2_KEYBOARD;	/* ���� */
						break;
					}
				}
			}
		}
	}
	return dev;
}

/*
*********************************************************************************************************
*	�� �� ��: PS2_SetKeyboardLed
*	����˵��: ���ü���ָʾ��
*	��    ��: _id : �ĸ�ָʾ�ơ�LED_CpasLock , LED_NumLock,	LED_ScrollLock
*			  _on : 0 ��ʾ��, 1 ��ʾ��
*	�� �� ֵ: 0 ��ʾʧ�ܣ� 1��ʾ�ɹ�
*********************************************************************************************************
*/
void PS2_SetKeyboardLed(uint8_t _id, uint8_t _on)
{
	/*
		0xED (Set/Reset LEDs) �����ڱ���������һ�������ֽ�����ָʾ������Num Lock, Caps Lock,
		and Scroll Lock LED ��״̬ ��������ֽڵĶ�������

		bin7      bin6      bin5      bin4      bin3      bin2       bin1      bin0
		Always 0  Always 0  Always 0  Always 0  Always 0  Caps Lock  Num Lock  Scroll Lock
		  "Scroll Lock" - Scroll Lock LED off(0)/on(1)
		  "Num Lock" - Num Lock LED off(0)/on(1)
		  "Caps Lock" - Caps Lock LED off(0)/on(1)
	*/
	uint8_t data;
	//uint32_t Rsp;

	if (_id == LED_CapsLock)
	{
		g_tPS2.ksCapsLock = _on;
	}
	else if (_id == LED_NumLock)
	{
		g_tPS2.KsNumLock = _on;
	}
	else if (_id == LED_ScrollLock)
	{
		g_tPS2.KsScrollLock = _on;
	}

	data = 0;
	if (g_tPS2.ksCapsLock == 1)
	{
		data |= (1 << 2);
	}
	if (g_tPS2.KsNumLock == 1)
	{
		data |= (1 << 1);
	}
	if (g_tPS2.KsScrollLock == 1)
	{
		data |= (1 << 0);
	}

#if 1	/* �� systick ms�жϷ�������б�ִ�� */	
	g_tPS2.LedReq = 1;
	g_tPS2.LedData = data;
#else	
	ps2_printf("Host : %02X",0xED);
	PS2_SendCmd(0xED);
	if (PS2_WaitMsg(&Rsp, 20))
	{
		ps2_printf("   KeyBoard : %X\r\n",Rsp);
	}

	PS2_SendCmd(data);
	if (PS2_WaitMsg(&Rsp, 20))
	{
		ps2_printf("   KeyBoard : %X\r\n",Rsp);
	}
#endif	
}

/*
*********************************************************************************************************
*	�� �� ��: PS2_InitKeyboard
*	����˵��: ��ʼ������
*	��    ��: ��
*	�� �� ֵ: 0 ��ʾʧ�ܣ� 1��ʾ�ɹ�
*********************************************************************************************************
*/
uint8_t PS2_InitKeyboard(void)
{
	uint32_t Rsp;	/* ����������Ӧ������ */
	uint16_t i;
	uint8_t err = 0;

	const uint8_t ucCmdList[] = {
		0xFF,	// Reset command    ->  Mouse: FA AA 00

		0xED,	// ����3��ָʾ��
		0x07,

		0xED,	// �ر�3��ָʾ��
		0x00,
	};

	g_tPS2.ksShift = 0;					/* 1��ʾ Shift �������� */
	g_tPS2.ksCtrl = 0;					/* 1��ʾ Ctrl �������� */
	g_tPS2.ksAlt = 0;					/* 1��ʾ Alt �������� */
	g_tPS2.ksCapsLock = 0;				/* 1��ʾ CapsLock ״ָ̬ʾ���� */
	g_tPS2.KsNumLock = 0;				/* 1��ʾ NumLock ״ָ̬ʾ���� */
	g_tPS2.KsScrollLock = 0;			/* 1��ʾ ScrollLock ״ָ̬ʾ���� */

	ps2_printf("Init Keyboard ...\r\n");
	for (i = 0; i < sizeof(ucCmdList); i++)
	{
		ps2_printf("Host : %02X",ucCmdList[i]);
		PS2_SendCmd(ucCmdList[i]);
		if (PS2_WaitMsg(&Rsp, 20))
		{
			ps2_printf("   KeyBoard : %X\r\n",Rsp);

			if (ucCmdList[i] == 0xFF)	/* ����Ǹ�λָ���ȵ�����ڲ��Լ죬Ȼ�󷵻�0xAA 00 */
			{
				ps2_printf("   KeyBoard resetting ...\r\n");
				if (PS2_WaitMsg(&Rsp, 1000))
				{
					ps2_printf("   KeyBoard : %X\r\n",Rsp);
				}
			}
		}
		else
		{
			ps2_printf("   KeyBoard : ��Ӧ��\r\n");
			err = 1;
		}
	}

	if (err == 1)
	{
		return 0;
	}
	return 1;
}

/*
*********************************************************************************************************
*	�� �� ��: PS2_InitMouse
*	����˵��: ��ʼ�����
*	��    ��: ��
*	�� �� ֵ: 0 ��ʾʧ�ܣ� 1��ʾ�ɹ�
*********************************************************************************************************
*/
uint8_t PS2_InitMouse(void)
{
	uint32_t Rsp;	/* ����������Ӧ������ */
	uint16_t i;
	const uint8_t ucCmdList[] = {
		0xFF,	// Reset command    ->  Mouse: FA AA 00

		0xF3,	// Set Sample Rate:Attempt to Enter Microsoft
				// Mouse: FA  Acknowledge : Scrolling Mouse mode

		0xC8,	// Decimal 200  ->  Mouse: FA  Acknowledge       :

		0xF3,	// Set Sample Rate -> Mouse: FA  Acknowledge

		0x64,	// decimal 100 -> Mouse: FA  Acknowledge       :
		0xF3,	// Set Sample Rate -> Mouse: FA  Acknowledge       :
		0x50,	// decimal 80 -> Mouse: FA  Acknowledge       :

		0xF2,	// Read Device Type ->	Mouse: FA  Acknowledge       :
									//  Mouse: 00  Mouse ID  Response 03 if microsoft scrolling mouse
		0xF3,	// Set Sample Rate ->	Mouse: FA  Acknowledge
		0x0A,	// decimal 10  -> Mouse: FA  Acknowledge
		0xF2,	// Read Device Type  ->  Mouse: FA  Acknowledge
				//						 Mouse: 00  Mouse ID

		0xE8,	// Set resolution ->  Mouse: FA  Acknowledge
		0x03,	// 8 Counts/mm  -> 	Mouse: FA  Acknowledge
		0xE6,	// Set Scaling 1:1  -> Mouse: FA  Acknowledge
		0xF3,	// Set Sample Rate  -> Mouse: FA  Acknowledge
		0x28,	// decimal 40  ->  Mouse: FA  Acknowledge
		0xF4,	// Enable  ->  Mouse: FA  Acknowledge
	};

	ps2_printf("Init Mouse ...\r\n");
	for (i = 0; i < sizeof(ucCmdList); i++)
	{
		ps2_printf("Host : %02X",ucCmdList[i]);
		PS2_SendCmd(ucCmdList[i]);
		if (PS2_WaitMsg(&Rsp, 20))
		{
			ps2_printf("   Mouse : %X\r\n",Rsp);

			if (ucCmdList[i] == 0xFF)	/* ����Ǹ�λָ���ȵ�����ڲ��Լ죬Ȼ�󷵻�0xAA 00 */
			{
				ps2_printf("   Mouse resetting ...\r\n");
				if (PS2_WaitMsg(&Rsp, 1000))
				{
					ps2_printf("   Mouse : %X\r\n",Rsp);
				}
			}
		}
		else
		{
			ps2_printf("   Mouse : ��Ӧ��\r\n");
		}
	}
	return 1;
}

/*
*********************************************************************************************************
*	�� �� ��: PS2_IsMousePacket
*	����˵��: �ж����ݰ��ǲ���λ�����ݰ�
*	��    ��: _input:4�ֽ�����
*	�� �� ֵ: 1 ��ʾ�����λ�����ݰ�  0 ��ʾ����
*********************************************************************************************************
*/
uint8_t PS2_IsMousePacket(uint32_t _input)
{
	/* ��귢�͵ĵ�1���ֽ� */
	if (_input & 0x08000000)
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
*	�� �� ��: PS2_DecodeMouse
*	����˵��: ����������ݰ�
*	��    ��: _input: ��귢�͵����ݰ���3�ֽڻ�4�ֽڣ�
*			  _res: ������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void PS2_DecodeMouse(uint32_t _input, MOUSE_PACKET_T *_res)
{
	uint32_t data;

	data = _input;
	if ((_input & 0xFF000000) == 0)		/* ��׼��꣬3�ֽڸ�ʽ */
	{
		data <<= 8;			/* ����1���ֽ�, ����ͳһ3�ֽ����ݰ���4�ֽ����ݰ����ý������ */
		_res->Intelli = 0;	/* 0��ʾ��׼��� */
	}
	else
	{
		_res->Intelli = 1;	/* 1��ʾ���������� */
	}

	/* ������꣬ 4�ֽڸ�ʽ */
		/*
		        Bit7       Bit6       Bit5      Bit4     Bit3      Bit 2      Bit 1      Bit 0
		Byte 1  Yoverflow  Xoverflow  YsignBit  XsignBit Always 1  MiddleBtn  RightBtn   LeftBtn
		Byte 2       X Movement
		Byte 3       Y Movement
		Byte 4  Always 0   Always 0   5th Btn   4th Btn  Z3        Z2         Z1         Z0
		*/
	/* ��׼��꣬3�ֽڸ�ʽ��ǰ3�ֽں���������ʽ��ͬ��û�е�4�ֽ� */


	/* Yֵ��� */
	if (data & 0x80000000)
	{
		_res->Yoverflow = 1;
	}
	else
	{
		_res->Yoverflow = 0;
	}

	/* Xֵ��� */
	if (data & 0x40000000)
	{
		_res->Xoverflow = 1;
	}
	else
	{
		_res->Xoverflow = 0;
	}

	/* ���X��λ�� */
	_res->Xmove = ((data >> 16) & 0xFF);
	if (data & 0x10000000)
	{
		_res->Xmove |= 0xFF00;	/* ��չ����λ */
	}

	/* ���Y��λ�� */
	_res->Ymove = ((data >> 8) & 0xFF);
	if (data & 0x20000000)
	{
		_res->Ymove |= 0xFF00;	/* ��չ����λ */
	}

	/* ����м� */
	if (data & 0x04000000)
	{
		_res->BtnMid = 1;
	}
	else
	{
		_res->BtnMid = 0;
	}

	/* ����Ҽ� */
	if (data & 0x02000000)
	{
		_res->BtnRight = 1;
	}
	else
	{
		_res->BtnRight = 0;
	}

	/* ������ */
	if (data & 0x01000000)
	{
		_res->BtnLeft = 1;
	}
	else
	{
		_res->BtnLeft = 0;
	}

	/*---------------- ����Ĵ����������������� ----------------*/

	/* ����5���� */
	if (data & 0x00000020)
	{
		_res->Btn5 = 1;
	}
	else
	{
		_res->Btn5 = 0;
	}

	/* ����4���� */
	if (data & 0x00000010)
	{
		_res->Btn4 = 1;
	}
	else
	{
		_res->Btn4 = 0;
	}

	/*
		�����֡���������������(1����ֱ��+1��ˮƽ��)��ꡣ
			+1 ��ʾ��ֱ�������Ϲ�����-1��ʾ��ֱ�������¹�����
			+2 ��ʾˮƽ�������ҹ�����-2��ʾˮƽ��������������
	*/
	_res->Zmove = (data & 0xF);
	if (data & 0x00000008)
	{
		_res->Zmove |= 0xFFF0;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: PS2_ISR
*	����˵��: �жϷ������
*	��    ��: _width ������ȣ���λ 10us
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void PS2_ISR(void)
{
	static uint8_t s_Byte;
	static uint8_t s_Pos;
	static uint8_t s_1Bits;
	uint8_t data;
	int32_t time;
	static int32_t s_last_time = 0;

	if (g_tPS2.Sending == 0)	/* �������ռ��̺�������ݵ�״̬ */
	{
		if (PS2_DATA_IS_HIGH())	/* ��ȡ PS2_DATA ���ߵĵ�ƽ */
		{
			data = 1;
		}
		else
		{
			data = 0;
		}

		/* ����Ĵ������ڳ�ʱͬ��. ���2���жϵ�ʱ��������2ms ��ͬ����ʼλ */
		time = bsp_GetRunTime();
		if (time - s_last_time > 2)
		{
			g_tPS2.Status = 0;
		}
		s_last_time = time;

		/* ����״̬�� */
		switch (g_tPS2.Status)
		{
			case 0:			/* ��ʼλ0 */
				if (data == 0)
				{
					g_tPS2.Status = 1;

					s_Pos = 0;
					s_Byte = 0;
					s_1Bits = 0;	/* �ֽ���1������ */
				}
				break;

			case 1:			/* 8λ����λ ��bit0 �ȴ��� */
				s_Byte >>= 1;
				if (data == 1)
				{
					s_Byte += 0x80;
					s_1Bits++;		/* ͳ��1�ĸ��� */
				}
				s_Pos++;
				if (s_Pos == 8)
				{
					g_tPS2.Status = 2;	/* ������һ��״̬����У��λ */
				}
				break;

			case 2:			/* ��У��λ */
				s_1Bits += data;
				if (s_1Bits % 2)
				{
					g_tPS2.Status = 3;	/* ��ȷ��������һ��״̬����ֹͣλ */
				}
				else
				{
					/* У�鲻��ȷ, �쳣 */
					g_tPS2.Status = 0;
				}
				break;

			case 3:			/* ����ֹͣλ */
				if (data == 1)
				{
					if (g_tPS2.Len < PS2_MAX_LEN)
					{
						g_tPS2.CodeBuf[g_tPS2.Len++] = s_Byte;
					}
					g_tPS2.RxTimeOut = 6;		/* 6ms ��ʱ�жϽ��� */
				}
				g_tPS2.Status = 0;	/* ������һ��bit */
				break;
		}
	}
	else		/* g_tPS2.Sending == 1, �������������״̬ */
	{
		/* �ȴ�Bit0 */
		switch (g_tPS2.Status)
		{
			case 0:					/* ��ʼλ */
				g_tPS2.TxTimeOut = 0;
			
				g_tPS2.Status = 2;
				s_1Bits = 0;
				s_Pos = 0;
				s_Byte = g_tPS2.Cmd;
				break;

			case 2:					/* ����8������λ */
				if (s_Byte & 0x01)
				{
					PS2_DATA_1();	/* ��Data������ */
					s_1Bits++;		/* ͳ��1�ĸ��� */
				}
				else
				{
					PS2_DATA_0();	/* ��Data������ */
				}
				s_Byte >>= 1;
				if (++s_Pos >= 8)
				{
					g_tPS2.Status = 3;
				}
				break;

			case 3:					/* ������У��λ */
				if (s_1Bits % 2)
				{
					PS2_DATA_0();	/* ��Data������ */
				}
				else
				{
					PS2_DATA_1();	/* ��Data������ */
				}
				g_tPS2.Status = 4;
				break;

			case 4:					/* ����ֹͣλ */
				PS2_DATA_1();	/* ��Data������ */
				g_tPS2.Status = 5;
				break;

			case 5:					/* �豸��ʱ������ DATA�� ��ʾACKӦ���ź� */
				if (PS2_DATA_IS_HIGH())	/* ��ȡ PS2_DATA ���ߵĵ�ƽ */
				{
					g_tPS2.Ack = 1;
				}
				else
				{
					g_tPS2.Ack = 0;
				}

				/* �������״̬ */
				g_tPS2.Sending = 0;
				g_tPS2.Status = 0;
				break;
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: EXTI9_5_IRQHandler
*	����˵��: �ⲿ�жϷ������.
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
#ifndef EXTI9_5_ISR_MOVE_OUT		/* bsp.h �ж�����У���ʾ�������Ƶ� stam32f4xx_it.c�� �����ظ����� */
void BUSY_IRQHandler(void)
{
	HAL_GPIO_EXTI_IRQHandler(PIN_PS2_CLK);
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
	if (GPIO_Pin == PIN_PS2_CLK)
	{
		PS2_ISR();
	}
}
#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
