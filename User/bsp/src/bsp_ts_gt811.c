/**
 * @file bsp_ts_gt811.c
 * @brief 安富莱 BSP 通用驱动（GT811 触摸）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : ���ݴ���оƬGT811��������
*	�ļ����� : bsp_ct811.c
*	��    �� : V1.0
*	˵    �� : GT811����оƬ��������
*	�޸ļ�¼ :
*		�汾��   ����        ����     ˵��
*		V1.0    2014-12-25  armfly   ��ʽ����
*
*	Copyright (C), 2014-2015, ���������� www.armfly.com
*********************************************************************************************************
*/

#include "bsp.h"

#define GT811_READ_XY_REG 	0x721	/* ����Ĵ��� */
#define GT811_CONFIG_REG	0x6A2	/* ���ò����Ĵ��� */

/* GT811���ò�����һ����д�� */
//const uint8_t s_GT811_CfgParams[]=
uint8_t s_GT811_CfgParams[]=
{
	/*
		0x6A2  R/W  Sen_CH0    ������ 1 �Ÿ�Ӧ�߶�Ӧ�� IC ��Ӧ�� 
		0x6A3  R/W  Sen_CH1    ������ 2 �Ÿ�Ӧ�߶�Ӧ�� IC ��Ӧ��
		0x6A4  R/W  Sen_CH2    ������ 3 �Ÿ�Ӧ�߶�Ӧ�� IC ��Ӧ��
		0x6A5  R/W  Sen_CH3    ������ 4 �Ÿ�Ӧ�߶�Ӧ�� IC ��Ӧ��
		0x6A6  R/W  Sen_CH4    ������ 5 �Ÿ�Ӧ�߶�Ӧ�� IC ��Ӧ��
		0x6A7  R/W  Sen_CH5    ������ 6 �Ÿ�Ӧ�߶�Ӧ�� IC ��Ӧ�� 
		0x6A8  R/W  Sen_CH6    ������ 7 �Ÿ�Ӧ�߶�Ӧ�� IC ��Ӧ��
		0x6A9  R/W  Sen_CH7    ������ 8 �Ÿ�Ӧ�߶�Ӧ�� IC ��Ӧ��
		0x6AA  R/W  Sen_CH8    ������ 9 �Ÿ�Ӧ�߶�Ӧ�� IC ��Ӧ��
		0x6AB  R/W  Sen_CH9    ������ 10 �Ÿ�Ӧ�߶�Ӧ�� IC ��Ӧ��
	*/
    0x12,0x10,0x0E,0x0C,0x0A,0x08,0x06,0x04,0x02,0x00,

	/*
	0x6AC  R/W  Dr0_Con  CHSELEF0  F1DELAY0
	0x6AD  R/W  Dr0_Con  F2DELAY0  F3DELAY0
	
	0x6AE  R/W  Dr1_Con  CHSELEF1  F1DELAY1
	0x6AF  R/W  Dr1_Con  F2DELAY1  F3DELAY1
	
	0x6B0  R/W  Dr2_Con  CHSELEF2  F1DELAY2
	0x6B1  R/W  Dr2_Con  F2DELAY2  F3DELAY2
	
	0x6B2  R/W  Dr3_Con  CHSELEF3  F1DELAY3
	0x6B3  R/W  Dr3_Con  F2DELAY3  F3DELAY3
	
	0x6B4  R/W  Dr4_Con  CHSELEF4  F1DELAY4
	0x6B5  R/W  Dr4_Con  F2DELAY4  F3DELAY4
	
	0x6B6  R/W  Dr5_Con  CHSELEF5  F1DELAY5
	0x6B7  R/W  Dr5_Con  F2DELAY5  F3DELAY5
	
	0x6B8  R/W  Dr6_Con  CHSELEF6  F1DELAY6
	0x6B9  R/W  Dr6_Con  F2DELAY6  F3DELAY6
	
	0x6BA  R/W  Dr7_Con  CHSELEF7  F1DELAY7
	0x6BB  R/W  Dr7_Con  F2DELAY7  F3DELAY7
	
	0x6BC  R/W  Dr8_Con  CHSELEF8  F1DELAY8
	0x6BD  R/W  Dr8_Con  F2DELAY8  F3DELAY8
	
	0x6BE  R/W  Dr9_Con  CHSELEF9  F1DELAY9
	0x6BF  R/W  Dr9_Con  F2DELAY9  F3DELAY9
	
	0x6C0  R/W  Dr10_Con  CHSELEF10  F1DELAY10
	0x6C1  R/W  Dr10_Con  F2DELAY10  F3DELAY10
	
	0x6C2  R/W  Dr11_Con  CHSELEF11  F1DELAY11
	0x6C3  R/W  Dr11_Con  F2DELAY11  F3DELAY11
	
	0x6C4  R/W  Dr12_Con  CHSELEF12  F1DELAY12
	0x6C5  R/W  Dr12_Con  F2DELAY12  F3DELAY12
	
	0x6C6  R/W  Dr13_Con  CHSELEF13  F1DELAY13
	0x6C7  R/W  Dr13_Con  F2DELAY13  F3DELAY13
	
	0x6C8  R/W  Dr14_Con  CHSELEF14  F1DELAY14
	0x6C9  R/W  Dr14_Con  F2DELAY14  F3DELAY14
	
	0x6CA  R/W  Dr15_Con  CHSELEF15  F1DELAY15
	0x6CB  R/W  Dr15_Con  F2DELAY15  F3DELAY15
	*/
	0x05,0x55,0x15,0x55,0x25,0x55,0x35,0x55,0x45,0x55,0x55,0x55,0x65,0x55,0x75,0x55,
	0x85,0x55,0x95,0x55,0xA5,0x55,0xB5,0x55,0xC5,0x55,0xD5,0x55,0xE5,0x55,0xF5,0x55,	
	
	/*
	0x6CC  R/W  ADCCFG  оƬɨ����Ʋ���
	0x6CD  R/W  SCAN    оƬɨ����Ʋ���
	*/
	0x1B,0x03,
	
	/*
	0x6CE  R/W  F1SET  �������� 1 Ƶ��
	0x6CF  R/W  F2SET  �������� 2 Ƶ��
	0x6D0  R/W  F3SET  �������� 3 Ƶ��
	0x6D1  R/W  F1PNUM  �������� 1 ����
	0x6D2  R/W  F2PNUM  �������� 2 ���� 
	0x6D3  R/W  F3PNUM  �������� 3 ����
	*/
	0x00,0x00,0x00,0x13,0x13,0x13,
	
	/* 0x6D4  R/W  TOTALROW  ȫ��ʹ�õ�����ͨ����(����������+����������) */
	0x0F,
	
	/*
	0x6D5  R/W  TSROW  �������ϵ�������
	0x6D6  R/W  TOTALCOL  �������ϵĸ�Ӧ��
	*/
	0x0F,0x0A,
	
	/*
	0x6D7  R/W  Sc_Touch  ��Ļ������ֵ
	0x6D8  R/W  Sc_Leave  ��Ļ�ɼ���ֵ
	*/
	0x50,0x30,
	
	/* 
	0x6D9  R/W  Md_Switch  ����  DD2    R1  R0  INT    SITO    RT    ST 
	0x6DA  R/W  LPower_C  ����  Auto �ް������͹���ʱ�䣬0-63 ��Ч���� s Ϊ��λ
	*/
	0x05,0x03,
	
	/* 0x6DB  R/W  Refresh  ����ˢ�����ʿ��Ʋ�����50Hz~100Hz����0-100 ��Ч */	
	0x64,
	
	/* 0x6DC  R/W  Touch_N  ����  ʹ�ܴ����������1-5 ��Ч */
	0x05,
	
	/* 
	0x6DD  R/W  X_Ou_Max_L X ����������ֵ  480
	0x6DE  R/W  X_Ou_Max_H
	
	0x6DF  R/W  Y_Ou_Max_L  Y ����������ֵ  800
	0x6E0  R/W  Y_Ou_Max_H
	*/
#if 1
	0x58,0x02,
	0x00,0x04,
#else
	0xe0,0x01,
	0x20,0x03,
#endif

	/*
	0x6E1  R/W  X _Th  X ����������ޣ�0-255���� 4 ��ԭʼ�����Ϊ��λ
	0x6E2  R/W  Y_Th  Y ����������ޣ�0-255���� 4 ��ԭʼ�����Ϊ��λ
	*/ 
	0x00,  0x00,
	
	/*
	0x6E3  R/W  X_Co_Sm  X ����ƽ�����Ʊ�����0-255 �����ã�0 ��ʾ��
	0x6E4  R/W  Y_Co_Sm  Y ����ƽ�����Ʊ�����0-255 �����ã�0 ��ʾ��
	0x6E5  R/W  X_Sp_Lim  X ����ƽ�������ٶȣ�0-255 �����ã�0 ��ʾ��
	0x6E6  R/W  Y_Sp_ Lim  Y ����ƽ�������ٶȣ�0-255 �����ã�0 ��ʾ��
	*/
	0x32,0x2C,0x34,0x2E,
	
	/*
	0x6E7  R/W  X_Bor_Lim  Reserved  Reserved
	0x6E8  R/W  Y_Bor_Lim  Reserved  Reserved
	*/
	0x00,0x00,
	
	/* 0x6E9  R/W  Filter  ��������֡��  ���괰���˲�ֵ���� 4 Ϊ���� */
	0x04,
	
	/* 0x6EA  R/W  Large_Tc  0-255 ��Ч����һ������������������ڴ�������Ϊ��������� */
	0x14,
	
	/* 0x6EB  R/W  Shake_Cu  Touch �¼�����ȥ��  ��ָ�����Ӷൽ��ȥ�� */
	0x22,
	
	/* 0x6EC  R/W  Noise_R  ����  ���������������� nibble����Ч */
	0x04,
	
	/* 0x6ED~0x6F1 R/W    ���� */
	0x00,0x00,0x00,0x00,0x00,
	
	
    0x20,0x14,0xEC,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0C,0x30,
    0x25,0x28,0x14,0x00,0x00,0x00,0x00,0x00,0x00,0x01,
};

#if 0
uint8_t s_GT811_CfgParams[]=
{
    0x30,0x0f,0x05,0x06,0x28,0x02,0x14,0x14,0x10,0x28,0xb2,
	800>>8,
	800&0xFF,
	480>>8,
	480&0xFF,
	0xed,
    0xcb,0xa9,0x87,0x65,0x43,0x21,0x00,0x00,0x00,0x00,0x00,0x4d,0xc1,0x20,0x01,0x01,
    0x41,0x64,0x3c,0x1e,0x28,0x0e,0x00,0x00,0x00,0x00,0x50,0x3c,0x32,0x71,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x01
};  

uint8_t s_GT811_CfgParams[]=
{
        0x06,0xA2,  
        0x12,0x10,0x0E,0x0C,0x0A,0x08,0x06,0x04,0x02,0x00,0xE2,0x53,0xD2,0x53,0xC2,0x53,  
        0xB2,0x53,0xA2,0x53,0x92,0x53,0x82,0x53,0x72,0x53,0x62,0x53,0x52,0x53,0x42,0x53,  
        0x32,0x53,0x22,0x53,0x12,0x53,0x02,0x53,0xF2,0x53,0x0F,0x13,0x40,0x40,0x40,0x10,  
        0x10,0x10,0x0F,0x0F,0x0A,0x35,0x25,0x0C,0x03,0x00,0x05,0x20,0x03,0xE0,0x01,0x00,  
        0x00,0x34,0x2C,0x36,0x2E,0x00,0x00,0x03,0x19,0x03,0x08,0x00,0x00,0x00,0x00,0x00,  
        0x14,0x10,0xEC,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0D,0x40,  
        0x30,0x3C,0x28,0x00,0x00,0x00,0x00,0xC0,0x12,0x01     
};  
#endif



/*
*********************************************************************************************************
*   函 数 名: GT811_WriteReg
*   功能说明: GT811_WriteReg 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void GT811_WriteReg(uint16_t _usRegAddr, uint8_t *_pRegBuf, uint8_t _ucLen);
/*
*********************************************************************************************************
*   函 数 名: GT811_ReadReg
*   功能说明: GT811_ReadReg 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void GT811_ReadReg(uint16_t _usRegAddr, uint8_t *_pRegBuf, uint8_t _ucLen);

GT811_T g_GT811;

/*
*********************************************************************************************************
*	�� �� ��: GT811_InitHard
*	����˵��: ���ô���оƬ.  �ڵ��øú���ǰ������ִ�� bsp_touch.c �еĺ��� bsp_DetectLcdType() ʶ��id
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void GT811_InitHard(void)
{
	uint16_t ver;

	g_GT811.TimerCount = 0;
	
	#if 0
	{
		i2c_Start();					/* ���߿�ʼ�ź� */

		i2c_SendByte(g_GT811.i2c_addr);	/* �����豸��ַ+д�ź� */
		i2c_WaitAck();

		i2c_SendByte(0x0F);	/* ��ַ��8λ */
		i2c_WaitAck();

		i2c_SendByte(0xFF);	/* ��ַ��8λ */
		i2c_WaitAck();		
	
		i2c_Stop();   
	}

	for (int i = 0; i < 3; i++)
	{
		GT811_ReadVersion();
		bsp_DelayUS(10000);
	}	
	#endif

	ver = GT811_ReadVersion();
	printf("GT811 Version : %04X\r\n", ver);

	/* I2C���߳�ʼ���� bsp.c ��ִ�� */
	
	GT811_WriteReg(GT811_CONFIG_REG, (uint8_t *)s_GT811_CfgParams, sizeof(s_GT811_CfgParams));
	
	/* ����ģ���������÷ֱ��ʼĴ��� */
	if (g_GT811.i2c_addr == GT811_I2C_ADDR1)
	{
		const uint8_t tab[4] = {0xe0, 0x01, 0x20, 0x03};
		GT811_WriteReg(0x6DD, (uint8_t *)tab, 4);
	}
	else	/* GT811_I2C_ADDR3 */
	{
		const uint8_t tab[4] = {0x58, 0x02, 0x00, 0x04};
		GT811_WriteReg(0x6DD, (uint8_t *)tab, 4);
	}
	
	
	g_GT811.TimerCount = 0;
	g_GT811.Enable = 1;
}

/*
*********************************************************************************************************
*	�� �� ��: GT811_ReadVersion
*	����˵��: ���GT811��оƬ�汾
*	��    ��: ��
*	�� �� ֵ: 16λ�汾
*********************************************************************************************************
*/
uint16_t GT811_ReadVersion(void)
{
	uint8_t buf[2];

	GT811_ReadReg(0x717, buf, 2);

	return ((uint16_t)buf[0] << 8) + buf[1];
}


/*
*********************************************************************************************************
*	�� �� ��: GT811_WriteReg
*	����˵��: д1���������Ķ���Ĵ���
*	��    ��: _usRegAddr : �Ĵ�����ַ
*			  _pRegBuf : �Ĵ������ݻ�����
*			 _ucLen : ���ݳ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void GT811_WriteReg(uint16_t _usRegAddr, uint8_t *_pRegBuf, uint8_t _ucLen)
{
	uint8_t i;

    i2c_Start();					/* ���߿�ʼ�ź� */

    i2c_SendByte(g_GT811.i2c_addr);	/* �����豸��ַ+д�ź� */
	i2c_WaitAck();

    i2c_SendByte(_usRegAddr >> 8);	/* ��ַ��8λ */
	i2c_WaitAck();

    i2c_SendByte(_usRegAddr);		/* ��ַ��8λ */
	i2c_WaitAck();

	for (i = 0; i < _ucLen; i++)
	{
	    i2c_SendByte(_pRegBuf[i]);		/* �Ĵ������� */
		i2c_WaitAck();
	}

    i2c_Stop();                   			/* ����ֹͣ�ź� */
}

/*
*********************************************************************************************************
*	�� �� ��: GT811_ReadReg
*	����˵��: ��1���������Ķ���Ĵ���
*	��    ��: _usRegAddr : �Ĵ�����ַ
*			  _pRegBuf : �Ĵ������ݻ�����
*			 _ucLen : ���ݳ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void GT811_ReadReg(uint16_t _usRegAddr, uint8_t *_pRegBuf, uint8_t _ucLen)
{
	uint8_t i;

    i2c_Start();					/* ���߿�ʼ�ź� */

    i2c_SendByte(g_GT811.i2c_addr);	/* �����豸��ַ+д�ź� */
	i2c_WaitAck();

    i2c_SendByte(_usRegAddr >> 8);	/* ��ַ��8λ */
	i2c_WaitAck();

    i2c_SendByte(_usRegAddr);		/* ��ַ��8λ */
	i2c_WaitAck();

	i2c_Start();
    i2c_SendByte(g_GT811.i2c_addr + 0x01);	/* �����豸��ַ+���ź� */
	i2c_WaitAck();

	for (i = 0; i < _ucLen - 1; i++)
	{
	    _pRegBuf[i] = i2c_ReadByte();	/* ���Ĵ������� */
		i2c_Ack();
	}

	/* ���һ������ */
	 _pRegBuf[i] = i2c_ReadByte();		/* ���Ĵ������� */
	i2c_NAck();

    i2c_Stop();							/* ����ֹͣ�ź� */
}

/*
*********************************************************************************************************
*	�� �� ��: GT811_Timer1ms
*	����˵��: ÿ��1ms����1��
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void GT811_Timer1ms(void)
{
	g_GT811.TimerCount++;
}

/*
*********************************************************************************************************
*	�� �� ��: GT811_Scan
*	����˵��: ��ȡGT811�������ݡ���ȡȫ�������ݣ���Ҫ 720us���ҡ����� bsp_Idle()��ִ��
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void GT811_Scan(void)
{
	uint8_t buf[48];
	//uint8_t i;
	static uint8_t s_tp_down = 0;
	uint16_t x, y;
	static uint16_t x_save, y_save;

	if (g_GT811.Enable == 0)
	{
		return;
	}
	
	/* 20ms ִ��һ�� */
	if (g_GT811.TimerCount < 20)
	{
		return;
	}

	g_GT811.TimerCount = 0;
	
	GT811_ReadReg(GT811_READ_XY_REG, buf, 1);		
	if ((buf[0] & 0x01) == 0)
	{
		if (s_tp_down == 1)
		{
			s_tp_down = 0;
			TOUCH_PutKey(TOUCH_RELEASE, x_save, y_save);
		}
		return;
	}
					
	GT811_ReadReg(GT811_READ_XY_REG + 1, &buf[1], 33);
	
	/*
	0x721  R  TouchpointFlag  Sensor_ID  key  tp4  tp3  tp2  tp1  tp0
	0x722  R  Touchkeystate     0  0  0  0  key4  key3  key2  key1

	0x723  R  Point0Xh  ������ 0��X ����� 8 λ
	0x724  R  Point0Xl  ������ 0��X ����� 8 λ
	0x725  R  Point0Yh  ������ 0��Y ����� 8 λ
	0x726  R  Point0Yl  ������ 0��Y ����� 8 λ
	0x727  R  Point0Pressure  ������ 0������ѹ��

	0x728  R  Point1Xh  ������ 1��X ����� 8 λ
	0x729  R  Point1Xl  ������ 1��X ����� 8 λ
	0x72A  R  Point1Yh  ������ 1��Y ����� 8 λ
	0x72B  R  Point1Yl  ������ 1��Y ����� 8 λ
	0x72C  R  Point1Pressure  ������ 1������ѹ��

	0x72D  R  Point2Xh  ������ 2��X ����� 8 λ
	0x72E  R  Point2Xl  ������ 2��X ����� 8 λ
	0x72F  R  Point2Yh  ������ 2��Y ����� 8 λ
	0x730  R  Point2Yl  ������ 2��Y ����� 8 λ
	0x731  R  Point2Pressure  ������ 2������ѹ��

	0x732  R  Point3Xh  ������ 3��X ����� 8 λ
	0x733-0x738  R    Reserve  none
	0x739  R  Point3Xl  ������ 3��X ����� 8 λ
	0x73A  R  Point3Yh  ������ 3��Y ����� 8 λ
	0x73B  R  Point3Yl  ������ 3��Y ����� 8 λ
	0x73C  R  Point3Pressure  ������ 3������ѹ��

	0x73D  R  Point4Xh  ������ 4��X ����� 8 λ
	0x73E  R  Point4Xl  ������ 4��X ����� 8 λ
	0x73F  R  Point4Yh  ������ 4��Y ����� 8 λ
	0x740  R  Point4Yl  ������ 4��Y ����� 8 λ
	0x741  R  Point4Pressure  ������ 4������ѹ��

	0x742  R  Data_check_sum  Data check Sum
	*/

	g_GT811.TouchpointFlag = buf[0];
	g_GT811.Touchkeystate = buf[1];

	g_GT811.X0 = ((uint16_t)buf[2] << 8) + buf[3];
	g_GT811.Y0 = ((uint16_t)buf[4] << 8) + buf[5];
	g_GT811.P0 = buf[6];

	g_GT811.X1 = ((uint16_t)buf[7] << 8) + buf[8];
	g_GT811.Y1 = ((uint16_t)buf[9] << 8) + buf[10];
	g_GT811.P1 = buf[11];

	g_GT811.X2 = ((uint16_t)buf[12] << 8) + buf[13];
	g_GT811.Y2 = ((uint16_t)buf[14] << 8) + buf[15];
	g_GT811.P2 = buf[16];

	/* ������3�ĵ�ַ������ */
	g_GT811.X3 = ((uint16_t)buf[17] << 8) + buf[24];
	g_GT811.Y3 = ((uint16_t)buf[25] << 8) + buf[26];
	g_GT811.P3 = buf[27];

	g_GT811.X4 = ((uint16_t)buf[28] << 8) + buf[29];
	g_GT811.Y4 = ((uint16_t)buf[30] << 8) + buf[31];
	g_GT811.P4 = buf[32];

	/* ��ⰴ�� */
	{
		/* ����ת�� :
			���ݴ��������½��� (0��0);  ���Ͻ��� (479��799)
			��Ҫת��LCD���������� (���Ͻ��� (0��0), ���½��� (799��479)
		*/


		/* �����ж�ֵ�� */
		if (g_GT811.i2c_addr == GT811_I2C_ADDR1)
		{
			x = g_GT811.Y0;
			y = 479 - g_GT811.X0;
			
			if (x > 800)
			{
				x = 800;
			}
			
			if (y > 480)
			{
				y = 480;
			}
		}
		else
		{
			x = g_GT811.Y0;
			y = 599 - g_GT811.X0;
		
			if (x > 1024)
			{
				x = 1024;
			}
			
			if (y > 600)
			{
				y = 600;
			}
		}
	}
	
	if (s_tp_down == 0)
	{
		s_tp_down = 1;
		
		TOUCH_PutKey(TOUCH_DOWN, x, y);
	}
	else
	{
		TOUCH_PutKey(TOUCH_MOVE, x, y);
	}
	x_save = x;	/* �������꣬�����ͷ��¼� */
	y_save = y;

#if 0
	for (i = 0; i < 34; i++)
	{
		printf("%02X ", buf[i]);
	}
	printf("\r\n");

	printf("(%5d,%5d,%3d) ",  g_GT811.X0, g_GT811.Y0, g_GT811.P0);
	printf("(%5d,%5d,%3d) ",  g_GT811.X1, g_GT811.Y1, g_GT811.P1);
	printf("(%5d,%5d,%3d) ",  g_GT811.X2, g_GT811.Y2, g_GT811.P2);
	printf("(%5d,%5d,%3d) ",  g_GT811.X3, g_GT811.Y3, g_GT811.P3);
	printf("(%5d,%5d,%3d) ",  x, y, g_GT811.P4);
	printf("\r\n");
#endif	
}

/*
*********************************************************************************************************
*	�� �� ��: GT811_ReadSensorID
*	����˵��: ʶ����ʾģ����𡣶�ȡGT811 SensorID����״̬����3��״̬�����գ��ӵ�Դ���ӵء�
*	��    ��: ��
*	�� �� ֵ: ��ʾģ�����, 0, 1, 2
*********************************************************************************************************
*/
uint8_t GT811_ReadSensorID(void)
{
	uint8_t value;
	
	/* 	0x721  R  TouchpointFlag      Sensor_ID  key  tp4  tp3  tp2  tp1  tp0 */
	GT811_ReadReg(0x721, &value, 1);
	
	return (value >> 6);
}


/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
