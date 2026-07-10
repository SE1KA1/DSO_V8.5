/**
 * @file bsp_tft_lcd.c
 * @brief 安富莱 BSP 通用驱动（TFT LCD 通用）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : TFTҺ����ʾ������ģ��
*	�ļ����� : bsp_tft_lcd.c
*	��    �� : V4.2
*	˵    �� : ֧��3.0�� 3.5�� 4.3�� 5.0�� 7.0����ʾģ��.
*			  3.0���֧�ֵ�LCD�ڲ�����оƬ�ͺ���: SPFD5420A��OTM4001A��R61509V
*	�޸ļ�¼ :
*		�汾��  ����       ����    ˵��
*		v1.0    2011-08-21 armfly  ST�̼���汾 V3.5.0�汾��
*					a) ȡ�����ʼĴ����Ľṹ�壬ֱ�Ӷ���
*		V2.0    2011-10-16 armfly  ����R61509V������ʵ��ͼ����ʾ����
*		V2.1    2012-07-06 armfly  ����RA8875������֧��4.3����
*		V2.2    2012-07-13 armfly  �Ľ�LCD_DispStr������֧��12�����ַ�;�޸�LCD_DrawRect,�����һ����������
*		V2.3    2012-08-08 armfly  ���ײ�оƬ�Ĵ���������صĺ����ŵ��������ļ���֧��RA8875
*   	V3.0    2013-05-20 ����ͼ��ṹ; �޸�	LCD_DrawIconActive  �޸�DispStr����֧���ı�͸��
*		V3.1    2013-06-12 ���LCD_DispStr()����BUG�������Ƕ�ֿ��к��ָ�������256���������ѭ����
*		V3.2    2013-06-28 ����Label�ؼ�, ����ʾ�ַ�����֮ǰ��ʱ���Զ�������������
*		V3.3    2013-06-29 FSMC��ʼ��ʱ������ʱ��дʱ��Ͷ�ʱ��ֿ����á� LCD_FSMCConfig ������
*		V3.4    2013-07-06 ������ʾ32λ��Alphaͼ��ĺ��� LCD_DrawIcon32
*		V3.5    2013-07-24 ������ʾ32λ��AlphaͼƬ�ĺ��� LCD_DrawBmp32
*		V3.6    2013-07-30 �޸� DispEdit() ֧��12�����ֶ���
*		V3.7    2014-09-06 �޸� LCD_InitHard() ͬʱ֧�� RA8875-SPI�ӿں�8080�ӿ�
*		V3.8    2014-09-15 �������ɺ���:
*					��1�� LCD_DispStrEx() �����Զ������Զ���׵���ʾ�ַ�������
*					��2�� LCD_GetStrWidth() �����ַ��������ؿ���
*		V3.9    2014-10-18
*					(1) ���� LCD_ButtonTouchDown() LCD_ButtonTouchRelease �жϴ������겢�ػ水ť
*					(2) ����3.5��LCD����
*					(3) ���� LCD_SetDirection() ������������ʾ�����򣨺��� ������̬�л���
*		V4.0   2015-04-04 
*				(1) ��ť���༭��ؼ�����RA8875���壬��Ƕ�ֿ��RA8875�ֿ�ͳһ���롣����������� 
*				    FC_RA8875_16, FC_RA8875_24,	FC_RA8875_32
*				(2) FONT_T�ṹ���ԱFontCode�������� uint16_t �޸�Ϊ FONT_CODE_Eö�٣����ڱ��������;
*				(3) �޸� LCD_DispStrEx(), �����������������������_LCD_ReadAsciiDot(), _LCD_ReadHZDot()
*				(4) LCD_DispStr() �����򻯣�ֱ�ӵ��� LCD_DispStrEx() ʵ�֡�
*				(5) LCD_DispStrEx() ����֧�� RA8875���塣
*				(6) LCD_ButtonTouchDown() ���Ӱ�����ʾ��
*		V4.1   2015-04-18 
*				(1) ����RA885 ASCII����Ŀ��ȱ���LCD_DispStrEx() ��������֧��RA8875 ASCII�䳤���ȼ��㡣
*				(2) ���� LCD_HardReset(��������֧��LCD��λ��GPIO���ƵĲ�Ʒ��STM32-V5 ����ҪGPIO���ơ�
*		V4.2   2015-07-23
*				(1) ���Ӻ���LCD_InitButton()
*				(2) h�ļ���ʹ�ܰ�����ʾ�� #define BUTTON_BEEP()	BEEP_KeyTone();
*
*	Copyright (C), 2015-2016, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"
#include "fonts.h"

/* ����3����������Ҫ����ʹ����ͬʱ֧�ֲ�ͬ���� */
uint16_t g_LcdHeight = 240;			/* ��ʾ���ֱ���-�߶� */
uint16_t g_LcdWidth = 400;			/* ��ʾ���ֱ���-���� */
uint8_t s_ucBright;					/* �������Ȳ��� */
uint8_t g_LcdDirection = 0;				/* ��ʾ����.0��1��2��3 */

static void LCD_HardReset(void);
/*
*********************************************************************************************************
*   函 数 名: LCD_SetPwmBackLight
*   功能说明: LCD_SetPwmBackLight 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void LCD_SetPwmBackLight(uint8_t _bright);

/*
*********************************************************************************************************
*	�� �� ��: LCD_InitHard
*	����˵��: ��ʼ��LCD
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_InitHard(void)
{
	LCD_HardReset();	/* Ӳ����λ ��STM32-V5, V6 ���裩���������GPIO����LCD��λ�Ĳ�Ʒ */

	LCDH7_InitHard();
	
	LCD_SetDirection(0);

	LCD_ClrScr(CL_BLACK);	/* ��������ʾȫ�� */

//	LCD_SetBackLight(BRIGHT_DEFAULT);	 /* �򿪱��⣬����Ϊȱʡ���� */
}


/*
*********************************************************************************************************
*	�� �� ��: LCD_SetPwmBackLight
*	����˵��: ��ʼ������LCD�������GPIO,����ΪPWMģʽ��
*			���رձ���ʱ����CPU IO����Ϊ��������ģʽ���Ƽ�����Ϊ������������������͵�ƽ)����TIM3�ر� ʡ��
*	��    ��:  _bright ���ȣ�0����255������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void LCD_SetPwmBackLight(uint8_t _bright)
{
	/* ������CPU���PWM���ƣ�PA0/TIM5_CH1/TIM2_CH1 */
	//bsp_SetTIMOutPWM(GPIOA, GPIO_PIN_0, TIM5, 1, 100, (_bright * 10000) /255);
	//bsp_SetTIMOutPWM(GPIOA, GPIO_PIN_0, TIM5, 1, 20000, (_bright * 10000) /255);
	bsp_SetTIMOutPWM(GPIOA, GPIO_PIN_8, TIM1, 1, 20000, (_bright * 10000) /255);
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_SetBackLight
*	����˵��: ��ʼ������LCD�������GPIO,����ΪPWMģʽ��
*			���رձ���ʱ����CPU IO����Ϊ��������ģʽ���Ƽ�����Ϊ������������������͵�ƽ)����TIM3�ر� ʡ��
*	��    ��: _bright ���ȣ�0����255������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_SetBackLight(uint8_t _bright)
{
	s_ucBright =  _bright;	/* ���汳��ֵ */

	LCD_SetPwmBackLight(s_ucBright);
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_GetBackLight
*	����˵��: ��ñ������Ȳ���
*	��    ��: ��
*	�� �� ֵ: �������Ȳ���
*********************************************************************************************************
*/
uint8_t LCD_GetBackLight(void)
{
	return s_ucBright;
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_HardReset
*	����˵��: Ӳ����λ. ��Ը�λ������GPIO���ƵĲ�Ʒ��
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_HardReset(void)
{
#if 0	
	GPIO_InitTypeDef GPIO_InitStructure;

	/* ʹ�� GPIOʱ�� */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	
	/* ���ñ���GPIOΪ�������ģʽ */
	GPIO_InitStructure.GPIO_Pin = GPIO_PIN_1;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_ResetBits(GPIOB, GPIO_PIN_1);
	bsp_DelayMS(20);
	GPIO_SetBits(GPIOB, GPIO_PIN_1);
#endif	
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_GetChipDescribe
*	����˵��: ��ȡLCD����оƬ���������ţ�������ʾ
*	��    ��: char *_str : �������ַ�������˻�����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_GetChipDescribe(char *_str)
{	
	LCDH7_GetChipDescribe(_str);
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_GetHeight
*	����˵��: ��ȡLCD�ֱ���֮�߶�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
uint16_t LCD_GetHeight(void)
{
	return g_LcdHeight;
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_GetWidth
*	����˵��: ��ȡLCD�ֱ���֮����
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
uint16_t LCD_GetWidth(void)
{
	return g_LcdWidth;
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_DispOn
*	����˵��: ����ʾ
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_DispOn(void)
{
	;
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_DispOff
*	����˵��: �ر���ʾ
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_DispOff(void)
{
	;
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_ClrScr
*	����˵��: �����������ɫֵ����
*	��    ��: _usColor : ����ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_ClrScr(uint16_t _usColor)
{
	LCDH7_ClrScr(_usColor);
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_DispStr
*	����˵��: ��LCDָ�����꣨���Ͻǣ���ʾһ���ַ���
*	��    ��:
*		_usX : X����
*		_usY : Y����
*		_ptr  : �ַ���ָ��
*		_tFont : ����ṹ�壬������ɫ������ɫ(֧��͸��)��������롢���ּ��Ȳ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_DispStr(uint16_t _usX, uint16_t _usY, char *_ptr, FONT_T *_tFont)
{
	LCD_DispStrEx(_usX, _usY, _ptr, _tFont, 0, 0);
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_GetFontWidth
*	����˵��: ��ȡ����Ŀ��ȣ����ص�λ)
*	��    ��:
*		_tFont : ����ṹ�壬������ɫ������ɫ(֧��͸��)��������롢���ּ��Ȳ���
*	�� �� ֵ: ����Ŀ��ȣ����ص�λ)
*********************************************************************************************************
*/
uint16_t LCD_GetFontWidth(FONT_T *_tFont)
{
	uint16_t font_width = 16;

	switch (_tFont->FontCode)
	{
		case FC_ST_12:
			font_width = 12;
			break;

		case FC_ST_16:
		case FC_RA8875_16:			
			font_width = 16;
			break;
			
		case FC_RA8875_24:			
		case FC_ST_24:
			font_width = 24;
			break;
			
		case FC_ST_32:
		case FC_RA8875_32:	
			font_width = 32;
			break;			
	}
	return font_width;
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_GetFontHeight
*	����˵��: ��ȡ����ĸ߶ȣ����ص�λ)
*	��    ��:
*		_tFont : ����ṹ�壬������ɫ������ɫ(֧��͸��)��������롢���ּ��Ȳ���
*	�� �� ֵ: ����Ŀ��ȣ����ص�λ)
*********************************************************************************************************
*/
uint16_t LCD_GetFontHeight(FONT_T *_tFont)
{
	uint16_t height = 16;

	switch (_tFont->FontCode)
	{
		case FC_ST_12:
			height = 12;
			break;

		case FC_ST_16:
		case FC_RA8875_16:			
			height = 16;
			break;
			
		case FC_RA8875_24:			
		case FC_ST_24:
			height = 24;
			break;
			
		case FC_ST_32:
		case FC_RA8875_32:	
			height = 32;
			break;			
	}
	return height;
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_GetStrWidth
*	����˵��: �����ַ�������(���ص�λ)
*	��    ��:
*		_ptr  : �ַ���ָ��
*		_tFont : ����ṹ�壬������ɫ������ɫ(֧��͸��)��������롢���ּ��Ȳ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
uint16_t LCD_GetStrWidth(char *_ptr, FONT_T *_tFont)
{
	char *p = _ptr;
	uint16_t width = 0;
	uint8_t code1, code2;
	uint16_t font_width;

	font_width = LCD_GetFontWidth(_tFont);

	while (*p != 0)
	{
		code1 = *p;	/* ��ȡ�ַ������ݣ� �����ݿ�����ascii���룬Ҳ���ܺ��ִ���ĸ��ֽ� */
		if (code1 < 0x80)	/* ASCII */
		{
			switch(_tFont->FontCode)
			{
				case FC_RA8875_16:
					font_width = g_RA8875_Ascii16_width[code1 - 0x20];
					break;
				
				case FC_RA8875_24:
					font_width = g_RA8875_Ascii24_width[code1 - 0x20];
					break;
				
				case FC_RA8875_32:
					font_width = g_RA8875_Ascii32_width[code1 - 0x20];
					break;
				
				case FC_ST_12:
					font_width = 6;
					break;

				case FC_ST_16:		
					font_width = 8;
					break;
					
				case FC_ST_24:			
					font_width = 12;
					break;
					
				case FC_ST_32:
					font_width = 16;
					break;
				
				default:
					font_width = 8;
					break;					
			}
			
		}
		else	/* ���� */
		{
			code2 = *++p;
			if (code2 == 0)
			{
				break;
			}
			font_width = LCD_GetFontWidth(_tFont);
			
		}
		width += font_width;
		p++;
	}

	return width;
}

/*
*********************************************************************************************************
*	�� �� ��: _LCD_ReadAsciiDot
*	����˵��: ��ȡ1��ASCII�ַ��ĵ�������
*	��    ��:
*		_code : ASCII�ַ��ı��룬1�ֽڡ�1-128
*		_fontcode ���������
*		_pBuf : ��Ŷ������ַ���������
*	�� �� ֵ: ���ֿ���
*********************************************************************************************************
*/
static void _LCD_ReadAsciiDot(uint8_t _code, uint8_t _fontcode, uint8_t *_pBuf)
{
	const uint8_t *pAscDot;
	uint8_t font_bytes = 0;

	pAscDot = 0;
	switch (_fontcode)
	{
		case FC_ST_12:		/* 12���� */
			font_bytes = 24;
			pAscDot = g_Ascii12;	
			break;
		
		case FC_ST_24:
		case FC_ST_32:
		case FC_ST_16:
			/* ȱʡ��16���� */
			font_bytes = 32;
			pAscDot = g_Ascii16;
			break;
		
		case FC_RA8875_16:
		case FC_RA8875_24:
		case FC_RA8875_32:
			return;
	}	

	/* ��CPU�ڲ�Flash�е�ascii�ַ������Ƶ�buf */
	memcpy(_pBuf, &pAscDot[_code * (font_bytes / 2)], (font_bytes / 2));	
}

/*
*********************************************************************************************************
*	�� �� ��: _LCD_ReadHZDot
*	����˵��: ��ȡ1�����ֵĵ�������
*	��    ��:
*		_code1, _cod2 : ��������. GB2312����
*		_fontcode ���������
*		_pBuf : ��Ŷ������ַ���������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void _LCD_ReadHZDot(uint8_t _code1, uint8_t _code2,  uint8_t _fontcode, uint8_t *_pBuf)
{
	#ifdef USE_SMALL_FONT	/* ʹ��CPU �ڲ�Flash С�ֿ� */
		uint8_t *pDot;
		uint8_t font_bytes = 0;
		uint32_t address;
		uint16_t m;

		pDot = 0;	/* �������ڱ���澯 */
		switch (_fontcode)
		{
			case FC_ST_12:		/* 12���� */
				font_bytes = 24;
				pDot = (uint8_t *)g_Hz12;	
				break;
			
			case FC_ST_16:
				font_bytes = 32;
				pDot = (uint8_t *)g_Hz16;
				break;
	
			case FC_ST_24:
				font_bytes = 72;
				pDot = (uint8_t *)g_Hz24;
				break;			
				
			case FC_ST_32:	
				font_bytes = 128;
				pDot = (uint8_t *)g_Hz32;
				break;						
			
			case FC_RA8875_16:
			case FC_RA8875_24:
			case FC_RA8875_32:
				return;
		}	

		m = 0;
		while(1)
		{
			address = m * (font_bytes + 2);
			m++;
			if ((_code1 == pDot[address + 0]) && (_code2 == pDot[address + 1]))
			{
				address += 2;
				memcpy(_pBuf, &pDot[address], font_bytes);
				break;
			}
			else if ((pDot[address + 0] == 0xFF) && (pDot[address + 1] == 0xFF))
			{
				/* �ֿ�������ϣ�δ�ҵ��������ȫFF */
				memset(_pBuf, 0xFF, font_bytes);
				break;
			}
		}
	#else	/* ��ȫ�ֿ� */
		uint8_t *pDot = 0;
		uint8_t font_bytes = 0;
			
		switch (_fontcode)
		{
			case FC_ST_12:		/* 12���� */
				font_bytes = 24;
				pDot = (uint8_t *)HZK12_ADDR;	
				break;
			
			case FC_ST_16:
				font_bytes = 32;
				pDot = (uint8_t *)HZK16_ADDR;
				break;
	
			case FC_ST_24:
				font_bytes = 72;
				pDot = (uint8_t *)HZK24_ADDR;
				break;			
				
			case FC_ST_32:	
				font_bytes = 128;
				pDot = (uint8_t *)HZK32_ADDR;
				break;						
			
			case FC_RA8875_16:
			case FC_RA8875_24:
			case FC_RA8875_32:
				return;
		}			
	
		/* �˴���Ҫ�����ֿ��ļ����λ�ý����޸� */
		if (_code1 >=0xA1 && _code1 <= 0xA9 && _code2 >=0xA1)
		{
			pDot += ((_code1 - 0xA1) * 94 + (_code2 - 0xA1)) * font_bytes;
		}
		else if (_code1 >=0xB0 && _code1 <= 0xF7 && _code2 >=0xA1)
		{
			pDot += ((_code1 - 0xB0) * 94 + (_code2 - 0xA1) + 846) * font_bytes;
		}
		memcpy(_pBuf, pDot, font_bytes);
	#endif
}
			
/*
*********************************************************************************************************
*	�� �� ��: LCD_DispStrEx
*	����˵��: ��LCDָ�����꣨���Ͻǣ���ʾһ���ַ����� ��ǿ�ͺ�����֧����\��\�Ҷ��룬֧�ֶ���������
*	��    ��:
*		_usX : X����
*		_usY : Y����
*		_ptr  : �ַ���ָ��
*		_tFont : ����ṹ�壬������ɫ������ɫ(֧��͸��)��������롢���ּ��Ȳ���������ָ��RA8875�ֿ���ʾ����
*		_Width : �ַ�����ʾ����Ŀ���. 0 ��ʾ�������������򣬴�ʱ_Align��Ч
*		_Align :�ַ�������ʾ����Ķ��뷽ʽ��
*				ALIGN_LEFT = 0,
*				ALIGN_CENTER = 1,
*				ALIGN_RIGHT = 2
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_DispStrEx(uint16_t _usX, uint16_t _usY, char *_ptr, FONT_T *_tFont, uint16_t _Width,
	uint8_t _Align)
{
	uint32_t i;
	uint8_t code1;
	uint8_t code2;
	uint8_t buf[32 * 32 / 8];	/* ���֧��24������ */
	uint8_t width;
	uint16_t m;
	uint8_t font_width = 0;
	uint8_t font_height = 0;
	uint16_t x, y;
	uint16_t offset;
	uint16_t str_width;	/* �ַ���ʵ�ʿ���  */

	switch (_tFont->FontCode)
	{
		case FC_ST_12:		/* 12���� */
			font_height = 12;
			font_width = 12;
			break;
		
		case FC_ST_16:
			font_height = 16;
			font_width = 16;
			break;

		case FC_ST_24:
			font_height = 24;
			font_width = 24;
			break;
						
		case FC_ST_32:	
			font_height = 32;
			font_width = 32;
			break;					
	}
	
	str_width = LCD_GetStrWidth(_ptr, _tFont);	/* �����ַ���ʵ�ʿ���(RA8875�ڲ�ASCII�������Ϊ�䳤 */
	offset = 0;
	if (_Width > str_width)
	{
		if (_Align == ALIGN_RIGHT)	/* �Ҷ��� */
		{
			offset = _Width - str_width;
		}
		else if (_Align == ALIGN_CENTER)	/* ����� */
		{
			offset = (_Width - str_width) / 2;
		}
		else	/* ����� ALIGN_LEFT */
		{
			;
		}
	}

	/* ������ɫ, �м������ұ߶���  */
	if (offset > 0)
	{
		LCD_Fill_Rect(_usX, _usY, LCD_GetFontHeight(_tFont), offset,  _tFont->BackColor);
		_usX += offset;
	}
	
	/* �Ҳ����ɫ */
	if (_Width > str_width)
	{
		LCD_Fill_Rect(_usX + str_width, _usY, LCD_GetFontHeight(_tFont), _Width - str_width - offset,  _tFont->BackColor);
	}
	
	/* ʹ��CPU�ڲ��ֿ�. ������Ϣ��CPU��ȡ */
	{
		/* ��ʼѭ�������ַ� */
		while (*_ptr != 0)
		{
			code1 = *_ptr;	/* ��ȡ�ַ������ݣ� �����ݿ�����ascii���룬Ҳ���ܺ��ִ���ĸ��ֽ� */
			if (code1 < 0x80)
			{
				/* ��ascii�ַ������Ƶ�buf */
				//memcpy(buf, &pAscDot[code1 * (font_bytes / 2)], (font_bytes / 2));
				_LCD_ReadAsciiDot(code1, _tFont->FontCode, buf);	/* ��ȡASCII�ַ����� */
				width = font_width / 2;
			}
			else
			{
				code2 = *++_ptr;
				if (code2 == 0)
				{
					break;
				}
				/* ��1�����ֵĵ��� */
				_LCD_ReadHZDot(code1, code2, _tFont->FontCode, buf);
				width = font_width;
			}
	
			y = _usY;
			/* ��ʼˢLCD */
			for (m = 0; m < font_height; m++)	/* �ַ��߶� */
			{
				x = _usX;
				for (i = 0; i < width; i++)	/* �ַ����� */
				{
					if ((buf[m * ((2 * width) / font_width) + i / 8] & (0x80 >> (i % 8 ))) != 0x00)
					{
						LCD_PutPixel(x, y, _tFont->FrontColor);	/* ����������ɫΪ����ɫ */
					}
					else
					{
						if (_tFont->BackColor != CL_MASK)	/* ͸��ɫ */
						{
							LCD_PutPixel(x, y, _tFont->BackColor);	/* ����������ɫΪ���ֱ���ɫ */
						}
					}
	
					x++;
				}
				y++;
			}
	
			if (_tFont->Space > 0)
			{
				/* ������ֵ�ɫ��_tFont->usBackColor�������ּ����ڵ���Ŀ��ȣ���ô��Ҫ������֮�����(��ʱδʵ��) */
			}
			_usX += width + _tFont->Space;	/* �е�ַ���� */
			_ptr++;			/* ָ����һ���ַ� */
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_PutPixel
*	����˵��: ��1������
*	��    ��:
*			_usX,_usY : ��������
*			_usColor  : ������ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_PutPixel(uint16_t _usX, uint16_t _usY, uint16_t _usColor)
{
	LCDH7_PutPixel(_usX, _usY, _usColor);
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_GetPixel
*	����˵��: ��ȡ1������
*	��    ��:
*			_usX,_usY : ��������
*			_usColor  : ������ɫ
*	�� �� ֵ: RGB��ɫֵ
*********************************************************************************************************
*/
uint16_t LCD_GetPixel(uint16_t _usX, uint16_t _usY)
{
	uint16_t usRGB;

	usRGB = LCDH7_GetPixel(_usX, _usY);
	return usRGB;
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_DrawLine
*	����˵��: ���� Bresenham �㷨����2��仭һ��ֱ�ߡ�
*	��    ��:
*			_usX1, _usY1 : ��ʼ������
*			_usX2, _usY2 : ��ֹ��Y����
*			_usColor     : ��ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_DrawLine(uint16_t _usX1 , uint16_t _usY1 , uint16_t _usX2 , uint16_t _usY2 , uint16_t _usColor)
{
	LCDH7_DrawLine(_usX1 , _usY1 , _usX2, _usY2 , _usColor);
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_DrawPoints
*	����˵��: ���� Bresenham �㷨������һ��㣬������Щ�����������������ڲ�����ʾ��
*	��    ��:
*			x, y     : ��������
*			_usColor : ��ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_DrawPoints(uint16_t *x, uint16_t *y, uint16_t _usSize, uint16_t _usColor)
{
	uint16_t i;

	for (i = 0 ; i < _usSize - 1; i++)
	{
		LCD_DrawLine(x[i], y[i], x[i + 1], y[i + 1], _usColor);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_DrawRect
*	����˵��: ����ˮƽ���õľ��Ρ�
*	��    ��:
*			_usX,_usY: �������Ͻǵ�����
*			_usHeight : ���εĸ߶�
*			_usWidth  : ���εĿ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_DrawRect(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor)
{
	LCDH7_DrawRect(_usX, _usY, _usHeight, _usWidth, _usColor);
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_Fill_Rect
*	����˵��: ��һ����ɫֵ���һ�����Ρ���emWin ����ͬ������ LCD_FillRect����˼����»������֡�
*	��    ��:
*			_usX,_usY: �������Ͻǵ�����
*			_usHeight : ���εĸ߶�
*			_usWidth  : ���εĿ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_Fill_Rect(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor)
{
	LCDH7_FillRect(_usX, _usY, _usHeight, _usWidth, _usColor);
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_DrawCircle
*	����˵��: ����һ��Բ���ʿ�Ϊ1������
*	��    ��:
*			_usX,_usY  : Բ�ĵ�����
*			_usRadius  : Բ�İ뾶
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_DrawCircle(uint16_t _usX, uint16_t _usY, uint16_t _usRadius, uint16_t _usColor)
{
	LCDH7_DrawCircle(_usX, _usY, _usRadius, _usColor);
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_DrawBMP
*	����˵��: ��LCD����ʾһ��BMPλͼ��λͼ����ɨ�����: �����ң����ϵ���
*	��    ��:
*			_usX, _usY : ͼƬ������
*			_usHeight  : ͼƬ�߶�
*			_usWidth   : ͼƬ����
*			_ptr       : ͼƬ����ָ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_DrawBMP(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t *_ptr)
{
	LCDH7_DrawBMP(_usX, _usY, _usHeight, _usWidth, _ptr);
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_DrawWin
*	����˵��: ��LCD�ϻ���һ������
*	��    ��: �ṹ��ָ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_DrawWin(WIN_T *_pWin)
{
	uint16_t TitleHegiht;

	TitleHegiht = 20;

	/* ���ƴ������ */
	LCD_DrawRect(_pWin->Left, _pWin->Top, _pWin->Height, _pWin->Width, WIN_BORDER_COLOR);
	LCD_DrawRect(_pWin->Left + 1, _pWin->Top + 1, _pWin->Height - 2, _pWin->Width - 2, WIN_BORDER_COLOR);

	/* ���ڱ����� */
	LCD_Fill_Rect(_pWin->Left + 2, _pWin->Top + 2, TitleHegiht, _pWin->Width - 4, WIN_TITLE_COLOR);

	/* ������� */
	LCD_Fill_Rect(_pWin->Left + 2, _pWin->Top + TitleHegiht + 2, _pWin->Height - 4 - TitleHegiht,
		_pWin->Width - 4, WIN_BODY_COLOR);

	LCD_DispStr(_pWin->Left + 3, _pWin->Top + 2, _pWin->pCaption, _pWin->Font);
}


/*
*********************************************************************************************************
*	�� �� ��: LCD_DrawIcon
*	����˵��: ��LCD�ϻ���һ��ͼ�꣬�Ľ��Զ���Ϊ����
*	��    ��: _pIcon : ͼ��ṹ
*			  _tFont : ��������
*			  _ucFocusMode : ����ģʽ��0 ��ʾ����ͼ��  1��ʾѡ�е�ͼ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_DrawIcon(const ICON_T *_tIcon, FONT_T *_tFont, uint8_t _ucFocusMode)
{
	const uint16_t *p;
	uint16_t usNewRGB;
	uint16_t x, y;		/* ���ڼ�¼�����ڵ�������� */

	p = _tIcon->pBmp;
	for (y = 0; y < _tIcon->Height; y++)
	{
		for (x = 0; x < _tIcon->Width; x++)
		{
			usNewRGB = *p++;	/* ��ȡͼ�����ɫֵ��ָ���1 */
			/* ��ͼ���4��ֱ���и�Ϊ���ǣ��������Ǳ���ͼ�� */
			if ((y == 0 && (x < 6 || x > _tIcon->Width - 7)) ||
				(y == 1 && (x < 4 || x > _tIcon->Width - 5)) ||
				(y == 2 && (x < 3 || x > _tIcon->Width - 4)) ||
				(y == 3 && (x < 2 || x > _tIcon->Width - 3)) ||
				(y == 4 && (x < 1 || x > _tIcon->Width - 2)) ||
				(y == 5 && (x < 1 || x > _tIcon->Width - 2))	||

				(y == _tIcon->Height - 1 && (x < 6 || x > _tIcon->Width - 7)) ||
				(y == _tIcon->Height - 2 && (x < 4 || x > _tIcon->Width - 5)) ||
				(y == _tIcon->Height - 3 && (x < 3 || x > _tIcon->Width - 4)) ||
				(y == _tIcon->Height - 4 && (x < 2 || x > _tIcon->Width - 3)) ||
				(y == _tIcon->Height - 5 && (x < 1 || x > _tIcon->Width - 2)) ||
				(y == _tIcon->Height - 6 && (x < 1 || x > _tIcon->Width - 2))
				)
			{
				;
			}
			else
			{
				if (_ucFocusMode != 0)	/* 1��ʾѡ�е�ͼ�� */
				{
					/* ����ԭʼ���ص����ȣ�ʵ��ͼ�걻����ѡ�е�Ч�� */
					uint16_t R,G,B;
					uint16_t bright = 15;

					/* rrrr rggg gggb bbbb */
					R = (usNewRGB & 0xF800) >> 11;
					G = (usNewRGB & 0x07E0) >> 5;
					B =  usNewRGB & 0x001F;
					if (R > bright)
					{
						R -= bright;
					}
					else
					{
						R = 0;
					}
					if (G > 2 * bright)
					{
						G -= 2 * bright;
					}
					else
					{
						G = 0;
					}
					if (B > bright)
					{
						B -= bright;
					}
					else
					{
						B = 0;
					}
					usNewRGB = (R << 11) + (G << 5) + B;
				}

				LCD_PutPixel(x + _tIcon->Left, y + _tIcon->Top, usNewRGB);
			}
		}
	}

	/* ����ͼ���µ����� */
	{
		uint16_t len;
		uint16_t width;

		len = strlen(_tIcon->Text);

		if  (len == 0)
		{
			return;	/* ���ͼ���ı�����Ϊ0������ʾ */
		}

		/* �����ı����ܿ��� */
		if (_tFont->FontCode == FC_ST_12)		/* 12���� */
		{
			width = 6 * (len + _tFont->Space);
		}
		else	/* FC_ST_16 */
		{
			width = 8 * (len + _tFont->Space);
		}


		/* ˮƽ���� */
		x = (_tIcon->Left + _tIcon->Width / 2) - width / 2;
		y = _tIcon->Top + _tIcon->Height + 2;
		LCD_DispStr(x, y, (char *)_tIcon->Text, _tFont);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_Blend565
*	����˵��: ������͸���� ��ɫ���
*	��    ��: src : ԭʼ����
*			  dst : ��ϵ���ɫ
*			  alpha : ͸���� 0-32
*	�� �� ֵ: ��
*********************************************************************************************************
*/
uint16_t LCD_Blend565(uint16_t src, uint16_t dst, uint8_t alpha)
{
	uint32_t src2;
	uint32_t dst2;

	src2 = ((src << 16) |src) & 0x07E0F81F;
	dst2 = ((dst << 16) | dst) & 0x07E0F81F;
	dst2 = ((((dst2 - src2) * alpha) >> 5) + src2) & 0x07E0F81F;
	return (dst2 >> 16) | dst2;
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_DrawIcon32
*	����˵��: ��LCD�ϻ���һ��ͼ��, ����͸����Ϣ��λͼ(32λ�� RGBA). ͼ���´�����
*	��    ��: _pIcon : ͼ��ṹ
*			  _tFont : ��������
*			  _ucFocusMode : ����ģʽ��0 ��ʾ����ͼ��  1��ʾѡ�е�ͼ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_DrawIcon32(const ICON_T *_tIcon, FONT_T *_tFont, uint8_t _ucFocusMode)
{
	const uint8_t *p;
	uint16_t usOldRGB, usNewRGB;
	int16_t x, y;		/* ���ڼ�¼�����ڵ�������� */
	uint8_t R1,G1,B1,A;	/* ������ɫ�ʷ��� */
	uint8_t R0,G0,B0;	/* ������ɫ�ʷ��� */

	p = (const uint8_t *)_tIcon->pBmp;
	p += 54;		/* ֱ��ָ��ͼ�������� */

	/* ����BMPλͼ���򣬴������ң���������ɨ�� */
	for (y = _tIcon->Height - 1; y >= 0; y--)
	{
		for (x = 0; x < _tIcon->Width; x++)
		{
			B1 = *p++;
			G1 = *p++;
			R1 = *p++;
			A = *p++;	/* Alpha ֵ(͸����)��0-255, 0��ʾ͸����1��ʾ��͸��, �м�ֵ��ʾ͸���� */

			if (A == 0x00)	/* ��Ҫ͸��,��ʾ���� */
			{
				;	/* ����ˢ�±��� */
			}
			else if (A == 0xFF)	/* ��ȫ��͸���� ��ʾ������ */
			{
				usNewRGB = RGB(R1, G1, B1);
				if (_ucFocusMode == 1)
				{
					usNewRGB = LCD_Blend565(usNewRGB, CL_YELLOW, 10);
				}
				LCD_PutPixel(x + _tIcon->Left, y + _tIcon->Top, usNewRGB);
			}
			else 	/* ��͸�� */
			{
				/* ���㹫ʽ�� ʵ����ʾ��ɫ = ǰ����ɫ * Alpha / 255 + ������ɫ * (255-Alpha) / 255 */
				usOldRGB = LCD_GetPixel(x + _tIcon->Left, y + _tIcon->Top);
				//usOldRGB = 0xFFFF;
				R0 = RGB565_R(usOldRGB);
				G0 = RGB565_G(usOldRGB);
				B0 = RGB565_B(usOldRGB);

				R1 = (R1 * A) / 255 + R0 * (255 - A) / 255;
				G1 = (G1 * A) / 255 + G0 * (255 - A) / 255;
				B1 = (B1 * A) / 255 + B0 * (255 - A) / 255;
				usNewRGB = RGB(R1, G1, B1);
				if (_ucFocusMode == 1)
				{
					usNewRGB = LCD_Blend565(usNewRGB, CL_YELLOW, 10);
				}
				LCD_PutPixel(x + _tIcon->Left, y + _tIcon->Top, usNewRGB);
			}
		}
	}

	/* ����ͼ���µ����� */
	{
		uint16_t len;
		uint16_t width;

		len = strlen(_tIcon->Text);

		if  (len == 0)
		{
			return;	/* ���ͼ���ı�����Ϊ0������ʾ */
		}

		/* �����ı����ܿ��� */
		if (_tFont->FontCode == FC_ST_12)		/* 12���� */
		{
			width = 6 * (len + _tFont->Space);
		}
		else	/* FC_ST_16 */
		{
			width = 8 * (len + _tFont->Space);
		}


		/* ˮƽ���� */
		x = (_tIcon->Left + _tIcon->Width / 2) - width / 2;
		y = _tIcon->Top + _tIcon->Height + 2;
		LCD_DispStr(x, y, (char *)_tIcon->Text, _tFont);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_DrawBmp32
*	����˵��: ��LCD�ϻ���һ��32λ��BMPͼ, ����͸����Ϣ��λͼ(32λ�� RGBA)
*	��    ��: _usX, _usY : ��ʾ����
*			  _usHeight, _usWidth : ͼƬ�߶ȺͿ���
*			  _pBmp : ͼƬ���ݣ���BMP�ļ�ͷ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_DrawBmp32(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint8_t *_pBmp)
{
	const uint8_t *p;
	uint16_t usOldRGB, usNewRGB;
	int16_t x, y;		/* ���ڼ�¼�����ڵ�������� */
	uint8_t R1,G1,B1,A;	/* ������ɫ�ʷ��� */
	uint8_t R0,G0,B0;	/* ������ɫ�ʷ��� */

	p = (const uint8_t *)_pBmp;
	p += 54;		/* ֱ��ָ��ͼ�������� */

	/* ����BMPλͼ���򣬴������ң���������ɨ�� */
	for (y = _usHeight - 1; y >= 0; y--)
	{
		for (x = 0; x < _usWidth; x++)
		{
			B1 = *p++;
			G1 = *p++;
			R1 = *p++;
			A = *p++;	/* Alpha ֵ(͸����)��0-255, 0��ʾ͸����1��ʾ��͸��, �м�ֵ��ʾ͸���� */

			if (A == 0x00)	/* ��Ҫ͸��,��ʾ���� */
			{
				;	/* ����ˢ�±��� */
			}
			else if (A == 0xFF)	/* ��ȫ��͸���� ��ʾ������ */
			{
				usNewRGB = RGB(R1, G1, B1);
				//if (_ucFocusMode == 1)
				//{
				//	usNewRGB = Blend565(usNewRGB, CL_YELLOW, 10);
				//}
				LCD_PutPixel(x + _usX, y + _usY, usNewRGB);
			}
			else 	/* ��͸�� */
			{
				/* ���㹫ʽ�� ʵ����ʾ��ɫ = ǰ����ɫ * Alpha / 255 + ������ɫ * (255-Alpha) / 255 */
				usOldRGB = LCD_GetPixel(x + _usX, y + _usY);
				R0 = RGB565_R(usOldRGB);
				G0 = RGB565_G(usOldRGB);
				B0 = RGB565_B(usOldRGB);

				R1 = (R1 * A) / 255 + R0 * (255 - A) / 255;
				G1 = (G1 * A) / 255 + G0 * (255 - A) / 255;
				B1 = (B1 * A) / 255 + B0 * (255 - A) / 255;
				usNewRGB = RGB(R1, G1, B1);
				//if (_ucFocusMode == 1)
				//{
				//	usNewRGB = Blend565(usNewRGB, CL_YELLOW, 10);
				//}
				LCD_PutPixel(x + _usX, y + _usY, usNewRGB);
			}
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_DrawLabel
*	����˵��: ����һ���ı���ǩ
*	��    ��: �ṹ��ָ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_DrawLabel(LABEL_T *_pLabel)
{
	char dispbuf[256];
	uint16_t i;
	uint16_t NewLen;

	NewLen = strlen(_pLabel->pCaption);

	if (NewLen > _pLabel->MaxLen)
	{
		LCD_DispStr(_pLabel->Left, _pLabel->Top, _pLabel->pCaption, _pLabel->Font);
		_pLabel->MaxLen = NewLen;
	}
	else
	{
		for (i = 0; i < NewLen; i++)
		{
			dispbuf[i] = _pLabel->pCaption[i];
		}
		for (; i < _pLabel->MaxLen; i++)
		{
			dispbuf[i] = ' ';		/* ĩβ���ո� */
		}
		dispbuf[i] = 0;
		LCD_DispStr(_pLabel->Left, _pLabel->Top, dispbuf, _pLabel->Font);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_DrawCheckBox
*	����˵��: ����һ������
*	��    ��: �ṹ��ָ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_DrawCheckBox(CHECK_T *_pCheckBox)
{
	uint16_t x, y;

	/* Ŀǰֻ����16�����ֵĴ�С */

	/* ������� */
	x = _pCheckBox->Left;
	LCD_DrawRect(x, _pCheckBox->Top, CHECK_BOX_H, CHECK_BOX_W, CHECK_BOX_BORDER_COLOR);
	LCD_DrawRect(x + 1, _pCheckBox->Top + 1, CHECK_BOX_H - 2, CHECK_BOX_W - 2, CHECK_BOX_BORDER_COLOR);
	LCD_Fill_Rect(x + 2, _pCheckBox->Top + 2, CHECK_BOX_H - 4, CHECK_BOX_W - 4, CHECK_BOX_BACK_COLOR);

	/* �����ı���ǩ */
	x = _pCheckBox->Left + CHECK_BOX_W + 2;
	y = _pCheckBox->Top + CHECK_BOX_H / 2 - 8;
	LCD_DispStr(x, y, _pCheckBox->pCaption, _pCheckBox->Font);

	if (_pCheckBox->Checked)
	{
		FONT_T font;

	    font.FontCode = FC_ST_16;
		font.BackColor = CL_MASK;
		font.FrontColor = CHECK_BOX_CHECKED_COLOR;	/* ������ɫ */
		font.Space = 0;
		x = _pCheckBox->Left;
		LCD_DispStr(x + 3, _pCheckBox->Top + 3, "��", &font);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_DrawEdit
*	����˵��: ��LCD�ϻ���һ���༭��
*	��    ��: _pEdit �༭��ṹ��ָ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_DrawEdit(EDIT_T *_pEdit)
{
	uint16_t len, x, y;
	uint8_t width;

	/* ��XP���ƽ��༭�� */
	LCD_DrawRect(_pEdit->Left, _pEdit->Top, _pEdit->Height, _pEdit->Width, EDIT_BORDER_COLOR);
	LCD_Fill_Rect(_pEdit->Left + 1, _pEdit->Top + 1, _pEdit->Height - 2, _pEdit->Width - 2, EDIT_BACK_COLOR);

	/* ���־��� */
	if (_pEdit->Font->FontCode == FC_ST_12)
	{
		width = 6;
	}
	else
	{
		width = 8;
	}
	len = strlen(_pEdit->pCaption);
	x = _pEdit->Left +  (_pEdit->Width - len * width) / 2;
	y = _pEdit->Top + _pEdit->Height / 2 - width;

	LCD_DispStr(x, y, _pEdit->pCaption, _pEdit->Font);
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_DrawEdit
*	����˵��: ��LCD�ϻ���һ���༭��
*	��    ��:
*			_usX, _usY : ͼƬ������
*			_usHeight  : ͼƬ�߶�
*			_usWidth   : ͼƬ����
*			_ptr       : ͼƬ����ָ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_DrawButton(BUTTON_T *_pBtn)
{
	uint16_t x, y, h;
	FONT_T font;	/* ��ť����ʱ����Ҫ�����������ã������Ҫһ������������ */

	font.FontCode = _pBtn->Font->FontCode;
	font.FrontColor = _pBtn->Font->FrontColor;
	font.BackColor = _pBtn->Font->BackColor;
	font.Space = _pBtn->Font->Space;	
			
	if (_pBtn->Focus == 1)
	{
		font.BackColor = BUTTON_ACTIVE_COLOR;
	}
	else
	{
		/* ��ť�ı���ɫͳһ���������ܵ���ָ�� */
		font.BackColor = BUTTON_BACK_COLOR;
	}
	
	/* ��XP���ƽ��༭�� */
	LCD_DrawRect(_pBtn->Left, _pBtn->Top, _pBtn->Height, _pBtn->Width, BUTTON_BORDER_COLOR);
	LCD_DrawRect(_pBtn->Left + 1, _pBtn->Top + 1, _pBtn->Height - 2, _pBtn->Width - 2, BUTTON_BORDER1_COLOR);
	LCD_DrawRect(_pBtn->Left + 2, _pBtn->Top + 2, _pBtn->Height - 4, _pBtn->Width - 4, BUTTON_BORDER2_COLOR);

	h =  LCD_GetFontHeight(&font);
	x = _pBtn->Left + 3;
	y = _pBtn->Top + _pBtn->Height / 2 - h / 2;		
	
	LCD_Fill_Rect(_pBtn->Left + 3, _pBtn->Top + 3, _pBtn->Height - 6, _pBtn->Width - 6, font.BackColor);	/* ѡ�к�ĵ�ɫ */
	LCD_DispStrEx(x, y, _pBtn->pCaption, &font, _pBtn->Width - 6, ALIGN_CENTER);	/* ˮƽ���� */		
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_DrawGroupBox
*	����˵��: ��LCD�ϻ���һ�������
*	��    ��: _pBox �����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_DrawGroupBox(GROUP_T *_pBox)
{
	uint16_t x, y;

	/* ����Ӱ�� */
	LCD_DrawRect(_pBox->Left + 1, _pBox->Top + 5, _pBox->Height, _pBox->Width - 1, CL_BOX_BORDER2);

	/* �������� */
	LCD_DrawRect(_pBox->Left, _pBox->Top + 4, _pBox->Height, _pBox->Width - 1, CL_BOX_BORDER1);

	/* ��ʾ�������⣨���������Ͻǣ� */
	x = _pBox->Left + 9;
	y = _pBox->Top;
	LCD_DispStr(x, y, _pBox->pCaption, _pBox->Font);
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_DispControl
*	����˵��: ���ƿؼ�
*	��    ��: _pControl �ؼ�ָ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_DispControl(void *_pControl)
{
	uint8_t id;

	id = *(uint8_t *)_pControl;	/* ��ȡID */

	switch (id)
	{
		case ID_ICON:
			//void LCD_DrawIcon(const ICON_T *_tIcon, FONT_T *_tFont, uint8_t _ucFocusMode);
			break;

		case ID_WIN:
			LCD_DrawWin((WIN_T *)_pControl);
			break;

		case ID_LABEL:
			LCD_DrawLabel((LABEL_T *)_pControl);
			break;

		case ID_BUTTON:
			LCD_DrawButton((BUTTON_T *)_pControl);
			break;

		case ID_CHECK:
			LCD_DrawCheckBox((CHECK_T *)_pControl);
			break;

		case ID_EDIT:
			LCD_DrawEdit((EDIT_T *)_pControl);
			break;

		case ID_GROUP:
			LCD_DrawGroupBox((GROUP_T *)_pControl);
			break;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_SetDirection
*	����˵��: ������ʾ����ʾ���򣨺��� ������
*	��    ��: ��ʾ������� 0 ��������, 1=����180�ȷ�ת, 2=����, 3=����180�ȷ�ת
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_SetDirection(uint8_t _dir)
{
	g_LcdDirection =  _dir;		/* ������ȫ�ֱ��� */

	LCDH7_SetDirection(_dir);
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_ButtonTouchDown
*	����˵��: �жϰ�ť�Ƿ񱻰���. ��鴥�������Ƿ��ڰ�ť�ķ�Χ֮�ڡ����ػ水ť��
*	��    ��:  _btn : ��ť����
*			  _usX, _usY: ��������
*	�� �� ֵ: 1 ��ʾ�ڷ�Χ��
*********************************************************************************************************
*/
uint8_t LCD_ButtonTouchDown(BUTTON_T *_btn, uint16_t _usX, uint16_t _usY)
{
	if ((_usX > _btn->Left) && (_usX < _btn->Left + _btn->Width)
		&& (_usY > _btn->Top) && (_usY < _btn->Top + _btn->Height))
	{
		BUTTON_BEEP();	/* ������ʾ�� bsp_tft_lcd.h �ļ���ͷ����ʹ�ܺ͹ر� */
		_btn->Focus = 1;
		LCD_DrawButton(_btn);
		return 1;
	}
	else
	{
		return 0;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: LCD_ButtonTouchRelease
*	����˵��: �жϰ�ť�Ƿ񱻴����ͷ�. ���ػ水ť���ڴ����ͷ��¼��б����á�
*	��    ��:  _btn : ��ť����
*			  _usX, _usY: ��������
*	�� �� ֵ: 1 ��ʾ�ڷ�Χ��
*********************************************************************************************************
*/
uint8_t LCD_ButtonTouchRelease(BUTTON_T *_btn, uint16_t _usX, uint16_t _usY)
{
	_btn->Focus = 0;
	LCD_DrawButton(_btn);

	if ((_usX > _btn->Left) && (_usX < _btn->Left + _btn->Width)
		&& (_usY > _btn->Top) && (_usY < _btn->Top + _btn->Height))
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
*	�� �� ��: LCD_InitButton
*	����˵��: ��ʼ����ť�ṹ���Ա��
*	��    ��:  _x, _y : ����
*			  _h, _w : �߶ȺͿ���
*			  _pCaption : ��ť����
*			  _pFont : ��ť����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD_InitButton(BUTTON_T *_btn, uint16_t _x, uint16_t _y, uint16_t _h, uint16_t _w, char *_pCaption, FONT_T *_pFont)
{
	_btn->Left = _x;
	_btn->Top = _y;
	_btn->Height = _h;
	_btn->Width = _w;
	_btn->pCaption = _pCaption;	
	_btn->Font = _pFont;
	_btn->Focus = 0;
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
