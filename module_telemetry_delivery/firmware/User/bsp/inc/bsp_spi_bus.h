/**
 * @file bsp_spi_bus.h
 * @brief 安富莱 BSP 通用驱动（SPI 总线）头文件；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : SPI��������
*	�ļ����� : bsp_spi_bus.h
*	��    �� : V1.0
*	˵    �� : ͷ�ļ�
*
*	Copyright (C), 2014-2015, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_SPI_BUS_H
/* 宏定义: __BSP_SPI_BUS_H - 头文件保护宏。 */
#define __BSP_SPI_BUS_H

/* �ض�����SPI SCKʱ�ӣ�������ֲ */
#define SPI_BAUDRATEPRESCALER_100M      SPI_BAUDRATEPRESCALER_2			/* 100M */
#define SPI_BAUDRATEPRESCALER_50M       SPI_BAUDRATEPRESCALER_4			/* 50M */
#define SPI_BAUDRATEPRESCALER_12_5M     SPI_BAUDRATEPRESCALER_8			/* 12.5M */
#define SPI_BAUDRATEPRESCALER_6_25M     SPI_BAUDRATEPRESCALER_16		/* 6.25M */
#define SPI_BAUDRATEPRESCALER_3_125M    SPI_BAUDRATEPRESCALER_32		/* 3.125M */
#define SPI_BAUDRATEPRESCALER_1_5625M   SPI_BAUDRATEPRESCALER_64		/* 1.5625M */
#define SPI_BAUDRATEPRESCALER_781_25K   SPI_BAUDRATEPRESCALER_128		/* 781.25K */
#define SPI_BAUDRATEPRESCALER_390_625K  SPI_BAUDRATEPRESCALER_256		/* 390.625K */

void bsp_InitSPIBus(void);
/*
*********************************************************************************************************
*   函 数 名: bsp_spiTransfer
*   功能说明: bsp_spiTransfer 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void bsp_spiTransfer(void);
/*
*********************************************************************************************************
*   函 数 名: bsp_InitSPIParam
*   功能说明: bsp_InitSPIParam 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void bsp_InitSPIParam(uint32_t _BaudRatePrescaler, uint32_t _CLKPhase, uint32_t _CLKPolarity);

/*
*********************************************************************************************************
*   函 数 名: bsp_SpiBusEnter
*   功能说明: bsp_SpiBusEnter 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void bsp_SpiBusEnter(void);
/*
*********************************************************************************************************
*   函 数 名: bsp_SpiBusExit
*   功能说明: bsp_SpiBusExit 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void bsp_SpiBusExit(void);
/*
*********************************************************************************************************
*   函 数 名: bsp_SpiBusBusy
*   功能说明: bsp_SpiBusBusy 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t bsp_SpiBusBusy(void);

#define	SPI_BUFFER_SIZE		(4 * 1024)				/*  */

extern uint8_t g_spiTxBuf[SPI_BUFFER_SIZE];
extern uint8_t g_spiRxBuf[SPI_BUFFER_SIZE];
extern uint32_t g_spiLen;

extern uint8_t g_spi_busy;

#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
