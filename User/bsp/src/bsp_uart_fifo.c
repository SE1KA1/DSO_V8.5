/**
 * @file bsp_uart_fifo.c
 * @brief COM1 FIFO 收发与中断服务。
 */

/*
*********************************************************************************************************
*
*	ģ������ : �����ж�+FIFO����ģ��
*	�ļ����� : bsp_uart_fifo.c
*	��    �� : V1.8
*	˵    �� : ���ô����ж�+FIFOģʽʵ�ֶ�����ڵ�ͬʱ����
*	�޸ļ�¼ :
*		�汾��  ����       ����    ˵��
*		V1.0    2013-02-01 armfly  ��ʽ����
*		V1.1    2013-06-09 armfly  FiFo�ṹ����TxCount��Ա�����������жϻ�������; ���� ��FiFo�ĺ���
*		V1.2	2014-09-29 armfly  ����RS485 MODBUS�ӿڡ����յ����ֽں�ֱ��ִ�лص�������
*		V1.3	2015-07-23 armfly  ���� UART_T �ṹ�Ķ�дָ�뼸����Ա������������ __IO ����,�����Ż���
*					�ᵼ�´��ڷ��ͺ���������
*		V1.4	2015-08-04 armfly  ���UART4����bug  GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_USART1);
*		V1.5	2015-10-08 armfly  �����޸Ĳ����ʵĽӿں���
*		V1.6	2018-09-07 armfly  ��ֲ��STM32H7ƽ̨
*		V1.7	2018-10-01 armfly  ���� Sending ��־����ʾ���ڷ�����
*		V1.8	2018-11-26 armfly  ����UART8����8������
*
*	Copyright (C), 2015-2030, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/* ����1��GPIO  PA9, PA10   RS323 DB9�ӿ� */
#define USART1_CLK_ENABLE()              __HAL_RCC_USART1_CLK_ENABLE()

/* 宏定义: USART1_TX_GPIO_CLK_ENABLE - USART1_TX_GPIO_CLK_ENABLE 的宏定义。 */
#define USART1_TX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOA_CLK_ENABLE()
/* 宏定义: USART1_TX_GPIO_PORT - USART1_TX_GPIO_PORT 的宏定义。 */
#define USART1_TX_GPIO_PORT              GPIOA
/* 宏定义: USART1_TX_PIN - USART1_TX_PIN 的宏定义。 */
#define USART1_TX_PIN                    GPIO_PIN_9
/* 宏定义: USART1_TX_AF - USART1_TX_AF 的宏定义。 */
#define USART1_TX_AF                     GPIO_AF7_USART1

/* 宏定义: USART1_RX_GPIO_CLK_ENABLE - USART1_RX_GPIO_CLK_ENABLE 的宏定义。 */
#define USART1_RX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOA_CLK_ENABLE()
/* 宏定义: USART1_RX_GPIO_PORT - USART1_RX_GPIO_PORT 的宏定义。 */
#define USART1_RX_GPIO_PORT              GPIOA
/* 宏定义: USART1_RX_PIN - USART1_RX_PIN 的宏定义。 */
#define USART1_RX_PIN                    GPIO_PIN_10
/* 宏定义: USART1_RX_AF - USART1_RX_AF 的宏定义。 */
#define USART1_RX_AF                     GPIO_AF7_USART1

/* ����2��GPIO --- PA2 PA3  GPS (ֻ��RX�� TX����̫��ռ�ã� */
#define USART2_CLK_ENABLE()              __HAL_RCC_USART2_CLK_ENABLE()

/* 宏定义: USART2_TX_GPIO_CLK_ENABLE - USART2_TX_GPIO_CLK_ENABLE 的宏定义。 */
#define USART2_TX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOA_CLK_ENABLE()
/* 宏定义: USART2_TX_GPIO_PORT - USART2_TX_GPIO_PORT 的宏定义。 */
#define USART2_TX_GPIO_PORT              GPIOA
/* 宏定义: USART2_TX_PIN - USART2_TX_PIN 的宏定义。 */
#define USART2_TX_PIN                    GPIO_PIN_2
/* 宏定义: USART2_TX_AF - USART2_TX_AF 的宏定义。 */
#define USART2_TX_AF                     GPIO_AF7_USART2

/* 宏定义: USART2_RX_GPIO_CLK_ENABLE - USART2_RX_GPIO_CLK_ENABLE 的宏定义。 */
#define USART2_RX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOA_CLK_ENABLE()
/* 宏定义: USART2_RX_GPIO_PORT - USART2_RX_GPIO_PORT 的宏定义。 */
#define USART2_RX_GPIO_PORT              GPIOA
/* 宏定义: USART2_RX_PIN - USART2_RX_PIN 的宏定义。 */
#define USART2_RX_PIN                    GPIO_PIN_3
/* 宏定义: USART2_RX_AF - USART2_RX_AF 的宏定义。 */
#define USART2_RX_AF                     GPIO_AF7_USART2

/* ����3��GPIO --- PB10 PB11  RS485 */
#define USART3_CLK_ENABLE()              __HAL_RCC_USART3_CLK_ENABLE()

/* 宏定义: USART3_TX_GPIO_CLK_ENABLE - USART3_TX_GPIO_CLK_ENABLE 的宏定义。 */
#define USART3_TX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOB_CLK_ENABLE()
/* 宏定义: USART3_TX_GPIO_PORT - USART3_TX_GPIO_PORT 的宏定义。 */
#define USART3_TX_GPIO_PORT              GPIOB
/* 宏定义: USART3_TX_PIN - USART3_TX_PIN 的宏定义。 */
#define USART3_TX_PIN                    GPIO_PIN_10
/* 宏定义: USART3_TX_AF - USART3_TX_AF 的宏定义。 */
#define USART3_TX_AF                     GPIO_AF7_USART3

/* 宏定义: USART3_RX_GPIO_CLK_ENABLE - USART3_RX_GPIO_CLK_ENABLE 的宏定义。 */
#define USART3_RX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOB_CLK_ENABLE()
/* 宏定义: USART3_RX_GPIO_PORT - USART3_RX_GPIO_PORT 的宏定义。 */
#define USART3_RX_GPIO_PORT              GPIOB
/* 宏定义: USART3_RX_PIN - USART3_RX_PIN 的宏定义。 */
#define USART3_RX_PIN                    GPIO_PIN_11
/* 宏定义: USART3_RX_AF - USART3_RX_AF 的宏定义。 */
#define USART3_RX_AF                     GPIO_AF7_USART3

/* ����4��GPIO --- PC10 PC11  ��SD��ռ�� */
#define UART4_CLK_ENABLE()              __HAL_RCC_UART4_CLK_ENABLE()

/* 宏定义: UART4_TX_GPIO_CLK_ENABLE - UART4_TX_GPIO_CLK_ENABLE 的宏定义。 */
#define UART4_TX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOC_CLK_ENABLE()
/* 宏定义: UART4_TX_GPIO_PORT - UART4_TX_GPIO_PORT 的宏定义。 */
#define UART4_TX_GPIO_PORT              GPIOC
/* 宏定义: UART4_TX_PIN - UART4_TX_PIN 的宏定义。 */
#define UART4_TX_PIN                    GPIO_PIN_10
/* 宏定义: UART4_TX_AF - UART4_TX_AF 的宏定义。 */
#define UART4_TX_AF                     GPIO_AF8_UART4

/* 宏定义: UART4_RX_GPIO_CLK_ENABLE - UART4_RX_GPIO_CLK_ENABLE 的宏定义。 */
#define UART4_RX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOC_CLK_ENABLE()
/* 宏定义: UART4_RX_GPIO_PORT - UART4_RX_GPIO_PORT 的宏定义。 */
#define UART4_RX_GPIO_PORT              GPIOC
/* 宏定义: UART4_RX_PIN - UART4_RX_PIN 的宏定义。 */
#define UART4_RX_PIN                    GPIO_PIN_11
/* 宏定义: UART4_RX_AF - UART4_RX_AF 的宏定义。 */
#define UART4_RX_AF                     GPIO_AF8_UART4

/* ����5��GPIO --- PC12/UART5_TX PD2/UART5_RX (��SD��ռ�ã� */
#define UART5_CLK_ENABLE()              __HAL_RCC_UART5_CLK_ENABLE()

/* 宏定义: UART5_TX_GPIO_CLK_ENABLE - UART5_TX_GPIO_CLK_ENABLE 的宏定义。 */
#define UART5_TX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOC_CLK_ENABLE()
/* 宏定义: UART5_TX_GPIO_PORT - UART5_TX_GPIO_PORT 的宏定义。 */
#define UART5_TX_GPIO_PORT              GPIOC
/* 宏定义: UART5_TX_PIN - UART5_TX_PIN 的宏定义。 */
#define UART5_TX_PIN                    GPIO_PIN_12
/* 宏定义: UART5_TX_AF - UART5_TX_AF 的宏定义。 */
#define UART5_TX_AF                     GPIO_AF8_UART5

/* 宏定义: UART5_RX_GPIO_CLK_ENABLE - UART5_RX_GPIO_CLK_ENABLE 的宏定义。 */
#define UART5_RX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOD_CLK_ENABLE()
/* 宏定义: UART5_RX_GPIO_PORT - UART5_RX_GPIO_PORT 的宏定义。 */
#define UART5_RX_GPIO_PORT              GPIOD
/* 宏定义: UART5_RX_PIN - UART5_RX_PIN 的宏定义。 */
#define UART5_RX_PIN                    GPIO_PIN_2
/* 宏定义: UART5_RX_AF - UART5_RX_AF 的宏定义。 */
#define UART5_RX_AF                     GPIO_AF8_UART5

/* ����6��GPIO --- PG14 PC7  GPRS */
#define USART6_CLK_ENABLE()              __HAL_RCC_USART6_CLK_ENABLE()

/* 宏定义: USART6_TX_GPIO_CLK_ENABLE - USART6_TX_GPIO_CLK_ENABLE 的宏定义。 */
#define USART6_TX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOG_CLK_ENABLE()
/* 宏定义: USART6_TX_GPIO_PORT - USART6_TX_GPIO_PORT 的宏定义。 */
#define USART6_TX_GPIO_PORT              GPIOG
/* 宏定义: USART6_TX_PIN - USART6_TX_PIN 的宏定义。 */
#define USART6_TX_PIN                    GPIO_PIN_14
/* 宏定义: USART6_TX_AF - USART6_TX_AF 的宏定义。 */
#define USART6_TX_AF                     GPIO_AF7_USART6

/* 宏定义: USART6_RX_GPIO_CLK_ENABLE - USART6_RX_GPIO_CLK_ENABLE 的宏定义。 */
#define USART6_RX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOC_CLK_ENABLE()
/* 宏定义: USART6_RX_GPIO_PORT - USART6_RX_GPIO_PORT 的宏定义。 */
#define USART6_RX_GPIO_PORT              GPIOC
/* 宏定义: USART6_RX_PIN - USART6_RX_PIN 的宏定义。 */
#define USART6_RX_PIN                    GPIO_PIN_7
/* 宏定义: USART6_RX_AF - USART6_RX_AF 的宏定义。 */
#define USART6_RX_AF                     GPIO_AF7_USART6

/* ����7��GPIO --- PB4/UART7_TX, PB3/UART7_RX   (��SPI3 ռ��) */
#define UART7_CLK_ENABLE()              __HAL_RCC_UART7_CLK_ENABLE()

/* 宏定义: UART7_TX_GPIO_CLK_ENABLE - UART7_TX_GPIO_CLK_ENABLE 的宏定义。 */
#define UART7_TX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOB_CLK_ENABLE()
/* 宏定义: UART7_TX_GPIO_PORT - UART7_TX_GPIO_PORT 的宏定义。 */
#define UART7_TX_GPIO_PORT              GPIOB
/* 宏定义: UART7_TX_PIN - UART7_TX_PIN 的宏定义。 */
#define UART7_TX_PIN                    GPIO_PIN_4
/* 宏定义: UART7_TX_AF - UART7_TX_AF 的宏定义。 */
#define UART7_TX_AF                     GPIO_AF11_UART7

/* 宏定义: UART7_RX_GPIO_CLK_ENABLE - UART7_RX_GPIO_CLK_ENABLE 的宏定义。 */
#define UART7_RX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOB_CLK_ENABLE()
/* 宏定义: UART7_RX_GPIO_PORT - UART7_RX_GPIO_PORT 的宏定义。 */
#define UART7_RX_GPIO_PORT              GPIOB
/* 宏定义: UART7_RX_PIN - UART7_RX_PIN 的宏定义。 */
#define UART7_RX_PIN                    GPIO_PIN_3
/* 宏定义: UART7_RX_AF - UART7_RX_AF 的宏定义。 */
#define UART7_RX_AF                     GPIO_AF11_UART7

/* ����8��GPIO --- PJ8/UART8_TX, PJ9/UART8_RX   (RGBӲ���ӿ�ռ��) */
#define UART8_CLK_ENABLE()              __HAL_RCC_UART8_CLK_ENABLE()

/* 宏定义: UART8_TX_GPIO_CLK_ENABLE - UART8_TX_GPIO_CLK_ENABLE 的宏定义。 */
#define UART8_TX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOJ_CLK_ENABLE()
/* 宏定义: UART8_TX_GPIO_PORT - UART8_TX_GPIO_PORT 的宏定义。 */
#define UART8_TX_GPIO_PORT              GPIOJ
/* 宏定义: UART8_TX_PIN - UART8_TX_PIN 的宏定义。 */
#define UART8_TX_PIN                    GPIO_PIN_8
/* 宏定义: UART8_TX_AF - UART8_TX_AF 的宏定义。 */
#define UART8_TX_AF                     GPIO_AF8_UART8

/* 宏定义: UART8_RX_GPIO_CLK_ENABLE - UART8_RX_GPIO_CLK_ENABLE 的宏定义。 */
#define UART8_RX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOJ_CLK_ENABLE()
/* 宏定义: UART8_RX_GPIO_PORT - UART8_RX_GPIO_PORT 的宏定义。 */
#define UART8_RX_GPIO_PORT              GPIOJ
/* 宏定义: UART8_RX_PIN - UART8_RX_PIN 的宏定义。 */
#define UART8_RX_PIN                    GPIO_PIN_9
/* 宏定义: UART8_RX_AF - UART8_RX_AF 的宏定义。 */
#define UART8_RX_AF                     GPIO_AF8_UART8

/* ����ÿ�����ڽṹ����� */
#if UART1_FIFO_EN == 1
	static UART_T g_tUart1;
	static uint8_t g_TxBuf1[UART1_TX_BUF_SIZE];		/* ���ͻ����� */
	static uint8_t g_RxBuf1[UART1_RX_BUF_SIZE];		/* ���ջ����� */
#endif

#if UART2_FIFO_EN == 1
	static UART_T g_tUart2;
	static uint8_t g_TxBuf2[UART2_TX_BUF_SIZE];		/* ���ͻ����� */
	static uint8_t g_RxBuf2[UART2_RX_BUF_SIZE];		/* ���ջ����� */
#endif

#if UART3_FIFO_EN == 1
	static UART_T g_tUart3;
	static uint8_t g_TxBuf3[UART3_TX_BUF_SIZE];		/* ���ͻ����� */
	static uint8_t g_RxBuf3[UART3_RX_BUF_SIZE];		/* ���ջ����� */
#endif

#if UART4_FIFO_EN == 1
	static UART_T g_tUart4;
	static uint8_t g_TxBuf4[UART4_TX_BUF_SIZE];		/* ���ͻ����� */
	static uint8_t g_RxBuf4[UART4_RX_BUF_SIZE];		/* ���ջ����� */
#endif

#if UART5_FIFO_EN == 1
	static UART_T g_tUart5;
	static uint8_t g_TxBuf5[UART5_TX_BUF_SIZE];		/* ���ͻ����� */
	static uint8_t g_RxBuf5[UART5_RX_BUF_SIZE];		/* ���ջ����� */
#endif

#if UART6_FIFO_EN == 1
	static UART_T g_tUart6;
	static uint8_t g_TxBuf6[UART6_TX_BUF_SIZE];		/* ���ͻ����� */
	static uint8_t g_RxBuf6[UART6_RX_BUF_SIZE];		/* ���ջ����� */
#endif

#if UART7_FIFO_EN == 1
	static UART_T g_tUart7;
	static uint8_t g_TxBuf7[UART7_TX_BUF_SIZE];		/* ���ͻ����� */
	static uint8_t g_RxBuf7[UART7_RX_BUF_SIZE];		/* ���ջ����� */
#endif

#if UART8_FIFO_EN == 1
	static UART_T g_tUart8;
	static uint8_t g_TxBuf8[UART8_TX_BUF_SIZE];		/* ���ͻ����� */
	static uint8_t g_RxBuf8[UART8_RX_BUF_SIZE];		/* ���ջ����� */
#endif
		
/*
*********************************************************************************************************
*   函 数 名: UartVarInit
*   功能说明: UartVarInit 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void UartVarInit(void);

/*
*********************************************************************************************************
*   函 数 名: InitHardUart
*   功能说明: InitHardUart 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void InitHardUart(void);
/*
*********************************************************************************************************
*   函 数 名: UartSend
*   功能说明: UartSend 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void UartSend(UART_T *_pUart, uint8_t *_ucaBuf, uint16_t _usLen);
/*
*********************************************************************************************************
*   函 数 名: UartGetChar
*   功能说明: UartGetChar 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t UartGetChar(UART_T *_pUart, uint8_t *_pByte);
/*
*********************************************************************************************************
*   函 数 名: UartIRQ
*   功能说明: UartIRQ 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void UartIRQ(UART_T *_pUart);
/*
*********************************************************************************************************
*   函 数 名: UartSendCharPolled
*   功能说明: UartSendCharPolled 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void UartSendCharPolled(USART_TypeDef *uart, uint8_t ch);

static uint8_t s_uart_ready;

/*
*********************************************************************************************************
*   函 数 名: RS485_InitTXE
*   功能说明: RS485_InitTXE 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void RS485_InitTXE(void);

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitUart
*	����˵��: ��ʼ������Ӳ��������ȫ�ֱ�������ֵ.
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitUart(void)
{
	
	s_uart_ready = 0;
	UartVarInit();		/* �����ȳ�ʼ��ȫ�ֱ���,������Ӳ�� */

	InitHardUart();		/* ���ô��ڵ�Ӳ������(�����ʵ�) */
	s_uart_ready = 1;
#if UART3_FIFO_EN == 1
	RS485_InitTXE();	/* ����RS485оƬ�ķ���ʹ��Ӳ��������Ϊ������� */
#endif
}

/*
*********************************************************************************************************
*	�� �� ��: ComToUart
*	����˵��: ��COM�˿ں�ת��ΪUARTָ��
*	��    ��: _ucPort: �˿ں�(COM1 - COM8)
*	�� �� ֵ: uartָ��
*********************************************************************************************************
*/
UART_T *ComToUart(COM_PORT_E _ucPort)
{
	if (_ucPort == COM1)
	{
		#if UART1_FIFO_EN == 1
			return &g_tUart1;
		#else
			return 0;
		#endif
	}
	else if (_ucPort == COM2)
	{
		#if UART2_FIFO_EN == 1
			return &g_tUart2;
		#else
			return 0;
		#endif
	}
	else if (_ucPort == COM3)
	{
		#if UART3_FIFO_EN == 1
			return &g_tUart3;
		#else
			return 0;
		#endif
	}
	else if (_ucPort == COM4)
	{
		#if UART4_FIFO_EN == 1
			return &g_tUart4;
		#else
			return 0;
		#endif
	}
	else if (_ucPort == COM5)
	{
		#if UART5_FIFO_EN == 1
			return &g_tUart5;
		#else
			return 0;
		#endif
	}
	else if (_ucPort == COM6)
	{
		#if UART6_FIFO_EN == 1
			return &g_tUart6;
		#else
			return 0;
		#endif
	}
	else if (_ucPort == COM7)
	{
		#if UART7_FIFO_EN == 1
			return &g_tUart7;
		#else
			return 0;
		#endif
	}
	else if (_ucPort == COM8)
	{
		#if UART8_FIFO_EN == 1
			return &g_tUart8;
		#else
			return 0;
		#endif
	}	
	else
	{
		Error_Handler(__FILE__, __LINE__);
		return 0;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: ComToUart
*	����˵��: ��COM�˿ں�ת��Ϊ USART_TypeDef* USARTx
*	��    ��: _ucPort: �˿ں�(COM1 - COM8)
*	�� �� ֵ: USART_TypeDef*,  USART1, USART2, USART3, UART4, UART5��USART6��UART7��UART8��
*********************************************************************************************************
*/
USART_TypeDef *ComToUSARTx(COM_PORT_E _ucPort)
{
	if (_ucPort == COM1)
	{
		#if UART1_FIFO_EN == 1
			return USART1;
		#else
			return 0;
		#endif
	}
	else if (_ucPort == COM2)
	{
		#if UART2_FIFO_EN == 1
			return USART2;
		#else
			return 0;
		#endif
	}
	else if (_ucPort == COM3)
	{
		#if UART3_FIFO_EN == 1
			return USART3;
		#else
			return 0;
		#endif
	}
	else if (_ucPort == COM4)
	{
		#if UART4_FIFO_EN == 1
			return UART4;
		#else
			return 0;
		#endif
	}
	else if (_ucPort == COM5)
	{
		#if UART5_FIFO_EN == 1
			return UART5;
		#else
			return 0;
		#endif
	}
	else if (_ucPort == COM6)
	{
		#if UART6_FIFO_EN == 1
			return USART6;
		#else
			return 0;
		#endif
	}
	else if (_ucPort == COM7)
	{
		#if UART7_FIFO_EN == 1
			return UART7;
		#else
			return 0;
		#endif
	}
	else if (_ucPort == COM8)
	{
		#if UART8_FIFO_EN == 1
			return UART8;
		#else
			return 0;
		#endif
	}	
	
	else
	{
		/* �����κδ��� */
		return 0;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: comSendBuf
*	����˵��: �򴮿ڷ���һ�����ݡ����ݷŵ����ͻ��������������أ����жϷ�������ں�̨��ɷ���
*	��    ��: _ucPort: �˿ں�(COM1 - COM8)
*			  _ucaBuf: �����͵����ݻ�����
*			  _usLen : ���ݳ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void comSendBuf(COM_PORT_E _ucPort, uint8_t *_ucaBuf, uint16_t _usLen)
{
	UART_T *pUart;

	pUart = ComToUart(_ucPort);
	if (pUart == 0)
	{
		return;
	}

	if (pUart->SendBefor != 0)
	{
		pUart->SendBefor();		/* �����RS485ͨ�ţ���������������н�RS485����Ϊ����ģʽ */
	}

	UartSend(pUart, _ucaBuf, _usLen);
}

/*
*********************************************************************************************************
*	�� �� ��: comSendChar
*	����˵��: �򴮿ڷ���1���ֽڡ����ݷŵ����ͻ��������������أ����жϷ�������ں�̨��ɷ���
*	��    ��: _ucPort: �˿ں�(COM1 - COM8)
*			  _ucByte: �����͵�����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void comSendChar(COM_PORT_E _ucPort, uint8_t _ucByte)
{
	comSendBuf(_ucPort, &_ucByte, 1);
}

/*
*********************************************************************************************************
*	�� �� ��: comGetChar
*	����˵��: �ӽ��ջ�������ȡ1�ֽڣ��������������������ݾ��������ء�
*	��    ��: _ucPort: �˿ں�(COM1 - COM8)
*			  _pByte: ���յ������ݴ���������ַ
*	�� �� ֵ: 0 ��ʾ������, 1 ��ʾ��ȡ����Ч�ֽ�
*********************************************************************************************************
*/
uint8_t comGetChar(COM_PORT_E _ucPort, uint8_t *_pByte)
{
	UART_T *pUart;

	pUart = ComToUart(_ucPort);
	if (pUart == 0)
	{
		return 0;
	}

	return UartGetChar(pUart, _pByte);
}

/*
*********************************************************************************************************
*	�� �� ��: comClearTxFifo
*	����˵��: ���㴮�ڷ��ͻ�����
*	��    ��: _ucPort: �˿ں�(COM1 - COM8)
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void comClearTxFifo(COM_PORT_E _ucPort)
{
	UART_T *pUart;

	pUart = ComToUart(_ucPort);
	if (pUart == 0)
	{
		return;
	}

	pUart->usTxWrite = 0;
	pUart->usTxRead = 0;
	pUart->usTxCount = 0;
}

/*
*********************************************************************************************************
*	�� �� ��: comClearRxFifo
*	����˵��: ���㴮�ڽ��ջ�����
*	��    ��: _ucPort: �˿ں�(COM1 - COM8)
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void comClearRxFifo(COM_PORT_E _ucPort)
{
	UART_T *pUart;

	pUart = ComToUart(_ucPort);
	if (pUart == 0)
	{
		return;
	}

	pUart->usRxWrite = 0;
	pUart->usRxRead = 0;
	pUart->usRxCount = 0;
}

/*
*********************************************************************************************************
*	�� �� ��: comSetBaud
*	����˵��: ���ô��ڵĲ�����. �������̶�����Ϊ��У�飬�շ���ʹ��ģʽ
*	��    ��: _ucPort: �˿ں�(COM1 - COM8)
*			  _BaudRate: �����ʣ�8��������  ������.0-12.5Mbps
*                                16�������� ������.0-6.25Mbps
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void comSetBaud(COM_PORT_E _ucPort, uint32_t _BaudRate)
{
	USART_TypeDef* USARTx;
	
	USARTx = ComToUSARTx(_ucPort);
	if (USARTx == 0)
	{
		return;
	}
	
	bsp_SetUartParam(USARTx,  _BaudRate, UART_PARITY_NONE, UART_MODE_TX_RX);
}

/* �����RS485ͨ�ţ��밴���¸�ʽ��д������ ���ǽ����� USART3��ΪRS485������ */

/*
*********************************************************************************************************
*	�� �� ��: RS485_InitTXE
*	����˵��: ����RS485����ʹ�ܿ��� TXE
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RS485_InitTXE(void)
{
	GPIO_InitTypeDef gpio_init;
	
	/* ��GPIOʱ�� */
	RS485_TXEN_GPIO_CLK_ENABLE();
	
	/* ��������Ϊ������� */
	gpio_init.Mode = GPIO_MODE_OUTPUT_PP;			/* ������� */
	gpio_init.Pull = GPIO_NOPULL;					/* ���������費ʹ�� */
	gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;	/* GPIO�ٶȵȼ� */
	gpio_init.Pin = RS485_TXEN_PIN;
	HAL_GPIO_Init(RS485_TXEN_GPIO_PORT, &gpio_init);	
}

/*
*********************************************************************************************************
*	�� �� ��: RS485_SetBaud
*	����˵��: �޸�485���ڵĲ����ʡ�
*	��    ��: _baud : 8��������  ������.0-12.5Mbps
*                     16�������� ������.0-6.25Mbps
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RS485_SetBaud(uint32_t _baud)
{
	comSetBaud(COM3, _baud);
}

/*
*********************************************************************************************************
*	�� �� ��: RS485_SendBefor
*	����˵��: ��������ǰ��׼������������RS485ͨ�ţ�������RS485оƬΪ����״̬��
*			  ���޸� UartVarInit()�еĺ���ָ����ڱ������������� g_tUart2.SendBefor = RS485_SendBefor
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RS485_SendBefor(void)
{
	RS485_TX_EN();	/* �л�RS485�շ�оƬΪ����ģʽ */
}

/*
*********************************************************************************************************
*	�� �� ��: RS485_SendOver
*	����˵��: ����һ�����ݽ�������ƺ���������RS485ͨ�ţ�������RS485оƬΪ����״̬��
*			  ���޸� UartVarInit()�еĺ���ָ����ڱ������������� g_tUart2.SendOver = RS485_SendOver
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RS485_SendOver(void)
{
	RS485_RX_EN();	/* �л�RS485�շ�оƬΪ����ģʽ */
}

/*
*********************************************************************************************************
*	�� �� ��: RS485_SendBuf
*	����˵��: ͨ��RS485оƬ����һ�����ݡ�ע�⣬���������ȴ�������ϡ�
*	��    ��: _ucaBuf : ���ݻ�����
*			  _usLen : ���ݳ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RS485_SendBuf(uint8_t *_ucaBuf, uint16_t _usLen)
{
	comSendBuf(COM3, _ucaBuf, _usLen);
}

/*
*********************************************************************************************************
*	�� �� ��: RS485_SendStr
*	����˵��: ��485���߷���һ���ַ�����0������
*	��    ��: _pBuf �ַ�����0����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RS485_SendStr(char *_pBuf)
{
	RS485_SendBuf((uint8_t *)_pBuf, strlen(_pBuf));
}

/*
*********************************************************************************************************
*	�� �� ��: RS485_ReciveNew
*	����˵��: ���յ��µ�����
*	��    ��: _byte ���յ���������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
//extern void MODH_ReciveNew(uint8_t _byte);
void RS485_ReciveNew(uint8_t _byte)
{
//	MODH_ReciveNew(_byte);
}

/*
*********************************************************************************************************
*	�� �� ��: UartVarInit
*	����˵��: ��ʼ��������صı���
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void UartVarInit(void)
{
#if UART1_FIFO_EN == 1
	g_tUart1.uart = USART1;						/* STM32 �����豸 */
	g_tUart1.pTxBuf = g_TxBuf1;					/* ���ͻ�����ָ�� */
	g_tUart1.pRxBuf = g_RxBuf1;					/* ���ջ�����ָ�� */
	g_tUart1.usTxBufSize = UART1_TX_BUF_SIZE;	/* ���ͻ�������С */
	g_tUart1.usRxBufSize = UART1_RX_BUF_SIZE;	/* ���ջ�������С */
	g_tUart1.usTxWrite = 0;						/* ����FIFOд���� */
	g_tUart1.usTxRead = 0;						/* ����FIFO������ */
	g_tUart1.usRxWrite = 0;						/* ����FIFOд���� */
	g_tUart1.usRxRead = 0;						/* ����FIFO������ */
	g_tUart1.usRxCount = 0;						/* ���յ��������ݸ��� */
	g_tUart1.usTxCount = 0;						/* �����͵����ݸ��� */
	g_tUart1.SendBefor = 0;						/* ��������ǰ�Ļص����� */
	g_tUart1.SendOver = 0;						/* ������Ϻ�Ļص����� */
	g_tUart1.ReciveNew = 0;						/* ���յ������ݺ�Ļص����� */
	g_tUart1.Sending = 0;						/* ���ڷ����б�־ */
#endif

#if UART2_FIFO_EN == 1
	g_tUart2.uart = USART2;						/* STM32 �����豸 */
	g_tUart2.pTxBuf = g_TxBuf2;					/* ���ͻ�����ָ�� */
	g_tUart2.pRxBuf = g_RxBuf2;					/* ���ջ�����ָ�� */
	g_tUart2.usTxBufSize = UART2_TX_BUF_SIZE;	/* ���ͻ�������С */
	g_tUart2.usRxBufSize = UART2_RX_BUF_SIZE;	/* ���ջ�������С */
	g_tUart2.usTxWrite = 0;						/* ����FIFOд���� */
	g_tUart2.usTxRead = 0;						/* ����FIFO������ */
	g_tUart2.usRxWrite = 0;						/* ����FIFOд���� */
	g_tUart2.usRxRead = 0;						/* ����FIFO������ */
	g_tUart2.usRxCount = 0;						/* ���յ��������ݸ��� */
	g_tUart2.usTxCount = 0;						/* �����͵����ݸ��� */
	g_tUart2.SendBefor = 0;						/* ��������ǰ�Ļص����� */
	g_tUart2.SendOver = 0;						/* ������Ϻ�Ļص����� */
	g_tUart2.ReciveNew = 0;						/* ���յ������ݺ�Ļص����� */
	g_tUart2.Sending = 0;						/* ���ڷ����б�־ */
#endif

#if UART3_FIFO_EN == 1
	g_tUart3.uart = USART3;						/* STM32 �����豸 */
	g_tUart3.pTxBuf = g_TxBuf3;					/* ���ͻ�����ָ�� */
	g_tUart3.pRxBuf = g_RxBuf3;					/* ���ջ�����ָ�� */
	g_tUart3.usTxBufSize = UART3_TX_BUF_SIZE;	/* ���ͻ�������С */
	g_tUart3.usRxBufSize = UART3_RX_BUF_SIZE;	/* ���ջ�������С */
	g_tUart3.usTxWrite = 0;						/* ����FIFOд���� */
	g_tUart3.usTxRead = 0;						/* ����FIFO������ */
	g_tUart3.usRxWrite = 0;						/* ����FIFOд���� */
	g_tUart3.usRxRead = 0;						/* ����FIFO������ */
	g_tUart3.usRxCount = 0;						/* ���յ��������ݸ��� */
	g_tUart3.usTxCount = 0;						/* �����͵����ݸ��� */
	g_tUart3.SendBefor = RS485_SendBefor;		/* ��������ǰ�Ļص����� */
	g_tUart3.SendOver = RS485_SendOver;			/* ������Ϻ�Ļص����� */
	g_tUart3.ReciveNew = RS485_ReciveNew;		/* ���յ������ݺ�Ļص����� */
	g_tUart3.Sending = 0;						/* ���ڷ����б�־ */
#endif

#if UART4_FIFO_EN == 1
	g_tUart4.uart = UART4;						/* STM32 �����豸 */
	g_tUart4.pTxBuf = g_TxBuf4;					/* ���ͻ�����ָ�� */
	g_tUart4.pRxBuf = g_RxBuf4;					/* ���ջ�����ָ�� */
	g_tUart4.usTxBufSize = UART4_TX_BUF_SIZE;	/* ���ͻ�������С */
	g_tUart4.usRxBufSize = UART4_RX_BUF_SIZE;	/* ���ջ�������С */
	g_tUart4.usTxWrite = 0;						/* ����FIFOд���� */
	g_tUart4.usTxRead = 0;						/* ����FIFO������ */
	g_tUart4.usRxWrite = 0;						/* ����FIFOд���� */
	g_tUart4.usRxRead = 0;						/* ����FIFO������ */
	g_tUart4.usRxCount = 0;						/* ���յ��������ݸ��� */
	g_tUart4.usTxCount = 0;						/* �����͵����ݸ��� */
	g_tUart4.SendBefor = 0;						/* ��������ǰ�Ļص����� */
	g_tUart4.SendOver = 0;						/* ������Ϻ�Ļص����� */
	g_tUart4.ReciveNew = 0;						/* ���յ������ݺ�Ļص����� */
	g_tUart4.Sending = 0;						/* ���ڷ����б�־ */
#endif

#if UART5_FIFO_EN == 1
	g_tUart5.uart = UART5;						/* STM32 �����豸 */
	g_tUart5.pTxBuf = g_TxBuf5;					/* ���ͻ�����ָ�� */
	g_tUart5.pRxBuf = g_RxBuf5;					/* ���ջ�����ָ�� */
	g_tUart5.usTxBufSize = UART5_TX_BUF_SIZE;	/* ���ͻ�������С */
	g_tUart5.usRxBufSize = UART5_RX_BUF_SIZE;	/* ���ջ�������С */
	g_tUart5.usTxWrite = 0;						/* ����FIFOд���� */
	g_tUart5.usTxRead = 0;						/* ����FIFO������ */
	g_tUart5.usRxWrite = 0;						/* ����FIFOд���� */
	g_tUart5.usRxRead = 0;						/* ����FIFO������ */
	g_tUart5.usRxCount = 0;						/* ���յ��������ݸ��� */
	g_tUart5.usTxCount = 0;						/* �����͵����ݸ��� */
	g_tUart5.SendBefor = 0;						/* ��������ǰ�Ļص����� */
	g_tUart5.SendOver = 0;						/* ������Ϻ�Ļص����� */
	g_tUart5.ReciveNew = 0;						/* ���յ������ݺ�Ļص����� */
	g_tUart5.Sending = 0;						/* ���ڷ����б�־ */
#endif


#if UART6_FIFO_EN == 1
	g_tUart6.uart = USART6;						/* STM32 �����豸 */
	g_tUart6.pTxBuf = g_TxBuf6;					/* ���ͻ�����ָ�� */
	g_tUart6.pRxBuf = g_RxBuf6;					/* ���ջ�����ָ�� */
	g_tUart6.usTxBufSize = UART6_TX_BUF_SIZE;	/* ���ͻ�������С */
	g_tUart6.usRxBufSize = UART6_RX_BUF_SIZE;	/* ���ջ�������С */
	g_tUart6.usTxWrite = 0;						/* ����FIFOд���� */
	g_tUart6.usTxRead = 0;						/* ����FIFO������ */
	g_tUart6.usRxWrite = 0;						/* ����FIFOд���� */
	g_tUart6.usRxRead = 0;						/* ����FIFO������ */
	g_tUart6.usRxCount = 0;						/* ���յ��������ݸ��� */
	g_tUart6.usTxCount = 0;						/* �����͵����ݸ��� */
	g_tUart6.SendBefor = 0;						/* ��������ǰ�Ļص����� */
	g_tUart6.SendOver = 0;						/* ������Ϻ�Ļص����� */
	g_tUart6.ReciveNew = 0;						/* ���յ������ݺ�Ļص����� */
	g_tUart6.Sending = 0;						/* ���ڷ����б�־ */
#endif

#if UART7_FIFO_EN == 1
	g_tUart7.uart = UART7;						/* STM32 �����豸 */
	g_tUart7.pTxBuf = g_TxBuf7;					/* ���ͻ�����ָ�� */
	g_tUart7.pRxBuf = g_RxBuf7;					/* ���ջ�����ָ�� */
	g_tUart7.usTxBufSize = UART7_TX_BUF_SIZE;	/* ���ͻ�������С */
	g_tUart7.usRxBufSize = UART7_RX_BUF_SIZE;	/* ���ջ�������С */
	g_tUart7.usTxWrite = 0;						/* ����FIFOд���� */
	g_tUart7.usTxRead = 0;						/* ����FIFO������ */
	g_tUart7.usRxWrite = 0;						/* ����FIFOд���� */
	g_tUart7.usRxRead = 0;						/* ����FIFO������ */
	g_tUart7.usRxCount = 0;						/* ���յ��������ݸ��� */
	g_tUart7.usTxCount = 0;						/* �����͵����ݸ��� */
	g_tUart7.SendBefor = 0;						/* ��������ǰ�Ļص����� */
	g_tUart7.SendOver = 0;						/* ������Ϻ�Ļص����� */
	g_tUart7.ReciveNew = 0;						/* ���յ������ݺ�Ļص����� */
	g_tUart7.Sending = 0;						/* ���ڷ����б�־ */
#endif

#if UART8_FIFO_EN == 1
	g_tUart8.uart = UART8;						/* STM32 �����豸 */
	g_tUart8.pTxBuf = g_TxBuf8;					/* ���ͻ�����ָ�� */
	g_tUart8.pRxBuf = g_RxBuf8;					/* ���ջ�����ָ�� */
	g_tUart8.usTxBufSize = UART8_TX_BUF_SIZE;	/* ���ͻ�������С */
	g_tUart8.usRxBufSize = UART8_RX_BUF_SIZE;	/* ���ջ�������С */
	g_tUart8.usTxWrite = 0;						/* ����FIFOд���� */
	g_tUart8.usTxRead = 0;						/* ����FIFO������ */
	g_tUart8.usRxWrite = 0;						/* ����FIFOд���� */
	g_tUart8.usRxRead = 0;						/* ����FIFO������ */
	g_tUart8.usRxCount = 0;						/* ���յ��������ݸ��� */
	g_tUart8.usTxCount = 0;						/* �����͵����ݸ��� */
	g_tUart8.SendBefor = 0;						/* ��������ǰ�Ļص����� */
	g_tUart8.SendOver = 0;						/* ������Ϻ�Ļص����� */
	g_tUart8.ReciveNew = 0;						/* ���յ������ݺ�Ļص����� */
	g_tUart8.Sending = 0;						/* ���ڷ����б�־ */
#endif
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_SetUartParam
*	����˵��: ���ô��ڵ�Ӳ�������������ʣ�����λ��ֹͣλ����ʼλ��У��λ���ж�ʹ�ܣ��ʺ���STM32- H7������
*	��    ��: Instance   USART_TypeDef���ͽṹ��
*             BaudRate   ������
*             Parity     У�����ͣ���У�����żУ��
*             Mode       ���ͺͽ���ģʽʹ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_SetUartParam(USART_TypeDef *Instance,  uint32_t BaudRate, uint32_t Parity, uint32_t Mode)
{
	UART_HandleTypeDef UartHandle;	
	
	/*##-1- ���ô���Ӳ������ ######################################*/
	/* �첽����ģʽ (UART Mode) */
	/* ��������:
	  - �ֳ�    = 8 λ
	  - ֹͣλ  = 1 ��ֹͣλ
	  - У��    = ����Parity
	  - ������  = ����BaudRate
	  - Ӳ�������ƹر� (RTS and CTS signals) */

	UartHandle.Instance        = Instance;

	UartHandle.Init.BaudRate   = BaudRate;
	UartHandle.Init.WordLength = UART_WORDLENGTH_8B;
	UartHandle.Init.StopBits   = UART_STOPBITS_1;
	UartHandle.Init.Parity     = Parity;
	UartHandle.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
	UartHandle.Init.Mode       = Mode;
	UartHandle.Init.OverSampling = UART_OVERSAMPLING_16;
	UartHandle.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	UartHandle.Init.Prescaler = UART_PRESCALER_DIV1;
	UartHandle.Init.FIFOMode = UART_FIFOMODE_DISABLE;
	UartHandle.Init.TXFIFOThreshold = UART_TXFIFO_THRESHOLD_1_8;
	UartHandle.Init.RXFIFOThreshold = UART_RXFIFO_THRESHOLD_1_8;
	UartHandle.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    
	if (HAL_UART_Init(&UartHandle) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: InitHardUart
*	����˵��: ���ô��ڵ�Ӳ�������������ʣ�����λ��ֹͣλ����ʼλ��У��λ���ж�ʹ�ܣ��ʺ���STM32-H7������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void InitHardUart(void)
{
	GPIO_InitTypeDef  GPIO_InitStruct;
	RCC_PeriphCLKInitTypeDef RCC_PeriphClkInit;
	
	/* 
       ����������ÿ���ע�͵���Ԥ��������Ϊ�˷����Ժ�ѡ������ʱ��ʹ�� 
       Ĭ������£�USART1��USART6ѡ���PCLK2��ʱ��100MHz��
       USART2��USART3��UART4��UART5��UART6��UART7��UART8ѡ���ʱ����PLCK1��ʱ��100MHz��
    */
	RCC_PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART16;
	RCC_PeriphClkInit.Usart16ClockSelection = RCC_USART16CLKSOURCE_D2PCLK2;
	HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphClkInit);	

#if UART1_FIFO_EN == 1		/* ����1 */
	/* ʹ�� GPIO TX/RX ʱ�� */
	USART1_TX_GPIO_CLK_ENABLE();
	USART1_RX_GPIO_CLK_ENABLE();
	
	/* ʹ�� USARTx ʱ�� */
	USART1_CLK_ENABLE();	

	/* ����TX���� */
	GPIO_InitStruct.Pin       = USART1_TX_PIN;
	GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull      = GPIO_PULLUP;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = USART1_TX_AF;
	HAL_GPIO_Init(USART1_TX_GPIO_PORT, &GPIO_InitStruct);	
	
	/* ����RX���� */
	GPIO_InitStruct.Pin = USART1_RX_PIN;
	GPIO_InitStruct.Alternate = USART1_RX_AF;
	HAL_GPIO_Init(USART1_RX_GPIO_PORT, &GPIO_InitStruct);

	/* ����NVIC the NVIC for UART */   
	HAL_NVIC_SetPriority(USART1_IRQn, 0, 1);
	HAL_NVIC_EnableIRQ(USART1_IRQn);
  
	/* ���ò����ʡ���żУ�� */
	bsp_SetUartParam(USART1,  UART1_BAUD, UART_PARITY_NONE, UART_MODE_TX_RX);

	SET_BIT(USART1->ICR, USART_ICR_TCCF);	/* ���TC������ɱ�־ */
	SET_BIT(USART1->RQR, USART_RQR_RXFRQ);  /* ���RXNE���ձ�־ */
	// USART_CR1_PEIE | USART_CR1_RXNEIE
	SET_BIT(USART1->CR1, USART_CR1_RXNEIE);	/* ʹ��PE. RX�����ж� */
#endif

#if UART2_FIFO_EN == 1		/* ����2 */
	/* ʹ�� GPIO TX/RX ʱ�� */
	USART2_TX_GPIO_CLK_ENABLE();
	USART2_RX_GPIO_CLK_ENABLE();
	
	/* ʹ�� USARTx ʱ�� */
	USART2_CLK_ENABLE();	

	/* ����TX���� */
	GPIO_InitStruct.Pin       = USART2_TX_PIN;
	GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull      = GPIO_PULLUP;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = USART2_TX_AF;
	HAL_GPIO_Init(USART2_TX_GPIO_PORT, &GPIO_InitStruct);	
	
	/* ����RX���� */
	GPIO_InitStruct.Pin = USART2_RX_PIN;
	GPIO_InitStruct.Alternate = USART2_RX_AF;
	HAL_GPIO_Init(USART2_RX_GPIO_PORT, &GPIO_InitStruct);

	/* ����NVIC the NVIC for UART */   
	HAL_NVIC_SetPriority(USART2_IRQn, 0, 2);
	HAL_NVIC_EnableIRQ(USART2_IRQn);
  
	/* ���ò����ʡ���żУ�� */
	bsp_SetUartParam(USART2,  UART2_BAUD, UART_PARITY_NONE, UART_MODE_RX);	// UART_MODE_TX_RX

	SET_BIT(USART2->ICR, USART_ICR_TCCF);	/* ���TC������ɱ�־ */
	SET_BIT(USART2->RQR, USART_RQR_RXFRQ);/* ���RXNE���ձ�־ */
	SET_BIT(USART2->CR1, USART_CR1_RXNEIE);	/* ʹ��PE. RX�����ж� */
#endif

#if UART3_FIFO_EN == 1			/* ����3 */
	/* ʹ�� GPIO TX/RX ʱ�� */
	USART3_TX_GPIO_CLK_ENABLE();
	USART3_RX_GPIO_CLK_ENABLE();
	
	/* ʹ�� USARTx ʱ�� */
	USART3_CLK_ENABLE();	

	/* ����TX���� */
	GPIO_InitStruct.Pin       = USART3_TX_PIN;
	GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull      = GPIO_PULLUP;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = USART3_TX_AF;
	HAL_GPIO_Init(USART3_TX_GPIO_PORT, &GPIO_InitStruct);	
	
	/* ����RX���� */
	GPIO_InitStruct.Pin = USART3_RX_PIN;
	GPIO_InitStruct.Alternate = USART3_RX_AF;
	HAL_GPIO_Init(USART3_RX_GPIO_PORT, &GPIO_InitStruct);

	/* ����NVIC the NVIC for UART */   
	HAL_NVIC_SetPriority(USART3_IRQn, 0, 3);
	HAL_NVIC_EnableIRQ(USART3_IRQn);
  
	/* ���ò����ʡ���żУ�� */
	bsp_SetUartParam(USART3,  UART3_BAUD, UART_PARITY_NONE, UART_MODE_TX_RX);

	SET_BIT(USART3->ICR, USART_ICR_TCCF);	/* ���TC������ɱ�־ */
	SET_BIT(USART3->RQR, USART_RQR_RXFRQ);/* ���RXNE���ձ�־ */
	SET_BIT(USART3->CR1, USART_CR1_RXNEIE);	/* ʹ��PE. RX�����ж� */
#endif

#if UART4_FIFO_EN == 1			/* ����4 TX = PC10   RX = PC11 */
	/* ʹ�� GPIO TX/RX ʱ�� */
	UART4_TX_GPIO_CLK_ENABLE();
	UART4_RX_GPIO_CLK_ENABLE();
	
	/* ʹ�� USARTx ʱ�� */
	UART4_CLK_ENABLE();	

	/* ����TX���� */
	GPIO_InitStruct.Pin       = UART4_TX_PIN;
	GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull      = GPIO_PULLUP;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = UART4_TX_AF;
	HAL_GPIO_Init(UART4_TX_GPIO_PORT, &GPIO_InitStruct);	
	
	/* ����RX���� */
	GPIO_InitStruct.Pin = UART4_RX_PIN;
	GPIO_InitStruct.Alternate = UART4_RX_AF;
	HAL_GPIO_Init(UART4_RX_GPIO_PORT, &GPIO_InitStruct);

	/* ����NVIC the NVIC for UART */   
	HAL_NVIC_SetPriority(UART4_IRQn, 0, 4);
	HAL_NVIC_EnableIRQ(UART4_IRQn);
  
	/* ���ò����ʡ���żУ�� */
	bsp_SetUartParam(UART4,  UART4_BAUD, UART_PARITY_NONE, UART_MODE_TX_RX);

	SET_BIT(UART4->ICR, USART_ICR_TCCF);	/* ���TC������ɱ�־ */
	SET_BIT(UART4->RQR, USART_RQR_RXFRQ);/* ���RXNE���ձ�־ */
	SET_BIT(UART4->CR1, USART_CR1_RXNEIE);	/* ʹ��RX�����ж� */
#endif

#if UART5_FIFO_EN == 1			/* ����5 TX = PC12   RX = PD2 */
	/* ʹ�� GPIO TX/RX ʱ�� */
	UART5_TX_GPIO_CLK_ENABLE();
	UART5_RX_GPIO_CLK_ENABLE();
	
	/* ʹ�� USARTx ʱ�� */
	UART5_CLK_ENABLE();	

	/* ����TX���� */
	GPIO_InitStruct.Pin       = UART5_TX_PIN;
	GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull      = GPIO_PULLUP;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = UART5_TX_AF;
	HAL_GPIO_Init(UART5_TX_GPIO_PORT, &GPIO_InitStruct);	
	
	/* ����RX���� */
	GPIO_InitStruct.Pin = UART5_RX_PIN;
	GPIO_InitStruct.Alternate = UART5_RX_AF;
	HAL_GPIO_Init(UART5_RX_GPIO_PORT, &GPIO_InitStruct);

	/* ����NVIC the NVIC for UART */   
	HAL_NVIC_SetPriority(UART5_IRQn, 0, 5);
	HAL_NVIC_EnableIRQ(UART5_IRQn);
  
	/* ���ò����ʡ���żУ�� */
	bsp_SetUartParam(UART5,  UART5_BAUD, UART_PARITY_NONE, UART_MODE_TX_RX);

	SET_BIT(UART5->ICR, USART_ICR_TCCF);	/* ���TC������ɱ�־ */
	SET_BIT(UART5->RQR, USART_RQR_RXFRQ);/* ���RXNE���ձ�־ */
	SET_BIT(UART5->CR1, USART_CR1_RXNEIE);	/* ʹ��RX�����ж� */
#endif

#if UART6_FIFO_EN == 1			/* USART6 */
	/* ʹ�� GPIO TX/RX ʱ�� */
	USART6_TX_GPIO_CLK_ENABLE();
	USART6_RX_GPIO_CLK_ENABLE();
	
	/* ʹ�� USARTx ʱ�� */
	USART6_CLK_ENABLE();	

	/* ����TX���� */
	GPIO_InitStruct.Pin       = USART6_TX_PIN;
	GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull      = GPIO_PULLUP;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = USART6_TX_AF;
	HAL_GPIO_Init(USART6_TX_GPIO_PORT, &GPIO_InitStruct);	
	
	/* ����RX���� */
	GPIO_InitStruct.Pin = USART6_RX_PIN;
	GPIO_InitStruct.Alternate = USART6_RX_AF;
	HAL_GPIO_Init(USART6_RX_GPIO_PORT, &GPIO_InitStruct);

	/* ����NVIC the NVIC for UART */   
	HAL_NVIC_SetPriority(USART6_IRQn, 0, 6);
	HAL_NVIC_EnableIRQ(USART6_IRQn);
	
	/* ���ò����ʡ���żУ�� */
	bsp_SetUartParam(USART6,  UART6_BAUD, UART_PARITY_NONE, UART_MODE_TX_RX);

	SET_BIT(USART6->ICR, USART_ICR_TCCF);	/* ���TC������ɱ�־ */
	SET_BIT(USART6->RQR, USART_RQR_RXFRQ);/* ���RXNE���ձ�־ */
	SET_BIT(USART6->CR1, USART_CR1_RXNEIE);	/* ʹ��PE. RX�����ж� */
#endif

#if UART7_FIFO_EN == 1			/* UART7 */
	/* ʹ�� GPIO TX/RX ʱ�� */
	UART7_TX_GPIO_CLK_ENABLE();
	UART7_RX_GPIO_CLK_ENABLE();
	
	/* ʹ�� USARTx ʱ�� */
	UART7_CLK_ENABLE();	

	/* ����TX���� */
	GPIO_InitStruct.Pin       = UART7_TX_PIN;
	GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull      = GPIO_PULLUP;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = UART7_TX_AF;
	HAL_GPIO_Init(UART7_TX_GPIO_PORT, &GPIO_InitStruct);	
	
	/* ����RX���� */
	GPIO_InitStruct.Pin = UART7_RX_PIN;
	GPIO_InitStruct.Alternate = UART7_RX_AF;
	HAL_GPIO_Init(UART7_RX_GPIO_PORT, &GPIO_InitStruct);

	/* ����NVIC the NVIC for UART */   
	HAL_NVIC_SetPriority(UART7_IRQn, 0, 6);
	HAL_NVIC_EnableIRQ(UART7_IRQn);
	
	/* ���ò����ʡ���żУ�� */
	bsp_SetUartParam(UART7,  UART7_BAUD, UART_PARITY_NONE, UART_MODE_TX_RX);

	SET_BIT(UART7->ICR, USART_ICR_TCCF);	/* ���TC������ɱ�־ */
	SET_BIT(UART7->RQR, USART_RQR_RXFRQ);	/* ���RXNE���ձ�־ */
	SET_BIT(UART7->CR1, USART_CR1_RXNEIE);	/* ʹ��PE. RX�����ж� */
#endif

#if UART8_FIFO_EN == 1			/* UART8 */
	/* ʹ�� GPIO TX/RX ʱ�� */
	UART8_TX_GPIO_CLK_ENABLE();
	UART7_RX_GPIO_CLK_ENABLE();
	
	/* ʹ�� USARTx ʱ�� */
	UART8_CLK_ENABLE();	

	/* ����TX���� */
	GPIO_InitStruct.Pin       = UART8_TX_PIN;
	GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull      = GPIO_PULLUP;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = UART8_TX_AF;
	HAL_GPIO_Init(UART8_TX_GPIO_PORT, &GPIO_InitStruct);	
	
	/* ����RX���� */
	GPIO_InitStruct.Pin = UART8_RX_PIN;
	GPIO_InitStruct.Alternate = UART8_RX_AF;
	HAL_GPIO_Init(UART8_RX_GPIO_PORT, &GPIO_InitStruct);

	/* ����NVIC the NVIC for UART */   
	HAL_NVIC_SetPriority(UART8_IRQn, 0, 6);
	HAL_NVIC_EnableIRQ(UART8_IRQn);
	
	/* ���ò����ʡ���żУ�� */
	bsp_SetUartParam(UART8,  UART8_BAUD, UART_PARITY_NONE, UART_MODE_TX_RX);
	
	SET_BIT(UART8->ICR, USART_ICR_TCCF);	/* ���TC������ɱ�־ */
	SET_BIT(UART8->RQR, USART_RQR_RXFRQ);	/* ���RXNE���ձ�־ */
	SET_BIT(UART8->CR1, USART_CR1_RXNEIE);	/* ʹ��PE. RX�����ж� */
#endif
}

/*
*********************************************************************************************************
*	�� �� ��: UartSend
*	����˵��: ��д���ݵ�UART���ͻ�����,�����������жϡ��жϴ�������������Ϻ��Զ��رշ����ж�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void UartSend(UART_T *_pUart, uint8_t *_ucaBuf, uint16_t _usLen)
{
	uint16_t i;

	for (i = 0; i < _usLen; i++)
	{
		/* ������ͻ������Ѿ����ˣ���ȴ��������� */
		while (1)
		{
			__IO uint16_t usCount;

			DISABLE_INT();
			usCount = _pUart->usTxCount;
			ENABLE_INT();

			if (usCount < _pUart->usTxBufSize)
			{
				break;
			}
			else if(usCount == _pUart->usTxBufSize)/* ���������������� */
			{
				if((_pUart->uart->CR1 & USART_CR1_TXEIE) == 0)
				{
					SET_BIT(_pUart->uart->CR1, USART_CR1_TXEIE);
				}  
			}
		}

		/* �����������뷢�ͻ����� */
		_pUart->pTxBuf[_pUart->usTxWrite] = _ucaBuf[i];

		DISABLE_INT();
		if (++_pUart->usTxWrite >= _pUart->usTxBufSize)
		{
			_pUart->usTxWrite = 0;
		}
		_pUart->usTxCount++;
		ENABLE_INT();
	}

	SET_BIT(_pUart->uart->CR1, USART_CR1_TXEIE);	/* ʹ�ܷ����жϣ��������գ� */
}

/*
*********************************************************************************************************
*	�� �� ��: UartGetChar
*	����˵��: �Ӵ��ڽ��ջ�������ȡ1�ֽ����� ��������������ã�
*	��    ��: _pUart : �����豸
*			  _pByte : ��Ŷ�ȡ���ݵ�ָ��
*	�� �� ֵ: 0 ��ʾ������  1��ʾ��ȡ������
*********************************************************************************************************
*/
static uint8_t UartGetChar(UART_T *_pUart, uint8_t *_pByte)
{
	uint16_t usCount;

	/* usRxWrite �������жϺ����б���д���������ȡ�ñ���ʱ����������ٽ������� */
	DISABLE_INT();
	usCount = _pUart->usRxCount;
	ENABLE_INT();

	/* �������д������ͬ���򷵻�0 */
	//if (_pUart->usRxRead == usRxWrite)
	if (usCount == 0)	/* �Ѿ�û������ */
	{
		return 0;
	}
	else
	{
		*_pByte = _pUart->pRxBuf[_pUart->usRxRead];		/* �Ӵ��ڽ���FIFOȡ1������ */

		/* ��дFIFO������ */
		DISABLE_INT();
		if (++_pUart->usRxRead >= _pUart->usRxBufSize)
		{
			_pUart->usRxRead = 0;
		}
		_pUart->usRxCount--;
		ENABLE_INT();
		return 1;
	}
}

/*
*********************************************************************************************************
*   �� �� ��: UartTxEmpty
*   ����˵��: �жϷ��ͻ������Ƿ�Ϊ�ա�
*   ��    ��:  _pUart : �����豸
*   �� �� ֵ: 1Ϊ�ա�0Ϊ���ա�
*********************************************************************************************************
*/
uint8_t UartTxEmpty(COM_PORT_E _ucPort)
{
   UART_T *pUart;
   uint8_t Sending;
   
   pUart = ComToUart(_ucPort);
   if (pUart == 0)
   {
      return 0;
   }

   Sending = pUart->Sending;

   if (Sending != 0)
   {
      return 0;
   }
   return 1;
}

/*
*********************************************************************************************************
*	�� �� ��: UartIRQ
*	����˵��: ���жϷ��������ã�ͨ�ô����жϴ�������
*	��    ��: _pUart : �����豸
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void UartIRQ(UART_T *_pUart)
{
	uint32_t isrflags   = READ_REG(_pUart->uart->ISR);
	uint32_t cr1its     = READ_REG(_pUart->uart->CR1);
	uint32_t cr3its     = READ_REG(_pUart->uart->CR3);
	
	/* ���������ж�  */
	if ((isrflags & USART_ISR_RXNE) != RESET)
	{
		/* �Ӵ��ڽ������ݼĴ�����ȡ���ݴ�ŵ�����FIFO */
		uint8_t ch;

		ch = READ_REG(_pUart->uart->RDR);
		_pUart->pRxBuf[_pUart->usRxWrite] = ch;
		if (++_pUart->usRxWrite >= _pUart->usRxBufSize)
		{
			_pUart->usRxWrite = 0;
		}
		if (_pUart->usRxCount < _pUart->usRxBufSize)
		{
			_pUart->usRxCount++;
		}

		/* �ص�����,֪ͨӦ�ó����յ�������,һ���Ƿ���1����Ϣ��������һ����� */
		//if (_pUart->usRxWrite == _pUart->usRxRead)
		//if (_pUart->usRxCount == 1)
		{
			if (_pUart->ReciveNew)
			{
				_pUart->ReciveNew(ch); /* ���磬����MODBUS����������ֽ��� */
			}
		}
	}

	/* �������ͻ��������ж� */
	if ( ((isrflags & USART_ISR_TXE) != RESET) && (cr1its & USART_CR1_TXEIE) != RESET)
	{
		//if (_pUart->usTxRead == _pUart->usTxWrite)
		if (_pUart->usTxCount == 0)
		{
			/* ���ͻ�������������ȡ��ʱ�� ��ֹ���ͻ��������ж� ��ע�⣺��ʱ���1�����ݻ�δ����������ϣ�*/
			//USART_ITConfig(_pUart->uart, USART_IT_TXE, DISABLE);
			CLEAR_BIT(_pUart->uart->CR1, USART_CR1_TXEIE);

			/* ʹ�����ݷ�������ж� */
			//USART_ITConfig(_pUart->uart, USART_IT_TC, ENABLE);
			SET_BIT(_pUart->uart->CR1, USART_CR1_TCIE);
		}
		else
		{
			_pUart->Sending = 1;
			
			/* �ӷ���FIFOȡ1���ֽ�д�봮�ڷ������ݼĴ��� */
			//USART_SendData(_pUart->uart, _pUart->pTxBuf[_pUart->usTxRead]);
			_pUart->uart->TDR = _pUart->pTxBuf[_pUart->usTxRead];
			if (++_pUart->usTxRead >= _pUart->usTxBufSize)
			{
				_pUart->usTxRead = 0;
			}
			_pUart->usTxCount--;
		}

	}
	/* ����bitλȫ��������ϵ��ж� */
	if (((isrflags & USART_ISR_TC) != RESET) && ((cr1its & USART_CR1_TCIE) != RESET))
	{
		//if (_pUart->usTxRead == _pUart->usTxWrite)
		if (_pUart->usTxCount == 0)
		{
			/* �������FIFO������ȫ��������ϣ���ֹ���ݷ�������ж� */
			//USART_ITConfig(_pUart->uart, USART_IT_TC, DISABLE);
			CLEAR_BIT(_pUart->uart->CR1, USART_CR1_TCIE);

			/* �ص�����, һ����������RS485ͨ�ţ���RS485оƬ����Ϊ����ģʽ��������ռ���� */
			if (_pUart->SendOver)
			{
				_pUart->SendOver();
			}
			
			_pUart->Sending = 0;
		}
		else
		{
			/* ��������£��������˷�֧ */

			/* �������FIFO�����ݻ�δ��ϣ���ӷ���FIFOȡ1������д�뷢�����ݼĴ��� */
			//USART_SendData(_pUart->uart, _pUart->pTxBuf[_pUart->usTxRead]);
			_pUart->uart->TDR = _pUart->pTxBuf[_pUart->usTxRead];
			if (++_pUart->usTxRead >= _pUart->usTxBufSize)
			{
				_pUart->usTxRead = 0;
			}
			_pUart->usTxCount--;
		}
	}
	
	/* ����жϱ�־ */
	SET_BIT(_pUart->uart->ICR, UART_CLEAR_PEF);
	SET_BIT(_pUart->uart->ICR, UART_CLEAR_FEF);
	SET_BIT(_pUart->uart->ICR, UART_CLEAR_NEF);
	SET_BIT(_pUart->uart->ICR, UART_CLEAR_OREF);
	SET_BIT(_pUart->uart->ICR, UART_CLEAR_IDLEF);
	SET_BIT(_pUart->uart->ICR, UART_CLEAR_TCF);
	SET_BIT(_pUart->uart->ICR, UART_CLEAR_LBDF);
	SET_BIT(_pUart->uart->ICR, UART_CLEAR_CTSF);
	SET_BIT(_pUart->uart->ICR, UART_CLEAR_CMF);
	SET_BIT(_pUart->uart->ICR, UART_CLEAR_WUF);
	SET_BIT(_pUart->uart->ICR, UART_CLEAR_TXFECF);
	
//	  *            @arg UART_CLEAR_PEF: Parity Error Clear Flag
//  *            @arg UART_CLEAR_FEF: Framing Error Clear Flag
//  *            @arg UART_CLEAR_NEF: Noise detected Clear Flag
//  *            @arg UART_CLEAR_OREF: OverRun Error Clear Flag
//  *            @arg UART_CLEAR_IDLEF: IDLE line detected Clear Flag
//  *            @arg UART_CLEAR_TCF: Transmission Complete Clear Flag
//  *            @arg UART_CLEAR_LBDF: LIN Break Detection Clear Flag
//  *            @arg UART_CLEAR_CTSF: CTS Interrupt Clear Flag
//  *            @arg UART_CLEAR_RTOF: Receiver Time Out Clear Flag
//  *            @arg UART_CLEAR_CMF: Character Match Clear Flag
//  *            @arg.UART_CLEAR_WUF:  Wake Up from stop mode Clear Flag
//  *            @arg UART_CLEAR_TXFECF: TXFIFO empty Clear Flag	
}

/*
*********************************************************************************************************
*	�� �� ��: USART1_IRQHandler  USART2_IRQHandler USART3_IRQHandler UART4_IRQHandler UART5_IRQHandler��
*	����˵��: USART�жϷ������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
#if UART1_FIFO_EN == 1
/*
*********************************************************************************************************
*   函 数 名: USART1_IRQHandler
*   功能说明: USART1_IRQHandler 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void USART1_IRQHandler(void)
{
	UartIRQ(&g_tUart1);
}
#endif

#if UART2_FIFO_EN == 1
/*
*********************************************************************************************************
*   函 数 名: USART2_IRQHandler
*   功能说明: USART2_IRQHandler 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void USART2_IRQHandler(void)
{
	UartIRQ(&g_tUart2);
}
#endif

#if UART3_FIFO_EN == 1
/*
*********************************************************************************************************
*   函 数 名: USART3_IRQHandler
*   功能说明: USART3_IRQHandler 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void USART3_IRQHandler(void)
{
	UartIRQ(&g_tUart3);
}
#endif

#if UART4_FIFO_EN == 1
/*
*********************************************************************************************************
*   函 数 名: UART4_IRQHandler
*   功能说明: UART4_IRQHandler 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UART4_IRQHandler(void)
{
	UartIRQ(&g_tUart4);
}
#endif

#if UART5_FIFO_EN == 1
/*
*********************************************************************************************************
*   函 数 名: UART5_IRQHandler
*   功能说明: UART5_IRQHandler 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UART5_IRQHandler(void)
{
	UartIRQ(&g_tUart5);
}
#endif

#if UART6_FIFO_EN == 1
/*
*********************************************************************************************************
*   函 数 名: USART6_IRQHandler
*   功能说明: USART6_IRQHandler 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void USART6_IRQHandler(void)
{
	UartIRQ(&g_tUart6);
}
#endif

#if UART7_FIFO_EN == 1
/*
*********************************************************************************************************
*   函 数 名: UART7_IRQHandler
*   功能说明: UART7_IRQHandler 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UART7_IRQHandler(void)
{
	UartIRQ(&g_tUart7);
}
#endif

#if UART8_FIFO_EN == 1
/*
*********************************************************************************************************
*   函 数 名: UART8_IRQHandler
*   功能说明: UART8_IRQHandler 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UART8_IRQHandler(void)
{
	UartIRQ(&g_tUart8);
}
#endif

/*
*********************************************************************************************************
*   函 数 名: UartSendCharPolled
*   功能说明: UartSendCharPolled 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void UartSendCharPolled(USART_TypeDef *uart, uint8_t ch)
{
	uint32_t timeout = 1000000u;

	if ((uart == 0) || ((uart->CR1 & USART_CR1_UE) == 0u))
	{
		return;
	}

	while (((uart->ISR & USART_ISR_TXE) == 0u) && (timeout > 0u))
	{
		timeout--;
	}
	if (timeout == 0u)
	{
		return;
	}

	uart->TDR = ch;
}

/*
*********************************************************************************************************
*   函 数 名: bsp_Uart1SendStringPolled
*   功能说明: bsp_Uart1SendStringPolled 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void bsp_Uart1SendStringPolled(const char *text)
{
#if UART1_FIFO_EN == 1
	if ((s_uart_ready == 0u) || (text == 0))
	{
		return;
	}

	while (*text != '\0')
	{
		UartSendCharPolled(USART1, (uint8_t)*text);
		text++;
	}
#else
	(void)text;
#endif
}

/*
*********************************************************************************************************
*	�� �� ��: fputc
*	����˵��: �ض���putc��������������ʹ��printf�����Ӵ���1��ӡ���
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
int fputc(int ch, FILE *f)
{
	(void)f;

#if UART1_FIFO_EN == 1
	if (s_uart_ready == 0u)
	{
		return ch;
	}
	if (__get_PRIMASK() != 0u)
	{
		UartSendCharPolled(USART1, (uint8_t)ch);
		return ch;
	}
#endif

	comSendChar(COM1, (uint8_t)ch);
	
	return ch;
}

/*
*********************************************************************************************************
*	�� �� ��: fgetc
*	����˵��: �ض���getc��������������ʹ��getchar�����Ӵ���1��������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
int fgetc(FILE *f)
{

#if 1	/* �Ӵ��ڽ���FIFO��ȡ1������, ֻ��ȡ�����ݲŷ��� */
	uint8_t ucData;

	while(comGetChar(COM1, &ucData) == 0);

	return ucData;
#else
	/* �ȴ����յ����� */
	while((USART1->ISR & USART_ISR_RXNE) == 0)
	{}

	return (int)USART1->RDR;
#endif
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/

