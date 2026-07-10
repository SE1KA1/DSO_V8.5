/**
 * @file bsp_fmc_sdram.h
 * @brief FMC 外部 SDRAM 驱动：32MB @0xC0000000 初始化与测试 API。
 */

/*
*********************************************************************************************************
*
*	模块名称 : 外部SDRAM驱动模块
*	文件名称 : bsp_fmc_sdram.h
*	�?    �? : V1.0
*	�?    �? : 头文�?
*
*	�?改�?�录 :
*		版本�?  日期       作�?    说明
*		v1.0    2014-05-04 armfly  ST固件库版�? V1.3.0
*
*	Copyright (C), 2014-2015, 安富莱电�? www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_FMC_SDRAM_H
/* 宏定义: _BSP_FMC_SDRAM_H - 头文件保护宏。 */
#define _BSP_FMC_SDRAM_H

/* 宏定义: EXT_SDRAM_ADDR - EXT_SDRAM_ADDR 的宏定义。 */
#define EXT_SDRAM_ADDR  	((uint32_t)0xC0000000)
/* 宏定义: EXT_SDRAM_SIZE - EXT_SDRAM_SIZE 的宏定义。 */
#define EXT_SDRAM_SIZE		(32 * 1024 * 1024)

/* LCD显存,�?1�?, 分配2M字节 */
#define SDRAM_LCD_BUF1 		EXT_SDRAM_ADDR

/* LCD显存,�?2�?, 分配2M字节 */
#define SDRAM_LCD_BUF2		(EXT_SDRAM_ADDR + SDRAM_LCD_SIZE)

#define SDRAM_LCD_SIZE		(2 * 1024 * 1024)		/* 每层2M */
#define SDRAM_LCD_LAYER		2						/* 2�? */

/* 剩下�?12M字节，提供给应用程序使用 */
#define SDRAM_APP_BUF		(EXT_SDRAM_ADDR + SDRAM_LCD_SIZE * SDRAM_LCD_LAYER)
/* 宏定义: SDRAM_APP_SIZE - SDRAM_APP_SIZE 的宏定义。 */
#define SDRAM_APP_SIZE		(EXT_SDRAM_SIZE - SDRAM_LCD_SIZE * SDRAM_LCD_LAYER)

/*
*********************************************************************************************************
*   函 数 名: bsp_InitExtSDRAM
*   功能说明: bsp_InitExtSDRAM 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void bsp_InitExtSDRAM(void);
/*
*********************************************************************************************************
*   函 数 名: bsp_TestExtSDRAM1
*   功能说明: bsp_TestExtSDRAM1 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t bsp_TestExtSDRAM1(void);
/*
*********************************************************************************************************
*   函 数 名: bsp_TestExtSDRAM2
*   功能说明: bsp_TestExtSDRAM2 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t bsp_TestExtSDRAM2(void);
/* �� [addr, addr+size_bytes) �� 32 λ����д��У�飬�� Test2 ͬ������� GUI �����ϵ��Լ� */
uint32_t bsp_TestExtSDRAM_Block(uint32_t addr, uint32_t size_bytes);

#endif

/***************************** 安富莱电�? www.armfly.com (END OF FILE) *********************************/
