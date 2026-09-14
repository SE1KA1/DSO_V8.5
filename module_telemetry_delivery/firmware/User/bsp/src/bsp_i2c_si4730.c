/**
 * @file bsp_i2c_si4730.c
 * @brief 安富莱 BSP 通用驱动（Si4730 收音机）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : AM/FM������Si4730 ����ģ��
*	�ļ����� : bsp_Si730.c
*	��    �� : V1.0
*	˵    �� : ����Si4730  Si4704������оƬ��ͨ��I2C���߿��Ƹ�оƬ��ʵ��AM/FM���ա�
*				Si4730��Si4704�������ݵġ�Si4704֧��FM,��֧��AM;  Si4730֧��AM����֧��FM,
*
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2013-02-01 armfly  ��ʽ����
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/*
	�ο��ĵ�:
		AN332 Si47xx Programming Guide.pdf		�������ָ��
		Si4730-31-34-35-D60.pdf					оƬ�����ֲᣨ�����������֣�

	������STM32-V5 ������Si4730���߷��䣺

	I2C���߿���Si4730, ��ַΪ ��0x22��, ͨ���� I2C_ADDR_SI4730 ����
		PH4/I2C2_SCL
 		PH5/I2C2_SDA

	I2C���ߵײ������� bsp_i2c_gpio.c
	��Ҫ���� bsp_InitI2C() ��������I2C��GPIO


	���Si4730�Ƿ���������Ե��� i2c_CheckDevice(I2C_ADDR_SI4730)����������0��ʾоƬ������
*/

/*
	FM (64�C108 MHz)
	AM (520�C1710 kHz)

*/

/*
	i2c ����ʱ�򣬼� AN332 page = 226

	ÿ�����������������START + STOP�źţ�����: [] ��ʾ��ȡ��������
	START ADDR+W [ACK] CMD  [ACK] ARG1 [ACK] ARG2 [ACK] ARG3 [ACK] STOP
	START  0x22    0  0x30    0   0x00   0   0x27   0   0x7E   0  STOP

	ѭ����ȡ�������ص�״̬��ֻ�� STARTUS = 0x80
	START ADDR+R [ACK] [STATUS] NACK STOP
	START  0x23    0    0x00    1   STOP

	��ȡ�������ص�����
	START ADDR+R [ACK] STATUS ACK RESP1 ACK RESP2 ACK RESP3 NACK STOP
	START  0x23    0   0x80   0  0x00   0  0x00   0  0x00   1   STOP

	��ע: [ACK] ��CPU����һ��SCL, Ȼ���ȡSDA
		  ACK   ��CPU����SDA=0 ,Ȼ����һ��SCL
*/

/*
	AN223 page = 271    FM ������ģʽ��������
	12.2. Programming Example for the FM/RDS Receiver
*/

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitSi4730
*	����˵��: ����Si4703����ģʽ
*	��    ��:��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitSi4730(void)
{
	;
}

/*
*********************************************************************************************************
*	�� �� ��: SI4730_Delay
*	����˵��: �ӳ�һ��ʱ��
*	��    ��: n ѭ������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SI4730_Delay(uint32_t n)
{
	uint32_t i;

	for (i = 0; i < n; i++);
}

/*
*********************************************************************************************************
*	�� �� ��: SI4730_SendCmd
*	����˵��: ��Si4730����CMD
*	��    ��: _pCmdBuf : ��������
*			 _CmdLen : ����ֽ���
*	�� �� ֵ: 0 ʧ��(������Ӧ��)�� 1 �ɹ�
*********************************************************************************************************
*/
uint8_t SI4730_SendCmd(uint8_t *_pCmdBuf, uint8_t _ucCmdLen)
{
	uint8_t ack;
	uint8_t i;

	i2c_Start();
	i2c_SendByte(I2C_ADDR_SI4730_W);
	ack = i2c_WaitAck();
	if (ack != 0)
	{
		goto err;
	}

	for (i = 0; i < _ucCmdLen; i++)
	{
		i2c_SendByte(_pCmdBuf[i]);
		ack = i2c_WaitAck();
		if (ack != 0)
		{
			goto err;
		}
	}

	i2c_Stop();
	return 1;

err:
	i2c_Stop();
	return 0;
}

/*
*********************************************************************************************************
*	�� �� ��: SI4730_WaitStatus80
*	����˵��: ��ȡSi4730��״̬������0x80ʱ���ء�
*	��    ��: _uiTimeOut : ��ѯ����
*			  _ucStopEn : ״̬0x80���ɹ����Ƿ���STOP
*	�� �� ֵ: 0 ʧ��(������Ӧ��)�� > 1 �ɹ�, ���ֱ�ʾʵ����ѯ����
*********************************************************************************************************
*/
uint32_t SI4730_WaitStatus80(uint32_t _uiTimeOut, uint8_t _ucStopEn)
{
	uint8_t ack;
	uint8_t status;
	uint32_t i;

	/* �ȴ�����״̬Ϊ 0x80 */
	for (i = 0; i < _uiTimeOut; i++)
	{
		i2c_Start();
		i2c_SendByte(I2C_ADDR_SI4730_R);	/* �� */
		ack = i2c_WaitAck();
		if (ack == 1)
		{
			i2c_NAck();
			i2c_Stop();
			return 0;	/* ������Ӧ��ʧ�� */
		}
		status = i2c_ReadByte();
		if ((status == 0x80) || (status == 0x81))	/* 0x81 ��Ϊ��ִ��0x23ָ�� ��ȡ�ź����� */
		{
			break;
		}
	}
	if (i == _uiTimeOut)
	{
		i2c_NAck();
		i2c_Stop();
		return 0;	/* ��ʱ�ˣ�ʧ�� */
	}

	/* �ɹ��ˣ� ����һ�µ�1�ξͳɹ������ */
	if (i == 0)
	{
		i = 1;

	}

	/* ��Ϊ��Щ�����Ҫ��ȡ����ֵ����˴˴������βξ����Ƿ���STOP */
	if  (_ucStopEn == 1)
	{
		i2c_NAck();
		i2c_Stop();
	}
	return i;
}

/*
*********************************************************************************************************
*	�� �� ��: SI4730_SetProperty
*	����˵��: ����Si4730���Բ���
*	��    ��: _usPropNumber : ������
*			  _usPropValue : ����ֵ
*	�� �� ֵ: 0 ʧ��(������Ӧ��)�� > 1 �ɹ�
*********************************************************************************************************
*/
uint8_t SI4730_SetProperty(uint16_t _usPropNumber, uint16_t _usPropValue)
{
	uint8_t ucCmdBuf[32];
	uint32_t uiTimeOut;

	ucCmdBuf[0] = 0x12;
	ucCmdBuf[1] = 0x00;

	ucCmdBuf[2] = _usPropNumber >> 8;
	ucCmdBuf[3] = _usPropNumber;

	ucCmdBuf[4] = _usPropValue >> 8;
	ucCmdBuf[5] = _usPropValue;
	SI4730_SendCmd(ucCmdBuf, 6);

	uiTimeOut = SI4730_WaitStatus80(100000, 1);
	if (uiTimeOut == 0)
	{
		return 0;
	}

	return 1;
}

/* ����2�������ǰ�I2C����ʱ����д�����Է��ֺܶ�����ǿ��Թ��õġ�������ǶԲ��ִ�����з�װ���ѱ���ʵ���������� */
#if 0
	/*
	*********************************************************************************************************
	*	�� �� ��: SI4730_PowerUp_FM_Revice
	*	����˵��: ����Si4703ΪFM����ģʽ�� ģ��ģʽ��������ģʽ)
	*	��    ��:��
	*	�� �� ֵ: 0 ʧ�ܣ� 1 �ɹ�
	*********************************************************************************************************
	*/
	uint8_t SI4730_PowerUp_FM_Revice(void)
	{
		uint8_t ack;
		uint8_t status;

		/* AN332  page = 277
			Powerup in Analog Mode
			CMD      0x01     POWER_UP
			ARG1     0xC0     Set to FM Receive. Enable interrupts.
			ARG2     0x05     Set to Analog Audio Output
			STATUS   ��0x80   Reply Status. Clear-to-send high.
		*/
		i2c_Start();
		i2c_SendByte(I2C_ADDR_SI4730_W);
		ack = i2c_WaitAck();
		i2c_SendByte(0x01);
		ack = i2c_WaitAck();
		i2c_SendByte(0xC0);
		ack = i2c_WaitAck();
		i2c_SendByte(0x05);
		ack = i2c_WaitAck();
		i2c_Stop();

		/* �ȴ���������״̬ 0x80 */
		{
			uint32_t i;

			for (i = 0; i < 2500; i++)
			{
				i2c_Start();
				i2c_SendByte(I2C_ADDR_SI4730_R);	/* �� */
				ack = i2c_WaitAck();
				status = i2c_ReadByte();
				i2c_NAck();
				i2c_Stop();

				if (status == 0x80)
				{
					break;
				}
			}

			/* ʵ�� 535 ��ѭ��Ӧ�������˳� */
			if (i == 2500)
			{
				return 0;
			}
		}

		return 1;
	}

	/*
	*********************************************************************************************************
	*	�� �� ��: SI4730_GetRevision
	*	����˵��: ��ȡ�������̼���Ϣ�� ����8�ֽ�����
	*	��    ��:_ReadBuf  ���ؽ������ڴ˻��������뱣֤��������С���ڵ���8
	*	�� �� ֵ: 0 ʧ�ܣ� 1 �ɹ�
	*********************************************************************************************************
	*/
	uint8_t SI4730_GetRevision(uint8_t *_ReadBuf)
	{
		uint8_t ack;
		uint8_t status;
		uint32_t i;

		/* AN223 page = 67 */

		/* ���� 0x10 ���� */
		i2c_Start();
		i2c_SendByte(I2C_ADDR_SI4730_W);
		ack = i2c_WaitAck();
		i2c_SendByte(0x10);
		ack = i2c_WaitAck();
		i2c_Stop();

		/* �ȴ�����״̬Ϊ 0x80 */
		for (i = 0; i < 50; i++)
		{
			i2c_Start();
			i2c_SendByte(I2C_ADDR_SI4730_R);	/* �� */
			ack = i2c_WaitAck();
			status = i2c_ReadByte();
			if (status == 0x80)
			{
				break;
			}
		}
		/* ʵ�� 2 ��ѭ��Ӧ�������˳� */
		if (i == 50)
		{
			i2c_NAck();
			i2c_Stop();
			return 0;
		}

		/* ������ȡ8���ֽڵ�����������Ϣ */
		for (i = 0; i < 8; i++)
		{
			i2c_Ack();
			_ReadBuf[i] = i2c_ReadByte();
		}
		i2c_NAck();
		i2c_Stop();
		return 1;
	}
#endif

/*
*********************************************************************************************************
*	�� �� ��: SI4730_PowerUp_FM_Revice
*	����˵��: ����Si4703ΪFM����ģʽ�� ģ��ģʽ��������ģʽ)
*	��    ��:��
*	�� �� ֵ: 0 ʧ�ܣ� 1 �ɹ�
*********************************************************************************************************
*/
uint8_t SI4730_PowerUp_FM_Revice(void)
{
	/* AN332  page = 277
		Powerup in Analog Mode
		CMD      0x01     POWER_UP
		ARG1     0xC0     Set to FM Receive. Enable interrupts.
		ARG2     0x05     Set to Analog Audio Output
		STATUS   ��0x80   Reply Status. Clear-to-send high.
	*/

	uint8_t ucCmdBuf[3];
	uint32_t uiTimeOut;

	ucCmdBuf[0] = 0x01;
	ucCmdBuf[1] = 0xD0; //0xC0;
	ucCmdBuf[2] = 0x05;
	SI4730_SendCmd(ucCmdBuf, 3);

	/*
		��1���βα�ʾ�����ѯ������ ����ɹ�������ֵuiTimeOut > 0 ��ʾʵ����ѯ����
		��2���β�1��ʾ��������STOP
	*/
	uiTimeOut = SI4730_WaitStatus80(1000, 1);
	if (uiTimeOut > 0)
	{
		return 1;
	}

	return 0;
}

/*
*********************************************************************************************************
*	�� �� ��: SI4730_PowerUp_AM_Revice
*	����˵��: ����Si4703ΪAM����ģʽ�� ģ��ģʽ��������ģʽ)
*	��    ��:��
*	�� �� ֵ: 0 ʧ�ܣ� 1 �ɹ�
*********************************************************************************************************
*/
uint8_t SI4730_PowerUp_AM_Revice(void)
{
	/* AN332  page = 277
		Powerup in Analog Mode
		CMD      0x01     POWER_UP
		ARG1     0xC0     Set to FM Receive. Enable interrupts.
		ARG2     0x05     Set to Analog Audio Output
		STATUS   ��0x80   Reply Status. Clear-to-send high.
	*/

	uint8_t ucCmdBuf[3];
	uint32_t uiTimeOut;

	ucCmdBuf[0] = 0x01;
	ucCmdBuf[1] = 0x91;
	ucCmdBuf[2] = 0x05;
	SI4730_SendCmd(ucCmdBuf, 3);

	/*
		��1���βα�ʾ�����ѯ������ ����ɹ�������ֵuiTimeOut > 0 ��ʾʵ����ѯ����
		��2���β�1��ʾ��������STOP
	*/
	uiTimeOut = SI4730_WaitStatus80(1000, 1);
	if (uiTimeOut > 0)
	{
		return 1;
	}


    SI4730_SetProperty(0x3403, 5);
    SI4730_SetProperty(0x3404, 25);

    SI4730_SetProperty(0x3402, 10); // Set spacing to 10kHz
    SI4730_SetProperty(0x3400, 520); // Set the band bottom to 520kHz
    SI4730_SetProperty(0x3401, 1710);   // Set the band top to 1710kHz

	return 0;
}

/*
*********************************************************************************************************
*	�� �� ��: SI4730_PowerDown
*	����˵��: �ر� Si470��Դ��ģ������ر�
*	��    ��:��
*	�� �� ֵ: 0 ʧ�ܣ� 1 �ɹ�
*********************************************************************************************************
*/
uint8_t SI4730_PowerDown(void)
{
	/*
		AN332  page = 15

		Moves the device from powerup to powerdown mode.
			Command 0x11. POWER_DOWN
	*/

	uint8_t ucCmdBuf[3];
	uint32_t uiTimeOut;

	ucCmdBuf[0] = 0x11;
	SI4730_SendCmd(ucCmdBuf, 1);

	/*
		��1���βα�ʾ�����ѯ������ ����ɹ�������ֵuiTimeOut > 0 ��ʾʵ����ѯ����
		��2���β�1��ʾ��������STOP
	*/
	uiTimeOut = SI4730_WaitStatus80(1000, 1);
	if (uiTimeOut > 0)
	{
		return 1;
	}

	return 0;
}


/*
*********************************************************************************************************
*	�� �� ��: SI4730_GetRevision
*	����˵��: ��ȡ�������̼���Ϣ�� ����8�ֽ�����
*	��    ��:_ReadBuf  ���ؽ������ڴ˻��������뱣֤��������С���ڵ���8
*	�� �� ֵ: 0 ʧ�ܣ� 1 �ɹ�
*********************************************************************************************************
*/
uint8_t SI4730_GetRevision(uint8_t *_ReadBuf)
{
	uint8_t ucCmdBuf[2];
	uint32_t uiTimeOut;
	uint8_t i;

	/* AN223 page = 67 */

	/* ���� 0x10 ���� */
	ucCmdBuf[0] = 0x10;
	SI4730_SendCmd(ucCmdBuf, 1);

	/*
		��1���βα�ʾ�����ѯ������ ����ɹ�������ֵuiTimeOut > 0 ��ʾʵ����ѯ����
		��2���β�0��ʾ�����󲻷���STOP�� ��Ϊ����Ҫ��ȡ������������
	*/
	uiTimeOut = SI4730_WaitStatus80(10, 0);
	if (uiTimeOut == 0)
	{
		return 0;
	}

	/* ������ȡ8���ֽڵ�����������Ϣ */
	for (i = 0; i < 8; i++)
	{
		i2c_Ack();
		_ReadBuf[i] = i2c_ReadByte();
	}
	i2c_NAck();
	i2c_Stop();
	return 1;
}

/*
*********************************************************************************************************
*	�� �� ��: SI4704_SetFMAntIntput
*	����˵��: ����FM��������
*	��    ��: _ch : 0 ��ʾFM��������(������)  1 ��ʾLPI��������(PCB������)
*	�� �� ֵ: 0 ʧ�ܣ� 1 �ɹ�
*********************************************************************************************************
*/
uint8_t SI4704_SetFMIntput(uint8_t _ch)
{
	/* AN332 - PAGE 91 
		Property 0x1107. FM_ANTENNA_INPUT */

	return SI4730_SetProperty(0x1107, _ch);	
}

/*
*********************************************************************************************************
*	�� �� ��: SI4730_SetFMFreq
*	����˵��: ����FM��гƵ��
*	��    ��:_uiFreq : Ƶ��ֵ, ��λ 10kHz
*	�� �� ֵ: 0 ʧ�ܣ� 1 �ɹ�
*********************************************************************************************************
*/
uint8_t SI4730_SetFMFreq(uint32_t _uiFreq)
{
	/* AN332 page = 70 */

	/*
		CMD		 0x20 	FM_TUNE_FREQ
		ARG1     0x00
		ARG2     0x27	Set frequency to 102.3 MHz = 0x27F6
		ARG3     0xF6
		ARG4     0x00   Set antenna tuning capacitor to auto.
		STATUS   ?0x80	Reply Status. Clear-to-send high.
	*/

	/* 64 and 108 MHz in 10 kHz units. */

	uint8_t ucCmdBuf[32];
	uint32_t uiTimeOut;
	uint32_t i;
	uint8_t status;

	ucCmdBuf[0] = 0x20;
	ucCmdBuf[1] = 0x00;
	ucCmdBuf[2] = _uiFreq >> 8;
	ucCmdBuf[3] = _uiFreq;
	ucCmdBuf[4] = 0;
	SI4730_SendCmd(ucCmdBuf, 5);

	uiTimeOut = SI4730_WaitStatus80(1000, 1);
	if (uiTimeOut == 0)
	{
		return 0;
	}



	/* �ȴ�����״̬Ϊ 0x81 */
	for (i = 0; i < 5000; i++)
	{
		/* 0x14. GET_INT_STATUS */
		ucCmdBuf[0] = 0x14;
		SI4730_SendCmd(ucCmdBuf, 1);

		SI4730_Delay(10000);

		i2c_Start();
		i2c_SendByte(I2C_ADDR_SI4730_R);	/* �� */
		i2c_WaitAck();
		status = i2c_ReadByte();
		i2c_Stop();
		if (status == 0x81)
		{
			break;
		}
	}

	if (i == 5000)
	{
		return 0;	/* ʧ�� */
	}
	return 1;
}

/*
*********************************************************************************************************
*	�� �� ��: SI4730_SetAMFreq
*	����˵��: ����AM��гƵ��
*	��    ��:_uiFreq : Ƶ��ֵ, ��λ 10kHz
*	�� �� ֵ: 0 ʧ�ܣ� 1 �ɹ�
*********************************************************************************************************
*/
uint8_t SI4730_SetAMFreq(uint32_t _uiFreq)
{
	/* AN332 page = 70 */

	/*
		CMD       0x40        AM_TUNE_FREQ
		ARG1      0x00
		ARG2      0x03        Set frequency to 1000 kHz = 0x03E8
		ARG3      0xE8
		ARG4      0x00        Automatically select tuning capacitor
		ARG5      0x00
		STATUS    ?0x80       Reply Status. Clear-to-send high.
	*/

	/* 64 and 108 MHz in 10 kHz units. */

	uint8_t ucCmdBuf[32];
	uint32_t uiTimeOut;
	uint32_t i;
	uint8_t status;

	ucCmdBuf[0] = 0x40;
	ucCmdBuf[1] = 0x00;
	ucCmdBuf[2] = _uiFreq >> 8;
	ucCmdBuf[3] = _uiFreq;
	ucCmdBuf[4] = 0x00;
	ucCmdBuf[5] = 0x00;
	SI4730_SendCmd(ucCmdBuf, 6);

	uiTimeOut = SI4730_WaitStatus80(10000, 1);
	if (uiTimeOut == 0)
	{
		return 0;
	}



	/* �ȴ�����״̬Ϊ 0x81 */
	for (i = 0; i < 5000; i++)
	{
		/* 0x14. GET_INT_STATUS */
		ucCmdBuf[0] = 0x14;
		SI4730_SendCmd(ucCmdBuf, 1);

		SI4730_Delay(10000);

		i2c_Start();
		i2c_SendByte(I2C_ADDR_SI4730_R);	/* �� */
		i2c_WaitAck();
		status = i2c_ReadByte();
		i2c_Stop();
		if (status == 0x81)
		{
			break;
		}
	}

	if (i == 5000)
	{
		return 0;	/* ʧ�� */
	}
	return 1;
}

/*
*********************************************************************************************************
*	�� �� ��: SI4730_SetAMFreqCap
*	����˵��: ����AM��гƵ��
*	��    ��:_uiFreq : Ƶ��ֵ, ��λ 10kHz    _usCap : ��г����
*	�� �� ֵ: 0 ʧ�ܣ� 1 �ɹ�
*********************************************************************************************************
*/
uint8_t SI4730_SetAMFreqCap(uint32_t _uiFreq, uint16_t _usCap)
{
	/* AN332 page = 70 */

	/*
		CMD       0x40        AM_TUNE_FREQ
		ARG1      0x00
		ARG2      0x03        Set frequency to 1000 kHz = 0x03E8
		ARG3      0xE8
		ARG4      0x00        Automatically select tuning capacitor
		ARG5      0x00
		STATUS    ?0x80       Reply Status. Clear-to-send high.
	*/

	/* 64 and 108 MHz in 10 kHz units. */

	uint8_t ucCmdBuf[32];
	uint32_t uiTimeOut;
	uint32_t i;
	uint8_t status;

	ucCmdBuf[0] = 0x40;
	ucCmdBuf[1] = 0x00;
	ucCmdBuf[2] = _uiFreq >> 8;
	ucCmdBuf[3] = _uiFreq;
	ucCmdBuf[4] = _usCap >> 8;
	ucCmdBuf[5] = _usCap;
	SI4730_SendCmd(ucCmdBuf, 6);

	uiTimeOut = SI4730_WaitStatus80(10000, 1);
	if (uiTimeOut == 0)
	{
		return 0;
	}

	/* �ȴ�����״̬Ϊ 0x81 */
	for (i = 0; i < 5000; i++)
	{
		/* 0x14. GET_INT_STATUS */
		ucCmdBuf[0] = 0x14;
		SI4730_SendCmd(ucCmdBuf, 1);

		SI4730_Delay(10000);

		i2c_Start();
		i2c_SendByte(I2C_ADDR_SI4730_R);	/* �� */
		i2c_WaitAck();
		status = i2c_ReadByte();
		i2c_Stop();
		if (status == 0x81)
		{
			break;
		}
	}

	if (i == 5000)
	{
		return 0;	/* ʧ�� */
	}
	return 1;
}

/*
*********************************************************************************************************
*	�� �� ��: SI4730_GetAMTuneStatus
*	����˵��: ��ȡAM��г״̬
*	��    ��: ���ؽ������ڴ˻��������뱣֤��������С���ڵ���7
*	�� �� ֵ: 0 ʧ�ܣ� 1 �ɹ�
*********************************************************************************************************
*/
uint8_t SI4730_GetAMTuneStatus(uint8_t *_ReadBuf)
{
	/*
		CMD       0x42           AM_TUNE_STATUS
		ARG1      0x01           Clear STC interrupt.
		STATUS    ?0x80          Reply Status. Clear-to-send high.

		RESP1     ?0x01          Channel is valid, AFC is not railed, and seek did not wrap at AM band boundary
		RESP2     ?0x03
		RESP3     ?0xE8          Frequency = 0x03E8 = 1000 kHz
		RESP4     ?0x2A          RSSI = 0x2A = 42d = 42 dB��V
		RESP5     ?0x1A          SNR = 0x1A = 26d = 26 dB
		RESP6     ?0x0D          Value the antenna tuning capacitor is set to.
		RESP7     ?0x95          0x0D95 = 3477 dec.
	
		���ݼ��� The tuning capacitance is 95 fF x READANTCAP + 7 pF	
	*/
	uint8_t ucCmdBuf[32];
	uint32_t uiTimeOut;
	uint32_t i;

	ucCmdBuf[0] = 0x42;
	ucCmdBuf[1] = 0x01;
	SI4730_SendCmd(ucCmdBuf, 2);

	uiTimeOut = SI4730_WaitStatus80(100, 0);
	if (uiTimeOut == 0)
	{
		return 0;
	}

	/* ������ȡ7���ֽڵ�����������Ϣ */
	for (i = 0; i < 7; i++)
	{
		i2c_Ack();
		_ReadBuf[i] = i2c_ReadByte();
	}
	i2c_NAck();
	i2c_Stop();
	return 1;

}

/*
*********************************************************************************************************
*	�� �� ��: SI4730_GetFMTuneStatus
*	����˵��: ��ȡFM��г״̬
*	��    ��: ���ؽ������ڴ˻��������뱣֤��������С���ڵ���7
*	�� �� ֵ: 0 ʧ�ܣ� 1 �ɹ�
*********************************************************************************************************
*/
uint8_t SI4730_GetFMTuneStatus(uint8_t *_ReadBuf)
{
	/*
		CMD      0x22     FM_TUNE_STATUS
		ARG1     0x01     Clear STC interrupt.
		STATUS   ?0x80    Reply Status. Clear-to-send high.

		RESP1    ?0x01    Valid Frequency.
		RESP2    ?0x27    Frequency = 0x27F6 = 102.3 MHz
		RESP3    ?0xF6
		RESP4    ?0x2D    RSSI = 45 dB��V
		RESP5    ?0x33    SNR = 51 dB
		RESP6    ?0x00    MULT[7:0]
		RESP7    ?0x00    Antenna tuning capacitor = 0 (range = 0�C191)  READANTCAP[7:0] (Si4704/05/06/2x only)
	*/
	uint8_t ucCmdBuf[32];
	uint32_t uiTimeOut;
	uint32_t i;

	ucCmdBuf[0] = 0x22;
	ucCmdBuf[1] = 0x01;
	SI4730_SendCmd(ucCmdBuf, 2);

	uiTimeOut = SI4730_WaitStatus80(100, 0);
	if (uiTimeOut == 0)
	{
		return 0;
	}

	/* ������ȡ7���ֽڵ�����������Ϣ */
	for (i = 0; i < 7; i++)
	{
		i2c_Ack();
		_ReadBuf[i] = i2c_ReadByte();
	}
	i2c_NAck();
	i2c_Stop();
	return 1;

}

/*
*********************************************************************************************************
*	�� �� ��: SI4730_GetAMSignalQuality
*	����˵��: ��ȡAM�����ź�����
*	��    ��: _ReadBuf ���ؽ������ڴ˻��������뱣֤��������С���ڵ���5
*	�� �� ֵ: 0 ʧ�ܣ� 1 �ɹ�
*********************************************************************************************************
*/
uint8_t SI4730_GetAMSignalQuality(uint8_t *_ReadBuf)
{
	/*
		AM_RSQ_STATUS
		Queries the status of the Received Signal Quality (RSQ) for
		the current channel.

		CMD        0x43      AM_RSQ_STATUS
		ARG1       0x01      Clear STC interrupt.
		STATUS     ?0x80     Reply Status. Clear-to-send high.

		RESP1      ?0x00     No SNR high, low, RSSI high, or low interrupts.
		RESP2      ?0x01     Channel is valid, soft mute is not activated, and AFC is not railed
		RESP3      ?0x00
		RESP4      ?0x2A     RSSI = 0x2A = 42d = 42 dB��V
		RESP5      ?0x1A     SNR = 0x1A = 26d = 26 dB
	*/
	uint8_t ucCmdBuf[32];
	uint32_t uiTimeOut;
	uint32_t i;

	ucCmdBuf[0] = 0x43;
	ucCmdBuf[1] = 0x01;
	SI4730_SendCmd(ucCmdBuf, 2);

	uiTimeOut = SI4730_WaitStatus80(100, 0);
	if (uiTimeOut == 0)
	{
		return 0;
	}

	/* ������ȡ5���ֽڵ�����������Ϣ */
	for (i = 0; i < 5; i++)
	{
		i2c_Ack();
		_ReadBuf[i] = i2c_ReadByte();
	}
	i2c_NAck();
	i2c_Stop();
	return 1;
}

/*
*********************************************************************************************************
*	�� �� ��: SI4730_GetFMSignalQuality
*	����˵��: ��ȡFM�����ź�����
*	��    ��: _ReadBuf ���ؽ������ڴ˻��������뱣֤��������С���ڵ���7
*	�� �� ֵ: 0 ʧ�ܣ� 1 �ɹ�
*********************************************************************************************************
*/
uint8_t SI4730_GetFMSignalQuality(uint8_t *_ReadBuf)
{
	/*
		FM_RSQ_STATUS
		Queries the status of the Received Signal Quality (RSQ) for
		the current channel.

		CMD      0x23    FM_RSQ_STATUS
		ARG1     0x01    Clear RSQINT
		STATUS   ?0x80   Reply Status. Clear-to-send high.
		RESP1    ?0x00   No blend, SNR high, low, RSSI high or low interrupts.
		RESP2    ?0x01   Soft mute is not engaged, no AFC rail, valid frequency.
		RESP3    ?0xD9   Pilot presence, 89% blend
		RESP4    ?0x2D   RSSI = 45 dB��V
		RESP5    ?0x33   SNR = 51 dB
		RESP6    ?0x00
		RESP7    ?0x00   Freq offset = 0 kHz
	*/
	uint8_t ucCmdBuf[32];
	uint32_t uiTimeOut;
	uint32_t i;

	ucCmdBuf[0] = 0x23;
	ucCmdBuf[1] = 0x01;
	SI4730_SendCmd(ucCmdBuf, 2);

	uiTimeOut = SI4730_WaitStatus80(1000, 0);
	if (uiTimeOut == 0)
	{
		return 0;
	}

	/* ������ȡ7���ֽڵ�����������Ϣ */
	for (i = 0; i < 7; i++)
	{
		i2c_Ack();
		_ReadBuf[i] = i2c_ReadByte();
	}
	i2c_NAck();
	i2c_Stop();
	return 1;
}

/*
*********************************************************************************************************
*	�� �� ��: SI4730_SetOutVlomue
*	����˵��: ����Si4730�������
*	��    ��: _ucVolume; ֵ��[0-63];
*	�� �� ֵ: 0 ʧ�ܣ� 1 �ɹ�
*********************************************************************************************************
*/
uint8_t SI4730_SetOutVolume(uint8_t _ucVolume)
{
	/* AN332 page = 123 */

	/*
		Property 0x4000. RX_VOLUME

		Sets the output volume level, 63 max, 0 min. Default is 63.
	*/

	if (_ucVolume > 63)
	{
		_ucVolume = 63;
	}

	return SI4730_SetProperty(0x4000, _ucVolume);
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
