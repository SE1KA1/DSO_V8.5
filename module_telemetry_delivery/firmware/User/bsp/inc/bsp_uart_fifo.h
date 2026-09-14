/**
 * @file bsp_uart_fifo.h
 * @brief UART FIFO 驱动：printf、VoFA、串口诊断底层。
 */

/*
*********************************************************************************************************
*
*	ģ������ : �����ж�+FIFO����ģ��
*	�ļ����� : bsp_uart_fifo.h
*	˵    �� : ͷ�ļ�
*
*	Copyright (C), 2015-2020, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_USART_FIFO_H_
/* 宏定义: _BSP_USART_FIFO_H_ - 头文件保护宏。 */
#define _BSP_USART_FIFO_H_

/*
	������STM32-V7 ���ڷ��䣺
	������1�� RS232 оƬ��1·��
		PA9/USART1_TX	  --- ��ӡ���Կ�
		P10/USART1_RX

	������2�� PA2 �ܽ�������̫���� RX�ܽ����ڽ���GPS�ź�
		PA2/USART2_TX/ETH_MDIO (������̫�����������ڷ�����)
		PA3/USART2_RX	;��GPSģ�����

	������3�� RS485 ͨ�� - TTL ���� �� ����
		PB10/USART3_TX
		PB11/USART3_RX

	������4�� --- ���������á� SD��ռ�� 
	������5�� --- ���������á� SD��ռ��

	������6��--- GPRSģ�� ��WIFIģ��(ESP8266) 
		PC6/USART6_TX
		PC7/USART6_RX
		
	������7�� --- ���������á� SPI3ռ��
	
	������8�� --- ���������á� LTDC��ʾ�ӿ���
*/


#define	UART1_FIFO_EN	1
/* 宏定义: UART2_FIFO_EN - UART2_FIFO_EN 的宏定义。 */
#define	UART2_FIFO_EN	0
/* 宏定义: UART3_FIFO_EN - UART3_FIFO_EN 的宏定义。 */
#define UART3_FIFO_EN 0
/* 宏定义: UART4_FIFO_EN - UART4_FIFO_EN 的宏定义。 */
#define	UART4_FIFO_EN	0
/* 宏定义: UART5_FIFO_EN - UART5_FIFO_EN 的宏定义。 */
#define	UART5_FIFO_EN	0
/* 宏定义: UART6_FIFO_EN - UART6_FIFO_EN 的宏定义。 */
#define	UART6_FIFO_EN	0
/* 宏定义: UART7_FIFO_EN - UART7_FIFO_EN 的宏定义。 */
#define	UART7_FIFO_EN	0
/* 宏定义: UART8_FIFO_EN - UART8_FIFO_EN 的宏定义。 */
#define	UART8_FIFO_EN	0

/* PB2 ����RS485оƬ�ķ���ʹ�� */
#define RS485_TXEN_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOB_CLK_ENABLE()
/* 宏定义: RS485_TXEN_GPIO_PORT - RS485_TXEN_GPIO_PORT 的宏定义。 */
#define RS485_TXEN_GPIO_PORT              GPIOB
/* 宏定义: RS485_TXEN_PIN - RS485_TXEN_PIN 的宏定义。 */
#define RS485_TXEN_PIN                    GPIO_PIN_2

/* 宏定义: RS485_RX_EN - RS485_RX_EN 的宏定义。 */
#define RS485_RX_EN()	RS485_TXEN_GPIO_PORT->BSRRH = RS485_TXEN_PIN
/* 宏定义: RS485_TX_EN - RS485_TX_EN 的宏定义。 */
#define RS485_TX_EN()	RS485_TXEN_GPIO_PORT->BSRRL = RS485_TXEN_PIN

/* ����˿ں� */
typedef enum
{
	COM1 = 0,	/* USART1 */
	COM2 = 1,	/* USART2 */
	COM3 = 2,	/* USART3 */
	COM4 = 3,	/* UART4 */
	COM5 = 4,	/* UART5 */
	COM6 = 5,	/* USART6 */
	COM7 = 6,	/* UART7 */	
	COM8 = 7	/* UART8 */	
}COM_PORT_E;

/* ���崮�ڲ����ʺ�FIFO��������С����Ϊ���ͻ������ͽ��ջ�����, ֧��ȫ˫�� */
#if UART1_FIFO_EN == 1
/* 宏定义: UART1_BAUD - UART1_BAUD 的宏定义。 */
	#define UART1_BAUD			1152000
/* 宏定义: UART1_TX_BUF_SIZE - UART1_TX_BUF_SIZE 的宏定义。 */
	#define UART1_TX_BUF_SIZE	1*1024
/* 宏定义: UART1_RX_BUF_SIZE - UART1_RX_BUF_SIZE 的宏定义。 */
	#define UART1_RX_BUF_SIZE	1*1024
#endif

#if UART2_FIFO_EN == 1
/* 宏定义: UART2_BAUD - UART2_BAUD 的宏定义。 */
	#define UART2_BAUD			9600
/* 宏定义: UART2_TX_BUF_SIZE - UART2_TX_BUF_SIZE 的宏定义。 */
	#define UART2_TX_BUF_SIZE	10
/* 宏定义: UART2_RX_BUF_SIZE - UART2_RX_BUF_SIZE 的宏定义。 */
	#define UART2_RX_BUF_SIZE	2*1024
#endif

#if UART3_FIFO_EN == 1
/* 宏定义: UART3_BAUD - UART3_BAUD 的宏定义。 */
	#define UART3_BAUD			9600
/* 宏定义: UART3_TX_BUF_SIZE - UART3_TX_BUF_SIZE 的宏定义。 */
	#define UART3_TX_BUF_SIZE	1*1024
/* 宏定义: UART3_RX_BUF_SIZE - UART3_RX_BUF_SIZE 的宏定义。 */
	#define UART3_RX_BUF_SIZE	1*1024
#endif

#if UART4_FIFO_EN == 1
/* 宏定义: UART4_BAUD - UART4_BAUD 的宏定义。 */
	#define UART4_BAUD			115200
/* 宏定义: UART4_TX_BUF_SIZE - UART4_TX_BUF_SIZE 的宏定义。 */
	#define UART4_TX_BUF_SIZE	1*1024
/* 宏定义: UART4_RX_BUF_SIZE - UART4_RX_BUF_SIZE 的宏定义。 */
	#define UART4_RX_BUF_SIZE	1*1024
#endif

#if UART5_FIFO_EN == 1
/* 宏定义: UART5_BAUD - UART5_BAUD 的宏定义。 */
	#define UART5_BAUD			115200
/* 宏定义: UART5_TX_BUF_SIZE - UART5_TX_BUF_SIZE 的宏定义。 */
	#define UART5_TX_BUF_SIZE	1*1024
/* 宏定义: UART5_RX_BUF_SIZE - UART5_RX_BUF_SIZE 的宏定义。 */
	#define UART5_RX_BUF_SIZE	1*1024
#endif

#if UART6_FIFO_EN == 1
/* 宏定义: UART6_BAUD - UART6_BAUD 的宏定义。 */
	#define UART6_BAUD			115200
/* 宏定义: UART6_TX_BUF_SIZE - UART6_TX_BUF_SIZE 的宏定义。 */
	#define UART6_TX_BUF_SIZE	1*1024
/* 宏定义: UART6_RX_BUF_SIZE - UART6_RX_BUF_SIZE 的宏定义。 */
	#define UART6_RX_BUF_SIZE	1*1024
#endif

#if UART7_FIFO_EN == 1
/* 宏定义: UART7_BAUD - UART7_BAUD 的宏定义。 */
	#define UART7_BAUD			115200
/* 宏定义: UART7_TX_BUF_SIZE - UART7_TX_BUF_SIZE 的宏定义。 */
	#define UART7_TX_BUF_SIZE	1*1024
/* 宏定义: UART7_RX_BUF_SIZE - UART7_RX_BUF_SIZE 的宏定义。 */
	#define UART7_RX_BUF_SIZE	1*1024
#endif

#if UART8_FIFO_EN == 1
/* 宏定义: UART8_BAUD - UART8_BAUD 的宏定义。 */
	#define UART8_BAUD			115200
/* 宏定义: UART8_TX_BUF_SIZE - UART8_TX_BUF_SIZE 的宏定义。 */
	#define UART8_TX_BUF_SIZE	1*1024
/* 宏定义: UART8_RX_BUF_SIZE - UART8_RX_BUF_SIZE 的宏定义。 */
	#define UART8_RX_BUF_SIZE	1*1024
#endif

/* �����豸�ṹ�� */
typedef struct
{
	USART_TypeDef *uart;		/* STM32�ڲ������豸ָ�� */
	uint8_t *pTxBuf;			/* ���ͻ����� */
	uint8_t *pRxBuf;			/* ���ջ����� */
	uint16_t usTxBufSize;		/* ���ͻ�������С */
	uint16_t usRxBufSize;		/* ���ջ�������С */
	__IO uint16_t usTxWrite;	/* ���ͻ�����дָ�� */
	__IO uint16_t usTxRead;		/* ���ͻ�������ָ�� */
	__IO uint16_t usTxCount;	/* �ȴ����͵����ݸ��� */

	__IO uint16_t usRxWrite;	/* ���ջ�����дָ�� */
	__IO uint16_t usRxRead;		/* ���ջ�������ָ�� */
	__IO uint16_t usRxCount;	/* ��δ��ȡ�������ݸ��� */

	void (*SendBefor)(void); 	/* ��ʼ����֮ǰ�Ļص�����ָ�루��Ҫ����RS485�л�������ģʽ�� */
	void (*SendOver)(void); 	/* ������ϵĻص�����ָ�루��Ҫ����RS485������ģʽ�л�Ϊ����ģʽ�� */
	void (*ReciveNew)(uint8_t _byte);	/* �����յ����ݵĻص�����ָ�� */
	uint8_t Sending;			/* ���ڷ����� */
}UART_T;

/*
*********************************************************************************************************
*   函 数 名: bsp_InitUart
*   功能说明: bsp_InitUart 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void bsp_InitUart(void);
/*
*********************************************************************************************************
*   函 数 名: bsp_Uart1SendStringPolled
*   功能说明: bsp_Uart1SendStringPolled 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void bsp_Uart1SendStringPolled(const char *text);
/*
*********************************************************************************************************
*   函 数 名: comSendBuf
*   功能说明: comSendBuf 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void comSendBuf(COM_PORT_E _ucPort, uint8_t *_ucaBuf, uint16_t _usLen);
/*
*********************************************************************************************************
*   函 数 名: comSendChar
*   功能说明: comSendChar 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void comSendChar(COM_PORT_E _ucPort, uint8_t _ucByte);
/*
*********************************************************************************************************
*   函 数 名: comGetChar
*   功能说明: comGetChar 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t comGetChar(COM_PORT_E _ucPort, uint8_t *_pByte);
/*
*********************************************************************************************************
*   函 数 名: comSendBuf
*   功能说明: comSendBuf 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void comSendBuf(COM_PORT_E _ucPort, uint8_t *_ucaBuf, uint16_t _usLen);
/*
*********************************************************************************************************
*   函 数 名: comClearTxFifo
*   功能说明: comClearTxFifo 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void comClearTxFifo(COM_PORT_E _ucPort);
/*
*********************************************************************************************************
*   函 数 名: comClearRxFifo
*   功能说明: comClearRxFifo 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void comClearRxFifo(COM_PORT_E _ucPort);
/*
*********************************************************************************************************
*   函 数 名: comSetBaud
*   功能说明: comSetBaud 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void comSetBaud(COM_PORT_E _ucPort, uint32_t _BaudRate);

/*
*********************************************************************************************************
*   函 数 名: USART_SetBaudRate
*   功能说明: USART_SetBaudRate 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void USART_SetBaudRate(USART_TypeDef* USARTx, uint32_t BaudRate);
/*
*********************************************************************************************************
*   函 数 名: bsp_SetUartParam
*   功能说明: bsp_SetUartParam 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void bsp_SetUartParam(USART_TypeDef *Instance,  uint32_t BaudRate, uint32_t Parity, uint32_t Mode);

/*
*********************************************************************************************************
*   函 数 名: RS485_SendBuf
*   功能说明: RS485_SendBuf 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void RS485_SendBuf(uint8_t *_ucaBuf, uint16_t _usLen);
/*
*********************************************************************************************************
*   函 数 名: RS485_SendStr
*   功能说明: RS485_SendStr 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void RS485_SendStr(char *_pBuf);
/*
*********************************************************************************************************
*   函 数 名: RS485_SetBaud
*   功能说明: RS485_SetBaud 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void RS485_SetBaud(uint32_t _baud);
/*
*********************************************************************************************************
*   函 数 名: UartTxEmpty
*   功能说明: UartTxEmpty 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t UartTxEmpty(COM_PORT_E _ucPort);

#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/



