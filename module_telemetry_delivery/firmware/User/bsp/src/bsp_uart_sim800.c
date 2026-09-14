/**
 * @file bsp_uart_sim800.c
 * @brief 安富莱 BSP 通用驱动（SIM800 蜂窝）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : GPRSģ��SIM800��������
*	�ļ����� : bsp_SIM800.c
*	��    �� : V1.1
*	˵    �� : ��װ��SIMCOM��˾��SIM800ģ����ص�AT���
*
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2015-02-01  armfly  ��ʽ����
*		V1.1    2015-06-21  armfly  
*							1.���ƴ��룬�޸�PowerOn()��������ֵ.
*							2.����������ʱ���жϳ�ʱ��ʹ�� bsp_GetRunTimer()����ʵ��
*
*	Copyright (C), 2015-2016, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/*
	������STM32-V6 ��������߷��䣺
	SIM800_TERM_ON   �� PI0/GPRS_TERM_ON
	SIM800_RESET   	 �� PB7/GPRS_RESET
	SIM800_RX   	 �� PA2/USART2_TX
	SIM800_TX   	 �� PA3/USART2_RX
*/

/*
	AT+CREG?  ��ѯ��ǰ����״̬

	AT+CSQ ��ѯ�ź���������

	AT+CIMI ��ѯSIM ����IMSI �š�

	AT+CIND? ��ȡ��ǰ��ָʾ״̬

	ATA ��������
	ATH �Ҷ���������
	
	ATI ��ʾSIM800ģ���Ӳ����Ϣ
	
	ATD10086; ����10086
*/

/* ������У���ʾʹ����չ��IO */
#define USE_HC574

#ifdef USE_HC574	/* ʹ����չIO */
	/* STM32��SIM800��PWRKEY���ż���1��NPN�����ܣ������Ҫ���� */
	#define PWRKEY_1()		HC574_SetPin(GPRS_TERM_ON, 0)
/* 宏定义: PWRKEY_0 - PWRKEY_0 的宏定义。 */
	#define PWRKEY_0()		HC574_SetPin(GPRS_TERM_ON, 1)

	/* STM32��MG323��RESET���ż���1��NPN�����ܣ������Ҫ����. MG323��RESET���ǵ����帴λ */
	#define MG_RESET_1()	HC574_SetPin(GPRS_RESET, 0)
/* 宏定义: MG_RESET_0 - MG_RESET_0 的宏定义。 */
	#define MG_RESET_0()	HC574_SetPin(PORT_RESET, 1);
#else	/* ʹ��CPU��IO */
	/* �����ڶ�Ӧ��RCCʱ�� */
	#define RCC_TERM_ON 	RCC_AHB1Periph_GPIOI
/* 宏定义: PORT_TERM_ON - PORT_TERM_ON 的宏定义。 */
	#define PORT_TERM_ON	GPIOI
/* 宏定义: PIN_TERM_ON - PIN_TERM_ON 的宏定义。 */
	#define PIN_TERM_ON		GPIO_PIN_0

/* 宏定义: RCC_RESET - RCC_RESET 的宏定义。 */
	#define RCC_RESET 		RCC_AHB1Periph_GPIOB
/* 宏定义: PORT_RESET - PORT_RESET 的宏定义。 */
	#define PORT_RESET		GPIOB
/* 宏定义: PIN_RESET - PIN_RESET 的宏定义。 */
	#define PIN_RESET		GPIO_PIN_7

	/* STM32��SIM800��PWRKEY���ż���1��NPN�����ܣ������Ҫ���� */
	#define PWRKEY_1()		GPIO_ResetBits(PORT_TERM_ON, PIN_TERM_ON);
/* 宏定义: PWRKEY_0 - PWRKEY_0 的宏定义。 */
	#define PWRKEY_0()		GPIO_SetBits(PORT_TERM_ON, PIN_TERM_ON);

	/* STM32��MG323��RESET���ż���1��NPN�����ܣ������Ҫ����. MG323��RESET���ǵ����帴λ */
	#define MG_RESET_1()	GPIO_ResetBits(PORT_RESET, PIN_RESET);
/* 宏定义: MG_RESET_0 - MG_RESET_0 的宏定义。 */
	#define MG_RESET_0()	GPIO_SetBits(PORT_RESET, PIN_RESET);
#endif

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitSIM800
*	����˵��: ��������ģ����ص�GPIO,  �ú����� bsp_Init() ���á�
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitSIM800(void)
{
#ifdef USE_HC574	/* ʹ����չIO */		
	PWRKEY_1();
#else
	GPIO_InitTypeDef GPIO_InitStructure;

	/* ��GPIOʱ�� */
	RCC_AHB1PeriphClockCmd(RCC_TERM_ON | RCC_RESET, ENABLE);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;		/* ��Ϊ����� */
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;		/* ��Ϊ����ģʽ */
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;	/* ���������費ʹ�� */
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;	/* IO������ٶ� */

	PWRKEY_1();
	
	GPIO_InitStructure.GPIO_Pin = PIN_TERM_ON;
	GPIO_Init(PORT_TERM_ON, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = PIN_RESET;
	GPIO_Init(PORT_RESET, &GPIO_InitStructure);
#endif	

	/* CPU�Ĵ��������Ѿ��� bsp_uart_fifo.c �е� bsp_InitUart() ���� */
}

/*
*********************************************************************************************************
*	�� �� ��: SIM800_PrintRxData
*	����˵��: ��ӡSTM32��SIM800�յ������ݵ�COM1���ڣ���Ҫ���ڸ��ٵ���
*	��    ��: _ch : �յ�������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SIM800_PrintRxData(uint8_t _ch)
{
	#ifdef SIM800_TO_COM1_EN
		comSendChar(COM1, _ch);		/* �����յ����ݴ�ӡ�����Դ���1 */
	#endif
}

/*
*********************************************************************************************************
*	�� �� ��: SIM800_PowerOn
*	����˵��: ģ���ϵ�. �����ڲ����ж��Ƿ��Ѿ�����������ѿ�����ֱ�ӷ���1
*	��    ��: ��
*	�� �� ֵ: 1 ��ʾ�ϵ�ɹ�  0: ��ʾ�쳣
*********************************************************************************************************
*/
uint8_t SIM800_PowerOn(void)
{
	uint8_t ret_value = 0;
	uint8_t i;
	
	/* �ж��Ƿ񿪻� */
	for (i = 0; i < 5; i++)
	{
		SIM800_SendAT("AT");
		if (SIM800_WaitResponse("OK", 100))
		{
			return 1;
		}
	}
	
	comClearRxFifo(COM_SIM800);	/* ���㴮�ڽ��ջ����� */

	/*  ͨ������ PWRKEY �������� 1.2 ��Ȼ���ͷţ�ʹģ�鿪����*/
	PWRKEY_0();
	bsp_DelayMS(2000);
	PWRKEY_1();

	/* �ȴ�ģ������ϵ磬������Զ����������ղ���RDT */
	//if (SIM800_WaitResponse("RDY", 5000) == 0)
	{	
		/* ��ʼͬ��������: ��������AT��ֻ�����յ���ȷ��OK 
		  ��ģ�鿪�������ӳ� 2 �� 3 ����ٷ���ͬ���ַ����û��ɷ��͡� AT�� (��д��Сд����)����ģ��
		  ͬ�������ʣ��������յ�ģ�鷵�ء� OK����			
		*/
		for (i = 0; i < 50; i++)
		{
			SIM800_SendAT("OK");
			if (SIM800_WaitResponse("OK", 100))
			{
				ret_value = 1;
				break;			/* ģ���ϵ�ɹ� */
			}
		}		
	}
	
	return ret_value;
}


/*
*********************************************************************************************************
*	�� �� ��: SIM800_PowerOff
*	����˵��: ����SIM800ģ��ػ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SIM800_PowerOff(void)
{
	/*
	�û�����ͨ����PWRKEY �ź�����1.5�������ػ�,����ʱ�䳬��33��ģ������¿�����
	
	�ػ������У�ģ�����ȴ�������ע�������ڲ��������밲ȫ״̬���ұ���������ݣ����ر��ڲ���
	Դ�������ϵ�ǰģ��Ĵ��ڽ����������ַ���
	NORMAL POWER DOWN
	��֮��ģ�齫����ִ��AT���ģ�����ػ�ģʽ����RTC���ڼ���״̬���ػ�ģʽ����ͨ��
	VDD_EXT��������⣬�ڹػ�ģʽ�´��������Ϊ�͵�ƽ��
	*/
	
	/* Ӳ���ػ� */
	PWRKEY_0();
	bsp_DelayMS(1500);
	PWRKEY_1();	
		
}

/*
*********************************************************************************************************
*	�� �� ��: SIM800_WaitResponse
*	����˵��: �ȴ�SIM800����ָ����Ӧ���ַ���. ����ȴ� OK
*	��    ��: _pAckStr : Ӧ����ַ����� ���Ȳ��ó���255
*			 _usTimeOut : ����ִ�г�ʱ��0��ʾһֱ�ȴ�. >����ʾ��ʱʱ�䣬��λ1ms
*	�� �� ֵ: 1 ��ʾ�ɹ�  0 ��ʾʧ��
*********************************************************************************************************
*/
uint8_t SIM800_WaitResponse(char *_pAckStr, uint16_t _usTimeOut)
{
	uint8_t ucData;
	uint8_t ucRxBuf[256];
	uint16_t pos = 0;
	uint32_t len;
	uint8_t ret;

	len = strlen(_pAckStr);
	if (len > 255)
	{
		return 0;
	}

	/* _usTimeOut == 0 ��ʾ���޵ȴ� */
	if (_usTimeOut > 0)
	{
		bsp_StartTimer(SIM800_TMR_ID, _usTimeOut);		/* ʹ��������ʱ��3����Ϊ��ʱ���� */
	}
	while (1)
	{
		bsp_Idle();				/* CPU����ִ�еĲ����� �� bsp.c �� bsp.h �ļ� */

		if (_usTimeOut > 0)
		{
			if (bsp_CheckTimer(SIM800_TMR_ID))
			{
				ret = 0;	/* ��ʱ */
				break;
			}
		}

		if (comGetChar(COM_SIM800, &ucData))
		{
			SIM800_PrintRxData(ucData);		/* �����յ����ݴ�ӡ�����Դ���1 */

			if (ucData == '\n')
			{
				if (pos > 0)	/* ��2���յ��س����� */
				{
					if (memcmp(ucRxBuf, _pAckStr,  len) == 0)
					{
						ret = 1;	/* �յ�ָ����Ӧ�����ݣ����سɹ� */
						break;
					}
					else
					{
						pos = 0;
					}
				}
				else
				{
					pos = 0;
				}
			}
			else
			{
				if (pos < sizeof(ucRxBuf))
				{
					/* ֻ����ɼ��ַ� */
					if (ucData >= ' ')
					{
						ucRxBuf[pos++] = ucData;
					}
				}
			}
		}
	}
	return ret;
}

/*
*********************************************************************************************************
*	�� �� ��: SIM800_ReadResponse
*	����˵��: ��ȡSIM800����Ӧ���ַ������ú��������ַ��䳬ʱ�жϽ����� ��������Ҫ����AT����ͺ�����
*	��    ��: _pBuf : ���ģ�鷵�ص������ַ���
*			  _usBufSize : ��������󳤶�
*			 _usTimeOut : ����ִ�г�ʱ��0��ʾһֱ�ȴ�. >0 ��ʾ��ʱʱ�䣬��λ1ms
*	�� �� ֵ: 0 ��ʾ���󣨳�ʱ��  > 0 ��ʾӦ������ݳ���
*********************************************************************************************************
*/
uint16_t SIM800_ReadResponse(char *_pBuf, uint16_t _usBufSize, uint16_t _usTimeOut)
{
	uint8_t ucData;
	uint16_t pos = 0;
	uint8_t ret;
	uint8_t status = 0;		/* ����״̬ */

	/* _usTimeOut == 0 ��ʾ���޵ȴ� */
	if (_usTimeOut > 0)
	{
		bsp_StartTimer(SIM800_TMR_ID, _usTimeOut);		/* ʹ��������ʱ����Ϊ��ʱ���� */
	}
	while (1)
	{
		bsp_Idle();				/* CPU����ִ�еĲ����� �� bsp.c �� bsp.h �ļ� */

		if (status == 2)		/* ���ڽ�����ЧӦ��׶Σ�ͨ���ַ��䳬ʱ�ж����ݽ������ */
		{
			if (bsp_CheckTimer(SIM800_TMR_ID))
			{
				_pBuf[pos]	 = 0;	/* ��β��0�� ���ں���������ʶ���ַ������� */
				ret = pos;		/* �ɹ��� �������ݳ��� */
				break;
			}
		}
		else
		{
			if (_usTimeOut > 0)
			{
				if (bsp_CheckTimer(SIM800_TMR_ID))
				{
					ret = 0;	/* ��ʱ */
					break;
				}
			}
		}
		
		if (comGetChar(COM_SIM800, &ucData))
		{			
			SIM800_PrintRxData(ucData);		/* �����յ����ݴ�ӡ�����Դ���1 */

			switch (status)
			{
				case 0:			/* ���ַ� */
					if (ucData == AT_CR)		/* ������ַ��ǻس�����ʾ AT������� */
					{
						_pBuf[pos++] = ucData;		/* ������յ������� */
						status = 2;	 /* ��Ϊ�յ�ģ��Ӧ���� */
					}
					else	/* ���ַ��� A ��ʾ AT������� */
					{
						status = 1;	 /* �����������͵�AT�����ַ�����������Ӧ�����ݣ�ֱ������ CR�ַ� */
					}
					break;
					
				case 1:			/* AT������Խ׶�, ����������. �����ȴ� */
					if (ucData == AT_CR)
					{
						status = 2;
					}
					break;
					
				case 2:			/* ��ʼ����ģ��Ӧ���� */
					/* ֻҪ�յ�ģ���Ӧ���ַ���������ַ��䳬ʱ�жϽ�������ʱ�����ܳ�ʱ�������� */
					bsp_StartTimer(SIM800_TMR_ID, 5);
					if (pos < _usBufSize - 1)
					{
						_pBuf[pos++] = ucData;		/* ������յ������� */
					}
					break;
			}
		}
	}
	return ret;
}

/*
*********************************************************************************************************
*	�� �� ��: SIM800_SendAT
*	����˵��: ��GSMģ�鷢��AT��� �������Զ���AT�ַ���������<CR>�ַ�
*	��    ��: _Str : AT�����ַ�����������ĩβ�Ļس�<CR>. ���ַ�0����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SIM800_SendAT(char *_Cmd)
{
	comClearRxFifo(COM_SIM800);	/* ���㴮�ڽ��ջ����� */	
	
	comSendBuf(COM_SIM800, (uint8_t *)_Cmd, strlen(_Cmd));
	comSendBuf(COM_SIM800, "\r", 1);
}

/*
*********************************************************************************************************
*	�� �� ��: SIM800_SetEarVolume
*	����˵��: ���ö�������
*	��    ��: _ucVolume : ������ 0 - 100, 0��ʾ��С����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SIM800_SetEarVolume(uint8_t _ucVolume)
{
	char CmdBuf[32];

	sprintf(CmdBuf, "AT+CLVL=%d", _ucVolume);
	SIM800_SendAT(CmdBuf);
}

/*
*********************************************************************************************************
*	�� �� ��: SIM800_SetMicGain
*	����˵��: ����MIC ���� .    ���ú����·ͨ���������ã���ֻ�����м���绰ǰʹ�á�
*	��    ��: _Channel : 0 ����Ƶͨ��  1 ������Ƶͨ��
*				_iGain : ���档 0-15  ; 0��ʾ0dB; 15��ʾ+22.5dB
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SIM800_SetMicGain(uint16_t _Channel, uint16_t _iGain)
{
	char CmdBuf[32];
		
	sprintf(CmdBuf, "AT+CMIC=%d,%d", _Channel, _iGain);
	SIM800_SendAT(CmdBuf);
	
	
}


/*
*********************************************************************************************************
*	�� �� ��: SIM800_DialTel
*	����˵��: ����绰
*	��    ��: _pTel �绰�����ַ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SIM800_DialTel(char *_pTel)
{
	char CmdBuf[32];

	sprintf(CmdBuf, "ATD%s;", _pTel);
	SIM800_SendAT(CmdBuf);
}

/*
*********************************************************************************************************
*	�� �� ��: SIM800_Hangup
*	����˵��: �Ҷϵ绰
*	��    ��: _pTel �绰�����ַ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SIM800_Hangup(void)
{
	SIM800_SendAT("ATH");
}

/*
*********************************************************************************************************
*	�� �� ��: SIM800_AnswerIncall
*	����˵��: ��������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SIM800_AnswerIncall(void)
{
	SIM800_SendAT("ATA");
}

/*
*********************************************************************************************************
*	�� �� ��: SIM800_GetHardInfo
*	����˵��: ��ȡģ���Ӳ����Ϣ. ���� ATI ����Ӧ������
*	��    ��: ��Ž���Ľṹ��ָ��
*	�� �� ֵ: 1 ��ʾ�ɹ��� 0 ��ʾʧ��
*********************************************************************************************************
*/
uint8_t SIM800_GetHardInfo(SIM800_INFO_T *_pInfo)
{
	/*
	ATI
	SIM800 R13.08

	OK
	
	AT+GSV
	SIMCOM_Ltd
	SIMCOM_SIM800
	Revision:1308B05SIM800M32
	*/	
	char buf[64];
	uint16_t len;
	char *p1, *p2;
	
	SIM800_SendAT("ATI");		/* ���� ATI ���� */	
	len = SIM800_ReadResponse(buf, sizeof(buf), 300);	/* ��ʱ 300ms */
	if (len == 0)
	{
		return 0;		
	}
	
	/* ��������Ϣ�涨��д SIMCOM_Ltd */
	sprintf(_pInfo->Manfacture, "SIMCOM_Ltd");	
	_pInfo->Model[0] = 0;
	_pInfo->Revision[0] = 0;

	/* Ӧ������ǰ2���ַ��� \r\n */
	p1 = buf;
	p2 = strchr(p1, '\n');
	
	/* �����ͺ� */
	p1 = p2 + 1;
	p2 = strchr(p1, ' ');
	len = p2 - p1;
	if (len >= sizeof(_pInfo->Model))
	{
		len = sizeof(_pInfo->Model) - 1;
	}		
	memcpy(_pInfo->Model, p1, len);
	_pInfo->Model[len] = 0;

	/* �����ͺ� */
	p1 = p2 + 1;
	p2 = strchr(p1, '\r');
	len = p2 - p1;
	if (len >= sizeof(_pInfo->Revision))
	{
		len = sizeof(_pInfo->Revision) - 1;
	}				
	memcpy(_pInfo->Revision, p1, len);
	_pInfo->Revision[len] = 0;	
	
	return 1;
}

/*
*********************************************************************************************************
*	�� �� ��: MG323_GetNetStatus
*	����˵��: ��ѯ��ǰ����״̬
*	��    ��: ��
*	�� �� ֵ: ����״̬, CREG_NO_REG, CREG_LOCAL_OK �ȡ�
*********************************************************************************************************
*/
uint8_t SIM800_GetNetStatus(void)
{
	/*
		AT+CREG?
		+CREG: 0,1
		
		OK				
	*/	
	char buf[128];
	uint16_t len, i;
	uint8_t status = 0;
	
	SIM800_SendAT("AT+CREG?");	/* ���� AT ���� */
	
	len = SIM800_ReadResponse(buf, sizeof(buf), 200);	/* ��ʱ 200ms */
	if (len == 0)
	{
		return 0;		
	}
	
	for (i = 2; i < len; i++)
	{
		if (buf[i] == ',')
		{
			i++;
			
			status = buf[i] - '0';
			break;
		}
	}
	return status;
}


/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
