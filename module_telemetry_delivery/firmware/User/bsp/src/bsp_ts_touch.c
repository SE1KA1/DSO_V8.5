/**
 * @file bsp_ts_touch.c
 * @brief 安富莱 BSP 通用驱动（触摸通用）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : ����ʽ����������ģ��
*	�ļ����� : bsp_touch.c
*	��    �� : V1.8
*	˵    �� : ����i2c�ӿڵĴ���оƬ�� 
*	�޸ļ�¼ :
*		�汾��  ����        ����    ˵��
*       v1.0    2012-07-06 armfly  ST�̼���V3.5.0�汾��
*		v1.1    2012-10-22 armfly  ����4��У׼
*		v1.2    2012-11-07 armfly  ���4��У׼��XY������֧��bug
*		v1.3    2012-12-17 armfly  ����У׼����������ڲ���:�ȴ�ʱ��
*		V1.4    2013-07-26 armfly  ���� TOUCH_DataFilter() �˲��㷨
*		V1.5    2013-07-32 armfly  �޸�TOUCH_WaitRelease(),��������Ҫ����
*		V1.6    2014-10-20 armfly
*					(1) �޸� TOUCH_PutKey() ������ʵ�ִ������ĺ�����������̬�л�.
*					(2) param �ṹ����У׼ʱ��ǰ����Ļ������� TouchDirection
*					(3) ����3.5��Ĵ���оƬ���޸�SPI������ú�����
*					(4) ���ڴ���оƬTSC2046�ʹ���FLASH,NRF24L01,MP3��ģ�鹲��SPI���ߡ������Ҫ
*						�ڴ����жϺ������ж����߳�ͻ. ���Ӻ��� bsp_SpiBusBusy() ��æ.
*					(5) TSC2046��������ģ��SPI (����ģ�ⷽʽ����SPI�豸����)
*		V1.7    2015-01-02 armfly  �ƻ�������ɨ����1msһ���޸�Ϊ10msһ�Ρ�δ����
*				2015-04-21 armfly �޸� TOUCH_InitHard() ������GT811_InitHard() ִ�к�ֱ��return
*		V1.8	2015-10-30 armfly ���� 4.3����ݴ��� FT5x06
*					(1) ���� void TOUCH_CapScan(void) ����
*		V2.0	2018-01-6 TOUCH_PutKey() ����GT911��GT811��FT5X06֧��
*		V2.1	2018-09-08 armfly ��ֲ��STM32H7ƽ̨��
*					- ���败������������ŵ�eeprom�̶���ַ�� 16K�ֽ����256�ֽڿռ�. ��APP��param.c����
*					- ͬʱ֧��2��У׼��4��У׼�����������������ˡ�
*					- void TOUCH_Calibration(uint8_t _PointCount); �����β�
*
*	Copyright (C), 2015-2020, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/* ���Դ�ӡ��� */
//#define touch_printf       printf
#define touch_printf(...) 

/* ���崥���ж�INT��GPIO�˿� */
#define TP_INT_GPIO_CLK_ENABLE()	__HAL_RCC_GPIOH_CLK_ENABLE()
/* 宏定义: TP_INT_GPIO_PORT - TP_INT_GPIO_PORT 的宏定义。 */
#define TP_INT_GPIO_PORT              GPIOH
/* 宏定义: TP_INT_PIN - TP_INT_PIN 的宏定义。 */
#define TP_INT_PIN                    GPIO_PIN_7

/* ÿ1msɨ��һ������ */
#define DOWN_VALID		30	/* ����30ms ��, ��ʼͳ��ADC */
#define SAMPLE_COUNT	20	/* ���º� 20ms����һ������ */

/*
	������У׼�������Ļ�����Ľǵ�ƫ������
	��1���� �� x1 = CALIB_OFFSET, y1 = CALIB_OFFSET
	��2���� �� x2 = LCD_GetWidth() - CALIB_OFFSET, y2 = LCD_GetHeight() - CALIB_OFFSET
*/
#define CALIB_OFFSET	20
/* 宏定义: TP_X1 - TP_X1 的宏定义。 */
#define TP_X1	CALIB_OFFSET
/* 宏定义: TP_Y1 - TP_Y1 的宏定义。 */
#define TP_Y1	CALIB_OFFSET

/* 宏定义: TP_X2 - TP_X2 的宏定义。 */
#define TP_X2	(LCD_GetWidth() - CALIB_OFFSET)
/* 宏定义: TP_Y2 - TP_Y2 的宏定义。 */
#define TP_Y2	(LCD_GetHeight() - CALIB_OFFSET)

/* 宏定义: TP_X3 - TP_X3 的宏定义。 */
#define TP_X3	CALIB_OFFSET
/* 宏定义: TP_Y3 - TP_Y3 的宏定义。 */
#define TP_Y3	(LCD_GetHeight() - CALIB_OFFSET)

/* 宏定义: TP_X4 - TP_X4 的宏定义。 */
#define TP_X4	(LCD_GetWidth() - CALIB_OFFSET)
/* 宏定义: TP_Y4 - TP_Y4 的宏定义。 */
#define TP_Y4	CALIB_OFFSET

/* ��ЧADCֵ���ж�����. ̫�ӽ�ADC�ٽ�ֵ��������Ϊ��Ч */
#define ADC_VALID_OFFSET	2

/* У׼���������EEPROM(AT24C128����16KB) ���64�ֽ����� */
#define TP_PARAM_EE_ADDR		(16*1024 - 64)

//#define TP_PARAM_FLASH_ADDR		ADDR_FLASH_SECTOR_3		/* 0x0800C000 �м��16KB����������Ų��� */
//#define TP_PARAM_FLASH_ADDR		ADDR_FLASH_SECTOR_11	/* 0x080E0000 Flash���128K����������Ų��� */

/* ����ģ���õ���ȫ�ֱ��� */
TOUCH_T g_tTP;

uint8_t g_TouchType;
uint8_t g_LcdType;

TP_CALIB_PARAM_T g_tTPParam;

/*
*********************************************************************************************************
*   函 数 名: TOUCH_PressValid
*   功能说明: TOUCH_PressValid 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t	TOUCH_PressValid(uint16_t _usX, uint16_t _usY);
/*
*********************************************************************************************************
*   函 数 名: TOUCH_DataFilter
*   功能说明: TOUCH_DataFilter 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t TOUCH_DataFilter(uint16_t *_pBuf, uint8_t _ucCount);
/*
*********************************************************************************************************
*   函 数 名: TOUCH_LoadParam
*   功能说明: TOUCH_LoadParam 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void TOUCH_LoadParam(void);
/*
*********************************************************************************************************
*   函 数 名: TOUCH_SaveParam
*   功能说明: TOUCH_SaveParam 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void TOUCH_SaveParam(void);
/*
*********************************************************************************************************
*   函 数 名: CalTwoPoint
*   功能说明: CalTwoPoint 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static int32_t CalTwoPoint(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x);
/*
*********************************************************************************************************
*   函 数 名: TOUCH_TransX
*   功能说明: TOUCH_TransX 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static int16_t TOUCH_TransX(uint16_t _usAdcX, uint16_t _usAdcY);
/*
*********************************************************************************************************
*   函 数 名: TOUCH_TransY
*   功能说明: TOUCH_TransY 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static int16_t TOUCH_TransY(uint16_t _usAdcX, uint16_t _usAdcY);
/*
*********************************************************************************************************
*   函 数 名: TOUCH_Abs
*   功能说明: TOUCH_Abs 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
int32_t TOUCH_Abs(int32_t x);

/*
*********************************************************************************************************
*	�� �� ��: bsp_DetectLcdType
*	����˵��: ͨ��I2C����оƬ��ʶ��LCDģ�����͡���������ȫ�ֱ��� g_LcdType �� g_TouchType
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_DetectLcdType(void)
{
	uint8_t i;
	
	g_TouchType = 0xFF;
	g_LcdType = 0xFF;
	
	bsp_DelayUS(5000);
	
	touch_printf("����ʶ�������ͺ�\r\n");
	
	/* 50ms���ȴ�GT811��λ����������̽��GT811оƬ ID */
	for (i = 0; i < 5; i++)
	{		
		/*
			GT811���ݴ������GT911��I2C��ַ��ͬ
			һ��� 0x28 �� 0xBA ���֡�
			ͨ����ȡ����IC��оƬID��ʶ��
		*/
		if (i2c_CheckDevice(0x28) == 0)
		{
			uint32_t id;
	
			bsp_DelayUS(500);
			g_GT911.i2c_addr = 0x28;
			id = GT911_ReadID();			
			if (id == 0x00313139)
			{			
				g_GT911.i2c_addr = 0x28;
				g_TouchType = CT_GT911;
				g_LcdType = LCD_70_800X480;
				touch_printf("��⵽7.0����ݴ�����GT911(0x28) 800x480\r\n");
			}
			else 	/* GT811 */
			{
				g_GT811.i2c_addr = 0x28;
				g_TouchType = CT_GT811;
				g_LcdType = LCD_70_800X480;		
				touch_printf("��⵽7.0����ݴ�����GT811(0x28) 800x480\r\n");
			}
			break;
		}

		if (i2c_CheckDevice(0xBA) == 0)
		{
			uint32_t id;
	
			bsp_DelayUS(500);
			g_GT911.i2c_addr = 0xBA;
			id = GT911_ReadID();			
			if (id == 0x00313139)
			{			
				g_GT911.i2c_addr = 0xBA;
				g_TouchType = CT_GT911;
				g_LcdType = LCD_70_800X480;
				touch_printf("��⵽7.0����ݴ�����GT911(0xBA) 800x480\r\n");
			}
			else 	/* GT811 */
			{
				g_GT811.i2c_addr = 0xBA;
				g_TouchType = CT_GT811;
				g_LcdType = LCD_70_800X480;		
				touch_printf("��⵽7.0����ݴ�����GT811(0xBA) 800x480\r\n");
			}
			break;
		}		
		
		/* FTϵ�е��ݴ������� : 4.3��id = 0x55    5.0��id = 0x0A  7.0��id = 0x06 */
		if (i2c_CheckDevice(FT5X06_I2C_ADDR) == 0)
		{
			uint8_t id;
			
			bsp_DelayUS(50000);	/* �ӳ�50ms */
			id = FT5X06_ReadID();			
			if (id == 0x55)
			{
				g_TouchType = CT_FT5X06;
				g_LcdType = LCD_43_480X272;		
				touch_printf("��⵽4.3����ݴ�����\r\n");
			}
			else if (id == 0x0A)
			{
				g_TouchType = CT_FT5X06;
				g_LcdType = LCD_50_800X480;		
				touch_printf("��⵽5.0����ݴ�����\r\n");				
			}
			else	/* id == 0x06 ��ʾ7���������FTоƬ�� */
			{
				g_TouchType = CT_FT5X06;
				g_LcdType = LCD_70_800X480;		
				touch_printf("��⵽7.0����ݴ�����FT\r\n");					
			}
			break;
		}

		/* ���败���� */		
		if (i2c_CheckDevice(STMPE811_I2C_ADDRESS) == 0)
		{
			/*			
				0  = 4.3������480X272��
				1  = 5.0������480X272��
				2  = 5.0������800X480��
				3  = 7.0������800X480��
				4  = 7.0������1024X600��
				5  = 3.5������480X320��			
			*/					
			uint8_t id;			
			
			g_TouchType = CT_STMPE811;	/* �������� */
			
			STMPE811_InitHard();	/* ���������ò��ܶ�ȡID */
			
			id = STMPE811_ReadIO();	/* ʶ��LCDӲ������ */

			touch_printf("��⵽���败����, id = %d\r\n", id);
			switch (id)
			{
				case 0:
					g_LcdType = LCD_43_480X272;
					break;

				case 1:
					g_LcdType = LCD_50_480X272;
					break;

				case 2:
					g_LcdType = LCD_50_800X480;
					break;

				case 3:
					g_LcdType = LCD_70_800X480;
					break;

				case 4:
					g_LcdType = LCD_70_1024X600;
					break;

				case 5:
					g_LcdType = LCD_35_480X320;
					break;			
				
				default:
					g_LcdType = LCD_35_480X320;
					break;
			}			
			break;			
		}		
		
		bsp_DelayMS(10);
	}
	
	if (i == 5)
	{
		touch_printf("δʶ�����ʾģ��\r\n");
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_IntWakeUpForGT
*	����˵��: GT928������GT911������INT���Ѻ�������λ��INT�����ߵ�ƽ���壬���Ի��Ѵ������ߵ�оƬ��V7���á�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TOUCH_IntWakeUpForGT(void)
{
	/* ����TP_INT�����ж����� */
	GPIO_InitTypeDef gpio_init;

	/* ��1������GPIOʱ�� */
	TP_INT_GPIO_CLK_ENABLE();
	
	/* ��2�����������еİ���GPIOΪ��������ģʽ */
	gpio_init.Mode = GPIO_MODE_OUTPUT_PP;   		/* ������� */
	gpio_init.Pull = GPIO_NOPULL;                 /* ���������費ʹ�� */
	
	gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;  /* GPIO�ٶȵȼ� */
	gpio_init.Pin = TP_INT_PIN;
	HAL_GPIO_Init(TP_INT_GPIO_PORT, &gpio_init);	
	
	TP_INT_GPIO_PORT->BSRRH = TP_INT_PIN;	/* INT = 0 */
	bsp_DelayUS(200);
	
	TP_INT_GPIO_PORT->BSRRL = TP_INT_PIN;	/* INT = 1 */
	bsp_DelayUS(2000);
	TP_INT_GPIO_PORT->BSRRH = TP_INT_PIN;	/* INT = 0 */
	bsp_DelayUS(200);
	
	gpio_init.Mode = GPIO_MODE_INPUT;   		/* �������� */
	gpio_init.Pull = GPIO_NOPULL;                /* ���������費ʹ�� */
	HAL_GPIO_Init(TP_INT_GPIO_PORT, &gpio_init);	
	
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_InitHard
*	����˵��: ��ʼ������оƬ�� ��֮ǰ��������ִ�� bsp_DetectLcdType() ʶ����������оƬ�ͺ�.
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TOUCH_InitHard(void)
{	
	//TOUCH_IntWakeUpForGT();  STM32-V7���������軽��GT911 GT928
	
	/* ����TP_INT�����ж����� */
	{
		GPIO_InitTypeDef gpio_init;

		/* ��1������GPIOʱ�� */
		TP_INT_GPIO_CLK_ENABLE();
		
		/* ��2�����������еİ���GPIOΪ��������ģʽ */
		gpio_init.Mode = GPIO_MODE_INPUT;   		/* �������� */
		gpio_init.Pull = GPIO_NOPULL;                 /* ���������費ʹ�� */
		
		gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;  /* GPIO�ٶȵȼ� */
		gpio_init.Pin = TP_INT_PIN;
		HAL_GPIO_Init(TP_INT_GPIO_PORT, &gpio_init);	
	}

	bsp_DetectLcdType();	/* �Զ�ʶ����оƬ�ͺ� */
	
    g_tTP.Enable = 0;
	
	switch (g_TouchType)
	{
		case CT_GT811:			/* ���ݴ��� 7�� */
			GT811_InitHard();
			break;

		case CT_GT911:			/* ���ݴ��� 7�� */
			GT911_InitHard();
			break;		
		
		case CT_FT5X06:			/* ���ݴ��� 4.3�� */
			FT5X06_InitHard();
			break;
		
		case CT_STMPE811:		/* ����� */
			//STMPE811_InitHard();   < bsp_DetectLcdType() �ڲ��Ѿ�ִ�г�ʼ�� 
			g_tTP.usMaxAdc = 4095;	/* 12λADC */	
		
			TOUCH_LoadParam();	/* ��ȡУ׼���� */
			g_tTP.Write = g_tTP.Read = 0;
			g_tTP.Enable = 1;
			break;
		
		default:
			break;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_PenInt
*	����˵��: �жϴ�������
*	��    ��: ��
*	�� �� ֵ: 0��ʾ�޴��ʰ��£�1��ʾ�д��ʰ���
*********************************************************************************************************
*/
uint8_t TOUCH_PenInt(void)
{
	if ((TP_INT_GPIO_PORT->IDR & TP_INT_PIN) == 0)
	{
		return 1;
	}
	return 0;
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_ReadAdcX
*	����˵��: ��ô�����X����ADC����ֵ�� �ѽ����˲�����
*	��    ��:  ��
*	�� �� ֵ: X ����ADCֵ
*********************************************************************************************************
*/
uint16_t TOUCH_ReadAdcX(void)
{
	uint16_t usAdc;

	__set_PRIMASK(1);  		/* ���ж� */
	usAdc = g_tTP.usAdcNowX;
	__set_PRIMASK(0);  		/* ���ж� */

	return usAdc;
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_ReadAdcY
*	����˵��: ��ô�����Y����ADC����ֵ�� �ѽ����˲�����
*	��    ��:  ��
*	�� �� ֵ: Y ����ֵ��������ֵ
*********************************************************************************************************
*/
uint16_t TOUCH_ReadAdcY(void)
{
	uint16_t usAdc;

	__set_PRIMASK(1);  		/* ���ж� */
	usAdc = g_tTP.usAdcNowY;
	__set_PRIMASK(0);  		/* ���ж� */

	return usAdc;
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_PutKey
*	����˵��: ��1������������ֵѹ�봥��FIFO�����������败�����β���ADCֵ�����ݴ������β�������ֵ
*	��    ��: _usX, _usY ����ֵ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TOUCH_PutKey(uint8_t _ucEvent, uint16_t _usX, uint16_t _usY)
{
	uint16_t xx, yy;
	uint16_t x = 0, y = 0;

	g_tTP.Event[g_tTP.Write] = _ucEvent;

	if (g_tTP.Enable == 1)	/* �������� �β���ADCֵ */
	{
		xx = TOUCH_TransX(_usX, _usY);
		yy = TOUCH_TransY(_usX, _usY);
	}
	else	/* GT811��FTX06��GT911 ���ݴ����ߴ˷�֮ */
	{
		/* ����ת���� ֱ��������ֵ */
		xx = _usX;
		yy = _usY;		
	}
	
	/* ��������������ʶ�� */
	switch (g_tTPParam.TouchDirection)
	{
		case 0:	/* У׼����ʱ����Ļ����Ϊ0 */
			if (g_LcdDirection == 0)		/* ���� */
			{
				x = xx;
				y = yy;
			}
			else if (g_LcdDirection == 1)	/* ����180��*/
			{
				x = g_LcdWidth - xx - 1;
				y = g_LcdHeight - yy - 1;
			}
			else if (g_LcdDirection == 2)	/* ���� */
			{
				y = xx;
				x = g_LcdWidth - yy - 1;
			}
			else if (g_LcdDirection == 3)	/* ����180�� */
			{
				y = g_LcdHeight - xx - 1;
				x = yy;
			}
			break;

		case 1:	/* У׼����ʱ����Ļ����Ϊ1 */
			if (g_LcdDirection == 0)		/* ���� */
			{
				x = g_LcdWidth - xx - 1;
				y = g_LcdHeight - yy - 1;
			}
			else if (g_LcdDirection == 1)	/* ����180��*/
			{
				x = xx;
				y = yy;
			}
			else if (g_LcdDirection == 2)	/* ���� */
			{
				y = g_LcdHeight - xx - 1;
				x = yy;
			}
			else if (g_LcdDirection == 3)	/* ����180�� */
			{
				y = xx;
				x = g_LcdWidth - yy - 1;
			}
			break;

		case 2:	/* У׼����ʱ����Ļ����Ϊ2 */
			if (g_LcdDirection == 0)		/* ���� */
			{
				y = xx;
				x = g_LcdWidth - yy - 1;
			}
			else if (g_LcdDirection == 1)	/* ����180��*/
			{
				y = g_LcdHeight - xx - 1;
				x = yy;
			}
			else if (g_LcdDirection == 2)	/* ���� */
			{
				x = xx;
				y = yy;
			}
			else if (g_LcdDirection == 3)	/* ����180�� */
			{
				x = g_LcdWidth - xx - 1;
				y = g_LcdHeight - yy - 1;
			}
			break;

		case 3:	/* У׼����ʱ����Ļ����Ϊ3 */
			if (g_LcdDirection == 0)		/* ���� */
			{
				y = xx;
				x = g_LcdWidth - yy - 1;
			}
			else if (g_LcdDirection == 1)	/* ����180��*/
			{
				y = g_LcdHeight - xx - 1;
				x = yy;
			}
			else if (g_LcdDirection == 2)	/* ���� */
			{
				x = g_LcdWidth - xx - 1;
				y = g_LcdHeight - yy - 1;
			}
			else if (g_LcdDirection == 3)	/* ����180�� */
			{
				x = xx;
				y = yy;
			}
			break;

		default:
			g_tTPParam.TouchDirection = 0;	/* ���������Чʱ������Ϊȱʡ�ĺ��� */
			break;
	}

	g_tTP.XBuf[g_tTP.Write] = x;
	g_tTP.YBuf[g_tTP.Write] = y;

	if (++g_tTP.Write  >= TOUCH_FIFO_SIZE)
	{
		g_tTP.Write = 0;
	}
	
	/* ������䣬��ӡadc������ */
	touch_printf("%d - (%d, %d) adcX=%d,adcY=%d\r\n", _ucEvent, x, y, g_tTP.usAdcNowX, g_tTP.usAdcNowY);
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_GetKey
*	����˵��: �Ӵ���FIFO��������ȡһ������ֵ��
*	��    ��: ��
*	�� �� ֵ:
*			TOUCH_NONE      ��ʾ���¼�
*			TOUCH_DOWN      ����
*			TOUCH_MOVE      �ƶ�
*			TOUCH_RELEASE	�ͷ�
*********************************************************************************************************
*/
uint8_t TOUCH_GetKey(int16_t *_pX, int16_t *_pY)
{
	uint8_t ret;

	if (g_tTP.Read == g_tTP.Write)
	{
		return TOUCH_NONE;
	}
	else
	{
		ret = g_tTP.Event[g_tTP.Read];
		*_pX = g_tTP.XBuf[g_tTP.Read];
		*_pY = g_tTP.YBuf[g_tTP.Read];

		if (++g_tTP.Read >= TOUCH_FIFO_SIZE)
		{
			g_tTP.Read = 0;
		}
		return ret;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_CelarFIFO
*	����˵��: �������FIFO������
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TOUCH_CelarFIFO(void)
{
	__set_PRIMASK(1);  		/* ���ж� */
	g_tTP.Write = g_tTP.Read;
	__set_PRIMASK(0);  		/* ���ж� */
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_InRect
*	����˵��: �жϵ�ǰ�����Ƿ�λ�ھ��ο���
*	��    ��:  _usX, _usY: ��������
*			_usRectX,_usRectY: �������
*			_usRectH��_usRectW : ���θ߶ȺͿ���
*	�� �� ֵ: 1 ��ʾ�ڷ�Χ��
*********************************************************************************************************
*/
uint8_t TOUCH_InRect(uint16_t _usX, uint16_t _usY,
	uint16_t _usRectX, uint16_t _usRectY, uint16_t _usRectH, uint16_t _usRectW)
{
	if ((_usX > _usRectX) && (_usX < _usRectX + _usRectW)
		&& (_usY > _usRectY) && (_usY < _usRectY + _usRectH))
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
*	�� �� ��: TOUCH_MoveValid
*	����˵��: �жϵ�ǰ������ϴ������Ƿ�ƫ��̫��
*	��    ��:  _usX1, _usY1: ����1
*			  _usX2, _usY2: ����2
*	�� �� ֵ: 1 ��ʾ��Ч�㣬 0 ��ʾ�ɵ�
*********************************************************************************************************
*/
uint8_t TOUCH_MoveValid(uint16_t _usX1, uint16_t _usY1, uint16_t _usX2, uint16_t _usY2)
{
	int16_t iX, iY;
	static uint8_t s_invalid_count = 0;

	iX = TOUCH_Abs(_usX1 - _usX2);
	iY = TOUCH_Abs(_usY1 - _usY2);

	if ((iX < 25) && (iY < 25))
	{
		s_invalid_count = 0;
		return 1;
	}
	else
	{
		if (++s_invalid_count >= 3)
		{
			s_invalid_count = 0;
			return 1;
		}
		return 0;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_CapScan
*	����˵��: I2C�ӿڵ��ݴ�����ɨ�躯�������� bsp_Idle()ִ�У�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TOUCH_CapScan(void)
{
	if (g_GT811.Enable == 1)
	{
		GT811_Scan();
		return;
	}
	
	if (g_tFT5X06.Enable == 1)
	{
		FT5X06_Scan();
		return;
	}
	
	if (g_GT911.Enable == 1)
	{
		GT911_Scan();
		return;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_Scan
*	����˵��: �������¼������򡣸ú����������Ե��ã�ÿms����1��. �� bsp_Timer.c
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TOUCH_Scan(void)
{
	uint16_t usAdcX;
	uint16_t usAdcY;
	static uint16_t s_usXBuf[SAMPLE_COUNT];
	static uint16_t s_usYBuf[SAMPLE_COUNT];
	static uint8_t s_ucPos = 0;
	static uint8_t s_count = 0;
	static uint8_t s_down = 0;
	static uint16_t s_usSaveAdcX, s_usSaveAdcY; /* ���ڴ���̧���¼������水�º��ƶ���������ֵ */
	static uint8_t s_ms = 0;

	if (g_GT811.Enable == 1)
	{
		GT811_Timer1ms();	/* ���ݴ�������������� */
		return;
	}

	if (g_GT911.Enable == 1)
	{
		GT911_Timer1ms();	/* ���ݴ�������������� */
		return;
	}	
	
	if (g_tFT5X06.Enable == 1)
	{
		FT5X06_Timer1ms();	/* ���ݴ�������������� */
		return;
	}
	
	/* �������ڵ��败�� */
	
	if (g_tTP.Enable == 0)	
	{
		return;
	}
	
	if (++s_ms >= 2)
	{
		return;
	}
	
	/* 2ms����һ�� */
	s_ms = 0;
	
	/* �����жϷ��� */
	if (TOUCH_PenInt())
	{
		/* ���ԭʼ��ADCֵ��δ�˲� */
		usAdcX = STMPE811_ReadX();
		usAdcY = STMPE811_ReadY();

		if (TOUCH_PressValid(usAdcX, usAdcY))
		{
			/* ��ѹ30ms֮��ſ�ʼ�ɼ����� */
			if (s_count >= DOWN_VALID / 2)
			{
				s_usXBuf[s_ucPos] = usAdcX;
				s_usYBuf[s_ucPos] = usAdcY;

				/* �ɼ�20ms���ݽ����˲� */
				if (++s_ucPos >= SAMPLE_COUNT / 2)
				{
					s_ucPos = 0;

					/* ��ADC����ֵ���������˲� */
					g_tTP.usAdcNowX = TOUCH_DataFilter(s_usXBuf, SAMPLE_COUNT / 2);
					g_tTP.usAdcNowY = TOUCH_DataFilter(s_usYBuf, SAMPLE_COUNT / 2);

					if (s_down == 0)
					{
						s_down = 1;
						/* ���������¼� */
						TOUCH_PutKey(TOUCH_DOWN, g_tTP.usAdcNowX, g_tTP.usAdcNowY);
						
						s_usSaveAdcX = g_tTP.usAdcNowX;
						s_usSaveAdcY = g_tTP.usAdcNowY;
					}
					else
					{
						if (TOUCH_MoveValid(s_usSaveAdcX, s_usSaveAdcY, g_tTP.usAdcNowX, g_tTP.usAdcNowY))
						{
							/* �����ƶ��¼� */
							TOUCH_PutKey(TOUCH_MOVE, g_tTP.usAdcNowX, g_tTP.usAdcNowY);
							
							s_usSaveAdcX = g_tTP.usAdcNowX;
							s_usSaveAdcY = g_tTP.usAdcNowY;
						}
						else
						{
							g_tTP.usAdcNowX = 0; /* for debug stop */
						}
					}
				}
			}
			else
			{
				s_count++;
			}
		}
		else
		{
			if (s_count > 0)
			{
				if (--s_count == 0)
				{
					/* �����ͷ��¼� */
					//TOUCH_PutKey(TOUCH_RELEASE, g_tTP.usAdcNowX, g_tTP.usAdcNowY);
					TOUCH_PutKey(TOUCH_RELEASE, s_usSaveAdcX, s_usSaveAdcY);

					g_tTP.usAdcNowX = 0;
					g_tTP.usAdcNowY = 0;

					s_count = 0;
					s_down = 0;
					
					STMPE811_ClearInt();		/* �崥���жϱ�־ */
				}
			}
			s_ucPos = 0;
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: CalTwoPoint
*	����˵��: ����2��ֱ�߷��̣�����Yֵ
*	��    ��:  2����������x������
*	�� �� ֵ: x��Ӧ��yֵ
*********************************************************************************************************
*/
static int32_t CalTwoPoint(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x)
{
	return y1 + ((int32_t)(y2 - y1) * (x - x1)) / (x2 - x1);
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_TransX
*	����˵��: ������ADCֵת��Ϊ��������
*	��    ��:  ��
*	�� �� ֵ: X ����ֵ��������ֵ
*********************************************************************************************************
*/
static int16_t TOUCH_TransX(uint16_t _usAdcX, uint16_t _usAdcY)
{
	if (g_tTPParam.CalibPointCount == 2)
	{
		uint16_t x;
		int32_t y;

		if (g_tTPParam.XYChange == 0)
		{
			x = _usAdcX;
			if (x == 0)
			{
				y = 0;
			}
			else
			{
				//y = CalTwoPoint(g_tTPParam.usAdcX1, TP_X1, g_tTPParam.usAdcX2, TP_X2, x);
				y = CalTwoPoint(g_tTPParam.usAdcX1, g_tTPParam.usLcdX1, g_tTPParam.usAdcX2, g_tTPParam.usLcdX2, x);
			}
		}
		else
		{
			x = _usAdcY;
			if (x == 0)
			{
				y = 0;
			}
			else
			{
				//y = CalTwoPoint(g_tTPParam.usAdcY1, TP_X1, g_tTPParam.usAdcY2, TP_X2, x);
				y = CalTwoPoint(g_tTPParam.usAdcY1, g_tTPParam.usLcdX1, g_tTPParam.usAdcY2, g_tTPParam.usLcdX2, x);
			}
		}
		return y;
	}
	else	/* 4��У׼ */
	{
		uint16_t x, x1, x2;
		int32_t y;

		if (g_tTPParam.XYChange == 0)	/* X Y ���겻���� */
		{
			x = _usAdcX;

			/* ���� Y ADC ʵʱ����ֱ�߷��̵Ĳο���x1, x2
				if  _usAdcY = usAdcY1 then  ȡ�� = (AdcX1, TP_X1, AdcX4, TP_X4, _usAdcY)
				if  _usAdcY = usAdcY2 then  ȡ�� = (AdcX3, TP_X3, AdcX2, TP_X2, _usAdcY)

				���� TP_X1 = TP_X3;  TP_X4 = TP_X1 , ���ǳ����趨��У׼λ�õ���������, �ǹ̶��ġ�
				���ǽ���Ҫ��̬����Ե�1���͵�3��������ͬ������2��ֱ�߷��̼��㡣
			*/
			x1 = CalTwoPoint(g_tTPParam.usAdcY1, g_tTPParam.usAdcX1, g_tTPParam.usAdcY2,  g_tTPParam.usAdcX3, _usAdcY);
			x2 = CalTwoPoint(g_tTPParam.usAdcY1, g_tTPParam.usAdcX4, g_tTPParam.usAdcY2,  g_tTPParam.usAdcX2, _usAdcY);
		}
		else						/* X Y ���꽻�� */
		{
			x = _usAdcY;

			/* ���� X ADC ʵʱ����ֱ�߷��̵Ĳο���x1, x2
				if  _usAdcX = usAdcX1 then  ȡ�� = (AdcY1, TP_X1, AdcY4, TP_X4, _usAdcX)
				if  _usAdcX = usAdcX2 then  ȡ�� = (AdcY3, TP_X3, AdcY2, TP_X2, _usAdcX)

				���� TP_X1 = TP_X3;  TP_X4 = TP_X1 , ���ǳ����趨��У׼λ�õ���������, �ǹ̶��ġ�
				���ǽ���Ҫ��̬����Ե�1���͵�3��������ͬ������2��ֱ�߷��̼��㡣
			*/
			x1 = CalTwoPoint(g_tTPParam.usAdcX1, g_tTPParam.usAdcY1, g_tTPParam.usAdcX2,  g_tTPParam.usAdcY3, _usAdcX);
			x2 = CalTwoPoint(g_tTPParam.usAdcX1, g_tTPParam.usAdcY4, g_tTPParam.usAdcX2,  g_tTPParam.usAdcY2, _usAdcX);
		}

		if (x == 0)
		{
			y = 0;
		}
		else
		{
			/* ����2��ֱ�߷��̣��������� */
			//y = CalTwoPoint(x1, TP_X1, x2, TP_X2, x);
			CalTwoPoint(x1, g_tTPParam.usLcdX1, x2, g_tTPParam.usLcdX2, x);
		}
		return y;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_TransY
*	����˵��: ������ADCֵת��Ϊ��������
*	��    ��:  ��
*	�� �� ֵ: Y ����ֵ��������ֵ
*********************************************************************************************************
*/
static int16_t TOUCH_TransY(uint16_t _usAdcX, uint16_t _usAdcY)
{
	if (g_tTPParam.CalibPointCount == 2)	/* 2��У׼ */
	{
		int32_t x;
		int32_t y;

		if (g_tTPParam.XYChange == 0)
		{
			x = _usAdcY;
			if (x == 0)
			{
				y = 0;
			}
			else
			{
				//y = CalTwoPoint(g_tTPParam.usAdcY1, TP_Y1, g_tTPParam.usAdcY2, TP_Y2, x);
				y = CalTwoPoint(g_tTPParam.usAdcY1, g_tTPParam.usLcdY1, g_tTPParam.usAdcY2, g_tTPParam.usLcdY2, x);
			}
		}
		else
		{
			x = _usAdcX;
			if (x == 0)
			{
				y = 0;
			}
			else
			{
				//y = CalTwoPoint(g_tTPParam.usAdcX1, TP_Y1, g_tTPParam.usAdcX2, TP_Y2, x);
				y = CalTwoPoint(g_tTPParam.usAdcX1, g_tTPParam.usAdcY1, g_tTPParam.usAdcX2, g_tTPParam.usLcdY2, x);
			}
		}
		return y;
	}
	else /* 4��У׼ */
	{
		int32_t x, x1, x2;
		int32_t y;

		if (g_tTPParam.XYChange == 0)	/* X Y ���겻���� */
		{
			x = _usAdcY;

			/* ���� X ADC ʵʱ����ֱ�߷��̵Ĳο���x1, x2
				if  _usAdcX = usAdcX1 then  ȡ�� = (AdcY1, TP_Y1, AdcY3, TP_Y3, _usAdcX)
				if  _usAdcX = usAdcX2 then  ȡ�� = (AdcY4, TP_Y4, AdcY2, TP_Y2, _usAdcX)

				���� TP_Y1 = TP_Y4;  TP_Y3 = TP_Y2 , ���ǳ����趨��У׼λ�õ���������, �ǹ̶��ġ�
				���ǽ���Ҫ��̬����Ե�1���͵�3��������ͬ������2��ֱ�߷��̼��㡣
			*/
			x1 = CalTwoPoint(g_tTPParam.usAdcX1, g_tTPParam.usAdcY1, g_tTPParam.usAdcX2,  g_tTPParam.usAdcY4, _usAdcX);
			x2 = CalTwoPoint(g_tTPParam.usAdcX1, g_tTPParam.usAdcY3, g_tTPParam.usAdcX2,  g_tTPParam.usAdcY2, _usAdcX);
		}
		else						/* X Y ���꽻�� */
		{
			x = _usAdcX;

			/* ���� X ADC ʵʱ����ֱ�߷��̵Ĳο���x1, x2
				if  _usAdcY = usAdcY1 then  ȡ�� = (AdcX1, TP_Y1, AdcX3, TP_Y3, _usAdcY)
				if  _usAdcY = usAdcY2 then  ȡ�� = (AdcX4, TP_Y4, AdcX2, TP_Y2, _usAdcY)

				���� TP_Y1 = TP_Y3;  TP_Y4 = TP_Y2 , ���ǳ����趨��У׼λ�õ���������, �ǹ̶��ġ�
				���ǽ���Ҫ��̬����Ե�1���͵�3��������ͬ������2��ֱ�߷��̼��㡣
			*/
			x1 = CalTwoPoint(g_tTPParam.usAdcY1, g_tTPParam.usAdcX1, g_tTPParam.usAdcY2,  g_tTPParam.usAdcX4, _usAdcY);
			x2 = CalTwoPoint(g_tTPParam.usAdcY1, g_tTPParam.usAdcX3, g_tTPParam.usAdcY2,  g_tTPParam.usAdcX2, _usAdcY);
		}

		if (x == 0)
		{
			y = 0;
		}
		else
		{
			/* ����2��ֱ�߷��̣��������� */
			//y = CalTwoPoint(x1, TP_Y1, x2, TP_Y2, x);
			y = CalTwoPoint(x1, g_tTPParam.usAdcY1, x2, g_tTPParam.usLcdY2, x);
		}
		return y;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_GetX
*	����˵��: ��õ�ǰ�ĵĴ�������X
*	��    ��:  ��
*	�� �� ֵ: X ����ֵ��������ֵ
*********************************************************************************************************
*/
int16_t TOUCH_GetX(void)
{
	return TOUCH_TransX(TOUCH_ReadAdcX(), TOUCH_ReadAdcY());
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_GetY
*	����˵��: ��õ�ǰ�ĵĴ�������Y
*	��    ��:  ��
*	�� �� ֵ: Y ����ֵ��������ֵ
*********************************************************************************************************
*/
int16_t TOUCH_GetY(void)
{
	return TOUCH_TransY(TOUCH_ReadAdcX(), TOUCH_ReadAdcY());
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_DataFilter
*	����˵��: �Բ������ݽ����˲�
*	��    ��:  ��
*	�� �� ֵ: X ����ֵ��������ֵ
*********************************************************************************************************
*/
static uint16_t TOUCH_DataFilter(uint16_t *_pBuf, uint8_t _ucCount)
{
#if 0
	uint8_t i;
	uint32_t uiSum;

	uiSum = 0;
	for (i = 0; i < _ucCount; i++)
	{
		uiSum += _pBuf[i];
	}
	return uiSum / _ucCount;
#else
	uint8_t flag;
	uint8_t i;
	uint16_t usTemp;
	uint32_t uiSum;

	/* �������� */
    do
	{
		flag = 0;
		for (i = 0; i < _ucCount - 1; i++)
		{
			if (_pBuf[i] > _pBuf[i+1])
			{
				usTemp = _pBuf[i + 1];
				_pBuf[i+1] = _pBuf[i];
				_pBuf[i] = usTemp;
				flag = 1;
			}
		}
	}while(flag);

	uiSum = 0;
	for (i = 0; i < _ucCount / 3; i++)
	{
		uiSum += _pBuf[_ucCount / 3 + i];
	}
	usTemp = uiSum / (_ucCount / 3);
	return usTemp;
#endif
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_DispPoint1
*	����˵��: ��ʾ��1��У׼��
*	��    ��:  _ucIndex = 0 : ��ʾ��1���㣻 _ucIndex = 1 ��ʾ��2����;
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void TOUCH_DispPoint(uint8_t _ucIndex)
{
	FONT_T tFont16;			/* ����һ������ṹ���������������������� */

	/* ����������� */
	{
		tFont16.FontCode = FC_ST_16;	/* ������� 16���� */
		tFont16.FrontColor = CL_WHITE;		/* ������ɫ 0 �� 1 */
		tFont16.BackColor = CL_BLUE;	/* ���ֱ�����ɫ */
		tFont16.Space = 0;			/* ���ּ�࣬��λ = ���� */
	}

/*
	��1���� �� x1 = CALIB_OFFSET, y1 = CALIB_OFFSET
	��2���� �� x2 = LCD_GetHeight() - CALIB_OFFSET, y2 = LCD_GetWidth - CALIB_OFFSET
*/
	if (_ucIndex == 0)
	{
		LCD_ClrScr(CL_BLUE);  		/* ������������ɫ */

		/* ����Ļ���ػ���2�����ο�(���ڼ������Ե�����Ƿ�����) */
		LCD_DrawRect(0, 0, LCD_GetHeight(), LCD_GetWidth(), CL_WHITE);
		LCD_DrawRect(2, 2, LCD_GetHeight() - 4, LCD_GetWidth() - 4, CL_YELLOW);

		LCD_DispStr(50, 10, "У׼������", &tFont16);		/* ��(8,3)���괦��ʾһ������ */

		LCD_DrawCircle(TP_X1, TP_Y1, 6, CL_WHITE);
	}
	else if (_ucIndex == 1)
	{
		LCD_DrawCircle(TP_X1, TP_Y1, 6, CL_BLUE);			/* ������1���� */

		LCD_DrawCircle(TP_X2, TP_Y2, 6, CL_WHITE);
	}
	else if (_ucIndex == 2)
	{
		LCD_DrawCircle(TP_X2, TP_Y2, 6, CL_BLUE);			/* ������2���� */

		LCD_DrawCircle(TP_X3, TP_Y3, 6, CL_WHITE);
	}
	else
	{
		LCD_DrawCircle(TP_X3, TP_Y3, 6, CL_BLUE);			/* ������3���� */

		LCD_DrawCircle(TP_X4, TP_Y4, 6, CL_WHITE);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_PressValid
*	����˵��: �жϰ�ѹ�Ƿ���Ч������X, Y��ADCֵ���д����ж�
*	��    ��:  ��
*	�� �� ֵ: 1 ��ʾ��Ч�� 0 ��ʾ��Ч
*********************************************************************************************************
*/
static uint8_t	TOUCH_PressValid(uint16_t _usX, uint16_t _usY)
{
	if ((_usX <= ADC_VALID_OFFSET) || (_usY <= ADC_VALID_OFFSET)
		|| (_usX >= g_tTP.usMaxAdc - ADC_VALID_OFFSET)
		|| (_usY >= g_tTP.usMaxAdc - ADC_VALID_OFFSET))
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_WaitRelease
*	����˵��: �ȴ������ͷ�
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void TOUCH_WaitRelease(void)
{
	uint8_t usCount = 0;

	for (;;)
	{
		if (TOUCH_PressValid(TOUCH_ReadAdcX(), TOUCH_ReadAdcY()) == 0)
		{
			if (++usCount > 5)
			{
				break;
			}
		}
		else
		{
			usCount = 0;
		}
		bsp_DelayMS(10);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_Abs
*	����˵��: �������ֵ
*	��    ��: x : �з�������
*	�� �� ֵ: ������
*********************************************************************************************************
*/
int32_t TOUCH_Abs(int32_t x)
{
	if (x >= 0)
	{
		return x;
	}
	else
	{
		return -x;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_Calibration
*	����˵��: ������У׼
*	��    ��: _PointCount : У׼������2 �� 4.
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TOUCH_Calibration(uint8_t _PointCount)
{
	uint16_t usAdcX;
	uint16_t usAdcY;
	uint8_t usCount;
	uint8_t i;
	uint32_t n;

	/* У׼������2���4�� */
	if (_PointCount == 4)
	{
		g_tTPParam.CalibPointCount = 4;
	}
	else
	{
		g_tTPParam.CalibPointCount = 2;
	}	
	
	TOUCH_CelarFIFO();		/* �����Ч�Ĵ����¼� */

	for (i = 0; i < g_tTPParam.CalibPointCount; i++)
	{
		TOUCH_DispPoint(i);		/* ��ʾУ׼�� */

		TOUCH_WaitRelease(); 	/* �ȴ������ͷ� */

		usCount = 0;
		for (n = 0; n < 500; n++)
		{
			usAdcX = TOUCH_ReadAdcX();
			usAdcY = TOUCH_ReadAdcY();

			if (TOUCH_PressValid(usAdcX, usAdcY))
			{
				if (++usCount > 5)
				{
					/* ��ѹ��Ч, ����У׼��ADC����ֵ */
					if (i == 0)
					{
						g_tTPParam.usAdcX1 = usAdcX;
						g_tTPParam.usAdcY1 = usAdcY;
					}
					else if (i == 1)
					{
						g_tTPParam.usAdcX2 = usAdcX;
						g_tTPParam.usAdcY2 = usAdcY;
					}
					else if (i == 2)
					{
						g_tTPParam.usAdcX3 = usAdcX;
						g_tTPParam.usAdcY3 = usAdcY;
					}
					else
					{
						g_tTPParam.usAdcX4 = usAdcX;
						g_tTPParam.usAdcY4 = usAdcY;
					}
					break;
				}
			}
			else
			{
				usCount = 0;
			}
			bsp_DelayMS(10);
		}
		if (n == 500)
		{
			return;
		}
	}

	TOUCH_WaitRelease(); 	/* �ȴ������ͷ� */

	/* ʶ������ X, Y �� ��ʾ���� X��Y �Ƿ���Ҫ���� */
	g_tTPParam.XYChange = 0;		/* 1��ʾX Y��Ҫ���� */
	if (LCD_GetHeight() < LCD_GetWidth())
	{
		if (TOUCH_Abs(g_tTPParam.usAdcX1 - g_tTPParam.usAdcX2) < TOUCH_Abs(g_tTPParam.usAdcY1 - g_tTPParam.usAdcY2))
		{
			g_tTPParam.XYChange = 1;
		}
	}
	else
	{
		if (TOUCH_Abs(g_tTPParam.usAdcX1 - g_tTPParam.usAdcX2) > TOUCH_Abs(g_tTPParam.usAdcY1 - g_tTPParam.usAdcY2))
		{
			g_tTPParam.XYChange = 1;
		}
	}

	g_tTPParam.usLcdX1 = TP_X1;
	g_tTPParam.usLcdY1 = TP_Y1;
	g_tTPParam.usLcdX2 = TP_X2;
	g_tTPParam.usLcdY2 = TP_Y2;
	g_tTPParam.usLcdX3 = TP_X3;
	g_tTPParam.usLcdY3 = TP_Y3;
	g_tTPParam.usLcdX4 = TP_X4;
	g_tTPParam.usLcdY4 = TP_Y4;

	/* �����һ������У׼����������Flash ����EEPROM */
	TOUCH_SaveParam();
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_SaveParam
*	����˵��: ����У׼����	s_usAdcX1 s_usAdcX2 s_usAdcY1 s_usAdcX2
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void TOUCH_SaveParam(void)
{
	
	g_tTPParam.TouchDirection = g_LcdDirection;	/* 2014-09-11 ������Ļ����, ������Ļ��תʱ�����ٴ�У׼ */

	#if 1
		/* д��EEPROM */
		ee_WriteBytes((uint8_t *)&g_tTPParam, TP_PARAM_EE_ADDR, sizeof(g_tTPParam));
	#else
		/* д��CPU Flash */
		bsp_WriteCpuFlash(TP_PARAM_FLASH_ADDR, (uint8_t *)&g_tTPParam, sizeof(g_tTPParam));
	#endif	
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_LoadParam
*	����˵��: ��ȡУ׼����
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void TOUCH_LoadParam(void)
{
	#if 1
		/* ��ȡEEPROM�еĲ��� */
		ee_ReadBytes((uint8_t *)&g_tTPParam, TP_PARAM_EE_ADDR, sizeof(g_tTPParam));
	#else
		/* ��ȡCPU Flash�еĲ��� */
		bsp_ReadCpuFlash(TP_PARAM_FLASH_ADDR, (uint8_t *)&g_tTPParam, sizeof(g_tTPParam
	#endif	
	
	if (g_tTPParam.TouchDirection > 4)
	{
		g_tTPParam.TouchDirection = 0;
		TOUCH_SaveParam();
	}
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
