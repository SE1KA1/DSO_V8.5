/**
 * @file bsp_fmc_nand_flash.c
 * @brief 安富莱 BSP 通用驱动（NAND Flash）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : NAND Flash����ģ��
*	�ļ����� : bsp_nand.c
*	��    �� : V1.3
*	˵    �� : �ṩNAND Flash (HY27UF081G2A�� 8bit 128K�ֽ� ��ҳ)�ĵײ�ӿں�������������ԭ����
*
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2013-02-01  armfly  ��ʽ����
*		V1.1    2014-05-02  armfly  ����512MB�ͺ�: H27U4G8F2DTR,
*					(1) ���� NAND_ReadONFI() ����;
*					(2) ���� NAND_ReadParamPage() ����;
*					(3) ��� NAND_IsFreeBlock(uint32_t _ulBlockNo) ������BUG��
*					(4) ��� FMC_NAND_PageCopyBack() �� FMC_NAND_PageCopyBackEx()�� A18������ͬ��BUG
*					(5) �޸� NAND_FindFreeBlock(), �����βΣ�����������ż�����
*					(6) �޸� ��ӡ����ĺ��������ӱ�ʶ 1 ��ʾ���ÿ� 0 ��ʾ���п�
*					(7) NAND_MarkUsedBlock() ���õĵط������ββ��ԡ� ulPhyPageNo / NAND_BLOCK_SIZE
*		V1.2	2015-07-23   armfly  ���Ӻ��� uint8_t NAND_GetBlockInfo(void); ����LCD��ʾ��
*		V1.3	2015-10-13   armfly  ��ֲ��STM32F429
*					(1) ���� WAIT_BUSY()��, ��Ҫ���������ȴ��ӳ�ʱ�䡣
*					(2) h�ļ��� #define BAD_BALOK_TEST_CYCLE	2	(3�޸�Ϊ2, ����2��ʧ�ܣ���ǻ���)
*
*	Copyright (C), 2015-2030, ���������� www.armfly.com
*
*********************************************************************************************************
*/
#include "bsp.h"

/* ������Դ�ӡ��䣬�����Ŵ� */
#define printf_err	printf
//#define printf_err(...)

//#define printf_ok	printf
#define printf_ok(...)

#define WRITE_PAGE_VERIFY_EN	/* д page ����ʱУ�� */

/*
	�����IAR��KEIL�ı༭�����Ķ����뽫�༭������������Ϊ�����壨9��/��ţ���������TAB����Ϊ4��
	���򣬷��򴦳��ֲ���������⡣

	�������Ƶĵط���
	��1���ڲ���NAND Flashʱ�����������һ����ѭ�������Ӳ�������쳣����������������
 		while( GPIO_ReadInputDataBit(GPIOG, GPIO_Pin_6) == 0 )

 	��2��û������ECCУ�鹦�ܡ�ECC���Լ��1����2��bit�������ֻ��1��bit����������޸����bit�����
 		����2��bit��������ܼ�ⲻ����

 	��3������д�ļ�����ʱ���ᵼ���ؽ�LUT��Ŀǰ���ؽ�LUT�Ĵ���ִ��Ч�ʻ������ߣ��д����ơ�

	��Ӳ��˵����
	���������������õ�NAND FlahsΪ����ʿ��HY27UF081G2A,  439�������Ϊ H27U4G8F2DTR
	��1��NAND Flash��Ƭѡ�ź����ӵ�CPU��FMC_NCE2���������NAND Flash�ĵ�ַ�ռ�Ϊ 0x70000000����CPU������
		�ֲ��FSMC�½�)
	��2����FSMC�������ж�������豸����TFT��SRAM��CH374T��NOR������˱���ȷ�����������豸��Ƭѡ���ڽ�ֹ
		״̬�����򽫳������߳�ͻ���� ���μ����ļ���ʼ��FSMC GPIO�ĺ�����


	��NAND Flash �ṹ���塿
     ��������16x4�ֽڣ�ÿpage 2048�ֽڣ�ÿ512�ֽ�һ��������ÿ��������Ӧ16�Լ��ı�������

	 ÿ��PAGE���߼��ṹ��ǰ��512Bx4����������������16Bx4�Ǳ�����
	��������������������������������������������������������������������������������������������������������������������������������
	�� Main area  ���� Main area  ���� Main area  ����Main area   ���� Spare area ���� Spare area ���� Spare area ����Spare area  ��
	��            ����            ����            ����            ����            ����            ����            ����            ��
	��   512B     ����    512B    ����    512B    ����    512B    ����    16B     ����     16B    ����     16B    ����    16B     ��
	��������������������������������������������������������������������������������������������������������������������������������

	 ÿ16B�ı��������߼��ṹ����:(�����Ƽ���׼��
	������������������������������������������������������������������������������������������������������������������������������������������������������������
	��  BI  ����RESER ����LSN0����LSN1����LSN2����RESER ����RESER ����RESER ����ECC0����ECC1����ECC2����ECC0����S-ECC1����S-ECC0����RESER ����RESER ����RESER ��
	��      ���� VED  ����    ����    ����    ���� VED  ���� VED  ���� VED  ����    ����    ����    ����    ����      ����      ���� VED  ���� VED  ���� VED  ��
	������������������������������������������������������������������������������������������������������������������������������������������������������������

	K9F1G08U0A �� HY27UF081G2A �Ǽ��ݵġ�оƬ����ʱ�����̱�֤оƬ�ĵ�1�����Ǻÿ顣����ǻ��飬���ڸÿ�ĵ�1��PAGE�ĵ�1���ֽ�
	���ߵ�2��PAGE������1��PAGE�����޷����Ϊ0xFFʱ���ĵ�1���ֽ�д���0xFFֵ��������ֵ������ģ�����ֱ���ж��Ƿ����0xFF���ɡ�

	ע�⣺������Щ����˵NAND Flash���̵�Ĭ�������ǽ������Ƕ��ڵ�1��PAGE�ĵ�6���ֽڴ������˵���Ǵ��󡣻������ڵ�6���ֽڽ���Բ���С������512�ֽڣ���NAND Flash
	���������е�NAND Flash���������׼������ڸ���NAND Flashʱ������ϸ�Ķ�оƬ�������ֲᡣ


	Ϊ�˱�����NAND Flash ����ֲFat�ļ�ϵͳ�����Ƕ�16B�ı������������·��䷽��:
	������������������������������������������������������������������������������������������������������������������������������������
	�� BI ����USED����LBN0����LBN1����ECC0����ECC1����ECC2����ECC3����ECC4����ECC5����S-ECC1����S-ECC0����RSVD����RSVD����RSVD����RSVD��
	��    ����    ����    ����    ����    ����    ����    ����    ����    ����    ����      ����      ����    ����    ����    ����    ��
	������������������������������������������������������������������������������������������������������������������������������������
    - BI : �����־(Bad Block Identifier)��ÿ��BLOCK�ĵ�1��PAGE���ߵ�2��PAGE�ĵ�1���ֽ�ָʾ�ÿ��Ƿ񻵿顣0xFF��ʾ�ÿ飬����0xFF��ʾ���顣
    - USED : �ÿ�ʹ�ñ�־��0xFF��ʾ���п飻0xF0��ʾ���ÿ顣
    - LBN0 LBN1 : �߼����(Logic Block No) ����0��ʼ���롣ֻ��ÿ��BLOCK�ĵ�1��PAGE��Ч������PAGE���ֶι̶�Ϊ0xFF FF
    - ECC0 ~ ECC6 : 512B����������ECCУ�� �����������ṩECC�㷨��256�ֽڶ�Ӧ3���ֽڵ�ECC)
    - S-ECC1 S-ECC0 : LSN0��LSN2��ECCУ��
    - RSVD : �����ֽڣ�Reserved

	��������� & ĥ��ƽ�⡿
	(1) �ڲ�ȫ������s_usLUT[]�����򱣴�������š�������������߼���ĵ�ַӳ�䡣
	(2) ��ʽ��ʱ����98%�ĺÿ����������ݴ洢��ʣ���2%���ڱ������������滻����
	(3) д������512B)ʱ�������������Ϊ�գ���ֱ��д�룬���ٲ���Ҫ�Ŀ������������Ч���NAND Flash�������Ͷ�д���ܡ�
	(4) д����ʱ������������ݲ�Ϊ�գ����ĩβ��ʼ����һ�����п��滻���ɿ飬�滻����д������ɺ󣬽��ɿ����������עΪ���У�֮���ؽ�LUT��
	(5) �鸴��ʱ���������NAND FlashӲ����Copy-Back���ܣ������Դҳ���ڴ���д��Ŀ��ҳ��������������߶�дЧ�ʡ�
	(6) ĥ��ƽ�⻹����ȱ�ݣ�Ч�����á�ECCУ����δʵ�֡�

*/

/* ����NAND Flash��������ַ���������Ӳ�������� */
#define Bank_NAND_ADDR     ((uint32_t)0x80000000)

/* �������NAND Flash�õ�3���� */
#define NAND_CMD_AREA		*(__IO uint8_t *)(Bank_NAND_ADDR | CMD_AREA)
/* 宏定义: NAND_ADDR_AREA - NAND_ADDR_AREA 的宏定义。 */
#define NAND_ADDR_AREA		*(__IO uint8_t *)(Bank_NAND_ADDR | ADDR_AREA)
/* 宏定义: NAND_DATA_AREA - NAND_DATA_AREA 的宏定义。 */
#define NAND_DATA_AREA		*(__IO uint8_t *)(Bank_NAND_ADDR | DATA_AREA)


/* 宏定义: BUSY_GPIO - BUSY_GPIO 的宏定义。 */
#define BUSY_GPIO	GPIOD
/* 宏定义: BUSY_PIN - BUSY_PIN 的宏定义。 */
#define BUSY_PIN	GPIO_PIN_6

/* ����ȴ���������������쳣, �˴�Ӧ���жϳ�ʱ*/
#define WAIT_BUSY()	{	\
	uint16_t k; 	\
	for (k = 0; k < 200; k++)	\
	{	\
		if ((BUSY_GPIO->IDR & BUSY_PIN) == 0) break;	\
	}	\
	for (k = 0; k < 2000; k++)	\
	{	\
		if ((BUSY_GPIO->IDR & BUSY_PIN) != 0) break;	\
	}	\
}

	
/* �߼����ӳ������ÿ�������2%���ڱ��������������ά������1024�� LUT = Look Up Table */
static uint16_t s_usLUT[NAND_BLOCK_COUNT];

static uint16_t s_usValidDataBlockCount;	/* ��Ч�����ݿ���� */

static uint8_t s_ucTempBuf[NAND_PAGE_TOTAL_SIZE];	/* �󻺳�����2112�ֽ�. ���ڶ����Ƚ� */

static uint8_t NAND_BuildLUT(void);
/*
*********************************************************************************************************
*   函 数 名: FMC_NAND_GetStatus
*   功能说明: FMC_NAND_GetStatus 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t FMC_NAND_GetStatus(void);
/*
*********************************************************************************************************
*   函 数 名: NAND_FindFreeBlock
*   功能说明: NAND_FindFreeBlock 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t NAND_FindFreeBlock (uint32_t _ulSrcPageNo);
/*
*********************************************************************************************************
*   函 数 名: NAND_MarkUsedBlock
*   功能说明: NAND_MarkUsedBlock 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t NAND_MarkUsedBlock(uint32_t _ulBlockNo);

/*
*********************************************************************************************************
*   函 数 名: NAND_AddrToPhyBlockNo
*   功能说明: NAND_AddrToPhyBlockNo 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t NAND_AddrToPhyBlockNo(uint32_t _ulMemAddr);
/*
*********************************************************************************************************
*   函 数 名: NAND_IsBufOk
*   功能说明: NAND_IsBufOk 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t NAND_IsBufOk(uint8_t *_pBuf, uint32_t _ulLen, uint8_t _ucValue);
/*
*********************************************************************************************************
*   函 数 名: NAND_WriteToNewBlock
*   功能说明: NAND_WriteToNewBlock 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t NAND_WriteToNewBlock(uint32_t _ulPhyPageNo, uint8_t *_pWriteBuf, uint16_t _usOffset, uint16_t _usSize);
/*
*********************************************************************************************************
*   函 数 名: NAND_IsFreeBlock
*   功能说明: NAND_IsFreeBlock 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t NAND_IsFreeBlock(uint32_t _ulBlockNo);

/*
*********************************************************************************************************
*   函 数 名: NAND_LBNtoPBN
*   功能说明: NAND_LBNtoPBN 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint16_t NAND_LBNtoPBN(uint32_t _uiLBN);

/*
*********************************************************************************************************
*   函 数 名: FMC_NAND_ReadPage
*   功能说明: FMC_NAND_ReadPage 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t FMC_NAND_ReadPage(uint8_t *_pBuffer, uint32_t _ulPageNo, uint16_t _usAddrInPage, uint16_t _usByteCount);
/*
*********************************************************************************************************
*   函 数 名: FMC_NAND_WritePage
*   功能说明: FMC_NAND_WritePage 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t FMC_NAND_WritePage(uint8_t *_pBuffer, uint32_t _ulPageNo, uint16_t _usAddrInPage, uint16_t _usByteCount);
/*
*********************************************************************************************************
*   函 数 名: FMC_NAND_CompPage
*   功能说明: FMC_NAND_CompPage 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t FMC_NAND_CompPage(uint8_t *_pBuffer, uint32_t _ulPageNo, uint16_t _usAddrInPage, uint16_t _usByteCount);

/*
*********************************************************************************************************
*   函 数 名: FMC_NAND_EraseBlock
*   功能说明: FMC_NAND_EraseBlock 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t FMC_NAND_EraseBlock(uint32_t _ulBlockNo);
/*
*********************************************************************************************************
*   函 数 名: bsp_FMC_NAND_Init
*   功能说明: bsp_FMC_NAND_Init 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void bsp_FMC_NAND_Init(void);
/*
*********************************************************************************************************
*   函 数 名: FMC_NAND_Reset
*   功能说明: FMC_NAND_Reset 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t FMC_NAND_Reset(void);

/*
*********************************************************************************************************
*	�� �� ��: NAND_Init
*	����˵��: ��ʼ��NAND Flash�ӿ�
*	��    ��:  ��
*	�� �� ֵ: ִ�н����
*				- NAND_FAIL ��ʾʧ��
*				- NAND_OK ��ʾ�ɹ�
*********************************************************************************************************
*/
uint8_t NAND_Init(void)
{
	uint8_t Status;

	bsp_FMC_NAND_Init();		/* ����FSMC��GPIO����NAND Flash�ӿ� */

	FMC_NAND_Reset();			/* ͨ����λ���λNAND Flash����״̬ */

	Status = NAND_BuildLUT();	/* ����������� LUT = Look up table */
	return Status;
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_FMC_NAND_Init
*	����˵��: ����FSMC��GPIO����NAND Flash�ӿڡ�������������ڶ�дnand flashǰ������һ�Ρ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void bsp_FMC_NAND_Init(void)
{
	/* --NAND Flash GPIOs ����  ------*/
	{
		/*
			PD0/FMC_D2
			PD1/FMC_D3
			PD4/FMC_NOE
			PD5/FMC_NWE
		
				PD7/FMC_NCE2  --�滻Ϊ PG9/FMC_NCE3
		
			PD11/FMC_A16/FMC_CLE
			PD12/FMC_A17/FMC_ALE
			PD14/FMC_D0
			PD15/FMC_D1

			PE7/FMC_D4
			PE8/FMC_D5
			PE9/FMC_D6
			PE10/FMC_D7

			PD6/FMC_NWAIT	(�������ò�ѯ��ʽ��æ���˿�����Ϊ��ͨGPIO���빦��ʹ��)
		*/
		GPIO_InitTypeDef gpio_init_structure;

		/* ʹ�� GPIOʱ�� */
		__HAL_RCC_GPIOD_CLK_ENABLE();
		__HAL_RCC_GPIOE_CLK_ENABLE();

		/* ʹ��FMCʱ�� */
		__HAL_RCC_FMC_CLK_ENABLE();

		/* ���� GPIOD ��ص�IOΪ����������� */
		gpio_init_structure.Mode = GPIO_MODE_AF_PP;
		gpio_init_structure.Pull = GPIO_PULLUP;
		gpio_init_structure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
		gpio_init_structure.Alternate = GPIO_AF12_FMC;
		
		/* ����GPIOD */
		gpio_init_structure.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 |
									GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_14 |
									GPIO_PIN_15;
		HAL_GPIO_Init(GPIOD, &gpio_init_structure);

		/* ����GPIOE */
		gpio_init_structure.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
		HAL_GPIO_Init(GPIOE, &gpio_init_structure);
		
		/* ����GPIOG NAND Ƭѡ */
		gpio_init_structure.Pin = GPIO_PIN_9;
		HAL_GPIO_Init(GPIOG, &gpio_init_structure);		

		/* busy R/B ����Ϊ���� */
		gpio_init_structure.Mode = GPIO_MODE_INPUT;			/* �������� */
		gpio_init_structure.Pull = GPIO_NOPULL;				/* ���������費ʹ�� */
		gpio_init_structure.Speed = GPIO_SPEED_FREQ_HIGH;  	/* GPIO�ٶȵȼ� */
		gpio_init_structure.Pin = BUSY_PIN;	
		HAL_GPIO_Init(BUSY_GPIO, &gpio_init_structure);	
	}

	{
		NAND_HandleTypeDef hNand = {0};
		
		FMC_NAND_PCC_TimingTypeDef CommonSpaceTiming;
		FMC_NAND_PCC_TimingTypeDef AttributeSpaceTiming;
		
		hNand.Instance = FMC_NAND_DEVICE;									
		hNand.Init.NandBank = FMC_NAND_BANK3;						/* ����FSMC NAND BANK �� */
		hNand.Init.Waitfeature = FMC_NAND_PCC_WAIT_FEATURE_DISABLE;	/* ����ȴ�ʱ��ʹ��, ��ֹ */
		hNand.Init.MemoryDataWidth = FMC_NAND_PCC_MEM_BUS_WIDTH_8;	/* ���ݿ��� 8bit */
		hNand.Init.EccComputation = FMC_NAND_ECC_DISABLE;			/* ECC������;������� - ��ֹ */
		hNand.Init.ECCPageSize = FMC_NAND_ECC_PAGE_SIZE_2048BYTE;	/* ECC ҳ���С */
		hNand.Init.TCLRSetupTime = 2;								/* CLE�ͺ�RE��֮����ӳ٣�HCLK������ */	
		hNand.Init.TARSetupTime = 2;                                 /* ALE�ͺ�RE��֮����ӳ٣�HCLK������ */
   
		/* 2-5-3-1 V6 OK   4-10-6-2 �쳣 */
		CommonSpaceTiming.SetupTime = 10;
		CommonSpaceTiming.WaitSetupTime = 10;
		CommonSpaceTiming.HoldSetupTime = 10;
		CommonSpaceTiming.HiZSetupTime = 10;
    
		AttributeSpaceTiming.SetupTime = 10;
		AttributeSpaceTiming.WaitSetupTime = 10;
		AttributeSpaceTiming.HoldSetupTime = 10;
		AttributeSpaceTiming.HiZSetupTime = 10;

		HAL_NAND_Init(&hNand, &CommonSpaceTiming, &AttributeSpaceTiming); 
	}
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_ReadID
*	����˵��: ��NAND Flash��ID��ID�洢���β�ָ���Ľṹ������С�
*	��    ��:  ��
*	�� �� ֵ: 32bit��NAND Flash ID
*********************************************************************************************************
*/
uint32_t NAND_ReadID(void)
{
	uint32_t data = 0;

	/* �������� Command to the command area */
	NAND_CMD_AREA = 0x90;
	NAND_ADDR_AREA = 0x00;

	/* ˳���ȡNAND Flash��ID */
	data = *(__IO uint32_t *)(Bank_NAND_ADDR | DATA_AREA);
	data =  ((data << 24) & 0xFF000000) |
			((data << 8 ) & 0x00FF0000) |
			((data >> 8 ) & 0x0000FF00) |
			((data >> 24) & 0x000000FF) ;
	return data;
}

/*
*********************************************************************************************************
*	�� �� ��: FMC_NAND_PageCopyBack
*	����˵��: ��һҳ���ݸ��Ƶ�����һ��ҳ��Դҳ��Ŀ��ҳ���ڵ�block����ͬΪż����ͬΪ��������A18������ͬ��
*	��    ��:  - _ulSrcPageNo: Դҳ��
*             - _ulTarPageNo: Ŀ��ҳ��
*	�� �� ֵ: ִ�н����
*				- NAND_FAIL ��ʾʧ��
*				- NAND_OK ��ʾ�ɹ�
*
*	˵    ���������ֲ��Ƽ�����ҳ����֮ǰ����У��Դҳ��λУ�飬������ܻ����λ���󡣱�����δʵ�֡�
*
*********************************************************************************************************
*/
static uint8_t FMC_NAND_PageCopyBack(uint32_t _ulSrcPageNo, uint32_t _ulTarPageNo)
{
	if ((_ulSrcPageNo & 0x40) != (_ulTarPageNo & 0x40))
	{
		printf_err("Error : FMC_NAND_PageCopyBackEx(src=%d, tar=%d) \r\n", _ulSrcPageNo, _ulTarPageNo);
		return NAND_FAIL;
	}

	NAND_CMD_AREA = NAND_CMD_COPYBACK_A;

	/* ����Դҳ��ַ �� ���� HY27UF081G2A
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		��1�ֽڣ� A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr ��bit7 - bit0)
		��2�ֽڣ� 0    0    0    0    A11  A10  A9   A8		(_usPageAddr ��bit11 - bit8, ��4bit������0)
		��3�ֽڣ� A19  A18  A17  A16  A15  A14  A13  A12
		��4�ֽڣ� A27  A26  A25  A24  A23  A22  A21  A20

		H27U4G8F2DTR (512MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		��1�ֽڣ� A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr ��bit7 - bit0)
		��2�ֽڣ� 0    0    0    0    A11  A10  A9   A8		(_usPageAddr ��bit11 - bit8, ��4bit������0)
		��3�ֽڣ� A19  A18  A17  A16  A15  A14  A13  A12
		��4�ֽڣ� A27  A26  A25  A24  A23  A22  A21  A20
		��5�ֽڣ� A28  A29  A30  A31  0    0    0    0
	*/
	NAND_ADDR_AREA = 0;
	NAND_ADDR_AREA = 0;
	NAND_ADDR_AREA = _ulSrcPageNo;
	NAND_ADDR_AREA = (_ulSrcPageNo & 0xFF00) >> 8;

	#if NAND_ADDR_5 == 1
		NAND_ADDR_AREA = (_ulSrcPageNo & 0xFF0000) >> 16;
	#endif

	NAND_CMD_AREA = NAND_CMD_COPYBACK_B;

	/* ����ȴ���������������쳣, �˴�Ӧ���жϳ�ʱ */
	WAIT_BUSY();

	NAND_CMD_AREA = NAND_CMD_COPYBACK_C;

	/* ����Ŀ��ҳ��ַ �� ���� HY27UF081G2A
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		��1�ֽڣ� A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr ��bit7 - bit0)
		��2�ֽڣ� 0    0    0    0    A11  A10  A9   A8		(_usPageAddr ��bit11 - bit8, ��4bit������0)
		��3�ֽڣ� A19  A18  A17  A16  A15  A14  A13  A12
		��4�ֽڣ� A27  A26  A25  A24  A23  A22  A21  A20

		H27U4G8F2DTR (512MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		��1�ֽڣ� A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr ��bit7 - bit0)
		��2�ֽڣ� 0    0    0    0    A11  A10  A9   A8		(_usPageAddr ��bit11 - bit8, ��4bit������0)
		��3�ֽڣ� A19  A18  A17  A16  A15  A14  A13  A12    --- A18 ��plane��ַ
		��4�ֽڣ� A27  A26  A25  A24  A23  A22  A21  A20
		��5�ֽڣ� A28  A29  A30  A31  0    0    0    0

		Source and Destination page in the copy back program sequence must belong to the same device plane ��A18��
		Դ��ַ��Ŀ���ַ�� A18������ͬ
	*/
	NAND_ADDR_AREA = 0;
	NAND_ADDR_AREA = 0;
	NAND_ADDR_AREA = _ulTarPageNo;
	NAND_ADDR_AREA = (_ulTarPageNo & 0xFF00) >> 8;

	#if NAND_ADDR_5 == 1
		NAND_ADDR_AREA = (_ulTarPageNo & 0xFF0000) >> 16;
	#endif

	NAND_CMD_AREA = NAND_CMD_COPYBACK_D;

	/* ������״̬ */
	if (FMC_NAND_GetStatus() == NAND_READY)
	{
		return NAND_OK;
	}

	printf_err("Error: FMC_NAND_PageCopyBack(%d, %d)\r\n", _ulSrcPageNo, _ulTarPageNo);
	return NAND_FAIL;
}

/*
*********************************************************************************************************
*	�� �� ��: FMC_NAND_PageCopyBackEx
*	����˵��: ��һҳ���ݸ��Ƶ�����һ��ҳ,������Ŀ��ҳ�еĲ������ݡ�Դҳ��Ŀ��ҳ���ڵ�BLOCK����ͬΪż����ͬΪ������
*	��    ��:  - _ulSrcPageNo: Դҳ��
*             - _ulTarPageNo: Ŀ��ҳ��
*			  - _usOffset: ҳ��ƫ�Ƶ�ַ��pBuf�����ݽ�д�������ַ��ʼ��Ԫ
*			  - _pBuf: ���ݻ�����
*			  - _usSize: ���ݴ�С
*	�� �� ֵ: ִ�н����
*				- NAND_FAIL ��ʾʧ��
*				- NAND_OK ��ʾ�ɹ�
*
*	˵    ���������ֲ��Ƽ�����ҳ����֮ǰ����У��Դҳ��λУ�飬������ܻ����λ���󡣱�����δʵ�֡�
*
*********************************************************************************************************
*/
static uint8_t FMC_NAND_PageCopyBackEx(uint32_t _ulSrcPageNo, uint32_t _ulTarPageNo, uint8_t *_pBuf, 
	uint16_t _usOffset, uint16_t _usSize)
{
	uint16_t i;

	if ((_ulSrcPageNo & 0x40) != (_ulTarPageNo & 0x40))
	{
		printf_err("Error A18 not same:  FMC_NAND_PageCopyBackEx(src=%d, tar=%d) \r\n", _ulSrcPageNo, _ulTarPageNo);
		return NAND_FAIL;
	}

	NAND_CMD_AREA = NAND_CMD_COPYBACK_A;

	/* ����Դҳ��ַ �� ���� HY27UF081G2A
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		��1�ֽڣ� A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr ��bit7 - bit0)
		��2�ֽڣ� 0    0    0    0    A11  A10  A9   A8		(_usPageAddr ��bit11 - bit8, ��4bit������0)
		��3�ֽڣ� A19  A18  A17  A16  A15  A14  A13  A12
		��4�ֽڣ� A27  A26  A25  A24  A23  A22  A21  A20

		H27U4G8F2DTR (512MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		��1�ֽڣ� A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr ��bit7 - bit0)
		��2�ֽڣ� 0    0    0    0    A11  A10  A9   A8		(_usPageAddr ��bit11 - bit8, ��4bit������0)
		��3�ֽڣ� A19  A18  A17  A16  A15  A14  A13  A12
		��4�ֽڣ� A27  A26  A25  A24  A23  A22  A21  A20
		��5�ֽڣ� A28  A29  A30  A31  0    0    0    0
	*/
	NAND_ADDR_AREA = 0;
	NAND_ADDR_AREA = 0;
	NAND_ADDR_AREA = _ulSrcPageNo;
	NAND_ADDR_AREA = (_ulSrcPageNo & 0xFF00) >> 8;

	#if NAND_ADDR_5 == 1
		NAND_ADDR_AREA = (_ulSrcPageNo & 0xFF0000) >> 16;
	#endif

	NAND_CMD_AREA = NAND_CMD_COPYBACK_B;

	/* ����ȴ���������������쳣, �˴�Ӧ���жϳ�ʱ */
	WAIT_BUSY();

	NAND_CMD_AREA = NAND_CMD_COPYBACK_C;

	/* ����Ŀ��ҳ��ַ �� ���� HY27UF081G2A
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		��1�ֽڣ� A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr ��bit7 - bit0)
		��2�ֽڣ� 0    0    0    0    A11  A10  A9   A8		(_usPageAddr ��bit11 - bit8, ��4bit������0)
		��3�ֽڣ� A19  A18  A17  A16  A15  A14  A13  A12
		��4�ֽڣ� A27  A26  A25  A24  A23  A22  A21  A20

		H27U4G8F2DTR (512MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		��1�ֽڣ� A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr ��bit7 - bit0)
		��2�ֽڣ� 0    0    0    0    A11  A10  A9   A8		(_usPageAddr ��bit11 - bit8, ��4bit������0)
		��3�ֽڣ� A19  A18  A17  A16  A15  A14  A13  A12
		��4�ֽڣ� A27  A26  A25  A24  A23  A22  A21  A20
		��5�ֽڣ� A28  A29  A30  A31  0    0    0    0
	*/
	NAND_ADDR_AREA = 0;
	NAND_ADDR_AREA = 0;
	NAND_ADDR_AREA = _ulTarPageNo;
	NAND_ADDR_AREA = (_ulTarPageNo & 0xFF00) >> 8;

	#if NAND_ADDR_5 == 1
		NAND_ADDR_AREA = (_ulTarPageNo & 0xFF0000) >> 16;
	#endif

	/* �м����������, Ҳ����ȴ� */

	NAND_CMD_AREA = NAND_CMD_COPYBACK_C;

	NAND_ADDR_AREA = _usOffset;
	NAND_ADDR_AREA = _usOffset >> 8;

	/* �������� */
	for(i = 0; i < _usSize; i++)
	{
		NAND_DATA_AREA = _pBuf[i];
	}

	NAND_CMD_AREA = NAND_CMD_COPYBACK_D;

	/* ������״̬ */
	if (FMC_NAND_GetStatus() == NAND_READY)
	{
		return NAND_OK;
	}

	printf_err("Error: FMC_NAND_PageCopyBackEx(src=%d, tar=%d, offset=%d, size=%d)\r\n",
			_ulSrcPageNo, _ulTarPageNo, _usOffset, _usSize);
	return NAND_FAIL;
}

/*
*********************************************************************************************************
*	�� �� ��: FMC_NAND_WritePage
*	����˵��: дһ��������NandFlashָ��ҳ���ָ��λ�ã�д������ݳ��Ȳ�����һҳ�Ĵ�С��
*	��    ��:  - _pBuffer: ָ�������д���ݵĻ�����
*             - _ulPageNo: ҳ�ţ����е�ҳͳһ���룬��ΧΪ��0 - 65535
*			  - _usAddrInPage : ҳ�ڵ�ַ����ΧΪ��0-2111
*             - _usByteCount: д����ֽڸ���
*	�� �� ֵ: ִ�н����
*				- NAND_FAIL ��ʾʧ��
*				- NAND_OK ��ʾ�ɹ�
*********************************************************************************************************
*/
static uint8_t FMC_NAND_WritePage(uint8_t *_pBuffer, uint32_t _ulPageNo, uint16_t _usAddrInPage, uint16_t _usByteCount)
{
	uint16_t i;

	/* ����ҳд���� */
	NAND_CMD_AREA = NAND_CMD_WRITE0;

	/* ����ҳ�ڵ�ַ �� ���� HY27UF081G2A
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		��1�ֽڣ� A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr ��bit7 - bit0)
		��2�ֽڣ� 0    0    0    0    A11  A10  A9   A8		(_usPageAddr ��bit11 - bit8, ��4bit������0)
		��3�ֽڣ� A19  A18  A17  A16  A15  A14  A13  A12
		��4�ֽڣ� A27  A26  A25  A24  A23  A22  A21  A20

		H27U4G8F2DTR (512MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		��1�ֽڣ� A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr ��bit7 - bit0)
		��2�ֽڣ� 0    0    0    0    A11  A10  A9   A8		(_usPageAddr ��bit11 - bit8, ��4bit������0)
		��3�ֽڣ� A19  A18  A17  A16  A15  A14  A13  A12
		��4�ֽڣ� A27  A26  A25  A24  A23  A22  A21  A20
		��5�ֽڣ� A28  A29  A30  A31  0    0    0    0
	*/
	NAND_ADDR_AREA = _usAddrInPage;
	NAND_ADDR_AREA = _usAddrInPage >> 8;
	NAND_ADDR_AREA = _ulPageNo;
	NAND_ADDR_AREA = (_ulPageNo & 0xFF00) >> 8;

	#if NAND_ADDR_5 == 1
		NAND_ADDR_AREA = (_ulPageNo & 0xFF0000) >> 16;
	#endif

	/* tADL = 100ns,  Address to Data Loading */
	for (i = 0; i < 20; i++);	/* ��Ҫ���� 100ns */

	/* д���� */
	for(i = 0; i < _usByteCount; i++)
	{
		NAND_DATA_AREA = _pBuffer[i];
	}
	NAND_CMD_AREA = NAND_CMD_WRITE_TRUE1;

	/* WE High to Busy , 100ns */
	WAIT_BUSY();
	
	/* ������״̬ */
	if (FMC_NAND_GetStatus() == NAND_READY)
	{
			/* �������ݽ���У�� */
		#ifdef WRITE_PAGE_VERIFY_EN
			FMC_NAND_ReadPage (s_ucTempBuf, _ulPageNo, _usAddrInPage, _usByteCount);
			if (memcmp(s_ucTempBuf,  _pBuffer, _usByteCount) != 0)
			{
				printf_err("Error1: FMC_NAND_WritePage(page=%d, addr=%d, count=%d)\r\n",
					_ulPageNo, _usAddrInPage, _usByteCount);				
				return NAND_FAIL;	
			}			
		#endif
		
		return NAND_OK;
	}

	printf_err("Error2: FMC_NAND_WritePage(page=%d, addr=%d, count=%d)\r\n",
		_ulPageNo, _usAddrInPage, _usByteCount);
	
	return NAND_FAIL;
}

/*
*********************************************************************************************************
*	�� �� ��: FMC_NAND_ReadPage
*	����˵��: ��NandFlashָ��ҳ���ָ��λ�ö�һ�����ݣ����������ݳ��Ȳ�����һҳ�Ĵ�С��
*	��    ��:  - _pBuffer: ָ�������д���ݵĻ�����
*             - _ulPageNo: ҳ�ţ����е�ҳͳһ���룬��ΧΪ��0 - 65535
*			  - _usAddrInPage : ҳ�ڵ�ַ����ΧΪ��0-2111
*             - _usByteCount: �ֽڸ�����  ����� 2048 + 64��
*	�� �� ֵ: ִ�н����
*				- NAND_FAIL ��ʾʧ��
*				- NAND_OK ��ʾ�ɹ�
*********************************************************************************************************
*/
static uint8_t FMC_NAND_ReadPage(uint8_t *_pBuffer, uint32_t _ulPageNo, uint16_t _usAddrInPage, uint16_t _usByteCount)
{
	uint16_t i;

    /* ����ҳ������� */
    NAND_CMD_AREA = NAND_CMD_AREA_A;

	/* ����ҳ�ڵ�ַ �� ���� HY27UF081G2A  (128MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		��1�ֽڣ� A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr ��bit7 - bit0)
		��2�ֽڣ� 0    0    0    0    A11  A10  A9   A8		(_usPageAddr ��bit11 - bit8, ��4bit������0)
		��3�ֽڣ� A19  A18  A17  A16  A15  A14  A13  A12
		��4�ֽڣ� A27  A26  A25  A24  A23  A22  A21  A20

		H27U4G8F2DTR (512MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		��1�ֽڣ� A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr ��bit7 - bit0)
		��2�ֽڣ� 0    0    0    0    A11  A10  A9   A8		(_usPageAddr ��bit11 - bit8, ��4bit������0)
		��3�ֽڣ� A19  A18  A17  A16  A15  A14  A13  A12
		��4�ֽڣ� A27  A26  A25  A24  A23  A22  A21  A20
		��5�ֽڣ� A28  A29  A30  A31  0    0    0    0
	*/
	NAND_ADDR_AREA = _usAddrInPage;
	NAND_ADDR_AREA = _usAddrInPage >> 8;
	NAND_ADDR_AREA = _ulPageNo;
	NAND_ADDR_AREA = (_ulPageNo & 0xFF00) >> 8;

	#if NAND_ADDR_5 == 1
		NAND_ADDR_AREA = (_ulPageNo & 0xFF0000) >> 16;
	#endif

	NAND_CMD_AREA = NAND_CMD_AREA_TRUE1;

	 /* ����ȴ���������������쳣, �˴�Ӧ���жϳ�ʱ */
	WAIT_BUSY();

	/* �����ݵ�������pBuffer */
	for(i = 0; i < _usByteCount; i++)
	{
		/* 
			����429��LTDC��ʾ�����ᶨʱ����SDRAM����˷��� NAND Flashʱ����ܿ�LTDC����SDRAM���������߳�ͻ��
			���µĽ��������ֵ���ʧЧ��
			����Ĵ������������׵������������ʾNANDͼƬʱ�������Ͽ�����������.					
		*/
		//while (LTDC->CDSR);
		
		_pBuffer[i] = NAND_DATA_AREA;
	}

	return NAND_OK;
}

/*
*********************************************************************************************************
*	�� �� ��: FMC_NAND_CompPage
*	����˵��: �Ƚ�����
*	��    ��:  - _pBuffer: ָ��������Ƚϵ����ݻ�����
*             - _ulPageNo: ҳ�ţ����е�ҳͳһ���룬��ΧΪ��0 - 65535
*			  - _usAddrInPage : ҳ�ڵ�ַ����ΧΪ��0-2111
*             - _usByteCount: �ֽڸ�����  ����� 2048 + 64��
*	�� �� ֵ: ִ�н����
*				- NAND_FAIL ��ʾʧ��
*				- NAND_OK ��ʾ�ɹ��� ���
*********************************************************************************************************
*/
static uint8_t FMC_NAND_CompPage(uint8_t *_pBuffer, uint32_t _ulPageNo, uint16_t _usAddrInPage, uint16_t _usByteCount)
{
	uint16_t i;

    /* ����ҳ������� */
    NAND_CMD_AREA = NAND_CMD_AREA_A;

	/* ����ҳ�ڵ�ַ �� ���� HY27UF081G2A  (128MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		��1�ֽڣ� A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr ��bit7 - bit0)
		��2�ֽڣ� 0    0    0    0    A11  A10  A9   A8		(_usPageAddr ��bit11 - bit8, ��4bit������0)
		��3�ֽڣ� A19  A18  A17  A16  A15  A14  A13  A12
		��4�ֽڣ� A27  A26  A25  A24  A23  A22  A21  A20

		H27U4G8F2DTR (512MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		��1�ֽڣ� A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr ��bit7 - bit0)
		��2�ֽڣ� 0    0    0    0    A11  A10  A9   A8		(_usPageAddr ��bit11 - bit8, ��4bit������0)
		��3�ֽڣ� A19  A18  A17  A16  A15  A14  A13  A12
		��4�ֽڣ� A27  A26  A25  A24  A23  A22  A21  A20
		��5�ֽڣ� A28  A29  A30  A31  0    0    0    0
	*/
	NAND_ADDR_AREA = _usAddrInPage;
	NAND_ADDR_AREA = _usAddrInPage >> 8;
	NAND_ADDR_AREA = _ulPageNo;
	NAND_ADDR_AREA = (_ulPageNo & 0xFF00) >> 8;

	#if NAND_ADDR_5 == 1
		NAND_ADDR_AREA = (_ulPageNo & 0xFF0000) >> 16;
	#endif

	NAND_CMD_AREA = NAND_CMD_AREA_TRUE1;

	 /* ����ȴ���������������쳣, �˴�Ӧ���жϳ�ʱ */
	WAIT_BUSY();

	/* �����ݵ�������pBuffer */
	for(i = 0; i < _usByteCount; i++)
	{
		//while (LTDC->CDSR);
		
		if (_pBuffer[i] != NAND_DATA_AREA)
		{
			return NAND_FAIL;
		}
	}

	return NAND_OK;
}

/*
*********************************************************************************************************
*	�� �� ��: FMC_NAND_WriteSpare
*	����˵��: ��1��PAGE��Spare��д������
*	��    ��:  - _pBuffer: ָ�������д���ݵĻ�����
*             - _ulPageNo: ҳ�ţ����е�ҳͳһ���룬��ΧΪ��0 - 65535
*			  - _usAddrInSpare : ҳ�ڱ�������ƫ�Ƶ�ַ����ΧΪ��0-63
*             - _usByteCount: д����ֽڸ���
*	�� �� ֵ: ִ�н����
*				- NAND_FAIL ��ʾʧ��
*				- NAND_OK ��ʾ�ɹ�
*********************************************************************************************************
*/
static uint8_t FMC_NAND_WriteSpare(uint8_t *_pBuffer, uint32_t _ulPageNo, uint16_t _usAddrInSpare, uint16_t _usByteCount)
{
	if (_usByteCount > NAND_SPARE_AREA_SIZE)
	{
		printf_err("Error: FMC_NAND_WriteSpare() %d\r\n",_usByteCount);
		return NAND_FAIL;
	}

	return FMC_NAND_WritePage(_pBuffer, _ulPageNo, NAND_PAGE_SIZE + _usAddrInSpare, _usByteCount);
}

/*
*********************************************************************************************************
*	�� �� ��: FMC_NAND_ReadSpare
*	����˵��: ��1��PAGE��Spare��������
*	��    ��:  - _pBuffer: ָ�������д���ݵĻ�����
*             - _ulPageNo: ҳ�ţ����е�ҳͳһ���룬��ΧΪ��0 - 65535
*			  - _usAddrInSpare : ҳ�ڱ�������ƫ�Ƶ�ַ����ΧΪ��0-63
*             - _usByteCount: д����ֽڸ���
*	�� �� ֵ: ִ�н����
*				- NAND_FAIL ��ʾʧ��
*				- NAND_OK ��ʾ�ɹ�
*********************************************************************************************************
*/
static uint8_t FMC_NAND_ReadSpare(uint8_t *_pBuffer, uint32_t _ulPageNo, uint16_t _usAddrInSpare, uint16_t _usByteCount)
{
	if (_usByteCount > NAND_SPARE_AREA_SIZE)
	{
		printf_err("Error: FMC_NAND_ReadSpare() %d\r\n",_usByteCount);
		return NAND_FAIL;
	}

	return FMC_NAND_ReadPage(_pBuffer, _ulPageNo, NAND_PAGE_SIZE + _usAddrInSpare, _usByteCount);
}

/*
*********************************************************************************************************
*	�� �� ��: FMC_NAND_WriteData
*	����˵��: ��1��PAGE����������д������
*	��    ��:  - _pBuffer: ָ�������д���ݵĻ�����
*             - _ulPageNo: ҳ�ţ����е�ҳͳһ���룬��ΧΪ��0 - 65535
*			  - _usAddrInPage : ҳ����������ƫ�Ƶ�ַ����ΧΪ��0-2047
*             - _usByteCount: д����ֽڸ���
*	�� �� ֵ: ִ�н����
*				- NAND_FAIL ��ʾʧ��
*				- NAND_OK ��ʾ�ɹ�
*********************************************************************************************************
*/
static uint8_t FMC_NAND_WriteData(uint8_t *_pBuffer, uint32_t _ulPageNo, uint16_t _usAddrInPage, uint16_t _usByteCount)
{
	if (_usByteCount > NAND_PAGE_SIZE)
	{
		printf_err("Error: FMC_NAND_WriteData() %d\r\n",_usByteCount);
		return NAND_FAIL;
	}

	return FMC_NAND_WritePage(_pBuffer, _ulPageNo, _usAddrInPage, _usByteCount);
}

/*
*********************************************************************************************************
*	�� �� ��: FMC_NAND_ReadData
*	����˵��: ��1��PAGE�������ݵ�����
*	��    ��:  - _pBuffer: ָ�������д���ݵĻ�����
*             - _ulPageNo: ҳ�ţ����е�ҳͳһ���룬��ΧΪ��0 - 65535
*			  - _usAddrInPage : ҳ����������ƫ�Ƶ�ַ����ΧΪ��0-2047
*             - _usByteCount: д����ֽڸ���
*	�� �� ֵ: ִ�н����
*				- NAND_FAIL ��ʾʧ��
*				- NAND_OK ��ʾ�ɹ�
*********************************************************************************************************
*/
static uint8_t FMC_NAND_ReadData(uint8_t *_pBuffer, uint32_t _ulPageNo, uint16_t _usAddrInPage, uint16_t _usByteCount)
{
	if (_usByteCount > NAND_PAGE_SIZE)
	{
		printf_err("Error: FMC_NAND_ReadData() %d\r\n",_usByteCount);
		return NAND_FAIL;
	}

	return FMC_NAND_ReadPage(_pBuffer, _ulPageNo, _usAddrInPage, _usByteCount);
}

/*
*********************************************************************************************************
*	�� �� ��: FMC_NAND_EraseBlock
*	����˵��: ����NAND Flashһ���飨block��
*	��    ��:  - _ulBlockNo: ��ţ���ΧΪ��0 - 1023,   0-4095
*	�� �� ֵ: NAND����״̬�������¼���ֵ��
*             - NAND_TIMEOUT_ERROR  : ��ʱ����
*             - NAND_READY          : �����ɹ�
*********************************************************************************************************
*/
static uint32_t FMC_NAND_EraseBlock(uint32_t _ulBlockNo)
{
	/* HY27UF081G2A  (128MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		��1�ֽڣ� A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr ��bit7 - bit0)
		��2�ֽڣ� 0    0    0    0    A11  A10  A9   A8		(_usPageAddr ��bit11 - bit8, ��4bit������0)
		��3�ֽڣ� A19  A18  A17  A16  A15  A14  A13  A12    A18�����ǿ��
		��4�ֽڣ� A27  A26  A25  A24  A23  A22  A21  A20

		H27U4G8F2DTR (512MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		��1�ֽڣ� A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr ��bit7 - bit0)
		��2�ֽڣ� 0    0    0    0    A11  A10  A9   A8		(_usPageAddr ��bit11 - bit8, ��4bit������0)
		��3�ֽڣ� A19  A18  A17  A16  A15  A14  A13  A12    A18�����ǿ��
		��4�ֽڣ� A27  A26  A25  A24  A23  A22  A21  A20
		��5�ֽڣ� A28  A29  A30  A31  0    0    0    0
	*/

	/* ���Ͳ������� */
	NAND_CMD_AREA = NAND_CMD_ERASE0;

	_ulBlockNo <<= 6;	/* ���ת��Ϊҳ��� */

	#if NAND_ADDR_5 == 0	/* 128MB�� */
		NAND_ADDR_AREA = _ulBlockNo;
		NAND_ADDR_AREA = _ulBlockNo >> 8;
	#else		/* 512MB�� */
		NAND_ADDR_AREA = _ulBlockNo;
		NAND_ADDR_AREA = _ulBlockNo >> 8;
		NAND_ADDR_AREA = _ulBlockNo >> 16;
	#endif

	NAND_CMD_AREA = NAND_CMD_ERASE1;

	return (FMC_NAND_GetStatus());
}

/*
*********************************************************************************************************
*	�� �� ��: FMC_NAND_Reset
*	����˵��: ��λNAND Flash
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static uint8_t FMC_NAND_Reset(void)
{
	NAND_CMD_AREA = NAND_CMD_RESET;

		/* ������״̬ */
	if (FMC_NAND_GetStatus() == NAND_READY)
	{
		return NAND_OK;
	}

	return NAND_FAIL;
}

/*
*********************************************************************************************************
*	�� �� ��: FMC_NAND_ReadStatus
*	����˵��: ʹ��Read statuc �����NAND Flash�ڲ�״̬
*	��    ��:  - Address: �������Ŀ��������ַ
*	�� �� ֵ: NAND����״̬�������¼���ֵ��
*             - NAND_BUSY: �ڲ���æ
*             - NAND_READY: �ڲ����У����Խ����²�����
*             - NAND_ERROR: ��ǰ������ִ��ʧ��
*********************************************************************************************************
*/
static uint8_t FMC_NAND_ReadStatus(void)
{
	uint8_t ucData = 0;
	uint8_t ucStatus = NAND_BUSY;

	/* ��״̬���� */
	NAND_CMD_AREA = NAND_CMD_STATUS;
	ucData++;
	ucData = *(__IO uint8_t *)(Bank_NAND_ADDR);

	if((ucData & NAND_ERROR) == NAND_ERROR)
	{
		ucStatus = NAND_ERROR;
	}
	else if((ucData & NAND_READY) == NAND_READY)
	{
		ucStatus = NAND_READY;
	}
	else
	{
		ucStatus = NAND_BUSY;
	}

	return (ucStatus);
}

/*
*********************************************************************************************************
*	�� �� ��: FMC_NAND_GetStatus
*	����˵��: ��ȡNAND Flash����״̬
*	��    ��:  - Address: �������Ŀ��������ַ
*	�� �� ֵ: NAND����״̬�������¼���ֵ��
*             - NAND_TIMEOUT_ERROR  : ��ʱ����
*             - NAND_READY          : �����ɹ�
*********************************************************************************************************
*/
static uint32_t FMC_NAND_GetStatus(void)
{
	//uint32_t ulTimeout = 0x10000;  128M,  512M
	uint32_t ulTimeout = 0x20000;
	uint32_t ucStatus = NAND_READY;

	ucStatus = FMC_NAND_ReadStatus();

	/* �ȴ�NAND������������ʱ����˳� */
	while ((ucStatus != NAND_READY) &&( ulTimeout != 0x00))
	{
		ucStatus = FMC_NAND_ReadStatus();
		ulTimeout--;
	}

	if(ulTimeout == 0x00)
	{
		ucStatus =  NAND_TIMEOUT_ERROR;
	}

	/* ���ز���״̬ */
	return (ucStatus);
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_WriteToNewBlock
*	����˵��: ���ɿ�����ݸ��Ƶ��¿飬�����µ����ݶ�д������¿�. 
*	��    ��:  	_ulPhyPageNo : Դҳ��
*				_pWriteBuf �� ���ݻ�����
*				_usOffset �� ҳ��ƫ�Ƶ�ַ
*				_usSize �����ݳ��ȣ�������4�ֽڵ�������
*	�� �� ֵ: ִ�н����
*				- NAND_FAIL ��ʾʧ��
*				- NAND_OK ��ʾ�ɹ�
*********************************************************************************************************
*/
uint8_t NAND_WriteToNewBlock(uint32_t _ulPhyPageNo, uint8_t *_pWriteBuf, uint16_t _usOffset, uint16_t _usSize)
{
	uint16_t n, i;
	uint16_t usNewBlock;
	uint16_t ulSrcBlock;
	uint16_t usOffsetPageNo;

	ulSrcBlock = _ulPhyPageNo / NAND_BLOCK_SIZE;		/* ��������ҳ�ŷ��ƿ�� */
	usOffsetPageNo = _ulPhyPageNo % NAND_BLOCK_SIZE;	/* ��������ҳ�ż�������ҳ���ڿ���ƫ��ҳ�� */
	/* ����ѭ����Ŀ���Ǵ���Ŀ���Ϊ�������� */
	for (n = 0; n < 10; n++)
	{
		/* �������ȫ0xFF�� ����ҪѰ��һ�����п��ÿ飬����ҳ�ڵ�����ȫ���Ƶ��¿��У�Ȼ���������� */
		usNewBlock = NAND_FindFreeBlock(_ulPhyPageNo);	/* �����һ��Block��ʼ����Ѱһ�����ÿ�.  */
		if (usNewBlock >= NAND_BLOCK_COUNT)
		{
			printf_err("Error1: NAND_FindFreeBlock() %d\r\n", usNewBlock);
			return NAND_FAIL;	/* ���ҿ��п�ʧ�� */
		}

		printf_ok("NAND_WriteToNewBlock(%d -> %d)\r\n", ulSrcBlock, usNewBlock);
		
		/* ʹ��page-copy���ܣ�����ǰ�飨usPBN��������ȫ�����Ƶ��¿飨usNewBlock�� */
		for (i = 0; i < NAND_BLOCK_SIZE; i++)
		{
			if (i == usOffsetPageNo)
			{
				/* ���д��������ڵ�ǰҳ������Ҫʹ�ô�������ݵ�Copy-Back���� */
				if (FMC_NAND_PageCopyBackEx(ulSrcBlock * NAND_BLOCK_SIZE + i, usNewBlock * NAND_BLOCK_SIZE + i,
					_pWriteBuf, _usOffset, _usSize) == NAND_FAIL)
				{
					printf_err("Error2: NAND_WriteToNewBlock() %d\r\n", ulSrcBlock);
					NAND_MarkBadBlock(usNewBlock);	/* ���¿���Ϊ���� */
					NAND_BuildLUT();				/* �ؽ�LUT�� */
					break;
				}

			}
			else
			{
				/* ʹ��NAND Flash �ṩ����ҳCopy-Back���ܣ�����������߲���Ч�� */
				if (FMC_NAND_PageCopyBack(ulSrcBlock * NAND_BLOCK_SIZE + i,
					usNewBlock * NAND_BLOCK_SIZE + i) == NAND_FAIL)
				{
					printf_err("Error3: NAND_WriteToNewBlock() %d\r\n", ulSrcBlock);
					
					NAND_MarkBadBlock(usNewBlock);	/* ���¿���Ϊ���� */
					NAND_BuildLUT();				/* �ؽ�LUT�� */
					break;
				}
			}
		}
		/* Ŀ�����³ɹ� */
		if (i == NAND_BLOCK_SIZE)
		{
			/* ����¿�Ϊ���ÿ� */
			if (NAND_MarkUsedBlock(usNewBlock) == NAND_FAIL)
			{
				NAND_MarkBadBlock(usNewBlock);	/* ���¿���Ϊ���� */
				NAND_BuildLUT();				/* �ؽ�LUT�� */
				continue;
			}

			/* ����ԴBLOCK  (���Դ��дʧ�ܣ�������������) */
			if (FMC_NAND_EraseBlock(ulSrcBlock) != NAND_READY)
			{
				printf_err("Error4: FMC_NAND_EraseBlock(), %d\r\n", ulSrcBlock);
				NAND_MarkBadBlock(ulSrcBlock);	/* ��Դ����Ϊ���� */
				NAND_BuildLUT();				/* �ؽ�LUT�� */
				continue;
			}
			NAND_BuildLUT();				/* �ؽ�LUT�� */
			break;
		}
		else	/* ����ʧ�� */
		{
			printf_err("Error4: NAND_WriteToNewBlock(), %d\r\n", ulSrcBlock);
		}
	}
	if (n == 10)
	{
		printf_err("Error5: NAND_WriteToNewBlock() n=%d\r\n", n);
		return NAND_FAIL;
	}

	return NAND_OK;	/* д��ɹ� */
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_Write
*	����˵��: дһ������
*	��    ��:  	_MemAddr : �ڴ浥Ԫƫ�Ƶ�ַ
*				_pReadbuff ����Ŵ�д���ݵĻ�������ָ��
*				_usSize �����ݳ��ȣ�������4�ֽڵ�������
*	�� �� ֵ: ִ�н����
*				- NAND_FAIL ��ʾʧ��
*				- NAND_OK ��ʾ�ɹ�
*********************************************************************************************************
*/
uint8_t NAND_Write(uint32_t _ulMemAddr, uint32_t *_pWriteBuf, uint16_t _usSize)
{
	uint16_t usPBN;			/* ������� */
	uint32_t ulPhyPageNo;	/* ����ҳ�� */
	uint16_t usAddrInPage;	/* ҳ��ƫ�Ƶ�ַ */
	uint32_t ulTemp;

	/* ���ݳ��ȱ�����4�ֽ������� */
	if ((_usSize % 4) != 0)
	{
		printf_err("Error:1 NAND_Write()  %d\r\n",_usSize);
		return NAND_FAIL;
	}
	/* ���ݳ��Ȳ��ܳ���512�ֽ�(��ѭ Fat��ʽ) */
	if (_usSize > 512)
	{
		printf_err("Error:2 NAND_Write() %d\r\n",_usSize);
		//return NAND_FAIL;
	}

	usPBN = NAND_AddrToPhyBlockNo(_ulMemAddr);	/* ��ѯLUT������������ */

	ulTemp = _ulMemAddr % (NAND_BLOCK_SIZE * NAND_PAGE_SIZE);
	ulPhyPageNo = usPBN * NAND_BLOCK_SIZE + ulTemp / NAND_PAGE_SIZE;	/* ��������ҳ�� */
	usAddrInPage = ulTemp % NAND_PAGE_SIZE;	/* ����ҳ��ƫ�Ƶ�ַ */

	/* �������������ݣ��ж��Ƿ�ȫFF */
	if (FMC_NAND_ReadData(s_ucTempBuf, ulPhyPageNo, usAddrInPage, _usSize) == NAND_FAIL)
	{
		return NAND_FAIL;	/* ��NAND Flashʧ�� */
	}
	/*�������ȫ0xFF, �����ֱ��д�룬������� */
	if (NAND_IsBufOk(s_ucTempBuf, _usSize, 0xFF) == 1)
	{
		if (FMC_NAND_WriteData((uint8_t *)_pWriteBuf, ulPhyPageNo, usAddrInPage, _usSize) == NAND_FAIL)
		{
			/* ������д�뵽����һ���飨���п飩 */
			return NAND_WriteToNewBlock(ulPhyPageNo, (uint8_t *)_pWriteBuf, usAddrInPage, _usSize);
		}

		/* ��Ǹÿ����� */
		if (NAND_MarkUsedBlock(ulPhyPageNo / NAND_BLOCK_SIZE) == NAND_FAIL)
		{
			/* ���ʧ�ܣ�������д�뵽����һ���飨���п飩 */
			return NAND_WriteToNewBlock(ulPhyPageNo, (uint8_t *)_pWriteBuf, usAddrInPage, _usSize);
		}
		return NAND_OK;	/* д��ɹ� */
	}

	/* ������д�뵽����һ���飨���п飩 */
	return NAND_WriteToNewBlock(ulPhyPageNo, (uint8_t *)_pWriteBuf, usAddrInPage, _usSize);
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_Read
*	����˵��: ��һ������
*	��    ��:  	_MemAddr : �ڴ浥Ԫƫ�Ƶ�ַ
*				_pReadbuff ����Ŷ������ݵĻ�������ָ��
*				_usSize �����ݳ��ȣ�������4�ֽڵ�������
*	�� �� ֵ: ִ�н����
*				- NAND_FAIL ��ʾʧ��
*				- NAND_OK ��ʾ�ɹ�
*********************************************************************************************************
*/
uint8_t NAND_Read(uint32_t _ulMemAddr, uint32_t *_pReadBuf, uint16_t _usSize)
{
	uint16_t usPBN;			/* ������� */
	uint32_t ulPhyPageNo;	/* ����ҳ�� */
	uint16_t usAddrInPage;	/* ҳ��ƫ�Ƶ�ַ */
	uint32_t ulTemp;

	/* ���ݳ��ȱ�����4�ֽ������� */
	if ((_usSize % 4) != 0)
	{
		printf_err("Error:1 NAND_Read(_usSize)  %d\r\n",_usSize);
		return NAND_FAIL;
	}

	usPBN = NAND_AddrToPhyBlockNo(_ulMemAddr);	/* ��ѯLUT������������ */
	if (usPBN >= NAND_BLOCK_COUNT)
	{
		/* û�и�ʽ����usPBN = 0xFFFF */
		printf_err("Error:1 NAND_Write() usPBN %d\r\n",usPBN);
		return NAND_FAIL;
	}

	ulTemp = _ulMemAddr % (NAND_BLOCK_SIZE * NAND_PAGE_SIZE);
	ulPhyPageNo = usPBN * NAND_BLOCK_SIZE + ulTemp / NAND_PAGE_SIZE;	/* ��������ҳ�� */
	usAddrInPage = ulTemp % NAND_PAGE_SIZE;	/* ����ҳ��ƫ�Ƶ�ַ */

	if (FMC_NAND_ReadData((uint8_t *)_pReadBuf, ulPhyPageNo, usAddrInPage, _usSize) == NAND_FAIL)
	{
		return NAND_FAIL;	/* ��NAND Flashʧ�� */
	}

	/* �ɹ� */
	return NAND_OK;
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_WriteMultiSectors
*	����˵��: �ú��������ļ�ϵͳ������д����������ݡ�������С������512�ֽڻ�2048�ֽ�
*	��    ��:  	_pBuf : ������ݵĻ�������ָ��
*				_SectorNo ��������
*				_SectorSize ��ÿ�������Ĵ�С ��һ���� 512�ֽڣ�
*				_SectorCount : ��������
*	�� �� ֵ: ִ�н����
*				- NAND_FAIL ��ʾʧ��
*				- NAND_OK ��ʾ�ɹ�
*********************************************************************************************************
*/
uint8_t NAND_WriteMultiSectors(uint8_t *_pBuf, uint32_t _SectorNo, uint16_t _SectorSize, uint32_t _SectorCount)
{
	uint32_t i;
	uint32_t usLBN;			/* �߼���� */
	uint32_t usPBN;			/* ������� */
	uint32_t uiPhyPageNo;	/* ����ҳ�� */
	uint16_t usAddrInPage;	/* ҳ��ƫ�Ƶ�ַ */
	uint32_t ulTemp;
	uint8_t ucReturn;

	/*
		HY27UF081G2A = 128M Flash.  �� 1024��BLOCK, ÿ��BLOCK����64��PAGE�� ÿ��PAGE����2048+64�ֽڣ�
		������С��λ��BLOCK�� �����С��λ���ֽڡ�

		ÿ��PAGE���߼��Ͽ��Է�Ϊ4��512�ֽ�������
	*/

	for (i = 0; i < _SectorCount; i++)
	{
		/* �����߼������ź�������С�����߼���� */
		//usLBN = (_SectorNo * _SectorSize) / (NAND_BLOCK_SIZE * NAND_PAGE_SIZE);
		/* (_SectorNo * _SectorSize) �˻����ܴ���32λ����˻���������д�� */
		usLBN = (_SectorNo + i) / (NAND_BLOCK_SIZE * (NAND_PAGE_SIZE / _SectorSize));
		usPBN = NAND_LBNtoPBN(usLBN);	/* ��ѯLUT������������ */
		if (usPBN >= NAND_BLOCK_COUNT)
		{
			printf_err("Error1: NAND_WriteMultiSectors(), no format. usLBN=%d, usPBN=%d\r\n", usLBN, usPBN);
			/* û�и�ʽ����usPBN = 0xFFFF */
			return NAND_FAIL;
		}

		//ulTemp = ((uint64_t)(_SectorNo + i) * _SectorSize) % (NAND_BLOCK_SIZE * NAND_PAGE_SIZE);
		ulTemp = ((_SectorNo + i) % (NAND_BLOCK_SIZE * (NAND_PAGE_SIZE / _SectorSize))) *  _SectorSize;
		uiPhyPageNo = usPBN * NAND_BLOCK_SIZE + ulTemp / NAND_PAGE_SIZE;	/* ��������ҳ�� */
		usAddrInPage = ulTemp % NAND_PAGE_SIZE;	/* ����ҳ��ƫ�Ƶ�ַ */

		/* ��� _SectorCount > 0, ������ҳ���׵�ַ������Խ����Ż� */
		if (usAddrInPage == 0)
		{
			/* ��δ���� */
		}
		
		memset(s_ucTempBuf, 0xFF, _SectorSize);

		/*�������ȫ0xFF, �����ֱ��д�룬������� */
		//if (NAND_IsBufOk(s_ucTempBuf, _SectorSize, 0xFF) == 1)
		if (FMC_NAND_CompPage(s_ucTempBuf, uiPhyPageNo, usAddrInPage, _SectorSize) == NAND_OK)
		{
			if (FMC_NAND_WriteData(&_pBuf[i * _SectorSize], uiPhyPageNo, usAddrInPage, _SectorSize) == NAND_FAIL)
			{
				printf_err("Error3: NAND_WriteMultiSectors(), Write Faile\r\n");
				/* ������д�뵽����һ���飨���п飩 */
				ucReturn = NAND_WriteToNewBlock(uiPhyPageNo, &_pBuf[i * _SectorSize], usAddrInPage, _SectorSize);
				if (ucReturn != NAND_OK)
				{
					printf_err("Error4: NAND_WriteMultiSectors(), Write Faile\r\n");
					return NAND_FAIL;	/* ʧ�� */
				}
				
				printf_err("Error4a: NAND_MarkBadBlock() Faile\r\n");
				/* ���Դ��Ϊ���� */
				NAND_MarkBadBlock(uiPhyPageNo / NAND_BLOCK_SIZE);	/* ��Դ����Ϊ���� */
				continue;
			}

			/* ��Ǹÿ����� */
			if (NAND_MarkUsedBlock(uiPhyPageNo / NAND_BLOCK_SIZE) == NAND_FAIL)
			{
				/* ���ʧ�ܣ�������д�뵽����һ���飨���п飩 */
				ucReturn = NAND_WriteToNewBlock(uiPhyPageNo, &_pBuf[i * _SectorSize], usAddrInPage, _SectorSize);
				if (ucReturn != NAND_OK)
				{
					printf_err("Error4: Mark Used Block Faile\r\n");
					return NAND_FAIL;	/* ʧ�� */
				}
				continue;
			}
		}
		else	/* Ŀ�������Ѿ������ݣ�����ȫFF, ��ֱ�ӽ�����д������һ�����п� */
		{
			/* ������д�뵽����һ���飨���п飩 */
			ucReturn = NAND_WriteToNewBlock(uiPhyPageNo, &_pBuf[i * _SectorSize], usAddrInPage, _SectorSize);
			if (ucReturn != NAND_OK)
			{
				printf_err("Error5: NAND_WriteMultiSectors(), Write Faile\r\n");
				return NAND_FAIL;	/* ʧ�� */
			}
			continue;
		}
	}
	return NAND_OK;		/* �ɹ� */
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_ReadMultiSectors
*	����˵��: �ú��������ļ�ϵͳ�������������ݡ���1������������������С������512�ֽڻ�2048�ֽ�
*	��    ��:  	_pBuf : ��Ŷ������ݵĻ�������ָ��
*				_SectorNo ��������
*				_SectorSize ��ÿ�������Ĵ�С
*				_SectorCount : ��������
*	�� �� ֵ: ִ�н����
*				- NAND_FAIL ��ʾʧ��
*				- NAND_OK ��ʾ�ɹ�
*********************************************************************************************************
*/
uint8_t NAND_ReadMultiSectors(uint8_t *_pBuf, uint32_t _SectorNo, uint16_t _SectorSize, uint32_t _SectorCount)
{
	uint32_t i;
	uint32_t usLBN;			/* �߼���� */
	uint32_t usPBN;			/* ������� */
	uint32_t uiPhyPageNo;	/* ����ҳ�� */
	uint16_t usAddrInPage;	/* ҳ��ƫ�Ƶ�ַ */
	uint32_t ulTemp;

	/*
		HY27UF081G2A = 128M Flash.  �� 1024��BLOCK, ÿ��BLOCK����64��PAGE�� ÿ��PAGE����2048+64�ֽڣ�
		������С��λ��BLOCK�� �����С��λ���ֽڡ�

		ÿ��PAGE���߼��Ͽ��Է�Ϊ4��512�ֽ�������
	*/

	for (i = 0; i < _SectorCount; i++)
	{
		/* �����߼������ź�������С�����߼���� */
		//usLBN = (_SectorNo * _SectorSize) / (NAND_BLOCK_SIZE * NAND_PAGE_SIZE);
		/* (_SectorNo * _SectorSize) �˻����ܴ���32λ����˻���������д�� */
		usLBN = (_SectorNo + i) / (NAND_BLOCK_SIZE * (NAND_PAGE_SIZE / _SectorSize));
		usPBN = NAND_LBNtoPBN(usLBN);	/* ��ѯLUT������������ */
		if (usPBN >= NAND_BLOCK_COUNT)
		{
			printf_err("Error: NAND_ReadMultiSectors(), not format, usPBN = %d\r\n", usPBN);
			/* û�и�ʽ����usPBN = 0xFFFF */
			return NAND_FAIL;
		}

		ulTemp = ((uint64_t)(_SectorNo + i) * _SectorSize) % (NAND_BLOCK_SIZE * NAND_PAGE_SIZE);
		uiPhyPageNo = usPBN * NAND_BLOCK_SIZE + ulTemp / NAND_PAGE_SIZE;	/* ��������ҳ�� */
		usAddrInPage = ulTemp % NAND_PAGE_SIZE;	/* ����ҳ��ƫ�Ƶ�ַ */

		if (FMC_NAND_ReadData((uint8_t *)&_pBuf[i * _SectorSize], uiPhyPageNo, usAddrInPage, _SectorSize) == NAND_FAIL)
		{
			printf_err("Error: NAND_ReadMultiSectors(), ReadData(page = %d, addr = %d)\r\n", uiPhyPageNo, usAddrInPage);
			return NAND_FAIL;	/* ��NAND Flashʧ�� */
		}
	}

	/* �ɹ� */
	return NAND_OK;
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_BuildLUT
*	����˵��: ���ڴ��д������������
*	��    ��:  ZoneNbr ������
*	�� �� ֵ: NAND_OK�� �ɹ��� 	NAND_FAIL��ʧ��
*********************************************************************************************************
*/
static uint8_t NAND_BuildLUT(void)
{
	uint16_t i;
	uint8_t buf[VALID_SPARE_SIZE];
	uint16_t usLBN;	/* �߼���� */

	/* */
	for (i = 0; i < NAND_BLOCK_COUNT; i++)
	{
		s_usLUT[i] = 0xFFFF;	/* �����Чֵ�������ؽ�LUT���ж�LUT�Ƿ���� */
	}
	for (i = 0; i < NAND_BLOCK_COUNT; i++)
	{
		/* ��ÿ����ĵ�1��PAGE��ƫ�Ƶ�ַΪLBN0_OFFSET������ */
		FMC_NAND_ReadSpare(buf, i * NAND_BLOCK_SIZE, 0, VALID_SPARE_SIZE);

		/* ����Ǻÿ飬���¼LBN0 LBN1 */
		if (buf[BI_OFFSET] == 0xFF)
		{
			usLBN = buf[LBN0_OFFSET] + buf[LBN1_OFFSET] * 256;	/* ����������߼���� */
			if (usLBN < NAND_BLOCK_COUNT)
			{
				/* ����Ѿ��Ǽǹ��ˣ����ж�Ϊ�쳣 */
				if (s_usLUT[usLBN] != 0xFFFF)
				{
					return NAND_FAIL;
				}

				s_usLUT[usLBN] = i;	/* ����LUT�� */
			}
		}
	}

	/* LUT������ϣ�����Ƿ���� */
	for (i = 0; i < NAND_BLOCK_COUNT; i++)
	{
		if (s_usLUT[i] >= NAND_BLOCK_COUNT)
		{
			s_usValidDataBlockCount = i;
			break;
		}
	}
	if (s_usValidDataBlockCount < 100)
	{
		/* ���� ������Ч�߼����С��100��������û�и�ʽ�� */
		return NAND_FAIL;
	}
	for (; i < s_usValidDataBlockCount; i++)
	{
		if (s_usLUT[i] != 0xFFFF)
		{
			return NAND_FAIL;	/* ����LUT���߼���Ŵ�����Ծ���󣬿�����û�и�ʽ�� */
		}
	}

	/* �ؽ�LUT���� */
	return NAND_OK;
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_AddrToPhyBlockNo
*	����˵��: �ڴ��߼���ַת��Ϊ�������
*	��    ��:  _ulMemAddr���߼��ڴ��ַ
*	�� �� ֵ: ����ҳ�ţ� ����� 0xFFFFFFFF ���ʾ����
*********************************************************************************************************
*/
static uint16_t NAND_AddrToPhyBlockNo(uint32_t _ulMemAddr)
{
	uint16_t usLBN;		/* �߼���� */
	uint16_t usPBN;		/* ������� */

	usLBN = _ulMemAddr / (NAND_BLOCK_SIZE * NAND_PAGE_SIZE);	/* �����߼���� */
	/* ����߼���Ŵ�����Ч�����ݿ������̶�����0xFFFF, ���øú����Ĵ���Ӧ�ü������ִ��� */
	if (usLBN >= s_usValidDataBlockCount)
	{
		return 0xFFFF;
	}
	/* ��ѯLUT�������������� */
	usPBN = s_usLUT[usLBN];
	return usPBN;
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_LBNtoPBN
*	����˵��: �߼����ת��Ϊ�������
*	��    ��: _uiLBN : �߼���� Logic Block No
*	�� �� ֵ: ������ţ� ����� 0xFFFFFFFF ���ʾ����
*********************************************************************************************************
*/
static uint16_t NAND_LBNtoPBN(uint32_t _uiLBN)
{
	uint16_t usPBN;		/* ������� */

	/* ����߼���Ŵ�����Ч�����ݿ������̶�����0xFFFF, ���øú����Ĵ���Ӧ�ü������ִ��� */
	if (_uiLBN >= s_usValidDataBlockCount)
	{
		return 0xFFFF;
	}
	/* ��ѯLUT�������������� */
	usPBN = s_usLUT[_uiLBN];
	return usPBN;
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_FindFreeBlock
*	����˵��: �����һ���鿪ʼ������һ�����õĿ顣A18������ͬ
*	��    ��: _ulSrcPageNo : Դҳ��
*	�� �� ֵ: ��ţ������0xFFFF��ʾʧ��
*********************************************************************************************************
*/
static uint16_t NAND_FindFreeBlock (uint32_t _ulSrcPageNo)
{
	uint16_t n;

	n = NAND_BLOCK_COUNT - 1;
	if (_ulSrcPageNo & 0x40)	/* ��Ҫ������ */
	{
		if ((n & 0x01) == 0)
		{
			n--;
		}
	}
	else	/* ��Ҫż���� */
	{
		if (n & 0x01)
		{
			n--;
		}
	}

	while (1)
	{
		if (NAND_IsFreeBlock(n))	/* �ǿ��п� */
		{
			return n;
		}

		if (n < 2)
		{
			return 0xFFFF;	/* û���ҵ����еĿ� */
		}
		n -= 2;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_IsBufOk
*	����˵��: �ж��ڴ滺�����������Ƿ�ȫ��Ϊָ��ֵ
*	��    ��:  - _pBuf : ���뻺����
*			  - _ulLen : ����������
*			  - __ucValue : ������ÿ����Ԫ����ȷ��ֵ
*	�� �� ֵ: 1 ��ȫ����ȷ�� 0 ������ȷ
*********************************************************************************************************
*/
static uint8_t NAND_IsBufOk(uint8_t *_pBuf, uint32_t _ulLen, uint8_t _ucValue)
{
	uint32_t i;

	for (i = 0; i < _ulLen; i++)
	{
		if (_pBuf[i] != _ucValue)
		{
			return 0;
		}
	}

	return 1;
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_IsBadBlock
*	����˵��: ���ݻ����Ǽ��NAND Flashָ���Ŀ��Ƿ񻵿�
*	��    ��: _ulBlockNo ����� 0 - 1023 ������128M�ֽڣ�2K Page��NAND Flash����1024���飩
*	�� �� ֵ: 0 ���ÿ���ã� 1 ���ÿ��ǻ���
*********************************************************************************************************
*/
 uint8_t NAND_IsBadBlock(uint32_t _ulBlockNo)
{
	uint8_t ucFlag;

	/* ���NAND Flash����ǰ�Ѿ���עΪ�����ˣ������Ϊ�ǻ��� */
	FMC_NAND_ReadSpare(&ucFlag, _ulBlockNo * NAND_BLOCK_SIZE, BI_OFFSET, 1);
	if (ucFlag != 0xFF)
	{
		return 1;
	}

	FMC_NAND_ReadSpare(&ucFlag, _ulBlockNo * NAND_BLOCK_SIZE + 1, BI_OFFSET, 1);
	if (ucFlag != 0xFF)
	{
		return 1;
	}
	return 0;	/* �Ǻÿ� */
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_IsFreeBlock
*	����˵��: ���ݻ����Ǻ�USED��־����Ƿ���ÿ�
*	��    ��: _ulBlockNo ����� 0 - 1023 ������128M�ֽڣ�2K Page��NAND Flash����1024���飩
*	�� �� ֵ: 1 ���ÿ���ã� 0 ���ÿ��ǻ��������ռ��
*********************************************************************************************************
*/
static uint8_t NAND_IsFreeBlock(uint32_t _ulBlockNo)
{
	uint8_t ucFlag;

	/* ���NAND Flash����ǰ�Ѿ���עΪ�����ˣ������Ϊ�ǻ��� */
	if (NAND_IsBadBlock(_ulBlockNo))
	{
		return 0;
	}

	//FMC_NAND_ReadPage(&ucFlag, _ulBlockNo * NAND_BLOCK_SIZE, USED_OFFSET, 1);  2014-05-03 bug
	FMC_NAND_ReadSpare(&ucFlag, _ulBlockNo * NAND_BLOCK_SIZE, USED_OFFSET, 1);
	if (ucFlag == 0xFF)
	{
		return 1;
	}
	return 0;
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_ScanBlock
*	����˵��: ɨ�����NAND Flashָ���Ŀ�
*			��ɨ������㷨��
*			1) ��1���飨�������������ͱ����������������������Ƿ�ȫ0xFF, ��ȷ�Ļ��������ԸĿ飬����ÿ�
				�ǻ���,��������
*			2) ��ǰ��д��ȫ 0x00��Ȼ���ȡ��⣬��ȷ�Ļ��������ԸĿ飬�����˳�
*			3) �ظ��ڣ�2���������ѭ��������50�ζ�û�з���������ô�ÿ�����,�������أ�����ÿ��ǻ��飬
*				��������
*			��ע�⡿
*			1) �ú���������Ϻ󣬻�ɾ�������������ݣ�����Ϊȫ0xFF;
*			2) �ú������˲������������⣬Ҳ�Ա������������в��ԡ�
*			3) ��д����ѭ���������Ժ�ָ����#define BAD_BALOK_TEST_CYCLE 50
*	��    ��:  _ulPageNo ��ҳ�� 0 - 65535 ������128M�ֽڣ�2K Page��NAND Flash����1024���飩
*	�� �� ֵ: NAND_OK ���ÿ���ã� NAND_FAIL ���ÿ��ǻ���
*********************************************************************************************************
*/
uint8_t NAND_ScanBlock(uint32_t _ulBlockNo)
{
	uint32_t i, k;
	uint32_t ulPageNo;

	#if 0
	/* ���NAND Flash����ǰ�Ѿ���עΪ�����ˣ������Ϊ�ǻ��� */
	if (NAND_IsBadBlock(_ulBlockNo))
	{
		return NAND_FAIL;
	}
	#endif

	/* ����Ĵ��뽫ͨ��������������̵ķ�ʽ������NAND Flashÿ����Ŀɿ��� */
	memset(s_ucTempBuf, 0x00, NAND_PAGE_TOTAL_SIZE);
	for (i = 0; i < BAD_BALOK_TEST_CYCLE; i++)
	{
		/* ��1������������� */
		if (FMC_NAND_EraseBlock(_ulBlockNo) != NAND_READY)
		{
			printf_err(" -- EraseBlock Error %d\r\n", _ulBlockNo);
			return NAND_FAIL;
		}
		
		/* ��2������������ÿ��page�����ݣ����ж��Ƿ�ȫ0xFF */
		ulPageNo = _ulBlockNo * NAND_BLOCK_SIZE;	/* ����ÿ��1��ҳ��ҳ�� */
		for (k = 0; k < NAND_BLOCK_SIZE; k++)
		{
			/* ������ҳ���� */
			FMC_NAND_ReadPage(s_ucTempBuf, ulPageNo, 0, NAND_PAGE_TOTAL_SIZE);

			/* �жϴ洢��Ԫ�ǲ���ȫ0xFF */
			if (NAND_IsBufOk(s_ucTempBuf, NAND_PAGE_TOTAL_SIZE, 0xFF) == 0)
			{
				printf_err(" -- IsBufOk 0xFF Error %d\r\n", _ulBlockNo);
				return NAND_FAIL;
			}

			ulPageNo++;		/* ����д��һ��ҳ */
		}

		/* ��2����дȫ0���������ж��Ƿ�ȫ0 */
		ulPageNo = _ulBlockNo * NAND_BLOCK_SIZE;	/* ����ÿ��1��ҳ��ҳ�� */
		for (k = 0; k < NAND_BLOCK_SIZE; k++)
		{
			/* ���buf[]������Ϊȫ0,��д��NAND Flash */
			memset(s_ucTempBuf, 0x00, NAND_PAGE_TOTAL_SIZE);
			if (FMC_NAND_WritePage(s_ucTempBuf, ulPageNo, 0, NAND_PAGE_TOTAL_SIZE) != NAND_OK)
			{
				printf_err(" -- WritePage 0xFF Error %d\r\n", _ulBlockNo);
				return NAND_FAIL;
			}

			/* ������ҳ����, �жϴ洢��Ԫ�ǲ���ȫ0x00 */
			FMC_NAND_ReadPage(s_ucTempBuf, ulPageNo, 0, NAND_PAGE_TOTAL_SIZE);
			if (NAND_IsBufOk(s_ucTempBuf, NAND_PAGE_TOTAL_SIZE, 0x00) == 0)
			{
				printf_err(" -- IsBufOk 0x00 Error %d\r\n", _ulBlockNo);
				return NAND_FAIL;
			}

			ulPageNo++;		/* ����һ��ҳ */
		}
	}

	/* ���һ�������������� */
	if (FMC_NAND_EraseBlock(_ulBlockNo) != NAND_READY)
	{
		printf_err(" -- Last Erase Block Error %d\r\n", _ulBlockNo);
		return NAND_FAIL;
	}
	ulPageNo = _ulBlockNo * NAND_BLOCK_SIZE;	/* ����ÿ��1��ҳ��ҳ�� */
	for (k = 0; k < NAND_BLOCK_SIZE; k++)
	{
		/* ������ҳ���� */
		FMC_NAND_ReadPage(s_ucTempBuf, ulPageNo, 0, NAND_PAGE_TOTAL_SIZE);

		/* �жϴ洢��Ԫ�ǲ���ȫ0xFF */
		if (NAND_IsBufOk(s_ucTempBuf, NAND_PAGE_TOTAL_SIZE, 0xFF) == 0)
		{
			printf_err(" --IsBufOk 0xFF. %d\r\n", _ulBlockNo);
			return NAND_FAIL;
		}

		ulPageNo++;		/* ����д��һ��ҳ */
	}

	return NAND_OK;

}

/*
*********************************************************************************************************
*	�� �� ��: NAND_MarkUsedBlock
*	����˵��: ���NAND Flashָ���Ŀ�Ϊ���ÿ�
*	��    ��: _ulBlockNo ����� 0 - 1023 ������128M�ֽڣ�2K Page��NAND Flash����1024���飩
*	�� �� ֵ: NAND_OK:��ǳɹ��� NAND_FAIL�����ʧ�ܣ��ϼ�����Ӧ�ý��л��鴦����
*********************************************************************************************************
*/
static uint8_t NAND_MarkUsedBlock(uint32_t _ulBlockNo)
{
	uint32_t ulPageNo;
	uint8_t ucFlag;

	/* �����ĵ�1��ҳ�� */
	ulPageNo = _ulBlockNo * NAND_BLOCK_SIZE;	/* ����ÿ��1��ҳ��ҳ�� */

	/* ���ڵ�1��page�������ĵ�USED_OFFSET���ֽ�д���0xFF���ݱ�ʾ���ÿ� */
	ucFlag = NAND_USED_BLOCK_FLAG;
	if (FMC_NAND_WriteSpare(&ucFlag, ulPageNo, USED_OFFSET, 1) == NAND_FAIL)
	{
		/* ������ʧ�ܣ�����Ҫ��ע�����Ϊ���� */
		return NAND_FAIL;
	}
	return NAND_OK;
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_MarkBadBlock
*	����˵��: ���NAND Flashָ���Ŀ�Ϊ����
*	��    ��: _ulBlockNo ����� 0 - 1023 ������128M�ֽڣ�2K Page��NAND Flash����1024���飩
*	�� �� ֵ: �̶�NAND_OK
*********************************************************************************************************
*/
void NAND_MarkBadBlock(uint32_t _ulBlockNo)
{
	uint32_t ulPageNo;
	uint8_t ucFlag;

	printf_err("NAND_MarkBadBlock(%d)\r\n", _ulBlockNo);

	/* �����ĵ�1��ҳ�� */
	ulPageNo = _ulBlockNo * NAND_BLOCK_SIZE;	/* ����ÿ��1��ҳ��ҳ�� */

	/* ���ڵ�1��page�������ĵ�BI_OFFSET���ֽ�д���0xFF���ݱ�ʾ���� */
	ucFlag = NAND_BAD_BLOCK_FLAG;
	if (FMC_NAND_WriteSpare(&ucFlag, ulPageNo, BI_OFFSET, 1) == NAND_FAIL)
	{
		/* �����1��ҳ���ʧ�ܣ����ڵ�2��ҳ��� */
		FMC_NAND_WriteSpare(&ucFlag, ulPageNo + 1, BI_OFFSET, 1);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_ScanAllBadBlock
*	����˵��: ɨ��������е�BLOCK, ����ǻ��飬����������
*	��    ��: _ulBlockNo ����� 0 - 1023 ������128M�ֽڣ�2K Page��NAND Flash����1024���飩
*	�� �� ֵ: �̶�NAND_OK
*********************************************************************************************************
*/
void NAND_ScanAllBadBlock(void)
{
	uint32_t i;
	
	for (i = 0; i < NAND_BLOCK_COUNT; i++)
	{
		if (NAND_ScanBlock(i) == NAND_OK)
		{
			printf("Scan Block %d (%d%%), Ok\r\n", i, i * 100 / NAND_BLOCK_COUNT);
		}
		else
		{
			printf("Scan Block %d (%d%%), Err\r\n", i, i * 100 / NAND_BLOCK_COUNT);
			NAND_MarkBadBlock(i);
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_Format
*	����˵��: NAND Flash��ʽ�����������е����ݣ��ؽ�LUT
*	��    ��:  ��
*	�� �� ֵ: NAND_OK : �ɹ��� NAND_Fail ��ʧ�ܣ�һ���ǻ����������ർ�£�
*********************************************************************************************************
*/
uint8_t NAND_Format(void)
{
	uint16_t i, n;
	uint16_t usGoodBlockCount;

	/* ����ÿ���� */
	usGoodBlockCount = 0;
	for (i = 0; i < NAND_BLOCK_COUNT; i++)
	{
		/* ����Ǻÿ飬����� */
		if (!NAND_IsBadBlock(i))
		{
			FMC_NAND_EraseBlock(i);
			usGoodBlockCount++;
		}
	}

	/* ����ÿ����������100����NAND Flash���� */
	if (usGoodBlockCount < 100)
	{
		return NAND_FAIL;
	}

	usGoodBlockCount = (usGoodBlockCount * 98) / 100;	/* 98%�ĺÿ����ڴ洢���� */

	/* ��������һ�� */
	n = 0; /* ͳ���ѱ�ע�ĺÿ� */
	for (i = 0; i < NAND_BLOCK_COUNT; i++)
	{
		if (!NAND_IsBadBlock(i))
		{
			/* ����Ǻÿ飬���ڸÿ�ĵ�1��PAGE��LBN0 LBN1��д��nֵ (ǰ���Ѿ�ִ���˿������ */
			if (FMC_NAND_WriteSpare((uint8_t *)&n, i * NAND_BLOCK_SIZE, LBN0_OFFSET, 2) != NAND_OK)
			{
				return NAND_FAIL;
			}
			n++;

			/* ���㲢д��ÿ��������ECCֵ ����ʱδ����*/

			if (n == usGoodBlockCount)
			{
				break;
			}
		}
	}

	NAND_BuildLUT();	/* ��ʼ��LUT�� */
	return NAND_OK;
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_FormatCapacity
*	����˵��: NAND Flash��ʽ�������Ч����. (���֧��4G)
*	��    ��:  ��
*	�� �� ֵ: ��������λ�ֽ�
*********************************************************************************************************
*/
uint32_t NAND_FormatCapacity(void)
{
	uint16_t usCount;

	/* �������ڴ洢���ݵ����ݿ��������������Ч������98%������ */
	usCount = (s_usValidDataBlockCount * DATA_BLOCK_PERCENT) / 100;

	return (usCount * NAND_BLOCK_SIZE * NAND_PAGE_SIZE);
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_DispBadBlockInfo
*	����˵��: ͨ�����ڴ�ӡ��NAND Flash�Ļ�����Ϣ
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void NAND_DispBadBlockInfo(void)
{
	uint32_t id;
	uint32_t i;
	uint32_t n;
	uint32_t used = 0;
	uint16_t bad_no[128];

	bsp_FMC_NAND_Init();	/* ��ʼ��FSMC */

	id = NAND_ReadID();

	printf("NAND Flash ID = 0x%04X, Type = ", id);
	if (id == HY27UF081G2A)
	{
		printf("HY27UF081G2A\r\n  1024 Blocks, 64 pages per block, 2048 + 64 bytes per page\r\n");
	}
	else if (id == K9F1G08U0A)
	{
		printf("K9F1G08U0A\r\n  1024 Blocks, 64 pages per block, 2048 + 64 bytes per page\r\n");
	}
	else if (id == K9F1G08U0B)
	{
		printf("K9F1G08U0B\r\n  1024 Blocks, 64 pages per block, 2048 + 64 bytes per page\r\n");
	}
	else if (id == H27U1G8F2BTR)
	{
		printf("H27U1G8FBDTR\r\n  1024 Blocks, 64 pages per block, 2048 + 64 bytes per page\r\n");
	}	
	else if (id == H27U4G8F2DTR)
	{
		printf("H27U4G8F2DTR\r\n  4096 Blocks, 64 pages per block, 2048 + 64 bytes per page\r\n");
	}
	else
	{
		printf("unkonow\r\n");
		return;
	}

	FMC_NAND_Reset();

	printf("Block Info : 0 is OK, * is Bad, - is Used\r\n");
	n = 0;	/* ����ͳ�� */
	used = 0; /* ���ÿ�ͳ�� */
	for (i = 0; i < NAND_BLOCK_COUNT; i++)
	{
		if (NAND_IsBadBlock(i))
		{
			if (n < sizeof(bad_no) / 2)
			{
				bad_no[n] = i;	/* ��¼����� */
			}
			printf("*");
			n++;
		}
		else
		{
			if (NAND_IsFreeBlock(i))
			{
				printf("0");
			}
			else
			{
				printf("-");	/* ���ÿ� */
				used++;
			}
		}

		if (((i + 1) % 8) == 0)
		{
			printf(" ");
		}

		if (((i + 1) % 64) == 0)
		{
			printf("\r\n");
		}
	}

	if (id == H27U4G8F2DTR)
	{
		printf("Bad Block Count = %d  ( < 80 is OK), Used = %d \r\n", n, used);
	}
	else
	{
		printf("Bad Block Count = %d\r\n", n);
	}
	
	/* ��ӡ������� */
	if (n > 0)
	{
		for (i = 0; i < n; i++)
		{
			printf("%4d ",  bad_no[i]);
		}
		
		printf("\r\n\r\n");
	}
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_GetBlockInfo
*	����˵��: ���NAND ���顢���ÿ���Ϣ
*	��    ��: ��
*	�� �� ֵ: 1 ��ʾ�ɹ�ʶ�� 0 ��ʾʶ��ʧ��
*********************************************************************************************************
*/
uint8_t NAND_GetBlockInfo(NAND_BLOCK_INFO_T *_pInfo)
{
	uint32_t id;
	uint32_t i;
	uint32_t n;
	uint32_t used = 0;
	uint32_t free = 0;

	bsp_FMC_NAND_Init();	/* ��ʼ��FSMC */

	id = NAND_ReadID();
	
	_pInfo->ChipID = id;
	if (id == HY27UF081G2A)
	{
		_pInfo->ChipID = HY27UF081G2A;
		strcpy(_pInfo->ChipName, "HY27UF081G2A");
	}
	else if (id == K9F1G08U0A)
	{
		_pInfo->ChipID = K9F1G08U0A;
		strcpy(_pInfo->ChipName, "K9F1G08U0A");
	}
	else if (id == K9F1G08U0B)
	{
		_pInfo->ChipID = K9F1G08U0B;
		strcpy(_pInfo->ChipName, "K9F1G08U0B");
	}
	else if (id == H27U4G8F2DTR)
	{
		_pInfo->ChipID = H27U4G8F2DTR;
		strcpy(_pInfo->ChipName, "H27U4G8F2DTR");
	}
	else if (id == H27U1G8F2BTR)
	{
		_pInfo->ChipID = H27U1G8F2BTR;
		strcpy(_pInfo->ChipName, "H27U1G8F2BTR");
	}	
	else
	{		
		return 0;
	}

	FMC_NAND_Reset();

	n = 0;	/* ����ͳ�� */
	used = 0; /* ���ÿ�ͳ�� */
	free = 0; /* δ�õĺÿ� */
	for (i = 0; i < NAND_BLOCK_COUNT; i++)
	{
		if (NAND_IsBadBlock(i))
		{
			n++;			/* ��¼���� */
		}
		else
		{
			if (NAND_IsFreeBlock(i))
			{
				free++;
			}
			else
			{	
				used++;		/* ���ÿ� */
			}
		}
	}

	_pInfo->Bad = n;
	_pInfo->Used = used;
	_pInfo->Free = free;
	return 1;
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_DispPhyPageData
*	����˵��: ͨ�����ڴ�ӡ��ָ��ҳ�����ݣ�2048+64��
*	��    ��: _uiPhyPageNo �� ����ҳ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void NAND_DispPhyPageData(uint32_t _uiPhyPageNo)
{
	uint32_t i, n;
	uint32_t ulBlockNo;
	uint16_t usOffsetPageNo;

	ulBlockNo = _uiPhyPageNo / NAND_BLOCK_SIZE;		/* ��������ҳ�ŷ��ƿ�� */
	usOffsetPageNo = _uiPhyPageNo % NAND_BLOCK_SIZE;	/* ��������ҳ�ż�������ҳ���ڿ���ƫ��ҳ�� */

	if (NAND_OK != FMC_NAND_ReadPage(s_ucTempBuf, _uiPhyPageNo, 0, NAND_PAGE_TOTAL_SIZE))
	{
		printf("FMC_NAND_ReadPage Failed() \r\n");
		return;
	}

	printf("Block = %d, Page = %d\r\n", ulBlockNo, usOffsetPageNo);

	/* ��ӡǰ�� 2048�ֽ����ݣ�ÿ512�ֽڿ�һ�� */
	for (n = 0; n < 4; n++)
	{
		for (i = 0; i < 512; i++)
		{
			printf(" %02X", s_ucTempBuf[i + n * 512]);

			if ((i & 31) == 31)
			{
				printf("\r\n");	/* ÿ����ʾ32�ֽ����� */
			}
			else if ((i & 31) == 15)
			{
				printf(" - ");
			}
		}
		printf("\r\n");
	}

	/* ��ӡǰ�� 2048�ֽ����ݣ�ÿ512�ֽڿ�һ�� */
	for (i = 0; i < 64; i++)
	{
		printf(" %02X", s_ucTempBuf[i + 2048]);

		if ((i & 15) == 15)
		{
			printf("\r\n");	/* ÿ����ʾ32�ֽ����� */
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_DispLogicPageData
*	����˵��: ͨ�����ڴ�ӡ��ָ��ҳ�����ݣ�2048+64��
*	��    ��: _uiLogicPageNo �� �߼�ҳ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void NAND_DispLogicPageData(uint32_t _uiLogicPageNo)
{
	uint32_t uiPhyPageNo;
	uint16_t usLBN;	/* �߼���� */
	uint16_t usPBN;	/* ������� */

	usLBN = _uiLogicPageNo / NAND_BLOCK_SIZE;
	usPBN = NAND_LBNtoPBN(usLBN);	/* ��ѯLUT������������ */
	if (usPBN >= NAND_BLOCK_COUNT)
	{
		/* û�и�ʽ����usPBN = 0xFFFF */
		return;
	}

	printf("LogicBlock = %d, PhyBlock = %d\r\n", _uiLogicPageNo, usPBN);

	/* ��������ҳ�� */
	uiPhyPageNo = usPBN * NAND_BLOCK_SIZE + _uiLogicPageNo % NAND_BLOCK_SIZE;
	NAND_DispPhyPageData(uiPhyPageNo);	/* ��ʾָ��ҳ���� */
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_ReadONFI
*	����˵��: ��NAND Flash��ONFI��Ϣ,  ��� H27U4G8F2DTR
*	��    ��: _pBuf ��Ž���Ļ�����, ��С4�ֽڡ�Ӧ�ù̶����� "ONFI" �ַ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void NAND_ReadONFI(uint8_t *_pBuf)
{
	uint16_t i;

	/* �������� Command to the command area */
	NAND_CMD_AREA = 0x90;
	NAND_ADDR_AREA = 0x20;

	/* �����ݵ�������pBuffer */
	for(i = 0; i < 256; i++)
	{	
		_pBuf[i] = NAND_DATA_AREA;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: NAND_ReadParamPage
*	����˵��: Read Parameter Page,  ��� H27U4G8F2DTR
*	��    ��: _pData ��Ž���Ļ�������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void NAND_ReadParamPage(PARAM_PAGE_T *_pData)
{
	uint16_t i;
	uint8_t *_pBuf = (uint8_t *)_pData;

	/* �������� Command to the command area */
	NAND_CMD_AREA = 0xEC;
	NAND_ADDR_AREA = 0x00;

	/* ����ȴ���������������쳣, �˴�Ӧ���жϳ�ʱ */
	WAIT_BUSY();

	/* �����ݵ�������pBuffer */
	for(i = 0; i < 256; i++)
	{
		_pBuf[i] = NAND_DATA_AREA;
	}

}

/*
*********************************************************************************************************
*	�� �� ��: NAND_DispParamPage
*	����˵��: ͨ�����ڴ�ӡ Parameter Page Data Structure Definition (����ҳ���ݽṹ) ��� H27U4G8F2DTR
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void NAND_DispParamPage(void)
{
	PARAM_PAGE_T tPage;
	char buf[32];

	FMC_NAND_Reset();

	NAND_ReadParamPage(&tPage);

#if 0
	uint8_t Sign[4];		/* = "ONFI" */
	uint16_t Revision; 		/* Bit1 = 1 ��ʾ֧�� ONFI Ver 1.0 */
	uint16_t Features;		/* */
	uint16_t OptionalCommands;
#endif

	printf("\r\n");
	printf("Read Parameter Page Data :\r\n");
	printf("Sign = %c%c%c%c\r\n", tPage.Sign[0], tPage.Sign[1], tPage.Sign[2], tPage.Sign[3]);
	printf("Revision = %04X\r\n", tPage.Revision);
	printf("Features = %04X\r\n", tPage.Features);
	printf("OptionalCommands = %04X\r\n", tPage.OptionalCommands);

	/* Manufacturer information block */
	memcpy(buf, tPage.Manufacturer, 12);
	buf[12] = 0;
	printf("Manufacturer = %s\r\n", buf);	/* ������ */

	memcpy(buf, tPage.Model, 20);
	buf[20] = 0;
	printf("Model = %s\r\n", buf);	/* �ͺ� */

	printf("JEDEC_ID = %02X\r\n", tPage.JEDEC_ID);	/* AD */
	printf("DateCode = %04X\r\n", tPage.DateCode);

#if 0
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
#endif

	printf("\r\n");

}


/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
