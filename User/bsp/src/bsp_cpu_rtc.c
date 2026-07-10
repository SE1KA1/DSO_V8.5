/**
 * @file bsp_cpu_rtc.c
 * @brief 安富莱 BSP 通用驱动（RTC 实时时钟）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : STM32�ڲ�RTCģ��
*	�ļ����� : bsp_cpu_rtc.h
*	��    �� : V1.0
*	˵    �� : ͷ�ļ�
*
*	�޸ļ�¼ :
*		�汾��  ����       ����    ˵��
*		v1.0    2015-08-08 armfly  �װ�.����������ԭ��
*
*	Copyright (C), 2015-2016, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/* 宏定义: RTC_INIT_FLAG - RTC_INIT_FLAG 的宏定义。 */
#define RTC_INIT_FLAG	0xA5A8

/* 宏定义: RTC_ASYNCH_PREDIV - RTC_ASYNCH_PREDIV 的宏定义。 */
#define RTC_ASYNCH_PREDIV  0x7F
/* 宏定义: RTC_SYNCH_PREDIV - RTC_SYNCH_PREDIV 的宏定义。 */
#define RTC_SYNCH_PREDIV   0x00FF

RTC_T g_tRTC;

/* ƽ���ÿ�������� */
const uint8_t mon_table[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/*
*********************************************************************************************************
*   函 数 名: RTC_Config
*   功能说明: RTC_Config 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void RTC_Config(void);

RTC_HandleTypeDef RtcHandle;	/* ������ȫ�ֱ��� */

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitRTC
*	����˵��: ��ʼ��CPU�ڲ�RTC
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitRTC(void)
{	
	uint32_t back_reg;

	RtcHandle.Instance = RTC;	
	
	/* ���ڼ���Ƿ��Ѿ����ù�RTC��������ù��Ļ����������ý���ʱ 
	����RTC���ݼĴ���Ϊ0xA5A5 ��������RTC���ݼĴ�������0xA5A5   ��ô��ʾû�����ù�����Ҫ����RTC.   */
	back_reg = HAL_RTCEx_BKUPRead(&RtcHandle, RTC_BKP_DR0); 
	if (back_reg != RTC_INIT_FLAG) 
	{ 		
		RTC_Config(); 	/* RTC ���� */ 
		
		RTC_WriteClock(2018, 9, 1, 0, 0, 0);	/* ���ó�ʼʱ�� */
		
		/* ���ñ��ݼĴ�������ʾ�Ѿ����ù�RTC */
		HAL_RTCEx_BKUPWrite(&RtcHandle, RTC_BKP_DR0, RTC_INIT_FLAG);		
	} 
	else 
	{ 
		/* ����ϵ縴λ��־�Ƿ����� */ 
		if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) != 0)
		{ 
			/* �����ϵ縴λ */ 
		} 
		/* ������Ÿ�λ��־�Ƿ����� */ 
		else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != 0) 
		{ 
			/* �������Ÿ�λ */  
		} 
		
		RTC_Config(); 	/* RTC ���� */ 
	}
}

/*
*********************************************************************************************************
*	�� �� ��: RTC_Config
*	����˵��: 1. ѡ��ͬ��RTCʱ��ԴLSI����LSE��
*             2. ����RTCʱ�ӡ�
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void RTC_Config(void)
{
	RCC_OscInitTypeDef        RCC_OscInitStruct;
	RCC_PeriphCLKInitTypeDef  PeriphClkInitStruct;

	/* To enable access on RTC registers */
	HAL_PWR_EnableBkUpAccess();
	
	/* Configure LSE/LSI as RTC clock source ###############################*/
	RCC_OscInitStruct.OscillatorType =  RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_LSE;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
	RCC_OscInitStruct.LSEState = RCC_LSE_ON;
	RCC_OscInitStruct.LSIState = RCC_LSI_OFF;
	if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{ 
		Error_Handler(__FILE__, __LINE__);
	}

	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
	PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
	if(HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
	{ 
		Error_Handler(__FILE__, __LINE__);
	}
	
	/* Configures the External Low Speed oscillator (LSE) drive capability */
	__HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_HIGH);	
	
	/* Enable RTC Clock */ 
	__HAL_RCC_RTC_ENABLE(); 
  
	/* Configure RTC prescaler and RTC data registers */	
	{
	 
		RtcHandle.Instance = RTC;

		/* RTC configured as follows:
		  - Hour Format    = Format 24
		  - Asynch Prediv  = Value according to source clock
		  - Synch Prediv   = Value according to source clock
		  - OutPut         = Output Disable
		  - OutPutPolarity = High Polarity
		  - OutPutType     = Open Drain */
		RtcHandle.Init.HourFormat     = RTC_HOURFORMAT_24;
		RtcHandle.Init.AsynchPrediv   = RTC_ASYNCH_PREDIV;
		RtcHandle.Init.SynchPrediv    = RTC_SYNCH_PREDIV;
		RtcHandle.Init.OutPut         = RTC_OUTPUT_DISABLE;
		RtcHandle.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
		RtcHandle.Init.OutPutType     = RTC_OUTPUT_TYPE_OPENDRAIN;
		if (HAL_RTC_Init(&RtcHandle) != HAL_OK)
		{
			/* Initialization Error */
			Error_Handler(__FILE__, __LINE__);
		}	
	}
}

/*
*********************************************************************************************************
*	�� �� ��: RTC_WriteClock
*	����˵��: ����RTCʱ��
*	��    �Σ���
*	�� �� ֵ: 1��ʾ�ɹ� 0��ʾ����
*********************************************************************************************************
*/
uint8_t RTC_WriteClock(uint16_t _year, uint8_t _mon, uint8_t _day, uint8_t _hour, uint8_t _min, uint8_t _sec)
{
	RTC_DateTypeDef  date;
	RTC_TimeTypeDef  time;

	RtcHandle.Instance = RTC;
	
	/* ���������պ����� */
	date.Year = _year - 2000;
	date.Month = _mon;
	date.Date = _day;
	date.WeekDay = RTC_CalcWeek(_year, _mon, _day);	/* ��5=5�� ����=7 */
	if (HAL_RTC_SetDate(&RtcHandle, &date, FORMAT_BIN) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	} 

	/* ����ʱ���룬�Լ���ʾ��ʽ */
	time.Hours   = _hour;
	time.Minutes = _min;
	time.Seconds = _sec; 
	time.TimeFormat = RTC_HOURFORMAT12_AM;
	time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE ;
	time.StoreOperation = RTC_STOREOPERATION_RESET;	
	if(HAL_RTC_SetTime(&RtcHandle, &time, FORMAT_BIN) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}  

	return 1;      
}

/*
*********************************************************************************************************
*	�� �� ��: RTC_ReadClock
*	����˵��: �õ���ǰʱ�ӡ��������� g_tRTC��
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RTC_ReadClock(void)
{
	RTC_DateTypeDef  date;
	RTC_TimeTypeDef  time;

	RtcHandle.Instance = RTC;
	
	/* CPU BUG: �����ȶ�ȡʱ�䣬�ٶ�ȡ���� */
	if (HAL_RTC_GetTime(&RtcHandle, &time, FORMAT_BIN) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	} 	
	
	if (HAL_RTC_GetDate(&RtcHandle, &date, FORMAT_BIN) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	} 
	
	g_tRTC.Year = date.Year + 2000;
	g_tRTC.Mon = date.Month;
	g_tRTC.Day = date.Date;
	
	g_tRTC.Hour = time.Hours;	/* Сʱ */
	g_tRTC.Min = time.Minutes; 	/* ���� */
	g_tRTC.Sec = time.Seconds;	/* �� */

	g_tRTC.Week = RTC_CalcWeek(g_tRTC.Year, g_tRTC.Mon, g_tRTC.Day);	/* �������� */
}   


/*
*********************************************************************************************************
*	�� �� ��: bsp_CalcWeek
*	����˵��: �������ڼ������ڼ�
*	��    ��: _year _mon _day  ������  (����2�ֽ��������º������ֽ�������
*	�� �� ֵ: �ܼ� ��1-7�� 7��ʾ����
*********************************************************************************************************
*/
uint8_t RTC_CalcWeek(uint16_t _year, uint8_t _mon, uint8_t _day)
{
	/*
	���գ�Zeller����ʽ
		��ʷ�ϵ�ĳһ�������ڼ���δ����ĳһ�������ڼ�������������⣬�кܶ���㹫ʽ������ͨ�ü��㹫ʽ��
	һЩ�ֶμ��㹫ʽ�����������������ǲ��գ�Zeller����ʽ��
	    ��w=y+[y/4]+[c/4]-2c+[26(m+1)/10]+d-1

		��ʽ�еķ��ź������£�
	     w�����ڣ�
	     c����ĸ�2λ��������-1
	     y���꣨��λ������
	     m���£�m���ڵ���3��С�ڵ���14�����ڲ��չ�ʽ�У�ĳ���1��2��Ҫ������һ���13��14�������㣬
	  	    ����2003��1��1��Ҫ����2002���13��1�������㣩��
	     d���գ�
	     [ ]����ȡ������ֻҪ�������֡�

	    �������W����7�������Ǽ��������ڼ������������0����Ϊ�����ա�
        �������Ǹ�������������������Ҫ���⴦����
            �������ܰ�ϰ�ߵ������ĸ�����������ֻ�ܰ������е������Ķ������ࡣΪ�˷���
        ���㣬���ǿ��Ը�������һ��7����������ʹ����Ϊһ��������Ȼ����������

		��2049��10��1�գ�100������죩Ϊ�����ò��գ�Zeller����ʽ���м��㣬�������£�
		���գ�Zeller����ʽ��w=y+[y/4]+[c/4]-2c+[26(m+1)/10]+d-1
		=49+[49/4]+[20/4]-2��20+[26�� (10+1)/10]+1-1
		=49+[12.25]+5-40+[28.6]
		=49+12+5-40+28
		=54 (����7��5)
		��2049��10��1�գ�100������죩������5��
	*/
	uint8_t y, c, m, d;
	int16_t w;

	if (_mon >= 3)
	{
		m = _mon;
		y = _year % 100;
		c = _year / 100;
		d = _day;
	}
	else	/* ĳ���1��2��Ҫ������һ���13��14�������� */
	{
		m = _mon + 12;
		y = (_year - 1) % 100;
		c = (_year - 1) / 100;
		d = _day;
	}

	w = y + y / 4 +  c / 4 - 2 * c + ((uint16_t)26*(m+1))/10 + d - 1;
	if (w == 0)
	{
		w = 7;	/* ��ʾ���� */
	}
	else if (w < 0)	/* ���w�Ǹ����������������ʽ��ͬ */
	{
		w = 7 - (-w) % 7;
	}
	else
	{
		w = w % 7;
	}
	
	/* 2018-10-20 else��������棬���������w�����0�� */
	if (w == 0)
	{
		w = 7;	/* ��ʾ���� */
	}
	
	return w;
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
