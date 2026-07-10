/**
 * @file bsp_fmc_io.h
 * @brief 安富莱 BSP 通用驱动（FMC 扩展 IO）头文件；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : STM32-V6��������չIO��������
*	�ļ����� : bsp_ext_io.g
*	˵    �� : V6��������FMC��������չ��32λ���IO����ַΪ (0x6820 0000)
*
*	Copyright (C), 2015-2020, ���������� www.armfly.com
*
*********************************************************************************************************
*/
#ifndef __BSP_EXT_IO_H
/* 宏定义: __BSP_EXT_IO_H - 头文件保护宏。 */
#define __BSP_EXT_IO_H

/* ���ⲿ���õĺ������� */
/*
	������STM32-V6 ��������չ���߷���: ���ߵ�ַ = 0x6820 0000
	D0  - GPRS_RERM_ON
	D1  - GPRS_RESET
	D2  - NRF24L01_CE
	D3  - NRF905_TX_EN
	D4  - NRF905_TRX_CE/VS1053_XDCS
	D5  - NRF905_PWR_UP
	D6  - ESP8266_G0
	D7  - ESP8266_G2
	
	D8  - LED1
	D9  - LED2
	D10 - LED3
	D11 - LED4
	D12 - TP_NRST
	D13 - AD7606_OS0
	D14 - AD7606_OS1
	D15 - AD7606_OS2
	
	Ԥ����8��5V���IO: Y50_0 - Y50_1
	D16  - Y50_0
	D17  - Y50_1
	D18  - Y50_2
	D19  - Y50_3
	D20  - Y50_4
	D21  - Y50_5
	D22  - Y50_6
	D23  - Y50_7	

	Ԥ����8��3.3V���IO: Y33_0 - Y33_1
	D24  - AD7606_RESET
	D25  - AD7606_RAGE
	D26  - Y33_2
	D27  - Y33_3
	D28  - Y33_4
	D29  - Y33_5
	D30  - Y33_6
	D31  - Y33_7				
*/

#ifndef GPIO_PIN_0
	#define GPIO_PIN_0                 ((uint16_t)0x0001)  /* Pin 0 selected */
	#define GPIO_PIN_1                 ((uint16_t)0x0002)  /* Pin 1 selected */
	#define GPIO_PIN_2                 ((uint16_t)0x0004)  /* Pin 2 selected */
	#define GPIO_PIN_3                 ((uint16_t)0x0008)  /* Pin 3 selected */
	#define GPIO_PIN_4                 ((uint16_t)0x0010)  /* Pin 4 selected */
	#define GPIO_PIN_5                 ((uint16_t)0x0020)  /* Pin 5 selected */
	#define GPIO_PIN_6                 ((uint16_t)0x0040)  /* Pin 6 selected */
	#define GPIO_PIN_7                 ((uint16_t)0x0080)  /* Pin 7 selected */
	#define GPIO_PIN_8                 ((uint16_t)0x0100)  /* Pin 8 selected */
	#define GPIO_PIN_9                 ((uint16_t)0x0200)  /* Pin 9 selected */
	#define GPIO_PIN_10                ((uint16_t)0x0400)  /* Pin 10 selected */
	#define GPIO_PIN_11                ((uint16_t)0x0800)  /* Pin 11 selected */
	#define GPIO_PIN_12                ((uint16_t)0x1000)  /* Pin 12 selected */
	#define GPIO_PIN_13                ((uint16_t)0x2000)  /* Pin 13 selected */
	#define GPIO_PIN_14                ((uint16_t)0x4000)  /* Pin 14 selected */
	#define GPIO_PIN_15                ((uint16_t)0x8000)  /* Pin 15 selected */
#endif	

#define GPIO_PIN_16                 ((uint32_t)0x00010000)  /* Pin 0 selected */
#define GPIO_PIN_17                 ((uint32_t)0x00020000)  /* Pin 1 selected */
#define GPIO_PIN_18                 ((uint32_t)0x00040000)  /* Pin 2 selected */
#define GPIO_PIN_19                 ((uint32_t)0x00080000)  /* Pin 3 selected */
#define GPIO_PIN_20                 ((uint32_t)0x00100000)  /* Pin 4 selected */
#define GPIO_PIN_21                 ((uint32_t)0x00200000)  /* Pin 5 selected */
#define GPIO_PIN_22                 ((uint32_t)0x00400000)  /* Pin 6 selected */
#define GPIO_PIN_23                 ((uint32_t)0x00800000)  /* Pin 7 selected */
#define GPIO_PIN_24                 ((uint32_t)0x01000000)  /* Pin 8 selected */
#define GPIO_PIN_25                 ((uint32_t)0x02000000)  /* Pin 9 selected */
#define GPIO_PIN_26                 ((uint32_t)0x04000000)  /* Pin 10 selected */
#define GPIO_PIN_27                 ((uint32_t)0x08000000)  /* Pin 11 selected */
#define GPIO_PIN_28                 ((uint32_t)0x10000000)  /* Pin 12 selected */
#define GPIO_PIN_29                 ((uint32_t)0x20000000)  /* Pin 13 selected */
#define GPIO_PIN_30                 ((uint32_t)0x40000000)  /* Pin 14 selected */
#define GPIO_PIN_31                 ((uint32_t)0x80000000)  /* Pin 15 selected */

/* Ϊ�˷�����䣬����������ĺ� */
#define	GPRS_TERM_ON   GPIO_PIN_0
/* 宏定义: GPRS_RESET - GPRS_RESET 的宏定义。 */
#define	GPRS_RESET     GPIO_PIN_1
/* 宏定义: NRF24L01_CE - NRF24L01_CE 的宏定义。 */
#define	NRF24L01_CE    GPIO_PIN_2
/* 宏定义: NRF905_TX_EN - NRF905_TX_EN 的宏定义。 */
#define	NRF905_TX_EN   GPIO_PIN_3
/* 宏定义: VS1053_XRESET - VS1053_XRESET 的宏定义。 */
	#define	VS1053_XRESET  GPIO_PIN_3
/* 宏定义: NRF905_TRX_CE - NRF905_TRX_CE 的宏定义。 */
#define	NRF905_TRX_CE  GPIO_PIN_4
	#define VS1053_XDCS    GPIO_PIN_4	/* NRF905_TRX_CE�� VS1053_XDCS ���� */
#define	NRF905_PWR_UP  GPIO_PIN_5
/* 宏定义: ESP8266_G0 - ESP8266_G0 的宏定义。 */
#define	ESP8266_G0     GPIO_PIN_6
/* 宏定义: ESP8266_G2 - ESP8266_G2 的宏定义。 */
#define	ESP8266_G2     GPIO_PIN_7
	
/* 宏定义: LED1 - LED1 的宏定义。 */
#define	LED1           GPIO_PIN_8
/* 宏定义: LED2 - LED2 的宏定义。 */
#define	LED2           GPIO_PIN_9
/* 宏定义: LED3 - LED3 的宏定义。 */
#define	LED3           GPIO_PIN_10
/* 宏定义: LED4 - LED4 的宏定义。 */
#define	LED4           GPIO_PIN_11
/* 宏定义: TP_NRST - TP_NRST 的宏定义。 */
#define	TP_NRST        GPIO_PIN_12
/* 宏定义: AD7606_OS0 - AD7606_OS0 的宏定义。 */
#define	AD7606_OS0     GPIO_PIN_13
/* 宏定义: AD7606_OS1 - AD7606_OS1 的宏定义。 */
#define	AD7606_OS1     GPIO_PIN_14
/* 宏定义: AD7606_OS2 - AD7606_OS2 的宏定义。 */
#define	AD7606_OS2     GPIO_PIN_15
	
/* 宏定义: Y50_0 - Y50_0 的宏定义。 */
#define	Y50_0          GPIO_PIN_16
/* 宏定义: Y50_1 - Y50_1 的宏定义。 */
#define	Y50_1          GPIO_PIN_17
/* 宏定义: Y50_2 - Y50_2 的宏定义。 */
#define	Y50_2          GPIO_PIN_18
/* 宏定义: Y50_3 - Y50_3 的宏定义。 */
#define	Y50_3          GPIO_PIN_19
/* 宏定义: Y50_4 - Y50_4 的宏定义。 */
#define	Y50_4          GPIO_PIN_20
/* 宏定义: Y50_5 - Y50_5 的宏定义。 */
#define	Y50_5          GPIO_PIN_21
/* 宏定义: Y50_6 - Y50_6 的宏定义。 */
#define	Y50_6          GPIO_PIN_22
/* 宏定义: Y50_7 - Y50_7 的宏定义。 */
#define	Y50_7          GPIO_PIN_23	

/* 宏定义: AD7606_RESET - AD7606_RESET 的宏定义。 */
#define	AD7606_RESET   GPIO_PIN_24
/* 宏定义: AD7606_RANGE - AD7606_RANGE 的宏定义。 */
#define	AD7606_RANGE   GPIO_PIN_25
/* 宏定义: Y33_2 - Y33_2 的宏定义。 */
#define	Y33_2          GPIO_PIN_26
/* 宏定义: Y33_3 - Y33_3 的宏定义。 */
#define	Y33_3          GPIO_PIN_27
/* 宏定义: Y33_4 - Y33_4 的宏定义。 */
#define	Y33_4          GPIO_PIN_28
/* 宏定义: Y33_5 - Y33_5 的宏定义。 */
#define	Y33_5          GPIO_PIN_29
/* 宏定义: Y33_6 - Y33_6 的宏定义。 */
#define	Y33_6          GPIO_PIN_30
/* 宏定义: Y33_7 - Y33_7 的宏定义。 */
#define	Y33_7          GPIO_PIN_31

/*
*********************************************************************************************************
*   函 数 名: bsp_InitExtIO
*   功能说明: bsp_InitExtIO 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void bsp_InitExtIO(void);
/*
*********************************************************************************************************
*   函 数 名: HC574_SetPin
*   功能说明: HC574_SetPin 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void HC574_SetPin(uint32_t _pin, uint8_t _value);
/*
*********************************************************************************************************
*   函 数 名: HC574_GetPin
*   功能说明: HC574_GetPin 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t HC574_GetPin(uint32_t _pin);
/*
*********************************************************************************************************
*   函 数 名: HC574_TogglePin
*   功能说明: HC574_TogglePin 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void HC574_TogglePin(uint32_t _pin);

extern __IO uint32_t g_HC574;

#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
