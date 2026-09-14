/**
 * @file bsp_uart_esp8266.c
 * @brief 安富莱 BSP 通用驱动（ESP8266 WiFi）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : ESP8266 ����WIFIģ����������
*	�ļ����� : bsp_esp8266.c
*	��    �� : V1.3
*	˵    �� : ��װ ESP8266 ģ����ص�AT����
*
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2014-11-29  armfly  ��ʽ����
*		V1.1    2014-12-11  armfly  �޸� ESP8266_WaitResponse() ����, ʵ�������ַ��жϡ�����TCP���ݷ��ͺ���.
*		V1.2    2014-12-22  armfly  ����GPIO2�� GPIO0 ���ŵ����á���Ӧ�°�Ӳ����
*		V1.3	2015-07-24  armfly	
*					(1) ���Ӻ��� uint8_t ESP8266_CreateTCPServer(void);
*					(2) �޸�ESP8266_JoinAP() ���ӷ���ֵ
*
*	Copyright (C), 2014-2015, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/* ESP8266 ģ�����ͼ
	ESP8266ģ��    STM32-V5������

		UTXD   ---  PC7/USART6_RX
		GND    ---  GND
		CH_PD  ---  PI0  (��3.3V �� IO����ģ����磬 0��ʾ����  1��ʾ�����ϵ繤����
		GPIO2  ---  PG15 (��3.3V �� IO����Ϊ��, ����)
		GPIO16 ---  PB7  (��3.3V �� IO���� wifi Ӳ����λ)
		GPIO0  ---  PG8  (��3.3V �� IO��������ģʽ��0��������ϵͳ������1��ʾ���������û�����ATָ�)
		VCC    ---  3.3  (����)
		URXD   ---  PG14/USART6_TX

	ESP8266ģ��    STM32-V6������ ��ʹ����չIO��
		UTXD   ---  PC7/USART6_RX
		GND    ---  GND
		CH_PD  ---  GPRS_TERM_ON  (��3.3V �� IO����ģ����磬 0��ʾ����  1��ʾ�����ϵ繤����
		GPIO2  ---  ESP8266_G2 (��3.3V �� IO����Ϊ��, ����)
		GPIO16 ---  GPRS_RESET  (��3.3V �� IO���� wifi Ӳ����λ)
		GPIO0  ---  ESP8266_G0  (��3.3V �� IO��������ģʽ��0��������ϵͳ������1��ʾ���������û�����ATָ�)
		VCC    ---  3.3  (����)
		URXD   ---  PG14/USART6_TX

	ģ��ȱʡ������ 9600;  ֧�ֵķ�Χ��110~460800bps          ---- �����ӻὫģ�鲨�����л�Ϊ 115200
	�ڰ����ϵ��ʼ��boot rom��һ��log����Ҫ�� 74880 �Ĳ�������������ӡ�������Ǵ�ӡ����������.

	----------- PD = 1 ֮�� 74880bps ��ӡ�������� ----------

	 ets Jan  8 2013,rst cause:1, boot mode:(3,6)

	load 0x40100000, len 25052, room 16
	tail 12
	chksum 0x0b
	ho 0 tail 12 room 4
	load 0x3ffe8000, len 3312, room 12
	tail 4
	chksum 0x53
	load 0x3ffe8cf0, len 6576, room 4
	tail 12
	chksum 0x0d
	csum 0x0d

	----------- ֮���� 9600bps ��ӡ ---------------

	[Vendor:www.ai-thinker.com Version:0.9.2.4]

	ready


	ʹ�ô��ڳ����ն�����ʱ����Ҫ���� �ն� - ���� - ģʽ ҳ�湴ѡ������ģʽ��.


	���޸Ĳ����ʡ�
	AT+CIOBAUD=?     ---- ��ѯ�������
	+CIOBAUD:(9600-921600)

	OK

	AT+CIOBAUD=115200
	BAUD->115200

	��ѡ�� WIFI Ӧ��ģʽ ��
	AT+CWMODE=1
		1   Station ģʽ
		2   AP ģʽ
		3   AP �� Station ģʽ

	���г���ǰ���� AP��
	AT+CWLAP=<ssid>,< mac >,<ch>
	AT+CWLAP

	��AT+CWJAP���� AP��
	AT+CWJAP=<ssid>,< pwd >

*/

#define AT_CR		'\r'
/* 宏定义: AT_LF - AT_LF 的宏定义。 */
#define AT_LF		'\n'

/* ������У���ʾʹ����չ��IO */
#define USE_HC574

#ifdef USE_HC574	/* ʹ����չIO */
	/* Ӳ������������� -- �� 3.3V ��ʼ����  */
	#define ESP_CH_PD_0()	HC574_SetPin(GPRS_TERM_ON, 0);
/* 宏定义: ESP_CH_PD_1 - ESP_CH_PD_1 的宏定义。 */
	#define ESP_CH_PD_1()	HC574_SetPin(GPRS_TERM_ON, 1);

	/* Ӳ����λ���� -- ���Բ��� */
	#define ESP_RESET_0()	HC574_SetPin(GPRS_RESET, 0);
/* 宏定义: ESP_RESET_1 - ESP_RESET_1 的宏定义。 */
	#define ESP_RESET_1()	HC574_SetPin(GPRS_RESET, 1);

	/* 1��ʾ����̼�����ģʽ 0��ʾ����ATָ��ģʽ */
	#define ESP_GPIO0_0()	HC574_SetPin(ESP8266_G0, 0);
/* 宏定义: ESP_GPIO0_1 - ESP_GPIO0_1 的宏定义。 */
	#define ESP_GPIO0_1()	HC574_SetPin(ESP8266_G0, 1);
	#define ESP_ENTER_ISP()	ESP_GPIO0_0()  /* ����̼�����ģʽ */
	#define ESP_EXIT_ISP()	ESP_GPIO0_1()	/* �˳��̼�����ģʽ */

	/* ����Ϊ�ߣ���������; */
	#define ESP_GPIO2_0()	HC574_SetPin(ESP8266_G2, 0);
/* 宏定义: ESP_GPIO2_1 - ESP_GPIO2_1 的宏定义。 */
	#define ESP_GPIO2_1()	HC574_SetPin(ESP8266_G2, 1);
#else	/* ʹ��CPU GPIO */
	#define RCC_CH_PD 		RCC_AHB1Periph_GPIOI
/* 宏定义: PORT_CH_PD - PORT_CH_PD 的宏定义。 */
	#define PORT_CH_PD		GPIOI
/* 宏定义: PIN_CH_PD - PIN_CH_PD 的宏定义。 */
	#define PIN_CH_PD		GPIO_PIN_0

/* 宏定义: RCC_RESET - RCC_RESET 的宏定义。 */
	#define RCC_RESET 		RCC_AHB1Periph_GPIOB
/* 宏定义: PORT_RESET - PORT_RESET 的宏定义。 */
	#define PORT_RESET		GPIOB
/* 宏定义: PIN_RESET - PIN_RESET 的宏定义。 */
	#define PIN_RESET		GPIO_PIN_7

/* 宏定义: RCC_GPIO2 - RCC_GPIO2 的宏定义。 */
	#define RCC_GPIO2 		RCC_AHB1Periph_GPIOG
/* 宏定义: PORT_GPIO2 - PORT_GPIO2 的宏定义。 */
	#define PORT_GPIO2		GPIOG
/* 宏定义: PIN_GPIO2 - PIN_GPIO2 的宏定义。 */
	#define PIN_GPIO2		GPIO_PIN_15

/* 宏定义: RCC_GPIO0 - RCC_GPIO0 的宏定义。 */
	#define RCC_GPIO0 		RCC_AHB1Periph_GPIOG
/* 宏定义: PORT_GPIO0 - PORT_GPIO0 的宏定义。 */
	#define PORT_GPIO0		GPIOG
/* 宏定义: PIN_GPIO0 - PIN_GPIO0 的宏定义。 */
	#define PIN_GPIO0		GPIO_PIN_8

	/* Ӳ������������� -- �� 3.3V ��ʼ����  */
	#define ESP_CH_PD_0()	GPIO_ResetBits(PORT_CH_PD, PIN_CH_PD);
/* 宏定义: ESP_CH_PD_1 - ESP_CH_PD_1 的宏定义。 */
	#define ESP_CH_PD_1()	GPIO_SetBits(PORT_CH_PD, PIN_CH_PD);

	/* Ӳ����λ���� -- ���Բ��� */
	#define ESP_RESET_0()	GPIO_ResetBits(PORT_RESET, PIN_RESET);
/* 宏定义: ESP_RESET_1 - ESP_RESET_1 的宏定义。 */
	#define ESP_RESET_1()	GPIO_SetBits(PORT_RESET, PIN_RESET);

	/* 1��ʾ����̼�����ģʽ 0��ʾ����ATָ��ģʽ */
	#define ESP_GPIO0_0()	GPIO_ResetBits(PORT_GPIO0, PIN_GPIO0);
/* 宏定义: ESP_GPIO0_1 - ESP_GPIO0_1 的宏定义。 */
	#define ESP_GPIO0_1()	GPIO_SetBits(PORT_GPIO0, PIN_GPIO0);
	#define ESP_ENTER_ISP()	ESP_GPIO0_0()  /* ����̼�����ģʽ */
	#define ESP_EXIT_ISP()	ESP_GPIO0_1()	/* �˳��̼�����ģʽ */

	/* ����Ϊ�ߣ���������; */
	#define ESP_GPIO2_0()	GPIO_ResetBits(PORT_GPIO2, PIN_GPIO2);
/* 宏定义: ESP_GPIO2_1 - ESP_GPIO2_1 的宏定义。 */
	#define ESP_GPIO2_1()	GPIO_SetBits(PORT_GPIO2, PIN_GPIO2);
#endif

char g_EspBuf[2048];	/* ���ڽ��� */

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitESP8266
*	����˵��: ��������ģ����ص�GPIO,  �ú����� bsp_Init() ���á�
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitESP8266(void)
{
#ifdef USE_HC574	/* ʹ����չIO */
	;
#else	
	GPIO_InitTypeDef GPIO_InitStructure;

	/* ��GPIOʱ�� */
	RCC_AHB1PeriphClockCmd(RCC_RESET | RCC_CH_PD | RCC_GPIO0 | RCC_GPIO2, ENABLE);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;		/* ��Ϊ����� */
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;		/* ��Ϊ����ģʽ */
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;	/* ���������費ʹ�� */
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;	/* IO������ٶ� */

	GPIO_InitStructure.GPIO_Pin = PIN_CH_PD;
	GPIO_Init(PORT_CH_PD, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = PIN_RESET;
	GPIO_Init(PORT_RESET, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = PIN_GPIO0;
	GPIO_Init(PORT_GPIO0, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = PIN_GPIO2;
	GPIO_Init(PORT_GPIO2, &GPIO_InitStructure);	
#endif

	ESP_GPIO2_1();
	
	/* CPU�Ĵ��������Ѿ��� bsp_uart_fifo.c �е� bsp_InitUart() ���� */
	ESP_CH_PD_0();

	ESP_EXIT_ISP();
	
	//ESP8266_Reset();

	comSetBaud(COM_ESP8266,115200);
}

/*
*********************************************************************************************************
*	�� �� ��: ESP8266_PrintRxData
*	����˵��: ��ӡSTM32��ESP8266�յ������ݵ�COM1���ڣ���Ҫ���ڸ��ٵ���
*	��    ��: _ch : �յ�������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ESP8266_PrintRxData(uint8_t _ch)
{
	#ifdef ESP8266_TO_COM1_EN
		comSendChar(COM1, _ch);		/* �����յ����ݴ�ӡ�����Դ���1 */
	#endif
}

/*
*********************************************************************************************************
*	�� �� ��: ESP8266_PowerOn
*	����˵��: ��ESP8266ģ���ϵ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ESP8266_PowerOn(void)
{
	/* WIFIģ���ϵ�ʱ������74880�����ʴ�ӡ������Ϣ:
		 ets Jan  8 2013,rst cause:1, boot mode:(3,6)

		load 0x40100000, len 25052, room 16 
		tail 12
		chksum 0x0b
		ho 0 tail 12 room 4
		load 0x3ffe8000, len 3312, room 12 
		tail 4
		chksum 0x53
		load 0x3ffe8cf0, len 6576, room 4 
		tail 12
		chksum 0x0d
		csum 0x0d	    <-----  ����ʶ�� csum �����Զ��л������������� 
	*/

	ESP_CH_PD_0();

	comSetBaud(COM_ESP8266, 74880);	/* ���ö˿ڲ�����Ϊ 74880 */

	ESP_CH_PD_1();

	ESP8266_Reset();

	/* �ȴ�ģ������ϵ磬��ʱ500ms �Զ��˳� */
	ESP8266_WaitResponse("csum", 1000);	/* �ȵȴ� csum */
	ESP8266_WaitResponse("\n", 1000);	/* �ٵȴ��س������ַ����� */

	comSetBaud(COM_ESP8266,115200);	/* 1��ʾӲ������CRS RTS��Ч;  0��ʾ����Ӳ������ */

	/* �ȴ�ģ������ϵ磬�ж��Ƿ���յ� ready */
	ESP8266_WaitResponse("ready", 5000);
	
	// ESP8266_SendAT("AT+RST");
	// ESP8266_WaitResponse("ready", 5000);
}

/*
*********************************************************************************************************
*	�� �� ��: ESP8266_PowerOff
*	����˵��: ����ESP8266ģ��ػ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ESP8266_PowerOff(void)
{
	ESP_CH_PD_0();
}

/*
*********************************************************************************************************
*	�� �� ��: ESP8266_PowerOn
*	����˵��: ��ESP8266ģ���ϵ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ESP8266_Reset(void)
{
	ESP_RESET_0();
	bsp_DelayMS(20);
	ESP_RESET_1();

	bsp_DelayMS(10);
}

/*
*********************************************************************************************************
*	�� �� ��: ESP8266_EnterISP
*	����˵��: ����ESP8266ģ�����̼�����ģʽ
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ESP8266_EnterISP(void)
{
	ESP_CH_PD_0();
	ESP_GPIO0_0()  /* 0 ��ʾ����̼�����ģʽ */
	ESP_CH_PD_1();
}

/*
*********************************************************************************************************
*	�� �� ��: ESP8266_ExitISP
*	����˵��: ����ESP8266ģ���˳��̼�����ģʽ
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ESP8266_ExitISP(void)
{
	ESP_CH_PD_0();
	ESP_GPIO0_1()  /* 1 ��ʾ�����û�����ATָ�ģʽ */
	ESP_CH_PD_1();
}

/*
*********************************************************************************************************
*	�� �� ��: ESP8266_WaitResponse
*	����˵��: �ȴ�ESP8266����ָ����Ӧ���ַ���, ���԰��������ַ���ֻҪ������ȫ���ɷ��ء�
*	��    ��: _pAckStr : Ӧ����ַ����� ���Ȳ��ó���255
*			 _usTimeOut : ����ִ�г�ʱ��0��ʾһֱ�ȴ�. >����ʾ��ʱʱ�䣬��λ1ms
*	�� �� ֵ: 1 ��ʾ�ɹ�  0 ��ʾʧ��
*********************************************************************************************************
*/
uint8_t ESP8266_WaitResponse(char *_pAckStr, uint16_t _usTimeOut)
{
	uint8_t ucData;
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
		bsp_StartTimer(ESP8266_TMR_ID, _usTimeOut);		/* ʹ��������ʱ��3����Ϊ��ʱ���� */
	}
	while (1)
	{
		bsp_Idle();				/* CPU����ִ�еĲ����� �� bsp.c �� bsp.h �ļ� */

		if (_usTimeOut > 0)
		{
			if (bsp_CheckTimer(ESP8266_TMR_ID))
			{
				ret = 0;	/* ��ʱ */
				break;
			}
		}

		if (comGetChar(COM_ESP8266, &ucData))
		{
			ESP8266_PrintRxData(ucData);		/* �����յ����ݴ�ӡ�����Դ���1 */

			if (ucData == _pAckStr[pos])
			{
				pos++;
				
				if (pos == len)
				{
					ret = 1;	/* �յ�ָ����Ӧ�����ݣ����سɹ� */
					break;
				}
			}
			else
			{
				pos = 0;
			}
		}
	}
	return ret;
}

/*
*********************************************************************************************************
*	�� �� ��: ESP8266_ReadLine
*	����˵��: ��ȡESP8266���ص�һ��Ӧ���ַ���(0x0D 0x0A����)���ú��������ַ��䳬ʱ�жϽ����� ��������Ҫ����AT����ͺ�����
*	��    ��: _pBuf : ���ģ�鷵�ص������ַ���
*			  _usBufSize : ��������󳤶�
*			 _usTimeOut : ����ִ�г�ʱ��0��ʾһֱ�ȴ�. >0 ��ʾ��ʱʱ�䣬��λ1ms
*	�� �� ֵ: 0 ��ʾ���󣨳�ʱ��  > 0 ��ʾӦ������ݳ���
*********************************************************************************************************
*/
uint16_t ESP8266_ReadLine(char *_pBuf, uint16_t _usBufSize, uint16_t _usTimeOut)
{
	uint8_t ucData;
	uint16_t pos = 0;
	uint8_t ret;

	/* _usTimeOut == 0 ��ʾ���޵ȴ� */
	if (_usTimeOut > 0)
	{
		bsp_StartTimer(ESP8266_TMR_ID, _usTimeOut);		/* ʹ��������ʱ����Ϊ��ʱ���� */
	}
	while (1)
	{
		bsp_Idle();				/* CPU����ִ�еĲ����� �� bsp.c �� bsp.h �ļ� */

		if (bsp_CheckTimer(ESP8266_TMR_ID))
		{
			_pBuf[pos] = 0;	/* ��β��0�� ���ں���������ʶ���ַ������� */
			ret = pos;		/* �ɹ��� �������ݳ��� */
			break;
		}

		if (comGetChar(COM_ESP8266, &ucData))
		{
			ESP8266_PrintRxData(ucData);		/* �����յ����ݴ�ӡ�����Դ���1 */

			bsp_StartTimer(ESP8266_TMR_ID, 500);
			_pBuf[pos++] = ucData;		/* ������յ������� */
			if (ucData == 0x0A)
			{
				_pBuf[pos] = 0;
				ret = pos;		/* �ɹ��� �������ݳ��� */
				break;
			}
		}
	}
	return ret;
}

/*
*********************************************************************************************************
*	�� �� ��: ESP8266_SendAT
*	����˵��: ��ģ�鷢��AT��� �������Զ���AT�ַ���������<CR>�ַ�
*	��    ��: _Str : AT�����ַ�����������ĩβ�Ļس�<CR>. ���ַ�0����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ESP8266_SendAT(char *_Cmd)
{
	comSendBuf(COM_ESP8266, (uint8_t *)_Cmd, strlen(_Cmd));
	comSendBuf(COM_ESP8266, "\r\n", 2);
}

/*
*********************************************************************************************************
*	�� �� ��: ESP8266_CreateTCPServer
*	����˵��: ����һ��TCP����ˡ� ���������ӵ�AP֮����С�
*	��    �Σ�_TcpPort : TCP �˿ں�
*	�� �� ֵ: 0 ��ʾʧ�ܡ� 1��ʾ����TCP�ɹ�
*********************************************************************************************************
*/
uint8_t ESP8266_CreateTCPServer(uint16_t _TcpPort)
{
	char cmd_buf[30];
	
	ESP8266_SendAT("AT+CIPMUX=1");	/* ���������� */
	if (ESP8266_WaitResponse("OK", 2000) == 0)
	{
		return 0;
	}
	
	/* ����TCP server, �˿�Ϊ _TcpPort */
	sprintf(cmd_buf, "AT+CIPSERVER=1,%d", _TcpPort);
	ESP8266_SendAT(cmd_buf);	
	if (ESP8266_WaitResponse("OK", 2000) == 0)
	{
		return 0;
	}

	ESP8266_SendAT("ATE0");		/* �رջ��Թ��ܣ��������͵��ַ���ģ�����践�� */
	if (ESP8266_WaitResponse("OK", 10000) == 0)
	{
		return 0;
	}
	
	return 1;
}


/*
*********************************************************************************************************
*	�� �� ��: ESP8266_SendTcpUdp
*	����˵��: ����TCP��UDP���ݰ�
*	��    ��: _databuf ����
*			 _len ���ݳ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ESP8266_SendTcpUdp(uint8_t *_databuf, uint16_t _len)
{
	char buf[32];

	if (_len > 2048)
	{
		_len = 2048;
	}

	sprintf(buf, "AT+CIPSEND=0,%d\r\n", _len);
	comSendBuf(COM_ESP8266, (uint8_t *)buf, strlen(buf));

	ESP8266_WaitResponse(">", 1000);

	comSendBuf(COM_ESP8266, _databuf, _len);
	ESP8266_WaitResponse("SEND OK", 8000);
}

/*
*********************************************************************************************************
*	�� �� ��: ESP8266_CloseTcpUdp
*	����˵��: �ر�TCP��UDP����. ���ڶ�·����
*	��    ��: _databuf ����
*			 _len ���ݳ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ESP8266_CloseTcpUdp(uint8_t _id)
{
	char buf[32];

	ESP8266_SendAT("ATE0");		/* �򿪻��Թ��� */
	ESP8266_WaitResponse("SEND OK", 50);
	
	sprintf(buf, "AT+CIPCLOSE=%d", _id);
	ESP8266_SendAT(buf);
}

/*
*********************************************************************************************************
*	�� �� ��: ESP8266_GetLocalIP
*	����˵��: ��ѯ����IP��ַ��MAC
*	��    ��: _ssid : AP�����ַ���
*			  _pwd : �����ַ���
*	�� �� ֵ: 1 ��ʾOK�� 0 ��ʾδ֪
*********************************************************************************************************
*/
uint8_t ESP8266_GetLocalIP(char *_ip, char *_mac)
{
	char buf[64];
	uint8_t i, m;
	uint8_t ret = 0;
	uint8_t temp;
	
	ESP8266_SendAT("AT+CIFSR");
	
	/*��ģ�齫Ӧ��:
		
	+CIFSR:STAIP,"192.168.1.18"
	+CIFSR:STAMAC,"18:fe:34:a6:44:75"
	
	OK	
	*/
	
	_ip[0] = 0;
	_mac[0] = 0;
	for (i = 0; i < 4; i++)
	{
		ESP8266_ReadLine(buf, sizeof(buf), 500);
		if (memcmp(buf, "+CIFSR:STAIP", 12) == 0)
		{
			
			for (m = 0; m < 20; m++)
			{
				temp = buf[14 + m];
				_ip[m] = temp;
				if (temp == '"')
				{
					_ip[m] = 0;
					ret = 1;
					break;
				}
			}
		}
		else if (memcmp(buf, "+CIFSR:STAMAC,", 14) == 0)
		{
			for (m = 0; m < 20; m++)
			{
				temp = buf[15 + m];
				_mac[m] = temp;
				if (temp == '"')
				{
					_mac[m] = 0;
					break;
				}
			}
		}
		else if (memcmp(buf, "OK", 2) == 0)
		{
			break;
		}
	}
	return ret;
}

/*
*********************************************************************************************************
*	�� �� ��: ESP8266_JoinAP
*	����˵��: ����AP
*	��    ��: _ssid : AP�����ַ���
*			  _pwd : �����ַ���
*	�� �� ֵ: 1 ��ʾ 0K  0 ��ʾʧ��
*********************************************************************************************************
*/
uint8_t ESP8266_JoinAP(char *_ssid, char *_pwd, uint16_t _timeout)
{
	char buf[64];

	sprintf(buf, "AT+CWJAP=\"%s\",\"%s\"", _ssid, _pwd);
	ESP8266_SendAT(buf);
	
	if (ESP8266_ReadLine(buf, 64, _timeout))
	{
		if (memcmp(buf, "AT+CWJAP", 8) == 0)		/* ��1�ζ������� ����� */
		{
			ESP8266_ReadLine(buf, 64, _timeout);	/* ����ǻس� */
			ESP8266_ReadLine(buf, 64, _timeout);	/* ����Ƕ�Ӧ���OK */
		}		
		if (memcmp(buf, "OK", 2) == 0)
		{
			return 1;
		}
	}	
	return 0;
}

/*
*********************************************************************************************************
*	�� �� ��: ESP8266_QuitAP
*	����˵��: �˳���ǰ��AP����
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ESP8266_QuitAP(void)
{
	ESP8266_SendAT("AT+ CWQAP");
}

/*
*********************************************************************************************************
*	�� �� ��: ESP8266_ScanAP
*	����˵��: ɨ��AP����������_pList �ṹ������. �˺�����ռ���5��ʱ�䡣ֱ���յ�OK��ERROR��
*	��    ��: _pList : AP�б�����;  
*			  _MaxNum : ���������AP��������Ҫ�Ƿ�ֹ�����������
*	�� �� ֵ: -1 ��ʾʧ��; 0 ��ʾ������0��; 1��ʾ1����
*********************************************************************************************************
*/
int16_t ESP8266_ScanAP(WIFI_AP_T *_pList, uint16_t _MaxNum)
{
	uint16_t i;
	uint16_t count;
	char buf[128];
	WIFI_AP_T *p;
	char *p1, *p2;
	uint16_t timeout;

	buf[127] = 0;
	ESP8266_SendAT("AT+CWLAP");
	
	p = (WIFI_AP_T *)_pList;
	count = 0;
	timeout = 8000;
	for (i = 0; i < _MaxNum; i++)
	{
		ESP8266_ReadLine(buf, 128, timeout);
		if (memcmp(buf, "OK", 2) == 0)
		{
			break;
		}
		else if (memcmp(buf, "ERROR", 5) == 0)
		{
			break;
		}
		else if (memcmp(buf, "+CWLAP:", 7) == 0)
		{		
			p1 = buf;
			
			/* +CWLAP:(4,"BaiTu",-87,"9c:21:6a:3c:89:52",1) */
			/* �������ܷ�ʽ */
			p1 = strchr(p1, '(');	/* ������(*/
			p1++;
			p->ecn = str_to_int(p1);
			
			/* ����ssid */
			p1 = strchr(p1, '"');	/* ��������1���ֺ� */
			p1++;
			p2 = strchr(p1, '"');	/* ��������2���ֺ� */
			memcpy(p->ssid, p1, p2 - p1);
			p->ssid[p2 - p1] = 0;
			
			/* ���� rssi */
			p1 = strchr(p2, ',');	/* ����������*/
			p1++;
			p->rssi = str_to_int(p1);
			
			/* ����mac */
			p1 = strchr(p1, '"');	/* �������ֺ�*/
			p1++;
			p2 = strchr(p1, '"');	/* �������ֺ�*/
			memcpy(p->mac, p1, p2 - p1);
			p->mac[p2 - p1] = 0;
			
			/* ����ch */
			p1 = strchr(p2, ',');	/* ����������*/
			p1++;
			p->ch = str_to_int(p1);
			
			/* ��Ч��AP���� */
			count++;
			
			p++;
			
			timeout = 2000;
		}
	}
	
	return count;
}

/*
*********************************************************************************************************
*	�� �� ��: ESP8266_RxNew
*	����˵��: ����������֡ +IPD
*	��    ��: _pRxBuf : ���յ������ݴ���ڴ˻�����
*	�� �� ֵ: ���յ������ݳ���. 0 ��ʾ������
*********************************************************************************************************
*/
uint16_t ESP8266_RxNew(uint8_t *_pRxBuf)
{
	uint8_t ucData;
	static uint8_t s_buf[512];	/* Լ���256 */
	static uint16_t s_len = 0;
	static uint8_t s_flag = 0;
	static uint16_t s_data_len = 0;
	char *p1;
	
	/* +IPD,0,7:ledon 1 */

	if (comGetChar(COM_ESP8266, &ucData))
	{
		ESP8266_PrintRxData(ucData);		/* �����յ����ݴ�ӡ�����Դ���1 */

		if (s_flag == 0)
		{
			if (s_len < sizeof(s_buf))
			{
				s_buf[s_len++] = ucData;		/* ������յ������� */
			}			
			if (ucData == '+')
			{
				s_len = 1;
				s_data_len = 0;
				s_buf[0] = 0;
			}
			if (s_len > 7 && ucData == ':')
			{
				p1 = (char *)&s_buf[7];
				s_data_len = str_to_int(p1);	
				s_flag = 1;	
				s_len = 0;
			}
		}
		else
		{
			if (s_len < sizeof(s_buf))
			{
				s_buf[s_len++] = ucData;		/* ������յ������� */
				
				if (s_len == s_data_len)
				{
					s_flag = 0;
					s_len = 0;
					
					memcpy(_pRxBuf, s_buf, s_data_len);
					
					return s_data_len;
				}
			}	
			else
			{
				s_flag = 0;
			}
		}

	}
	return 0;
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
