/**
 * @file bsp_qspi_w25q256.h
 * @brief 安富莱 BSP 通用驱动（W25Q256 QSPI Flash）头文件；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : W25Q256 QSPI����ģ��
*	�ļ����� : bsp_qspi_w25q256.h
*
*	Copyright (C),  2019-2030. ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_QSPI_W25Q256_H
/* 宏定义: _BSP_QSPI_W25Q256_H - 头文件保护宏。 */
#define _BSP_QSPI_W25Q256_H


/* W25Q256JV������Ϣ */
#define QSPI_FLASH_SIZE     25                      /* Flash��С��2^25 = 32MB*/
#define QSPI_SECTOR_SIZE    (4 * 1024)              /* ������С��4KB */
#define QSPI_PAGE_SIZE      256        				/* ҳ��С��256�ֽ� */
#define QSPI_END_ADDR    	(1 << QSPI_FLASH_SIZE)  /* ĩβ��ַ */
#define QSPI_FLASH_SIZES    32*1024*1024            /* Flash��С��2^25 = 32MB*/

/* W25Q256JV������� */
#define WRITE_ENABLE_CMD      0x06         /* дʹ��ָ�� */  
#define READ_ID_CMD2          0x9F         /* ��ȡID���� */  
#define READ_STATUS_REG_CMD   0x05         /* ��ȡ״̬���� */ 
#define SUBSECTOR_ERASE_4_BYTE_ADDR_CMD      0x21    /* 32bit��ַ��������ָ��, 4KB */
#define QUAD_IN_FAST_PROG_4_BYTE_ADDR_CMD    0x34    /* 32bit��ַ��4�߿���д������ */
#define QUAD_INOUT_FAST_READ_4_BYTE_ADDR_CMD 0xEC    /* 32bit��ַ��4�߿��ٶ�ȡ���� */


/* ���������������Ժ���չAPIʹ�� */
#if 0

/* Reset Operations */
#define RESET_ENABLE_CMD                     0x66
/* 宏定义: RESET_MEMORY_CMD - RESET_MEMORY_CMD 的宏定义。 */
#define RESET_MEMORY_CMD                     0x99

/* Identification Operations */
#define READ_ID_CMD                          0x9E
/* 宏定义: READ_ID_CMD2 - READ_ID_CMD2 的宏定义。 */
#define READ_ID_CMD2                         0x9F
/* 宏定义: MULTIPLE_IO_READ_ID_CMD - MULTIPLE_IO_READ_ID_CMD 的宏定义。 */
#define MULTIPLE_IO_READ_ID_CMD              0xAF
/* 宏定义: READ_SERIAL_FLASH_DISCO_PARAM_CMD - READ_SERIAL_FLASH_DISCO_PARAM_CMD 的宏定义。 */
#define READ_SERIAL_FLASH_DISCO_PARAM_CMD    0x5A

/* Read Operations */
#define READ_CMD                             0x03
/* 宏定义: READ_4_BYTE_ADDR_CMD - READ_4_BYTE_ADDR_CMD 的宏定义。 */
#define READ_4_BYTE_ADDR_CMD                 0x13

/* 宏定义: FAST_READ_CMD - FAST_READ_CMD 的宏定义。 */
#define FAST_READ_CMD                        0x0B
/* 宏定义: FAST_READ_DTR_CMD - FAST_READ_DTR_CMD 的宏定义。 */
#define FAST_READ_DTR_CMD                    0x0D
/* 宏定义: FAST_READ_4_BYTE_ADDR_CMD - FAST_READ_4_BYTE_ADDR_CMD 的宏定义。 */
#define FAST_READ_4_BYTE_ADDR_CMD            0x0C

/* 宏定义: DUAL_OUT_FAST_READ_CMD - DUAL_OUT_FAST_READ_CMD 的宏定义。 */
#define DUAL_OUT_FAST_READ_CMD               0x3B
/* 宏定义: DUAL_OUT_FAST_READ_DTR_CMD - DUAL_OUT_FAST_READ_DTR_CMD 的宏定义。 */
#define DUAL_OUT_FAST_READ_DTR_CMD           0x3D
/* 宏定义: DUAL_OUT_FAST_READ_4_BYTE_ADDR_CMD - DUAL_OUT_FAST_READ_4_BYTE_ADDR_CMD 的宏定义。 */
#define DUAL_OUT_FAST_READ_4_BYTE_ADDR_CMD   0x3C

/* 宏定义: DUAL_INOUT_FAST_READ_CMD - DUAL_INOUT_FAST_READ_CMD 的宏定义。 */
#define DUAL_INOUT_FAST_READ_CMD             0xBB
/* 宏定义: DUAL_INOUT_FAST_READ_DTR_CMD - DUAL_INOUT_FAST_READ_DTR_CMD 的宏定义。 */
#define DUAL_INOUT_FAST_READ_DTR_CMD         0xBD
/* 宏定义: DUAL_INOUT_FAST_READ_4_BYTE_ADDR_CMD - DUAL_INOUT_FAST_READ_4_BYTE_ADDR_CMD 的宏定义。 */
#define DUAL_INOUT_FAST_READ_4_BYTE_ADDR_CMD 0xBC

/* 宏定义: QUAD_OUT_FAST_READ_CMD - QUAD_OUT_FAST_READ_CMD 的宏定义。 */
#define QUAD_OUT_FAST_READ_CMD               0x6B
/* 宏定义: QUAD_OUT_FAST_READ_DTR_CMD - QUAD_OUT_FAST_READ_DTR_CMD 的宏定义。 */
#define QUAD_OUT_FAST_READ_DTR_CMD           0x6D
/* 宏定义: QUAD_OUT_FAST_READ_4_BYTE_ADDR_CMD - QUAD_OUT_FAST_READ_4_BYTE_ADDR_CMD 的宏定义。 */
#define QUAD_OUT_FAST_READ_4_BYTE_ADDR_CMD   0x6C

/* 宏定义: QUAD_INOUT_FAST_READ_CMD - QUAD_INOUT_FAST_READ_CMD 的宏定义。 */
#define QUAD_INOUT_FAST_READ_CMD             0xEB
/* 宏定义: QUAD_INOUT_FAST_READ_DTR_CMD - QUAD_INOUT_FAST_READ_DTR_CMD 的宏定义。 */
#define QUAD_INOUT_FAST_READ_DTR_CMD         0xED
/* 宏定义: QUAD_INOUT_FAST_READ_4_BYTE_ADDR_CMD - QUAD_INOUT_FAST_READ_4_BYTE_ADDR_CMD 的宏定义。 */
#define QUAD_INOUT_FAST_READ_4_BYTE_ADDR_CMD 0xEC

/* Write Operations */
#define WRITE_ENABLE_CMD                     0x06
/* 宏定义: WRITE_DISABLE_CMD - WRITE_DISABLE_CMD 的宏定义。 */
#define WRITE_DISABLE_CMD                    0x04

/* Register Operations */
#define READ_STATUS_REG_CMD                  0x05
/* 宏定义: WRITE_STATUS_REG_CMD - WRITE_STATUS_REG_CMD 的宏定义。 */
#define WRITE_STATUS_REG_CMD                 0x01

/* 宏定义: READ_LOCK_REG_CMD - READ_LOCK_REG_CMD 的宏定义。 */
#define READ_LOCK_REG_CMD                    0xE8
/* 宏定义: WRITE_LOCK_REG_CMD - WRITE_LOCK_REG_CMD 的宏定义。 */
#define WRITE_LOCK_REG_CMD                   0xE5

/* 宏定义: READ_FLAG_STATUS_REG_CMD - READ_FLAG_STATUS_REG_CMD 的宏定义。 */
#define READ_FLAG_STATUS_REG_CMD             0x70
/* 宏定义: CLEAR_FLAG_STATUS_REG_CMD - CLEAR_FLAG_STATUS_REG_CMD 的宏定义。 */
#define CLEAR_FLAG_STATUS_REG_CMD            0x50

/* 宏定义: READ_NONVOL_CFG_REG_CMD - READ_NONVOL_CFG_REG_CMD 的宏定义。 */
#define READ_NONVOL_CFG_REG_CMD              0xB5
/* 宏定义: WRITE_NONVOL_CFG_REG_CMD - WRITE_NONVOL_CFG_REG_CMD 的宏定义。 */
#define WRITE_NONVOL_CFG_REG_CMD             0xB1

/* 宏定义: READ_VOL_CFG_REG_CMD - READ_VOL_CFG_REG_CMD 的宏定义。 */
#define READ_VOL_CFG_REG_CMD                 0x85
/* 宏定义: WRITE_VOL_CFG_REG_CMD - WRITE_VOL_CFG_REG_CMD 的宏定义。 */
#define WRITE_VOL_CFG_REG_CMD                0x81

/* 宏定义: READ_ENHANCED_VOL_CFG_REG_CMD - READ_ENHANCED_VOL_CFG_REG_CMD 的宏定义。 */
#define READ_ENHANCED_VOL_CFG_REG_CMD        0x65
/* 宏定义: WRITE_ENHANCED_VOL_CFG_REG_CMD - WRITE_ENHANCED_VOL_CFG_REG_CMD 的宏定义。 */
#define WRITE_ENHANCED_VOL_CFG_REG_CMD       0x61

/* 宏定义: READ_EXT_ADDR_REG_CMD - READ_EXT_ADDR_REG_CMD 的宏定义。 */
#define READ_EXT_ADDR_REG_CMD                0xC8
/* 宏定义: WRITE_EXT_ADDR_REG_CMD - WRITE_EXT_ADDR_REG_CMD 的宏定义。 */
#define WRITE_EXT_ADDR_REG_CMD               0xC5

/* Program Operations */
#define PAGE_PROG_CMD                        0x02
/* 宏定义: PAGE_PROG_4_BYTE_ADDR_CMD - PAGE_PROG_4_BYTE_ADDR_CMD 的宏定义。 */
#define PAGE_PROG_4_BYTE_ADDR_CMD            0x12

/* 宏定义: DUAL_IN_FAST_PROG_CMD - DUAL_IN_FAST_PROG_CMD 的宏定义。 */
#define DUAL_IN_FAST_PROG_CMD                0xA2
/* 宏定义: EXT_DUAL_IN_FAST_PROG_CMD - EXT_DUAL_IN_FAST_PROG_CMD 的宏定义。 */
#define EXT_DUAL_IN_FAST_PROG_CMD            0xD2

/* 宏定义: QUAD_IN_FAST_PROG_CMD - QUAD_IN_FAST_PROG_CMD 的宏定义。 */
#define QUAD_IN_FAST_PROG_CMD                0x32
#define EXT_QUAD_IN_FAST_PROG_CMD            0x12 /*0x38*/
#define QUAD_IN_FAST_PROG_4_BYTE_ADDR_CMD    0x34

/* Erase Operations */
#define SUBSECTOR_ERASE_CMD                  0x20
/* 宏定义: SUBSECTOR_ERASE_4_BYTE_ADDR_CMD - SUBSECTOR_ERASE_4_BYTE_ADDR_CMD 的宏定义。 */
#define SUBSECTOR_ERASE_4_BYTE_ADDR_CMD      0x21

/* 宏定义: SECTOR_ERASE_CMD - SECTOR_ERASE_CMD 的宏定义。 */
#define SECTOR_ERASE_CMD                     0xD8
/* 宏定义: SECTOR_ERASE_4_BYTE_ADDR_CMD - SECTOR_ERASE_4_BYTE_ADDR_CMD 的宏定义。 */
#define SECTOR_ERASE_4_BYTE_ADDR_CMD         0xDC

/* 宏定义: BULK_ERASE_CMD - BULK_ERASE_CMD 的宏定义。 */
#define BULK_ERASE_CMD                       0xC7

/* 宏定义: PROG_ERASE_RESUME_CMD - PROG_ERASE_RESUME_CMD 的宏定义。 */
#define PROG_ERASE_RESUME_CMD                0x7A
/* 宏定义: PROG_ERASE_SUSPEND_CMD - PROG_ERASE_SUSPEND_CMD 的宏定义。 */
#define PROG_ERASE_SUSPEND_CMD               0x75

/* One-Time Programmable Operations */
#define READ_OTP_ARRAY_CMD                   0x4B
/* 宏定义: PROG_OTP_ARRAY_CMD - PROG_OTP_ARRAY_CMD 的宏定义。 */
#define PROG_OTP_ARRAY_CMD                   0x42

/* 4-byte Address Mode Operations */
#define ENTER_4_BYTE_ADDR_MODE_CMD           0xB7
/* 宏定义: EXIT_4_BYTE_ADDR_MODE_CMD - EXIT_4_BYTE_ADDR_MODE_CMD 的宏定义。 */
#define EXIT_4_BYTE_ADDR_MODE_CMD            0xE9

/* Quad Operations */
#define ENTER_QUAD_CMD                       0x35
/* 宏定义: EXIT_QUAD_CMD - EXIT_QUAD_CMD 的宏定义。 */
#define EXIT_QUAD_CMD                        0xF5

/* Default dummy clocks cycles */
#define DUMMY_CLOCK_CYCLES_READ              8
/* 宏定义: DUMMY_CLOCK_CYCLES_READ_QUAD - DUMMY_CLOCK_CYCLES_READ_QUAD 的宏定义。 */
#define DUMMY_CLOCK_CYCLES_READ_QUAD         10

/* 宏定义: DUMMY_CLOCK_CYCLES_READ_DTR - DUMMY_CLOCK_CYCLES_READ_DTR 的宏定义。 */
#define DUMMY_CLOCK_CYCLES_READ_DTR          6
/* 宏定义: DUMMY_CLOCK_CYCLES_READ_QUAD_DTR - DUMMY_CLOCK_CYCLES_READ_QUAD_DTR 的宏定义。 */
#define DUMMY_CLOCK_CYCLES_READ_QUAD_DTR     8

#endif


/*
*********************************************************************************************************
*   函 数 名: bsp_InitQSPI_W25Q256
*   功能说明: bsp_InitQSPI_W25Q256 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void bsp_InitQSPI_W25Q256(void);
/*
*********************************************************************************************************
*   函 数 名: QSPI_EraseSector
*   功能说明: QSPI_EraseSector 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void QSPI_EraseSector(uint32_t address);
/*
*********************************************************************************************************
*   函 数 名: QSPI_WriteBuffer
*   功能说明: QSPI_WriteBuffer 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t QSPI_WriteBuffer(uint8_t *_pBuf, uint32_t _uiWriteAddr, uint16_t _usWriteSize);
/*
*********************************************************************************************************
*   函 数 名: QSPI_ReadBuffer
*   功能说明: QSPI_ReadBuffer 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void QSPI_ReadBuffer(uint8_t * _pBuf, uint32_t _uiReadAddr, uint32_t _uiSize);
/*
*********************************************************************************************************
*   函 数 名: QSPI_ReadID
*   功能说明: QSPI_ReadID 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t QSPI_ReadID(void);
	
#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
