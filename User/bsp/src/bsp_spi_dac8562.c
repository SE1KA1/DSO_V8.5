/**
 * @file bsp_spi_dac8562.c
 * @brief 安富莱 BSP 通用驱动（DAC8562）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : DAC8562/8563 ����ģ��(��ͨ����16λDAC)
*	�ļ����� : bsp_dac8562.c
*	��    �� : V1.1
*	˵    �� : DAC8562/8563ģ���CPU֮�����SPI�ӿڡ�����������֧��Ӳ��SPI�ӿڡ�
*
*	�޸ļ�¼ :
*		�汾��  ����         ����     ˵��
*		V1.0    2014-01-17  armfly  ��ʽ����
*		V1.1    2015-10-09  armfly  �������ߴ���LDAC������Ҫ�ӵأ�CLRҲ��Ҫ�ӵ�
*
*	Copyright (C), 2015-2020, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/*
	DAC8562ģ�����ֱ�Ӳ嵽STM32-V7������CN19��ĸ(2*4P 2.54mm)�ӿ���

    DAC8562/8563ģ��    STM32-V6������
	  GND   ------  GND
	  VCC   ------  3.3V

	  LDAC  ------  ��չIO/NRF24L01_CE/DAC1_OUT    --- ����ӵػ�ָ��ǰ��0V
      SYNC  ------  PG10/NRF24L01_CSN

      SCLK  ------  PB3/SPI3_SCK
      DIN   ------  PB5/SPI3_MOSI

			------  PB4/SPI3_MISO         ---- DAC�޶�������
	  CLR   ------  PE4/NRF24L01_IRQ	  ---  �Ƽ���GND��
*/

/*
	DAC8562��������:
	1������2.7 - 5V;  ������ʹ��3.3V��
	4���ο���ѹ2.5V��ʹ���ڲ��ο�

	��SPI��ʱ���ٶ�Ҫ��: �ߴ�50MHz�� �ٶȺܿ�.
	SCLK�½��ض�ȡ����, ÿ�δ���24bit���ݣ� ��λ�ȴ�
*/

/* SYNC, Ҳ����CSƬѡ */	
#define CS_CLK_ENABLE() 	__HAL_RCC_GPIOG_CLK_ENABLE()
/* 宏定义: CS_GPIO - CS_GPIO 的宏定义。 */
#define CS_GPIO				GPIOG
/* 宏定义: CS_PIN - CS_PIN 的宏定义。 */
#define CS_PIN				GPIO_PIN_10
/* 宏定义: CS_1 - CS_1 的宏定义。 */
#define CS_1()				CS_GPIO->BSRRL = CS_PIN
/* 宏定义: CS_0 - CS_0 的宏定义。 */
#define CS_0()				CS_GPIO->BSRRH = CS_PIN

/* CLR */	
#define CLR_CLK_ENABLE() 	__HAL_RCC_GPIOE_CLK_ENABLE()
/* 宏定义: CLR_GPIO - CLR_GPIO 的宏定义。 */
#define CLR_GPIO			GPIOE
/* 宏定义: CLR_PIN - CLR_PIN 的宏定义。 */
#define CLR_PIN				GPIO_PIN_4
/* 宏定义: CLR_1 - CLR_1 的宏定义。 */
#define CLR_1()				CLR_GPIO->BSRRL = CLR_PIN
/* 宏定义: CLR_0 - CLR_0 的宏定义。 */
#define CLR_0()				CLR_GPIO->BSRRH = CLR_PIN

/* LDAC ʹ����չIO */	
#define LDAC_1()			HC574_SetPin(NRF24L01_CE, 1);
/* 宏定义: LDAC_0 - LDAC_0 的宏定义。 */
#define LDAC_0()			HC574_SetPin(NRF24L01_CE, 0);

/*
*********************************************************************************************************
*   函 数 名: DAC8562_WriteCmd
*   功能说明: DAC8562_WriteCmd 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void DAC8562_WriteCmd(uint32_t _cmd);

/* �����ѹ��DACֵ��Ĺ�ϵ�� ����У׼ x��dac y �ǵ�ѹ 0.1mV */
#define X1	0
/* 宏定义: Y1 - Y1 的宏定义。 */
#define Y1  -100000

/* 宏定义: X2 - X2 的宏定义。 */
#define X2	65535
/* 宏定义: Y2 - Y2 的宏定义。 */
#define Y2  100000

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitDAC8562
*	����˵��: ����GPIO����ʼ��DAC8562�Ĵ���
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitDAC8562(void)
{
	/* ����GPIO */
	{
		GPIO_InitTypeDef gpio_init;

		/* ��GPIOʱ�� */
		CS_CLK_ENABLE();
		CLR_CLK_ENABLE();
		
		gpio_init.Mode = GPIO_MODE_OUTPUT_PP;		/* ����������� */
		gpio_init.Pull = GPIO_NOPULL;				/* ���������費ʹ�� */
		gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;  	/* GPIO�ٶȵȼ� */	
		
		gpio_init.Pin = CS_PIN;	
		HAL_GPIO_Init(CS_GPIO, &gpio_init);	

		gpio_init.Pin = CLR_PIN;	
		HAL_GPIO_Init(CLR_GPIO, &gpio_init);			
	}
	
	CLR_0();		/* CLR��GND�ɿ�һЩ��CLR���½��ش��� */
	LDAC_0();		/* �����첽����ģʽ�������Ž�GND */

	/* ��ʼ��DAC8562�Ĵ��� */
	{
		/* Power up DAC-A and DAC-B */
		DAC8562_WriteCmd((4 << 19) | (0 << 16) | (3 << 0));

		/* LDAC pin inactive for DAC-B and DAC-A  ��ʹ��LDAC���Ÿ������� */
		DAC8562_WriteCmd((6 << 19) | (0 << 16) | (3 << 0));

		/* ��λ2��DAC���м�ֵ, ���0V */
		DAC8562_SetDacData(0, 32767);
		DAC8562_SetDacData(1, 32767);

		/* ѡ���ڲ��ο�����λ2��DAC������=2 ����λʱ���ڲ��ο��ǽ�ֹ��) */
		DAC8562_WriteCmd((7 << 19) | (0 << 16) | (1 << 0));
	}
}

/*
*********************************************************************************************************
*	�� �� ��: DAC8562_SetCS
*	����˵��: DAC8562 Ƭѡ���ƺ���
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void DAC8562_SetCS(uint8_t _Level)
{
	if (_Level == 0)
	{
		bsp_SpiBusEnter();	/* ռ��SPI����  */	
		bsp_InitSPIParam(SPI_BAUDRATEPRESCALER_8, SPI_PHASE_1EDGE, SPI_POLARITY_LOW);		
		CS_0();
	}
	else
	{		
		CS_1();	
		bsp_SpiBusExit();	/* �ͷ�SPI���� */
	}	
}

/*
*********************************************************************************************************
*	�� �� ��: DAC8562_WriteCmd
*	����˵��: ��SPI���߷���24��bit���ݡ�
*	��    ��: _cmd : ����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void DAC8562_WriteCmd(uint32_t _cmd)
{
	DAC8562_SetCS(0);

	g_spiLen = 0;
	g_spiTxBuf[g_spiLen++] = (_cmd >> 16);
	g_spiTxBuf[g_spiLen++] = (_cmd >> 8);
	g_spiTxBuf[g_spiLen++] = (_cmd);
	bsp_spiTransfer();		

	DAC8562_SetCS(1);
}

/*
*********************************************************************************************************
*	�� �� ��: DAC8562_SetDacData
*	����˵��: ����DAC��������������¡�
*	��    ��: _ch, ͨ��, 0 , 1
*		     _data : ����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void DAC8562_SetDacData(uint8_t _ch, uint16_t _dac)
{
	if (_ch == 0)
	{
		/* Write to DAC-A input register and update DAC-A; */
		DAC8562_WriteCmd((3 << 19) | (0 << 16) | (_dac << 0));
	}
	else if (_ch == 1)
	{
		/* Write to DAC-B input register and update DAC-A; */
		DAC8562_WriteCmd((3 << 19) | (1 << 16) | (_dac << 0));
	}
}

/*
*********************************************************************************************************
*	�� �� ��: DAC8562_DacToVoltage
*	����˵��: ��DACֵ����Ϊ��ѹֵ����λ0.1mV
*	��    ��: _dac  16λDAC��
*	�� �� ֵ: ��ѹ����λ0.1mV
*********************************************************************************************************
*/
int32_t DAC8562_DacToVoltage(uint16_t _dac)
{
	int32_t y;

	/* CaculTwoPoint(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x);*/
	y =  CaculTwoPoint(X1, Y1, X2, Y2, _dac);
	return y;
}

/*
*********************************************************************************************************
*	�� �� ��: DAC8562_VoltageToDac
*	����˵��: ��DACֵ����Ϊ��ѹֵ����λ 0.1mV
*	��    ��: _volt ��ѹ����λ0.1mV
*	�� �� ֵ: 16λDAC��
*********************************************************************************************************
*/
uint32_t DAC8562_VoltageToDac(int32_t _volt)
{
	/* CaculTwoPoint(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x);*/
	return CaculTwoPoint(Y1, X1, Y2, X2, _volt);
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
