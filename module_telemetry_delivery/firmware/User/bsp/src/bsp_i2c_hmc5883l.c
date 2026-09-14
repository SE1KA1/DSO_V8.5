/**
 * @file bsp_i2c_hmc5883l.c
 * @brief 安富莱 BSP 通用驱动（HMC5883L 磁力计）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : ���������HMC5883L����ģ��
*	�ļ����� : bsp_HMC5883L.c
*	��    �� : V1.0
*	˵    �� : ʵ��HMC5883L�Ķ�д������
*
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2013-02-01 armfly  ��ʽ����
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

/*
	Ӧ��˵��:����HMC5883Lǰ�����ȵ���һ�� bsp_InitI2C()�������ú�I2C��ص�GPIO.
*/

#include "bsp.h"

HMC5883L_T g_tMag;		/* ����һ��ȫ�ֱ���������ʵʱ���� */

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitHMC5883L
*	����˵��: ��ʼ��HMC5883L
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitHMC5883L(void)
{
	/* ����Mode�Ĵ��� */
	#if 1
		HMC5883L_WriteByte(0x00, 0x70);	
		HMC5883L_WriteByte(0x01, 0x20);	
		HMC5883L_WriteByte(0x02, 0x00);	
	#else	/* ��У׼ģʽ */
		HMC5883L_WriteByte(0x00, 0x70 + 2);	
		HMC5883L_WriteByte(0x01, 0x20);	
		HMC5883L_WriteByte(0x02, 0x00);		
	#endif
	
	
	g_tMag.CfgRegA = HMC5883L_ReadByte(0);
	g_tMag.CfgRegB = HMC5883L_ReadByte(1);
	g_tMag.ModeReg = HMC5883L_ReadByte(2);
	
	g_tMag.IDReg[0] = HMC5883L_ReadByte(10);
	g_tMag.IDReg[1] = HMC5883L_ReadByte(11);
	g_tMag.IDReg[2] = HMC5883L_ReadByte(12);
	g_tMag.IDReg[3] = 0;
	
	/* ������С���ֵ��ֵ */
	g_tMag.X_Min = 4096;
	g_tMag.X_Max = -4096;
	
	g_tMag.Y_Min = 4096;
	g_tMag.Y_Max = -4096;

	g_tMag.Z_Min = 4096;
	g_tMag.Z_Max = -4096;
}

/*
*********************************************************************************************************
*	�� �� ��: HMC5883L_WriteByte
*	����˵��: �� HMC5883L �Ĵ���д��һ������
*	��    ��: _ucRegAddr : �Ĵ�����ַ
*			  _ucRegData : �Ĵ�������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HMC5883L_WriteByte(uint8_t _ucRegAddr, uint8_t _ucRegData)
{
    i2c_Start();							/* ���߿�ʼ�ź� */

    i2c_SendByte(HMC5883L_SLAVE_ADDRESS);	/* �����豸��ַ+д�ź� */
	i2c_WaitAck();

    i2c_SendByte(_ucRegAddr);				/* �ڲ��Ĵ�����ַ */
	i2c_WaitAck();

    i2c_SendByte(_ucRegData);				/* �ڲ��Ĵ������� */
	i2c_WaitAck();

    i2c_Stop();                   			/* ����ֹͣ�ź� */
}

/*
*********************************************************************************************************
*	�� �� ��: HMC5883L_ReadByte
*	����˵��: ��ȡ MPU-6050 �Ĵ���������
*	��    ��: _ucRegAddr : �Ĵ�����ַ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
uint8_t HMC5883L_ReadByte(uint8_t _ucRegAddr)
{
	uint8_t ucData;

	i2c_Start();                  			/* ���߿�ʼ�ź� */
	i2c_SendByte(HMC5883L_SLAVE_ADDRESS);	/* �����豸��ַ+д�ź� */
	i2c_WaitAck();
	i2c_SendByte(_ucRegAddr);     			/* ���ʹ洢��Ԫ��ַ */
	i2c_WaitAck();

	i2c_Start();                  			/* ���߿�ʼ�ź� */

	i2c_SendByte(HMC5883L_SLAVE_ADDRESS+1); 	/* �����豸��ַ+���ź� */
	i2c_WaitAck();

	ucData = i2c_ReadByte();       			/* �����Ĵ������� */
	i2c_NAck();
	i2c_Stop();                  			/* ����ֹͣ�ź� */
	return ucData;
}


/*
*********************************************************************************************************
*	�� �� ��: HMC5883L_ReadData
*	����˵��: ��ȡ MPU-6050 ���ݼĴ����� ���������ȫ�ֱ��� g_tMag.  ��������Զ�ʱ���øó���ˢ������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HMC5883L_ReadData(void)
{
	uint8_t ucReadBuf[7];
	uint8_t i;

#if 1 /* ������ */
	i2c_Start();                  			/* ���߿�ʼ�ź� */
	i2c_SendByte(HMC5883L_SLAVE_ADDRESS);	/* �����豸��ַ+д�ź� */
	i2c_WaitAck();
	i2c_SendByte(DATA_OUT_X);     		/* ���ʹ洢��Ԫ��ַ  */
	i2c_WaitAck();

	i2c_Start();                  			/* ���߿�ʼ�ź� */

	i2c_SendByte(HMC5883L_SLAVE_ADDRESS + 1); /* �����豸��ַ+���ź� */
	i2c_WaitAck();

	for (i = 0; i < 6; i++)
	{
		ucReadBuf[i] = i2c_ReadByte();       			/* �����Ĵ������� */
		i2c_Ack();
	}

	/* �����һ���ֽڣ�ʱ�� NAck */
	ucReadBuf[6] = i2c_ReadByte();
	i2c_NAck();

	i2c_Stop();                  			/* ����ֹͣ�ź� */

#else	/* ���ֽڶ� */
	for (i = 0 ; i < 7; i++)
	{
		ucReadBuf[i] = HMC5883L_ReadByte(DATA_OUT_X + i);
	}
#endif

	/* �����������ݱ��浽ȫ�ֽṹ����� */
	g_tMag.X = (int16_t)((ucReadBuf[0] << 8) + ucReadBuf[1]);
	g_tMag.Z = (int16_t)((ucReadBuf[2] << 8) + ucReadBuf[3]);
	g_tMag.Y = (int16_t)((ucReadBuf[4] << 8) + ucReadBuf[5]);
	
	g_tMag.Status = ucReadBuf[6];
	
	/* ͳ�����ֵ����Сֵ */
	if ((g_tMag.X > - 2048) && (g_tMag.X < 2048))
	{
		if (g_tMag.X > g_tMag.X_Max)
		{
			g_tMag.X_Max = g_tMag.X;
		}
		if (g_tMag.X < g_tMag.X_Min)
		{
			g_tMag.X_Min = g_tMag.X;
		}	
	}

	if ((g_tMag.Y > - 2048) && (g_tMag.Y < 2048))
	{
		if (g_tMag.Y > g_tMag.Y_Max)
		{
			g_tMag.Y_Max = g_tMag.Y;
		}
		if (g_tMag.Y < g_tMag.Y_Min)
		{
			g_tMag.Y_Min = g_tMag.Y;
		}	
	}
	
	if ((g_tMag.Z > - 2048) && (g_tMag.Z < 2048))
	{
		if (g_tMag.Z > g_tMag.Z_Max)
		{
			g_tMag.Z_Max = g_tMag.Z;
		}
		if (g_tMag.Z < g_tMag.Z_Min)
		{
			g_tMag.Z_Min = g_tMag.Z;
		}	
	}
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/

