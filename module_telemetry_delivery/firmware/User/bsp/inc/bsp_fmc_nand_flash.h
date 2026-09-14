/**
 * @file bsp_fmc_nand_flash.h
 * @brief 安富莱 BSP 通用驱动（NAND Flash）头文件；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : NAND Flash ����ģ��
*	�ļ����� : bsp_nand.h
*	��    �� : V1.1
*	˵    �� : ͷ�ļ�
*
*	Copyright (C), 2014-2015, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_NAND_H
/* 宏定义: __BSP_NAND_H - 头文件保护宏。 */
#define __BSP_NAND_H

//typedef struct
//{
//  uint8_t Maker_ID;
//  uint8_t Device_ID;
//  uint8_t Third_ID;
//  uint8_t Fourth_ID;
//}NAND_IDTypeDef;

typedef struct
{
  uint16_t Zone;
  uint16_t Block;
  uint16_t Page;
} NAND_ADDRESS_T;

#define NAND_TYPE	HY27UF081G2A	/* for V5�� V6 */
//#define NAND_TYPE	H27U4G8F2DTR 	/*  */

/*
	������Ч�� NAND ID
	HY27UF081G2A  	= 0xAD 0xF1 0x80 0x1D
	K9F1G08U0A		= 0xEC 0xF1 0x80 0x15
	K9F1G08U0B		= 0xEC 0xF1 0x00 0x95

	H27U4G8F2DTR    = 0xAD DC 90 95
*/

/* NAND Flash �ͺ� */
#define HY27UF081G2A	0xADF1801D
/* 宏定义: K9F1G08U0A - K9F1G08U0A 的宏定义。 */
#define K9F1G08U0A		0xECF18015
/* 宏定义: K9F1G08U0B - K9F1G08U0B 的宏定义。 */
#define K9F1G08U0B		0xECF10095
#define H27U1G8F2BTR	0xADF1001D		/* STM32-V4 ȱʡ, ����V5��V6 ��ͳһ������ͺ� */

#define H27U4G8F2DTR	0xADDC9095

/* 宏定义: NAND_UNKNOW - NAND_UNKNOW 的宏定义。 */
#define NAND_UNKNOW		0xFFFFFFFF

/* Exported constants --------------------------------------------------------*/
/* NAND Area definition  for STM3210E-EVAL Board RevD */
//#define CMD_AREA                   (uint32_t)(1<<16)  /* A16 = CLE  high */
//#define ADDR_AREA                  (uint32_t)(1<<17)  /* A17 = ALE high */
#define DATA_AREA                  ((uint32_t)0x00000000)

/* FSMC NAND memory command */
//#define	NAND_CMD_AREA_A            ((uint8_t)0x00)
//#define	NAND_CMD_AREA_B            ((uint8_t)0x01)
//#define NAND_CMD_AREA_C            ((uint8_t)0x50)
//#define NAND_CMD_AREA_TRUE1        ((uint8_t)0x30)

//#define NAND_CMD_WRITE0            ((uint8_t)0x80)
//#define NAND_CMD_WRITE_TRUE1       ((uint8_t)0x10)

//#define NAND_CMD_ERASE0            ((uint8_t)0x60)
//#define NAND_CMD_ERASE1            ((uint8_t)0xD0)

//#define NAND_CMD_READID            ((uint8_t)0x90)

//#define NAND_CMD_LOCK_STATUS       ((uint8_t)0x7A)
//#define NAND_CMD_RESET             ((uint8_t)0xFF)

/* NAND memory status */
//#define NAND_BUSY                  ((uint8_t)0x00)
//#define NAND_ERROR                 ((uint8_t)0x01)
//#define NAND_READY                 ((uint8_t)0x40)
//#define NAND_TIMEOUT_ERROR         ((uint8_t)0x80)

/* FSMC NAND memory parameters */
/* ����HY27UF081G2A    K9F1G08 */
#if NAND_TYPE == HY27UF081G2A
	#define NAND_PAGE_SIZE             ((uint16_t)0x0800) /* 2 * 1024 bytes per page w/o Spare Area */
	#define NAND_BLOCK_SIZE            ((uint16_t)0x0040) /* 64 pages per block */
	#define NAND_ZONE_SIZE             ((uint16_t)0x0400) /* 1024 Block per zone */
	#define NAND_SPARE_AREA_SIZE       ((uint16_t)0x0040) /* last 64 bytes as spare area */
	#define NAND_MAX_ZONE              ((uint16_t)0x0001) /* 1 zones of 1024 block */
	#define NAND_ADDR_5					0			/* 0��ʾֻ�÷���4���ֽڵĵ�ַ��1��ʾ5�� */

	/* ������붨�� */
	#define NAND_CMD_COPYBACK_A			((uint8_t)0x00)		/* PAGE COPY-BACK �������� */
	#define NAND_CMD_COPYBACK_B			((uint8_t)0x35)
/* 宏定义: NAND_CMD_COPYBACK_C - NAND_CMD_COPYBACK_C 的宏定义。 */
	#define NAND_CMD_COPYBACK_C			((uint8_t)0x85)
/* 宏定义: NAND_CMD_COPYBACK_D - NAND_CMD_COPYBACK_D 的宏定义。 */
	#define NAND_CMD_COPYBACK_D			((uint8_t)0x10)

//	#define NAND_CMD_STATUS				((uint8_t)0x70)		/* ��NAND Flash��״̬�� */

	#define MAX_PHY_BLOCKS_PER_ZONE  1024	/* ÿ�������������� */
	#define MAX_LOG_BLOCKS_PER_ZONE  1000	/* ÿ��������߼���� */

	#define NAND_BLOCK_COUNT			1024 /* ����� */
	#define NAND_PAGE_TOTAL_SIZE		(NAND_PAGE_SIZE + NAND_SPARE_AREA_SIZE)	/* ҳ���ܴ�С */

#elif NAND_TYPE == H27U4G8F2DTR
	#define NAND_PAGE_SIZE             ((uint16_t)0x0800) /* 2 * 1024 bytes per page w/o Spare Area */
	#define NAND_BLOCK_SIZE            ((uint16_t)0x0040) /* 64 pages per block */
	#define NAND_ZONE_SIZE             ((uint16_t)0x1000) /* 4096 Block per zone */
	#define NAND_SPARE_AREA_SIZE       ((uint16_t)0x0040) /* last 64 bytes as spare area */
	#define NAND_MAX_ZONE              ((uint16_t)0x0001) /* 1 zones of 4096 block */
	#define NAND_ADDR_5					1			/* 1��ʾֻ����4���ֽڵĵ�ַ��1��ʾ5�� */

	/* ������붨�� */
	#define NAND_CMD_COPYBACK_A			((uint8_t)0x00)		/* PAGE COPY-BACK �������� */
	#define NAND_CMD_COPYBACK_B			((uint8_t)0x35)
/* 宏定义: NAND_CMD_COPYBACK_C - NAND_CMD_COPYBACK_C 的宏定义。 */
	#define NAND_CMD_COPYBACK_C			((uint8_t)0x85)
/* 宏定义: NAND_CMD_COPYBACK_D - NAND_CMD_COPYBACK_D 的宏定义。 */
	#define NAND_CMD_COPYBACK_D			((uint8_t)0x10)

	#define NAND_CMD_STATUS				((uint8_t)0x70)		/* ��NAND Flash��״̬�� */

	#define MAX_PHY_BLOCKS_PER_ZONE     4096	/* ÿ�������������� */
	#define MAX_LOG_BLOCKS_PER_ZONE     4000	/* ÿ��������߼���� */

	#define NAND_BLOCK_COUNT			4096 /* ����� */
	#define NAND_PAGE_TOTAL_SIZE		(NAND_PAGE_SIZE + NAND_SPARE_AREA_SIZE)	/* ҳ���ܴ�С */

#else
	#define NAND_PAGE_SIZE             ((uint16_t)0x0200) /* 512 bytes per page w/o Spare Area */
	#define NAND_BLOCK_SIZE            ((uint16_t)0x0020) /* 32x512 bytes pages per block */
	#define NAND_ZONE_SIZE             ((uint16_t)0x0400) /* 1024 Block per zone */
	#define NAND_SPARE_AREA_SIZE       ((uint16_t)0x0010) /* last 16 bytes as spare area */
	#define NAND_MAX_ZONE              ((uint16_t)0x0004) /* 4 zones of 1024 block */
#endif

#define NAND_BAD_BLOCK_FLAG			0x00	/* ���ڵ�1��page�������ĵ�1���ֽ�д���0xFF���ݱ�ʾ���� */
#define NAND_USED_BLOCK_FLAG		0xF0	/* ���ڵ�1��page�������ĵ�2���ֽ�д���0xFF���ݱ�ʾ��ʹ�õĿ� */

#define BI_OFFSET				0		/* ���ڵ�1��page�������ĵ�1���ֽ��ǻ����־ */
#define USED_OFFSET				1		/* ���ڵ�1��page�������ĵ�1���ֽ������ñ�־ */
#define LBN0_OFFSET				2		/* ���ڵ�1��page�������ĵ�3���ֽڱ�ʾ�߼���ŵ�8bit */
#define LBN1_OFFSET				3		/* ���ڵ�1��page�������ĵ�4���ֽڱ�ʾ�߼���Ÿ�8bit */
#define VALID_SPARE_SIZE		4		/* ʵ��ʹ�õı�������С,���ں����ڲ��������ݻ�������С */

/* FSMC NAND memory address computation */
#define ADDR_1st_CYCLE(ADDR)       (uint8_t)((ADDR)& 0xFF)               /* 1st addressing cycle */
#define ADDR_2nd_CYCLE(ADDR)       (uint8_t)(((ADDR)& 0xFF00) >> 8)      /* 2nd addressing cycle */
#define ADDR_3rd_CYCLE(ADDR)       (uint8_t)(((ADDR)& 0xFF0000) >> 16)   /* 3rd addressing cycle */
#define ADDR_4th_CYCLE(ADDR)       (uint8_t)(((ADDR)& 0xFF000000) >> 24) /* 4th addressing cycle */

/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
#define NAND_OK   0
/* 宏定义: NAND_FAIL - NAND_FAIL 的宏定义。 */
#define NAND_FAIL 1

/* 宏定义: FREE_BLOCK - FREE_BLOCK 的宏定义。 */
#define FREE_BLOCK  (1 << 12 )
/* 宏定义: BAD_BLOCK - BAD_BLOCK 的宏定义。 */
#define BAD_BLOCK   (1 << 13 )
/* 宏定义: VALID_BLOCK - VALID_BLOCK 的宏定义。 */
#define VALID_BLOCK (1 << 14 )
/* 宏定义: USED_BLOCK - USED_BLOCK 的宏定义。 */
#define USED_BLOCK  (1 << 15 )

/*
		LUT[]�ĸ�ʽ��
		uint16_t usGoodBlockFirst;				 // ��1���ÿ�
		uint16_t usDataBlockCount;	             // ���������ݴ洢�Ŀ����, �ӵ�2���ÿ鿪ʼ
		uint16_t usBakBlockStart;				 // ���ݿ���ʼ���
		uint32_t usPhyBlockNo[ulDataBlockCount]; // ����������顣���ֽ���ǰ�����ֽ��ں�
*/
#define DATA_BLOCK_PERCENT		98	/* ���ݿ�ռ����Ч�����İٷֱ� */
#define LUT_FIRST_GOOD_BLOCK	0	/* LUT[] ��1����Ԫ���ڴ洢��1����Ч��� */
#define LUT_DATA_BLOCK_COUNT	1	/* LUT[] ��2����Ԫ���ڴ洢����Ч��Ÿ��� */
#define LUT_BAK_BLOCK_START		2	/* LUT[] ��3����Ԫ���ڱ�������ʼ��� */
#define LUT_GOOD_BLOCK_START	3	/* LUT[] ��4����Ԫ������������ʼ��� */


/* Private Structures---------------------------------------------------------*/
typedef struct __SPARE_AREA {
	uint16_t LogicalIndex;
	uint16_t DataStatus;
	uint16_t BlockStatus;
} SPARE_AREA;

typedef enum {
  WRITE_IDLE = 0,
  POST_WRITE,
  PRE_WRITE,
  WRITE_CLEANUP,
  WRITE_ONGOING
}WRITE_STATE;

typedef enum {
  OLD_BLOCK = 0,
  UNUSED_BLOCK
}BLOCK_STATE;

/* ONFI �ṹ (for H27U4G8F2DTR)  page 26 */
/* ��������__packed�ؼ��ֱ�ʾ�ṹ���Ա��������� */
//__PACKED typedef struct
typedef struct
{
	uint8_t Sign[4];		/* = "ONFI" */
	uint16_t Revision; 		/* Bit1 = 1 ��ʾ֧�� ONFI Ver 1.0 */
	uint16_t Features;		/* */
	uint16_t OptionalCommands;
	uint8_t Rsv1[22];

	/* Manufacturer information block */
	uint8_t Manufacturer[12];	/* ������ */
	uint8_t Model[20];	/* �ͺ� */
	uint8_t JEDEC_ID;	/* AD */
	uint16_t DateCode;
	uint8_t Rsv2[13];

	/* Memory organization block */
	uint32_t PageDataSize;
	uint16_t PageSpareSize;
	uint32_t PartialPageDataSize;
	uint16_t PartialPageSpareSize;
	uint32_t BlockSize;
	uint32_t LogicalUnitSize;
	uint8_t LogicalUnitNumber;
	uint8_t AddressCycles;
	uint8_t CellBits;
	uint16_t BadBlockMax;
	uint16_t BlockEndurance;
	uint8_t ValidBlocksBegin;	/* ��ǰ�汣֤��Ч�Ŀ���� */
	uint16_t BlockEndurance2;	/* Block endurance for guaranteed valid blocks */
	uint8_t  ProgramsPerPage;	/* Number of programs per page */
	uint8_t PartialProgram;
	uint8_t ECCcorrectBits;
	uint8_t InterleavedAddrBits;	/* �����ĵ�ַλ */
	uint8_t InterleavedOperaton;
	uint8_t Rsv3[13];

	/* Electrical parameters block */
	uint8_t PinCapactance;
	uint16_t TimingMode;
	uint16_t ProgramCacheTimingMode;
	uint16_t PageProgTime;
	uint16_t BlockEraseTime;
	uint16_t PageReadTime;
	uint16_t ChangeColumnSetupTime;
	uint8_t Rsv4[23];

	/* Vendor block */
	uint16_t VendorRevision;
	uint8_t VendorSpecific[88];
	uint16_t IntegritaCRC;
}PARAM_PAGE_T;

/* Private macro --------------------------------------------------------------*/
//#define WEAR_LEVELLING_SUPPORT		ĥ��ƽ��֧��
#define WEAR_DEPTH         10			/* ĥ����� */
#define PAGE_TO_WRITE      (Transfer_Length/512)

#define BAD_BALOK_TEST_CYCLE	2		/* �б𻵿��㷨���ظ���д����  */

/* NAND ��ͳ�� */
typedef struct
{
	uint32_t ChipID;
	char ChipName[16];
	uint32_t Bad;
	uint32_t Free;
	uint32_t Used;
}NAND_BLOCK_INFO_T;

/* Private variables ----------------------------------------------------------*/
/* Private function prototypes ------------------------------------------------*/
/* exported functions ---------------------------------------------------------*/
uint8_t NAND_Init(void);
/*
*********************************************************************************************************
*   函 数 名: NAND_Write
*   功能说明: NAND_Write 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t NAND_Write(uint32_t _ulMemAddr, uint32_t *_pWriteBuf, uint16_t _usSize);
/*
*********************************************************************************************************
*   函 数 名: NAND_Read
*   功能说明: NAND_Read 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t NAND_Read(uint32_t _ulMemAddr, uint32_t *_pReadBuf, uint16_t _usSize);
/*
*********************************************************************************************************
*   函 数 名: NAND_Format
*   功能说明: NAND_Format 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t NAND_Format(void);
/*
*********************************************************************************************************
*   函 数 名: NAND_DispBadBlockInfo
*   功能说明: NAND_DispBadBlockInfo 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void NAND_DispBadBlockInfo(void);
/*
*********************************************************************************************************
*   函 数 名: NAND_ScanBlock
*   功能说明: NAND_ScanBlock 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t NAND_ScanBlock(uint32_t _ulPageNo);
/*
*********************************************************************************************************
*   函 数 名: NAND_FormatCapacity
*   功能说明: NAND_FormatCapacity 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t NAND_FormatCapacity(void);
/*
*********************************************************************************************************
*   函 数 名: NAND_ReadID
*   功能说明: NAND_ReadID 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t NAND_ReadID(void);

/*
*********************************************************************************************************
*   函 数 名: NAND_DispPhyPageData
*   功能说明: NAND_DispPhyPageData 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void NAND_DispPhyPageData(uint32_t _uiPhyPageNo);
/*
*********************************************************************************************************
*   函 数 名: NAND_DispLogicPageData
*   功能说明: NAND_DispLogicPageData 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void NAND_DispLogicPageData(uint32_t _uiLogicPageNo);

/*
*********************************************************************************************************
*   函 数 名: NAND_WriteMultiSectors
*   功能说明: NAND_WriteMultiSectors 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t NAND_WriteMultiSectors(uint8_t *_pBuf, uint32_t _SectorNo, uint16_t _SectorSize, uint32_t _SectorCount);
/*
*********************************************************************************************************
*   函 数 名: NAND_ReadMultiSectors
*   功能说明: NAND_ReadMultiSectors 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t NAND_ReadMultiSectors(uint8_t *_pBuf, uint32_t _SectorNo, uint16_t _SectorSize, uint32_t _SectorCount);

/*
*********************************************************************************************************
*   函 数 名: NAND_ReadONFI
*   功能说明: NAND_ReadONFI 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void NAND_ReadONFI(uint8_t *_pBuf);
/*
*********************************************************************************************************
*   函 数 名: NAND_ReadParamPage
*   功能说明: NAND_ReadParamPage 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void NAND_ReadParamPage(PARAM_PAGE_T *_pData);
/*
*********************************************************************************************************
*   函 数 名: NAND_DispParamPage
*   功能说明: NAND_DispParamPage 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void NAND_DispParamPage(void);


/*
*********************************************************************************************************
*   函 数 名: NAND_ScanAllBadBlock
*   功能说明: NAND_ScanAllBadBlock 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void NAND_ScanAllBadBlock(void);
/*
*********************************************************************************************************
*   函 数 名: NAND_GetBlockInfo
*   功能说明: NAND_GetBlockInfo 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t NAND_GetBlockInfo(NAND_BLOCK_INFO_T *_pInfo);
/*
*********************************************************************************************************
*   函 数 名: NAND_MarkBadBlock
*   功能说明: NAND_MarkBadBlock 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void NAND_MarkBadBlock(uint32_t _ulBlockNo);

#endif /* __FSMC_NAND_H */

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
