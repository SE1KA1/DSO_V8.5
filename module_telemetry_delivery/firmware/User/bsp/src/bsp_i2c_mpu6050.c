/**
 * @file bsp_i2c_mpu6050.c
 * @brief 安富莱 BSP 通用驱动（MPU6050 IMU）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : ����������MPU-6050����ģ��
*	�ļ����� : bsp_mpu6050.c
*	��    �� : V1.0
*	˵    �� : ʵ��MPU-6050�Ķ�д������
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
	Ӧ��˵��������MPU-6050ǰ�����ȵ���һ�� bsp_InitI2C()�������ú�I2C��ص�GPIO.
*/

#include "bsp.h"

MPU6050_T g_tMPU6050;		/* ����һ��ȫ�ֱ���������ʵʱ���� */

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitMPU6050
*	����˵��: ��ʼ��MPU-6050
*	��    ��:  ��
*	�� �� ֵ: 1 ��ʾ������ 0 ��ʾ������
*********************************************************************************************************
*/
void bsp_InitMPU6050(void)
{
	MPU6050_WriteByte(PWR_MGMT_1, 0x00);	//�������״̬
	MPU6050_WriteByte(SMPLRT_DIV, 0x07);
	MPU6050_WriteByte(CONFIG, 0x06);
	MPU6050_WriteByte(GYRO_CONFIG, 0xE8);
	MPU6050_WriteByte(ACCEL_CONFIG, 0x01);
}

/*
*********************************************************************************************************
*	�� �� ��: MPU6050_WriteByte
*	����˵��: �� MPU-6050 �Ĵ���д��һ������
*	��    ��: _ucRegAddr : �Ĵ�����ַ
*			  _ucRegData : �Ĵ�������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void MPU6050_WriteByte(uint8_t _ucRegAddr, uint8_t _ucRegData)
{
    i2c_Start();							/* ���߿�ʼ�ź� */

    i2c_SendByte(MPU6050_SLAVE_ADDRESS);	/* �����豸��ַ+д�ź� */
	i2c_WaitAck();

    i2c_SendByte(_ucRegAddr);				/* �ڲ��Ĵ�����ַ */
	i2c_WaitAck();

    i2c_SendByte(_ucRegData);				/* �ڲ��Ĵ������� */
	i2c_WaitAck();

    i2c_Stop();                   			/* ����ֹͣ�ź� */
}

/*
*********************************************************************************************************
*	�� �� ��: MPU6050_ReadByte
*	����˵��: ��ȡ MPU-6050 �Ĵ���������
*	��    ��: _ucRegAddr : �Ĵ�����ַ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
uint8_t MPU6050_ReadByte(uint8_t _ucRegAddr)
{
	uint8_t ucData;

	i2c_Start();                  			/* ���߿�ʼ�ź� */
	i2c_SendByte(MPU6050_SLAVE_ADDRESS);	/* �����豸��ַ+д�ź� */
	i2c_WaitAck();
	i2c_SendByte(_ucRegAddr);     			/* ���ʹ洢��Ԫ��ַ */
	i2c_WaitAck();

	i2c_Start();                  			/* ���߿�ʼ�ź� */

	i2c_SendByte(MPU6050_SLAVE_ADDRESS+1); 	/* �����豸��ַ+���ź� */
	i2c_WaitAck();

	ucData = i2c_ReadByte();       			/* �����Ĵ������� */
	i2c_NAck();
	i2c_Stop();                  			/* ����ֹͣ�ź� */
	return ucData;
}


/*
*********************************************************************************************************
*	�� �� ��: MPU6050_ReadData
*	����˵��: ��ȡ MPU-6050 ���ݼĴ����� ���������ȫ�ֱ��� g_tMPU6050.  ��������Զ�ʱ���øó���ˢ������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void MPU6050_ReadData(void)
{
	uint8_t ucReadBuf[14];
	uint8_t i;
	uint8_t ack;

#if 1 /* ������ */
	i2c_Start();                  			/* ���߿�ʼ�ź� */
	i2c_SendByte(MPU6050_SLAVE_ADDRESS);	/* �����豸��ַ+д�ź� */
	ack = i2c_WaitAck();
	if (ack != 0)
	{
		i2c_Stop(); 
		return;
	}
	i2c_SendByte(ACCEL_XOUT_H);     		/* ���ʹ洢��Ԫ��ַ  */
	ack = i2c_WaitAck();
	if (ack != 0)
	{
		i2c_Stop(); 
		return;
	}

	i2c_Start();                  			/* ���߿�ʼ�ź� */

	i2c_SendByte(MPU6050_SLAVE_ADDRESS + 1); /* �����豸��ַ+���ź� */
	ack = i2c_WaitAck();
	if (ack != 0)
	{
		i2c_Stop(); 
		return;
	}

	for (i = 0; i < 13; i++)
	{
		ucReadBuf[i] = i2c_ReadByte();       			/* �����Ĵ������� */
		i2c_Ack();
	}

	/* �����һ���ֽڣ�ʱ�� NAck */
	ucReadBuf[13] = i2c_ReadByte();
	i2c_NAck();

	i2c_Stop();                  			/* ����ֹͣ�ź� */

#else	/* ���ֽڶ� */
	for (i = 0 ; i < 14; i++)
	{
		ucReadBuf[i] = MPU6050_ReadByte(ACCEL_XOUT_H + i);
	}
#endif

	/* �����������ݱ��浽ȫ�ֽṹ����� */
	g_tMPU6050.Accel_X = (ucReadBuf[0] << 8) + ucReadBuf[1];
	g_tMPU6050.Accel_Y = (ucReadBuf[2] << 8) + ucReadBuf[3];
	g_tMPU6050.Accel_Z = (ucReadBuf[4] << 8) + ucReadBuf[5];

	g_tMPU6050.Temp = (int16_t)((ucReadBuf[6] << 8) + ucReadBuf[7]);

	g_tMPU6050.GYRO_X = (ucReadBuf[8] << 8) + ucReadBuf[9];
	g_tMPU6050.GYRO_Y = (ucReadBuf[10] << 8) + ucReadBuf[11];
	g_tMPU6050.GYRO_Z = (ucReadBuf[12] << 8) + ucReadBuf[13];
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
