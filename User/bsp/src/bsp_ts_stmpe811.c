/**
 * @file bsp_ts_stmpe811.c
 * @brief 安富莱 BSP 通用驱动（STMPE811 触摸）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
	ģ������ : ����ʽ����оƬSTMPE811����ģ��
	�ļ����� : TOUCH_STMPE811.c
*	��    �� : V1.0
*	˵    �� : STMPE811 ���败������������I2C�ӿ�.
*	�޸ļ�¼ :
*		�汾��  ����        ����    ˵��
*		V1.0    2014-06-08  armfly  �׷�������������ԭ����
*
*	Copyright (C), 2014-2015, �人�������������޹�˾ www.armfly.com
*
*********************************************************************************************************
*/

/*
	Ӧ��˵��������STMPE811ǰ�����ȵ���һ�� bsp_InitI2C()�������ú�I2C��ص�GPIO.
*/

#include "bsp.h"

/* 宏定义: touch_printf - touch_printf 的宏定义。 */
#define touch_printf(...)
//#define touch_printf  printf

static void STMPE811_Reset(void);
static uint16_t s_AdcX, s_AdcY;

/*
*********************************************************************************************************
*	�� �� ��: STMPE811_InitHard
*	����˵��: ����STMPE811�Ĵ���
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void STMPE811_InitHard(void)
{	
	STMPE811_Reset();
	
	/* STMPE811.pdf �� 45ҳ, �Ƽ��ı�̲���: 
	The following are the steps to configure the touch screen controller (TSC):
	a)  Disable the clock gating for the touch screen controller and ADC in the SYS_CFG2 register.		
	��SYS_CFG2��
		[7:4] RESERVED
		[3] TS_OFF: Switch off the clock supply to the temperature sensor
			1: Switches off the clock supply to the temperature sensor
		[2] GPIO_OFF: Switch off the clock supply to the GPIO
			1: Switches off the clock supply to the GPIO
		[1] TSC_OFF: Switch off the clock supplyto the touch screen controller
			1: Switches off the clock supply to the touch screen controller
		[0] ADC_OFF: Switch off the clock supply to the ADC
			1: Switches off the clock supply to the ADC	
	*/
	STMPE811_WriteReg1(REG811_SYS_CTRL2, 0x00);		/* ע��1�Ǳ�ʾ�ر�ʱ�ӣ� �˴��Ǵ����е�ʱ�� */
	

	/* Select Sample Time, bit number and ADC Reference */
	
	/* ѡ�����ʱ�䣬ADC�ֱ��ʣ�ADC�ο���Դ
	��ADC_CTRL1��
		[7] RESERVED
		[6:4] SAMPLE_TIMEn: ADC conversion time in number of clock   --- ADCת��ʱ�䣨���ٸ�ʱ�ӣ�
			000: 36
			001: 44
			010: 56
			011: 64
			100: 80
			101: 96
			110: 124
			111: Not valid
		[3] MOD_12B: Selects 10 or 12-bit ADC operation
			1: 12 bit ADC              ��ѡ��12λ��
			0: 10 bit ADC
		[2] RESERVED
		[1] REF_SEL: Selects between internal or external reference for the ADC
			1: External reference
			0: Internal reference      ��ѡ���ڲ��ο���
		[0] RESERVED
	*/
	STMPE811_WriteReg1(REG811_ADC_CTRL1, (4 << 4) | (1 << 3) | (0 << 1));
	
	/* �ȴ�20ms */
	bsp_DelayMS(20);  
	
	/* ѡ��ADCʱ���ٶ� : 3.25 MHz 
	��ADC_CTRL2��
		[7] RESERVED
		[6] RESERVED
		[5] RESERVED
		[4] RESERVED
		[3] RESERVED
		[2] RESERVED
		[1:0] ADC_FREQ: Selects the clock speed of ADC
			00: 1.625 MHz typ.
			01: 3.25 MHz typ.
			10: 6.5 MHz typ.
			11: 6.5 MHz typ.	
	*/
	STMPE811_WriteReg1(REG811_ADC_CTRL2, 0x01);
		
	/* ����8���ܽŵĹ���ģʽ�� 1��ʾGPIOģʽ��0��ʾΪ������������/ADC			
	��GPIO_ALT_FUNCT��
		Reset: 0x0F
		Description: Alternate function register. "��0�� sets the corresponding pin to function as touch 
			screen/ADC, and ��1�� sets it into GPIO mode.	
	*/  
	STMPE811_WriteReg1(REG811_GPIO_AF, 0x0F);	/* ��4λ����TSC, ��4λ����GPIO */
	
	/* Select 2 nF filter capacitor 
	��TSC_CFG��
		Description: Touch screen controller configuration register.
		[7:6] [AVE_CTRL_1/0: Average control   ---- ����ƽ���ĸ����� ѡ��4������ƽ��
			00=1 sample
			01=2 samples
			10=4 samples 
			11=8 samples
		[5:3] TOUCH_DET_DELAY_2/1/0: Touch detect delay  ---- ��������ӳ٣� ѡ�� 500us
			000 - 10 ��s
			001 - 50 ��s
			010 = 100��s
			011 = 500��s
			100 = 1ms
			101 = 5ms
			110 = 10 ms
			111 = 50 ms
		[2:0] SETTLING: Panel driver settling time     ----- ��������������ʱ�䣬  ѡ�� 5ms
			000 = 10��s
			001 = 100��s
			010 = 500��S
			011 = 1ms
			100 = 5ms
			101 = 10 ms
			110 = 50 ms
			111=100ms
			
		1. For large panels (> 6��), a capacitor of 10 nF is recommended at the touch screen terminals for noise filtering. 
		In this case, settling time of 1 ms or more is recommended.	
	*/
	STMPE811_WriteReg1(REG811_TSC_CFG, (2 << 6) | (3 << 3) | (4 << 0));   
	
	/* ���ô����жϵ����������� �˴�����Ϊ1����ֻҪ�д����ʹ����ж�
	��FIFO_TH��
	Description: Triggers an interrupt upon reaching or exceeding the threshold value. This field must not be set as zero.
		[7:0] FIFO_TH: touch screen controller FIFO threshold	
	*/
	STMPE811_WriteReg1(REG811_FIFO_TH, 0x01);
	
	/* Write 0x01 to clear the FIFO memory content. 
	��FIFO_STA��
		Description: Current status of FIFO..
		[7] FIFO_OFLOW: 
			Reads 1 if FIFO is overflow
		[6] FIFO_FULL: 
			Reads 1 if FIFO is full
		[5] FIFO_EMPTY:
			Reads 1 if FIFO is empty
		[4] FIFO_TH_TRIG:
			0 = Current FIFO size is still below the threshold value
			1 = Current FIFO size is at or beyond the threshold value
		[3:1] RESERVED
		[0] FIFO_RESET:
			Write '0' : FIFO put out of reset mode
			Write '1' : Resets FIFO. All data in FIFO will be cleared.
			When TSC is enabled, FIFOresets automatically.	---- �� TSCʹ��ʱ��FIFO���Զ���յ�	
	*/
	STMPE811_WriteReg1(REG811_FIFO_STA, 0x01);	/* ��λFIFO */
	STMPE811_WriteReg1(REG811_FIFO_STA, 0x00);	/* �˳�FIFO��λ״̬ */
	
	/* set the data format for Z value: 7 fractional part and 1 whole part 
	��TSC_FRACTION_Z��
		Reset: 0x00
		Description: This register allows to select the range and accuracy of the pressure measurement
		[7:3] RESERVED
		[2:0] FRACTION_Z: 
			000: Fractional part is 0, whole part is 8
			001: Fractional part is 1, whole part is 7
			010: Fractional part is 2, whole part is 6
			011: Fractional part is 3, whole part is 5
			100: Fractional part is 4, whole part is 4
			101: Fractional part is 5, whole part is 3
			110: Fractional part is 6, whole part is 2
			111: Fractional part is 7, whole part is 1	
	*/
	STMPE811_WriteReg1(REG811_TSC_FRACT_XYZ, 0x01);
	
	/* set the driving capability of the device for TSC pins: 50mA 
	��TSC_I_DRIVE��
		Description: This register sets the current limit value of the touch screen drivers
		[7:1] RESERVED
		[0] DRIVE: maximum current on the touch screen controller (TSC) driving channel
			0: 20 mA typical, 35 mA max
			1: 50 mA typical, 80 mA max	
	*/
	STMPE811_WriteReg1(REG811_TSC_I_DRIVE, 0x01);		/* ѡ��50ms�������� */
	
	/* Use no tracking index, touch-panel controller operation mode (XYZ) and 
	 enable the TSC
	��TSC_CTRL��
		Description: 4-wire touch screen controller (TSC) setup.
		[7] TSC_STA: TSC status
			Reads '1' when touch is detected
			Reads '0' when touch is not detected
			Writing to this register has no effect
		[6:4] TRACK: Tracking index  
		  --- �˶�׷�١������ǰ�������ϸ�������λ�Ƴ����趨ֵ�Ż��ϱ�
		  ��ʽ: (Current X - Previously Reported X) + (Current Y - Previously Reported Y) > Tracking Index
			000: No window tracking
			001: 4
			010: 8
			011: 16
			100: 32
			101: 64
			110: 92
			111: 127
		[3:1] OP_MOD: TSC operating mode
			000: X, Y, Z acquisition
			001: X, Y only			----- ѡ�� X Y ����ģʽ������ҪZ��ѹ��
			010: X only
			011: Y only
			100: Z only
			This field cannot be written on, when EN = 1
		[0] EN: Enable TSC			 
	*/
	STMPE811_WriteReg1(REG811_TSC_CTRL, (0 << 4) | (1 << 1) | (1 << 0));
	
	/*  Clear all the status pending bits 
	��INT_STA��
		Type: R
		Reset: 0x10
		Description: The interrupt status register monitors the status of the interruption from a particular 
		interrupt source to the host. Regardless of whether the INT_EN bits are enabled, the 
		INT_STA bits are still updated. Writing '1' to this register clears the corresponding 
		bits. Writing '0' has no effect.
		
		[7] GPIO: Any enabled GPIO interrupts
		[6] ADC: Any enabled ADC interrupts
		[5] TEMP_SENS: Temperature threshold triggering
		[4] FIFO_EMPTY: FIFO is empty
		[3] FIFO_FULL: FIFO is full
		[2] FIFO_OFLOW: FIFO is overflowed
		[1] FIFO_TH: FIFO is equal or above threshold value.
			This bit is set when FIFO level equals to threshold value. It will only be asserted again if FIFO 
			level drops to < threshold value, and increased back to threshold value.
		[0] TOUCH_DET: Touch is detected	
	*/
	STMPE811_WriteReg1(REG811_INT_STA, 0xFF); 
	
	STMPE811_WriteReg1(REG811_INT_EN, 0x01); 
	
	STMPE811_WriteReg1(REG811_INT_CTRL, 0x01); 
}

/*
*********************************************************************************************************
*	�� �� ��: STMPE811_ClearInt
*	����˵��: ��������ж�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void STMPE811_ClearInt(void)
{
	STMPE811_WriteReg1(REG811_INT_STA, 0xFF); 
}

/*
*********************************************************************************************************
*	�� �� ��: STMPE811_Reset
*	����˵��: ������λSTMPE811оƬ
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void STMPE811_Reset(void)
{
	/* 
	��SYS_CTRL1��
		[7:2] RESERVED
		[1] SOFT_RESET: Reset the STMPE811 using the serial communication interface
		[0] HIBERNATE: Force the device into hibernation mode.
			Forcing the device into hibernation mode by writing ��1�� to this bit would disable the h
				feature. If the hot-key feature is required, use the default auto-hibernation mode.
	*/
	STMPE811_WriteReg1(REG811_SYS_CTRL1, 0x02);
	
	/* �ȴ�оƬ��λ */
	bsp_DelayMS(20); 
	
	/* �˳���λ״̬�����еļĴ����ָ�ȱʡֵ */
	STMPE811_WriteReg1(REG811_SYS_CTRL1, 0x00);
}

/*
*********************************************************************************************************
*	�� �� ��: STMPE811_ReadID
*	����˵��: ��оƬID. Ӧ�÷���
*	��    ��: ��
*	�� �� ֵ: ����ID. 0x0811
*********************************************************************************************************
*/
uint16_t STMPE811_ReadID(void)
{
	return STMPE811_ReadReg2(REG811_CHIP_ID);
}

/*
*********************************************************************************************************
*	�� �� ��: STMPE811_ReadBytes
*	����˵��: ������ȡ��������
*	��    ��: 
*			 _pReadBuf : ��Ŷ��������ݵĻ�����ָ��
*			 _ucAddress : ��ʼ��ַ
*			 _ucSize : ���ݳ��ȣ���λΪ�ֽ�
*	�� �� ֵ: 0 ��ʾʧ�ܣ�1��ʾ�ɹ�
*********************************************************************************************************
*/
uint8_t STMPE811_ReadBytes(uint8_t *_pReadBuf, uint8_t _ucAddress, uint8_t _ucSize)
{
	uint8_t i;

	/* ��1��������I2C���������ź� */
	i2c_Start();

	/* ��2������������ֽڣ���7bit�ǵ�ַ��bit0�Ƕ�д����λ��0��ʾд��1��ʾ�� */
	i2c_SendByte(STMPE811_I2C_ADDRESS | I2C_WR);	/* �˴���дָ�� */

	/* ��3��������ACK */
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;	/* EEPROM������Ӧ�� */
	}

	/* ��4�������ͼĴ�����ַ */
	i2c_SendByte((uint8_t)_ucAddress);
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;	/* EEPROM������Ӧ�� */
	}

	/* ��6������������I2C���ߡ����濪ʼ��ȡ���� */
	i2c_Start();

	/* ��7������������ֽڣ���7bit�ǵ�ַ��bit0�Ƕ�д����λ��0��ʾд��1��ʾ�� */
	i2c_SendByte(STMPE811_I2C_ADDRESS | I2C_RD);	/* �˴��Ƕ�ָ�� */

	/* ��8��������ACK */
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;	/* EEPROM������Ӧ�� */
	}

	/* ��9����ѭ����ȡ���� */
	for (i = 0; i < _ucSize; i++)
	{
		_pReadBuf[i] = i2c_ReadByte();	/* ��1���ֽ� */

		/* ÿ����1���ֽں���Ҫ����Ack�� ���һ���ֽڲ���ҪAck����Nack */
		if (i != _ucSize - 1)
		{
			i2c_Ack();	/* �м��ֽڶ����CPU����ACK�ź�(����SDA = 0) */
		}
		else
		{
			i2c_NAck();	/* ���1���ֽڶ����CPU����NACK�ź�(����SDA = 1) */
		}
	}
	/* ����I2C����ֹͣ�ź� */
	i2c_Stop();
	return 1;	/* ִ�гɹ� */

cmd_fail: /* ����ִ��ʧ�ܺ��мǷ���ֹͣ�źţ�����Ӱ��I2C�����������豸 */
	/* ����I2C����ֹͣ�ź� */
	i2c_Stop();
	return 0;
}

/*
*********************************************************************************************************
*	�� �� ��: STMPE811_WriteBytes
*	����˵��: ����д����������
*	��    ��: 
*			 _pWriteBuf : ��Ҫд�뵽оƬ�����ݵĻ�����ָ��
*			 _ucAddress : ��ʼ��ַ
*			 _ucSize : ���ݳ��ȣ���λΪ�ֽ�
*	�� �� ֵ: 0 ��ʾʧ�ܣ�1��ʾ�ɹ�
*********************************************************************************************************
*/
uint8_t STMPE811_WriteBytes(uint8_t *_pWriteBuf, uint8_t _ucAddress, uint8_t _ucSize)
{
	uint8_t i;

	/* ��1��������I2C���������ź� */
	i2c_Start();

	/* ��2������������ֽڣ���7bit�ǵ�ַ��bit0�Ƕ�д����λ��0��ʾд��1��ʾ�� */
	i2c_SendByte(STMPE811_I2C_ADDRESS | I2C_WR);	/* �˴���дָ�� */

	/* ��3��������ACK */
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;	/* EEPROM������Ӧ�� */
	}

	/* ��4�������ͼĴ�����ַ */
	i2c_SendByte((uint8_t)_ucAddress);
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;	/* EEPROM������Ӧ�� */
	}

	/* ��5����ѭ��д���� */
	for (i = 0; i < _ucSize; i++)
	{
		i2c_SendByte(_pWriteBuf[i]);
		if (i2c_WaitAck() != 0)
		{
			goto cmd_fail;	/* EEPROM������Ӧ�� */
		}
	}
	/* ����I2C����ֹͣ�ź� */
	i2c_Stop();
	return 1;	/* ִ�гɹ� */

cmd_fail: /* ����ִ��ʧ�ܺ��мǷ���ֹͣ�źţ�����Ӱ��I2C�����������豸 */
	/* ����I2C����ֹͣ�ź� */
	i2c_Stop();
	return 0;
}

/*
*********************************************************************************************************
*	�� �� ��: STMPE811_WriteReg1
*	����˵��: д1���ֽڵ��Ĵ���
*	��    ��: _ucRegAddr : �Ĵ�����ַ
*			 _ucValue    : �Ĵ���ֵ
*	�� �� ֵ: 0 ��ʾʧ�ܣ�1��ʾ�ɹ�
*********************************************************************************************************
*/
void STMPE811_WriteReg1(uint8_t _ucRegAddr, uint8_t _ucValue)
{
	STMPE811_WriteBytes(&_ucValue, _ucRegAddr, 1);
}

/*
*********************************************************************************************************
*	�� �� ��: STMPE811_WriteReg1
*	����˵��: д2���ֽڵ��Ĵ���
*	��    ��: _ucRegAddr : �Ĵ�����ַ
*			 _ucValue    : �Ĵ���ֵ
*	�� �� ֵ: 0 ��ʾʧ�ܣ�1��ʾ�ɹ�
*********************************************************************************************************
*/
void STMPE811_WriteReg2(uint8_t _ucRegAddr, uint16_t _usValue)
{
	uint8_t buf[2];
	
	buf[0] = _usValue >> 8;
	buf[1] = _usValue;
	STMPE811_WriteBytes(buf, _ucRegAddr, 2);
}

/*
*********************************************************************************************************
*	�� �� ��: STMPE811_ReadReg1
*	����˵��: ���Ĵ���1�ֽ�
*	��    ��: _ucRegAddr : �Ĵ�����ַ
*			 _ucValue    : �Ĵ���ֵ
*	�� �� ֵ: �Ĵ���ֵ
*********************************************************************************************************
*/
uint8_t STMPE811_ReadReg1(uint8_t _ucRegAddr)
{
	uint8_t read;
	
	STMPE811_ReadBytes(&read, _ucRegAddr, 1);
	
	return read;
}

/*
*********************************************************************************************************
*	�� �� ��: STMPE811_ReadReg2
*	����˵��: ��2���ֽ�
*	��    ��: _ucRegAddr : �Ĵ�����ַ
*	�� �� ֵ: �Ĵ���ֵ
*********************************************************************************************************
*/
uint16_t STMPE811_ReadReg2(uint8_t _ucRegAddr)
{
	uint8_t buf[2];
	
	STMPE811_ReadBytes(buf, _ucRegAddr, 2);
	
	return (((uint16_t)buf[0] << 8) | buf[1]);
}

/*
*********************************************************************************************************
*	�� �� ��: STMPE811_ReadX
*	����˵��: ��ȡX����adc
*	��    ��: ��
*	�� �� ֵ: X����ֵadc
*********************************************************************************************************
*/
uint16_t STMPE811_ReadX(void)
{
	/* ���� XY ��ȡģʽ��������ȡ3�ֽ����ݣ�Ȼ��ֽ��X,Y 	
	 |  byte0   |     byte1      |   byte2  |  
	 | X[11:4], | X[3:0],Y[11:8] | Y[7:0]   |
	*/
	uint8_t buf[3];
	
#if 0
	STMPE811_ReadBytes(buf, REG811_TSC_DATA1, 3);
	
	s_AdcX = ((uint16_t)buf[0] << 4) | (buf[1] >> 4);
	s_AdcY = ((uint16_t)(buf[1] & 0xF) << 8) | buf[2];	
#else
	if (STMPE811_ReadReg1(REG811_TSC_CTRL) & 0x80)
	{	
		STMPE811_ReadBytes(buf, REG811_TSC_DATA1, 3);
		
		s_AdcX = ((uint16_t)buf[0] << 4) | (buf[1] >> 4);
		s_AdcY = ((uint16_t)(buf[1] & 0xF) << 8) | buf[2];
		
		#if 0
		/* for debug */
		{
			static int32_t s_t1 = 0;
			int32_t tt;
						
			tt = bsp_GetRunTime();
			if (tt - s_t1 > 1000)
			{
				printf("\r\n");
				s_t1 = tt;
			}
			printf("(%7d) %5d %5d\r\n", tt, s_AdcX, s_AdcY);
		}
		#endif
	}
	else
	{
		s_AdcX = 0;
		s_AdcY = 0;
	}
#endif
	
	return s_AdcX;
}

/*
*********************************************************************************************************
*	�� �� ��: STMPE811_ReadX
*	����˵��: ��ȡY����adc
*	��    ��: ��
*	�� �� ֵ: Y����ֵadc
*********************************************************************************************************
*/
uint16_t STMPE811_ReadY(void)
{
	return  s_AdcY;
}

/*
*********************************************************************************************************
*	�� �� ��: STMPE811_ReadGPIO
*	����˵��: ��ȡGPIO״̬
*	��    ��: ��
*	�� �� ֵ: GPIO״̬��
*********************************************************************************************************
*/
uint8_t STMPE811_ReadGPIO(void)
{
	/*  ����GPIO����
	��GPIO_DIR��
	Description: GPIO set pin direction register. 
		Writing ��0�� sets the corresponding GPIO to input state, and ��1�� sets it to output state. 
		All bits are ��0�� on reset.
	*/
	STMPE811_WriteReg1(REG811_GPIO_DIR, 0);		/* ȫ�������� */
	
	return STMPE811_ReadReg1(REG811_GPIO_MP_STA);		/* ��GPIO״̬ */	
}

/*
*********************************************************************************************************
*	�� �� ��: STMPE811_ReadIO
*	����˵��: ����GPIO�� BIT3��BIT2��BIT0 ����IO�ĵ�ƽ״̬ʶ����ʾ�����͡� �����ڰ�����������������ʾģ�顣
*	��    ��: ��
*	�� �� ֵ: IO״̬
*********************************************************************************************************
*/
uint8_t STMPE811_ReadIO(void)
{
	uint8_t gpio;
	uint8_t type;
	
	gpio = STMPE811_ReadGPIO();
	
	/* BIT3  BIT2 BIT0 */
	type = ((gpio >> 1) & 0x6) | (gpio & 0x01);
	
	return type;
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
