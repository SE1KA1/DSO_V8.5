/**
 * @file bsp_spi_flash.c
 * @brief 安富莱 BSP 通用驱动（SPI Flash）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : SPI�ӿڴ���FLASH ��дģ��
*	�ļ����� : bsp_spi_flash.c
*	��    �� : V1.0
*	˵    �� : ֧�� SST25VF016B��MX25L1606E �� W25Q64BVSSIG
*			   SST25VF016B ��д��������AAIָ��������д��Ч�ʡ�
*
*			   ȱʡʹ��STM32F4��Ӳ��SPI1�ӿڣ�ʱ��Ƶ�����Ϊ 42MHz ����Ƶʹ�ã�
*
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2013-02-01 armfly  ��ʽ����
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/* ����Flsh��ƬѡGPIO�˿ڣ� PD13  */
#define SF_CS_CLK_ENABLE() 			__HAL_RCC_GPIOD_CLK_ENABLE()
/* 宏定义: SF_CS_GPIO - SF_CS_GPIO 的宏定义。 */
#define SF_CS_GPIO					GPIOD
/* 宏定义: SF_CS_PIN - SF_CS_PIN 的宏定义。 */
#define SF_CS_PIN					GPIO_PIN_13
/* 宏定义: SF_CS_0 - SF_CS_0 的宏定义。 */
#define SF_CS_0()					SF_CS_GPIO->BSRRH = SF_CS_PIN
/* 宏定义: SF_CS_1 - SF_CS_1 的宏定义。 */
#define SF_CS_1()					SF_CS_GPIO->BSRRL = SF_CS_PIN
	
#define CMD_AAI       0xAD  	/* AAI �������ָ��(FOR SST25VF016B) */
#define CMD_DISWR	  0x04		/* ��ֹд, �˳�AAI״̬ */
#define CMD_EWRSR	  0x50		/* ����д״̬�Ĵ��������� */
#define CMD_WRSR      0x01  	/* д״̬�Ĵ������� */
#define CMD_WREN      0x06		/* дʹ������ */
#define CMD_READ      0x03  	/* ������������ */
#define CMD_RDSR      0x05		/* ��״̬�Ĵ������� */
#define CMD_RDID      0x9F		/* ������ID���� */
#define CMD_SE        0x20		/* ������������ */
#define CMD_BE        0xC7		/* ������������ */
#define DUMMY_BYTE    0xA5		/* ���������Ϊ����ֵ�����ڶ����� */

#define WIP_FLAG      0x01		/* ״̬�Ĵ����е����ڱ�̱�־��WIP) */

SFLASH_T g_tSF;

#if 0
/*
*********************************************************************************************************
*   函 数 名: sf_WriteStatus
*   功能说明: sf_WriteStatus 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void sf_WriteStatus(uint8_t _ucValue);
#endif

/*
*********************************************************************************************************
*   函 数 名: sf_WriteEnable
*   功能说明: sf_WriteEnable 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void sf_WriteEnable(void);
/*
*********************************************************************************************************
*   函 数 名: sf_WaitForWriteEnd
*   功能说明: sf_WaitForWriteEnd 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void sf_WaitForWriteEnd(void);
/*
*********************************************************************************************************
*   函 数 名: sf_NeedErase
*   功能说明: sf_NeedErase 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t sf_NeedErase(uint8_t * _ucpOldBuf, uint8_t *_ucpNewBuf, uint16_t _uiLen);
/*
*********************************************************************************************************
*   函 数 名: sf_CmpData
*   功能说明: sf_CmpData 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t sf_CmpData(uint32_t _uiSrcAddr, uint8_t *_ucpTar, uint32_t _uiSize);
/*
*********************************************************************************************************
*   函 数 名: sf_AutoWritePage
*   功能说明: sf_AutoWritePage 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t sf_AutoWritePage(uint8_t *_ucpSrc, uint32_t _uiWrAddr, uint16_t _usWrLen);

static uint8_t s_spiBuf[4*1024];	/* ����д�������ȶ�������page���޸Ļ�������������page��д */

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitSFlash
*	����˵��: ����falshӲ����ʼ���� ����CS GPIO�� ��ȡID��
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitSFlash(void)
{
	/* ����CS GPIO */
	{
		GPIO_InitTypeDef gpio_init;

		/* ��GPIOʱ�� */
		SF_CS_CLK_ENABLE();
		
		gpio_init.Mode = GPIO_MODE_OUTPUT_PP;		/* ����������� */
		gpio_init.Pull = GPIO_NOPULL;				/* ���������費ʹ�� */
		gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;  	/* GPIO�ٶȵȼ� */	
		gpio_init.Pin = SF_CS_PIN;	
		HAL_GPIO_Init(SF_CS_GPIO, &gpio_init);	
	}
	
	/* ��ȡоƬID, �Զ�ʶ��оƬ�ͺ� */
	sf_ReadInfo();
}	

/*
*********************************************************************************************************
*	�� �� ��: sf_SetCS
*	����˵��: ����FALSHƬѡ���ƺ���
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void sf_SetCS(uint8_t _Level)
{
	if (_Level == 0)
	{
		bsp_SpiBusEnter();	
		bsp_InitSPIParam(SPI_BAUDRATEPRESCALER_8, SPI_PHASE_1EDGE, SPI_POLARITY_LOW);		
		SF_CS_0();
	}
	else
	{		
		SF_CS_1();	
		bsp_SpiBusExit();		
	}
}

/*
*********************************************************************************************************
*	�� �� ��: sf_EraseSector
*	����˵��: ����ָ��������
*	��    ��:  _uiSectorAddr : ������ַ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void sf_EraseSector(uint32_t _uiSectorAddr)
{
	sf_WriteEnable();								/* ����дʹ������ */

	/* ������������ */
	sf_SetCS(0);									/* ʹ��Ƭѡ */
	g_spiLen = 0;
	g_spiTxBuf[g_spiLen++] = CMD_SE;						/* ���Ͳ������� */
	g_spiTxBuf[g_spiLen++] = ((_uiSectorAddr & 0xFF0000) >> 16);	/* ����������ַ�ĸ�8bit */
	g_spiTxBuf[g_spiLen++] = ((_uiSectorAddr & 0xFF00) >> 8);		/* ����������ַ�м�8bit */
	g_spiTxBuf[g_spiLen++] = (_uiSectorAddr & 0xFF);				/* ����������ַ��8bit */	
	bsp_spiTransfer();
	sf_SetCS(1);									/* ����Ƭѡ */

	sf_WaitForWriteEnd();							/* �ȴ�����Flash�ڲ�д������� */
}

/*
*********************************************************************************************************
*	�� �� ��: sf_EraseChip
*	����˵��: ��������оƬ
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void sf_EraseChip(void)
{	
	sf_WriteEnable();								/* ����дʹ������ */

	/* ������������ */
	sf_SetCS(0);		/* ʹ��Ƭѡ */
	g_spiLen = 0;
	g_spiTxBuf[g_spiLen++] = CMD_BE;				/* ������Ƭ�������� */
	bsp_spiTransfer();
	sf_SetCS(1);									/* ����Ƭѡ */

	sf_WaitForWriteEnd();							/* �ȴ�����Flash�ڲ�д������� */
}

/*
*********************************************************************************************************
*	�� �� ��: sf_PageWrite
*	����˵��: ��һ��page��д�������ֽڡ��ֽڸ������ܳ���ҳ���С��4K)
*	��    ��:  	_pBuf : ����Դ��������
*				_uiWriteAddr ��Ŀ�������׵�ַ
*				_usSize �����ݸ��������ܳ���ҳ���С
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void sf_PageWrite(uint8_t * _pBuf, uint32_t _uiWriteAddr, uint16_t _usSize)
{
	uint32_t i, j;

	if (g_tSF.ChipID == SST25VF016B_ID)
	{
		/* AAIָ��Ҫ��������ݸ�����ż�� */
		if ((_usSize < 2) && (_usSize % 2))
		{
			return ;
		}

		sf_WriteEnable();								/* ����дʹ������ */

		sf_SetCS(0);									/* ʹ��Ƭѡ */
		g_spiLen = 0;
		g_spiTxBuf[g_spiLen++] = CMD_AAI;								/* ����AAI����(��ַ�Զ����ӱ��) */
		g_spiTxBuf[g_spiLen++] = ((_uiWriteAddr & 0xFF0000) >> 16);		/* ����������ַ�ĸ�8bit */
		g_spiTxBuf[g_spiLen++] = ((_uiWriteAddr & 0xFF00) >> 8);		/* ����������ַ�м�8bit */
		g_spiTxBuf[g_spiLen++] = (_uiWriteAddr & 0xFF);					/* ����������ַ��8bit */		
		g_spiTxBuf[g_spiLen++] = (*_pBuf++);							/* ���͵�1������ */
		g_spiTxBuf[g_spiLen++] = (*_pBuf++);							/* ���͵�2������ */
		bsp_spiTransfer();
		sf_SetCS(1);									/* ����Ƭѡ */

		sf_WaitForWriteEnd();							/* �ȴ�����Flash�ڲ�д������� */

		_usSize -= 2;									/* ����ʣ���ֽ��� */

		for (i = 0; i < _usSize / 2; i++)
		{
			sf_SetCS(0);	/* ʹ��Ƭѡ */	
			g_spiLen = 0;
			g_spiTxBuf[g_spiLen++] = (CMD_AAI);						/* ����AAI����(��ַ�Զ����ӱ��) */
			g_spiTxBuf[g_spiLen++] = (*_pBuf++);					/* �������� */
			g_spiTxBuf[g_spiLen++] = (*_pBuf++);					/* �������� */
			bsp_spiTransfer();
			sf_SetCS(1);								/* ����Ƭѡ */
			sf_WaitForWriteEnd();						/* �ȴ�����Flash�ڲ�д������� */
		}

		/* ����д����״̬ */
		sf_SetCS(0);
		g_spiLen = 0;
		g_spiTxBuf[g_spiLen++] = (CMD_DISWR);
		bsp_spiTransfer();
		sf_SetCS(1);

		sf_WaitForWriteEnd();							/* �ȴ�����Flash�ڲ�д������� */
	}
	else	/* for MX25L1606E �� W25Q64BV */
	{
		for (j = 0; j < _usSize / 256; j++)
		{
			sf_WriteEnable();								/* ����дʹ������ */

			sf_SetCS(0);									/* ʹ��Ƭѡ */
			g_spiLen = 0;
			g_spiTxBuf[g_spiLen++] = (0x02);								/* ����AAI����(��ַ�Զ����ӱ��) */
			g_spiTxBuf[g_spiLen++] = ((_uiWriteAddr & 0xFF0000) >> 16);		/* ����������ַ�ĸ�8bit */
			g_spiTxBuf[g_spiLen++] = ((_uiWriteAddr & 0xFF00) >> 8);		/* ����������ַ�м�8bit */
			g_spiTxBuf[g_spiLen++] = (_uiWriteAddr & 0xFF);					/* ����������ַ��8bit */
			for (i = 0; i < 256; i++)
			{
				g_spiTxBuf[g_spiLen++] = (*_pBuf++);		/* �������� */
			}
			bsp_spiTransfer();
			sf_SetCS(1);								/* ��ֹƬѡ */

			sf_WaitForWriteEnd();						/* �ȴ�����Flash�ڲ�д������� */

			_uiWriteAddr += 256;
		}

		/* ����д����״̬ */
		sf_SetCS(0);
		g_spiLen = 0;
		g_spiTxBuf[g_spiLen++] = (CMD_DISWR);
		bsp_spiTransfer();
		sf_SetCS(1);

		sf_WaitForWriteEnd();							/* �ȴ�����Flash�ڲ�д������� */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: sf_ReadBuffer
*	����˵��: ������ȡ�����ֽڡ��ֽڸ������ܳ���оƬ������
*	��    ��:  	_pBuf : ����Դ��������
*				_uiReadAddr ���׵�ַ
*				_usSize �����ݸ���, ���Դ���PAGE_SIZE,���ǲ��ܳ���оƬ������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void sf_ReadBuffer(uint8_t * _pBuf, uint32_t _uiReadAddr, uint32_t _uiSize)
{
	uint16_t rem;
	uint16_t i;
	
	/* �����ȡ�����ݳ���Ϊ0���߳�������Flash��ַ�ռ䣬��ֱ�ӷ��� */
	if ((_uiSize == 0) ||(_uiReadAddr + _uiSize) > g_tSF.TotalSize)
	{
		return;
	}

	/* ������������ */
	sf_SetCS(0);									/* ʹ��Ƭѡ */
	g_spiLen = 0;
	g_spiTxBuf[g_spiLen++] = (CMD_READ);							/* ���Ͷ����� */
	g_spiTxBuf[g_spiLen++] = ((_uiReadAddr & 0xFF0000) >> 16);	/* ����������ַ�ĸ�8bit */
	g_spiTxBuf[g_spiLen++] = ((_uiReadAddr & 0xFF00) >> 8);		/* ����������ַ�м�8bit */
	g_spiTxBuf[g_spiLen++] = (_uiReadAddr & 0xFF);				/* ����������ַ��8bit */
	bsp_spiTransfer();
	
	/* ��ʼ�����ݣ����ʵײ�DMA���������ޣ�����ְ��� */
	for (i = 0; i < _uiSize / SPI_BUFFER_SIZE; i++)
	{
		g_spiLen = SPI_BUFFER_SIZE;
		bsp_spiTransfer();
		
		memcpy(_pBuf, g_spiRxBuf, SPI_BUFFER_SIZE);
		_pBuf += SPI_BUFFER_SIZE;
	}
	
	rem = _uiSize % SPI_BUFFER_SIZE;	/* ʣ���ֽ� */
	if (rem > 0)
	{
		g_spiLen = rem;
		bsp_spiTransfer();
		
		memcpy(_pBuf, g_spiRxBuf, rem);
	}
	
	sf_SetCS(1);									/* ����Ƭѡ */
}

/*
*********************************************************************************************************
*	�� �� ��: sf_CmpData
*	����˵��: �Ƚ�Flash������.
*	��    ��:  	_ucpTar : ���ݻ�����
*				_uiSrcAddr ��Flash��ַ
*				_uiSize �����ݸ���, ���Դ���PAGE_SIZE,���ǲ��ܳ���оƬ������
*	�� �� ֵ: 0 = ���, 1 = ����
*********************************************************************************************************
*/
static uint8_t sf_CmpData(uint32_t _uiSrcAddr, uint8_t *_ucpTar, uint32_t _uiSize)
{
	uint16_t i, j;
	uint16_t rem;

	/* �����ȡ�����ݳ���Ϊ0���߳�������Flash��ַ�ռ䣬��ֱ�ӷ��� */
	if ((_uiSrcAddr + _uiSize) > g_tSF.TotalSize)
	{
		return 1;
	}

	if (_uiSize == 0)
	{
		return 0;
	}

	sf_SetCS(0);									/* ʹ��Ƭѡ */
	g_spiLen = 0;
	g_spiTxBuf[g_spiLen++] = (CMD_READ);							/* ���Ͷ����� */
	g_spiTxBuf[g_spiLen++] = ((_uiSrcAddr & 0xFF0000) >> 16);		/* ����������ַ�ĸ�8bit */
	g_spiTxBuf[g_spiLen++] = ((_uiSrcAddr & 0xFF00) >> 8);			/* ����������ַ�м�8bit */
	g_spiTxBuf[g_spiLen++] = (_uiSrcAddr & 0xFF);					/* ����������ַ��8bit */
	bsp_spiTransfer();
	
	/* ��ʼ�����ݣ���Ϊ�ײ�DMA���������ޣ�����ְ��� */
	for (i = 0; i < _uiSize / SPI_BUFFER_SIZE; i++)
	{
		g_spiLen = SPI_BUFFER_SIZE;
		bsp_spiTransfer();
		
		for (j = 0; j < SPI_BUFFER_SIZE; j++)
		{
			if (g_spiRxBuf[j] != *_ucpTar++)
			{
				goto NOTEQ;		/* ����� */
			}
		}
	}
	
	rem = _uiSize % SPI_BUFFER_SIZE;	/* ʣ���ֽ� */
	if (rem > 0)
	{
		g_spiLen = rem;
		bsp_spiTransfer();
		
		for (j = 0; j < rem; j++)
		{
			if (g_spiRxBuf[j] != *_ucpTar++)
			{
				goto NOTEQ;		/* ����� */
			}
		}
	}
	sf_SetCS(1);
	return 0;		/* ��� */
	
NOTEQ:	
	sf_SetCS(1);	/* ����� */
	return 1;
}

/*
*********************************************************************************************************
*	�� �� ��: sf_NeedErase
*	����˵��: �ж�дPAGEǰ�Ƿ���Ҫ�Ȳ�����
*	��    ��:   _ucpOldBuf �� ������
*			   _ucpNewBuf �� ������
*			   _uiLen �����ݸ��������ܳ���ҳ���С
*	�� �� ֵ: 0 : ����Ҫ������ 1 ����Ҫ����
*********************************************************************************************************
*/
static uint8_t sf_NeedErase(uint8_t * _ucpOldBuf, uint8_t *_ucpNewBuf, uint16_t _usLen)
{
	uint16_t i;
	uint8_t ucOld;

	/*
	�㷨��1����old ��, new ����
	      old    new
		  1101   0101
	~     1111
		= 0010   0101

	�㷨��2��: old �󷴵Ľ���� new λ��
		  0010   old
	&	  0101   new
		 =0000

	�㷨��3��: ���Ϊ0,���ʾ�������. �����ʾ��Ҫ����
	*/

	for (i = 0; i < _usLen; i++)
	{
		ucOld = *_ucpOldBuf++;
		ucOld = ~ucOld;

		/* ע������д��: if (ucOld & (*_ucpNewBuf++) != 0) */
		if ((ucOld & (*_ucpNewBuf++)) != 0)
		{
			return 1;
		}
	}
	return 0;
}

/*
*********************************************************************************************************
*	�� �� ��: sf_AutoWritePage
*	����˵��: д1��PAGE��У��,�������ȷ������д���Ρ��������Զ���ɲ���������
*	��    ��:  	_pBuf : ����Դ��������
*				_uiWriteAddr ��Ŀ�������׵�ַ
*				_usSize �����ݸ��������ܳ���ҳ���С
*	�� �� ֵ: 0 : ���� 1 �� �ɹ�
*********************************************************************************************************
*/
static uint8_t sf_AutoWritePage(uint8_t *_ucpSrc, uint32_t _uiWrAddr, uint16_t _usWrLen)
{
	uint16_t i;
	uint16_t j;					/* ������ʱ */
	uint32_t uiFirstAddr;		/* ������ַ */
	uint8_t ucNeedErase;		/* 1��ʾ��Ҫ���� */
	uint8_t cRet;

	/* ����Ϊ0ʱ����������,ֱ����Ϊ�ɹ� */
	if (_usWrLen == 0)
	{
		return 1;
	}

	/* ���ƫ�Ƶ�ַ����оƬ�������˳� */
	if (_uiWrAddr >= g_tSF.TotalSize)
	{
		return 0;
	}

	/* ������ݳ��ȴ����������������˳� */
	if (_usWrLen > g_tSF.PageSize)
	{
		return 0;
	}

	/* ���FLASH�е�����û�б仯,��дFLASH */
	sf_ReadBuffer(s_spiBuf, _uiWrAddr, _usWrLen);
	if (memcmp(s_spiBuf, _ucpSrc, _usWrLen) == 0)
	{
		return 1;
	}

	/* �ж��Ƿ���Ҫ�Ȳ������� */
	/* ����������޸�Ϊ�����ݣ�����λ���� 1->0 ���� 0->0, ���������,���Flash���� */
	ucNeedErase = 0;
	if (sf_NeedErase(s_spiBuf, _ucpSrc, _usWrLen))
	{
		ucNeedErase = 1;
	}

	uiFirstAddr = _uiWrAddr & (~(g_tSF.PageSize - 1));

	if (_usWrLen == g_tSF.PageSize)		/* ������������д */
	{
		for	(i = 0; i < g_tSF.PageSize; i++)
		{
			s_spiBuf[i] = _ucpSrc[i];
		}
	}
	else						/* ��д�������� */
	{
		/* �Ƚ��������������ݶ��� */
		sf_ReadBuffer(s_spiBuf, uiFirstAddr, g_tSF.PageSize);

		/* ���������ݸ��� */
		i = _uiWrAddr & (g_tSF.PageSize - 1);
		memcpy(&s_spiBuf[i], _ucpSrc, _usWrLen);
	}

	/* д��֮�����У�飬�������ȷ����д�����3�� */
	cRet = 0;
	for (i = 0; i < 3; i++)
	{

		/* ����������޸�Ϊ�����ݣ�����λ���� 1->0 ���� 0->0, ���������,���Flash���� */
		if (ucNeedErase == 1)
		{
			sf_EraseSector(uiFirstAddr);		/* ����1������ */
		}

		/* ���һ��PAGE */
		sf_PageWrite(s_spiBuf, uiFirstAddr, g_tSF.PageSize);

		if (sf_CmpData(_uiWrAddr, _ucpSrc, _usWrLen) == 0)
		{
			cRet = 1;
			break;
		}
		else
		{
			if (sf_CmpData(_uiWrAddr, _ucpSrc, _usWrLen) == 0)
			{
				cRet = 1;
				break;
			}

			/* ʧ�ܺ��ӳ�һ��ʱ�������� */
			for (j = 0; j < 10000; j++);
		}
	}

	return cRet;
}

/*
*********************************************************************************************************
*	�� �� ��: sf_WriteBuffer
*	����˵��: д1��������У��,�������ȷ������д���Ρ��������Զ���ɲ���������
*	��    ��:  	_pBuf : ����Դ��������
*				_uiWrAddr ��Ŀ�������׵�ַ
*				_usSize �����ݸ��������ܳ���ҳ���С
*	�� �� ֵ: 1 : �ɹ��� 0 �� ʧ��
*********************************************************************************************************
*/
uint8_t sf_WriteBuffer(uint8_t* _pBuf, uint32_t _uiWriteAddr, uint16_t _usWriteSize)
{
	uint16_t NumOfPage = 0, NumOfSingle = 0, Addr = 0, count = 0, temp = 0;

	Addr = _uiWriteAddr % g_tSF.PageSize;
	count = g_tSF.PageSize - Addr;
	NumOfPage =  _usWriteSize / g_tSF.PageSize;
	NumOfSingle = _usWriteSize % g_tSF.PageSize;

	if (Addr == 0) /* ��ʼ��ַ��ҳ���׵�ַ  */
	{
		if (NumOfPage == 0) /* ���ݳ���С��ҳ���С */
		{
			if (sf_AutoWritePage(_pBuf, _uiWriteAddr, _usWriteSize) == 0)
			{
				return 0;
			}
		}
		else 	/* ���ݳ��ȴ��ڵ���ҳ���С */
		{
			while (NumOfPage--)
			{
				if (sf_AutoWritePage(_pBuf, _uiWriteAddr, g_tSF.PageSize) == 0)
				{
					return 0;
				}
				_uiWriteAddr +=  g_tSF.PageSize;
				_pBuf += g_tSF.PageSize;
			}
			if (sf_AutoWritePage(_pBuf, _uiWriteAddr, NumOfSingle) == 0)
			{
				return 0;
			}
		}
	}
	else  /* ��ʼ��ַ����ҳ���׵�ַ  */
	{
		if (NumOfPage == 0) /* ���ݳ���С��ҳ���С */
		{
			if (NumOfSingle > count) /* (_usWriteSize + _uiWriteAddr) > SPI_FLASH_PAGESIZE */
			{
				temp = NumOfSingle - count;

				if (sf_AutoWritePage(_pBuf, _uiWriteAddr, count) == 0)
				{
					return 0;
				}

				_uiWriteAddr +=  count;
				_pBuf += count;

				if (sf_AutoWritePage(_pBuf, _uiWriteAddr, temp) == 0)
				{
					return 0;
				}
			}
			else
			{
				if (sf_AutoWritePage(_pBuf, _uiWriteAddr, _usWriteSize) == 0)
				{
					return 0;
				}
			}
		}
		else	/* ���ݳ��ȴ��ڵ���ҳ���С */
		{
			_usWriteSize -= count;
			NumOfPage =  _usWriteSize / g_tSF.PageSize;
			NumOfSingle = _usWriteSize % g_tSF.PageSize;

			if (sf_AutoWritePage(_pBuf, _uiWriteAddr, count) == 0)
			{
				return 0;
			}

			_uiWriteAddr +=  count;
			_pBuf += count;

			while (NumOfPage--)
			{
				if (sf_AutoWritePage(_pBuf, _uiWriteAddr, g_tSF.PageSize) == 0)
				{
					return 0;
				}
				_uiWriteAddr +=  g_tSF.PageSize;
				_pBuf += g_tSF.PageSize;
			}

			if (NumOfSingle != 0)
			{
				if (sf_AutoWritePage(_pBuf, _uiWriteAddr, NumOfSingle) == 0)
				{
					return 0;
				}
			}
		}
	}
	return 1;	/* �ɹ� */
}

/*
*********************************************************************************************************
*	�� �� ��: sf_ReadID
*	����˵��: ��ȡ����ID
*	��    ��:  ��
*	�� �� ֵ: 32bit������ID (���8bit��0����ЧIDλ��Ϊ24bit��
*********************************************************************************************************
*/
uint32_t sf_ReadID(void)
{
	uint32_t uiID;
	uint8_t id1, id2, id3;

	sf_SetCS(0);									/* ʹ��Ƭѡ */
	g_spiLen = 0;
	g_spiTxBuf[0] = (CMD_RDID);								/* ���Ͷ�ID���� */
	g_spiLen = 4;
	bsp_spiTransfer();
	
	id1 = g_spiRxBuf[1];					/* ��ID�ĵ�1���ֽ� */
	id2 = g_spiRxBuf[2];					/* ��ID�ĵ�2���ֽ� */
	id3 = g_spiRxBuf[3];					/* ��ID�ĵ�3���ֽ� */
	sf_SetCS(1);									/* ����Ƭѡ */

	uiID = ((uint32_t)id1 << 16) | ((uint32_t)id2 << 8) | id3;

	return uiID;
}

/*
*********************************************************************************************************
*	�� �� ��: sf_ReadInfo
*	����˵��: ��ȡ����ID,�������������
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void sf_ReadInfo(void)
{
	/* �Զ�ʶ����Flash�ͺ� */
	{
		g_tSF.ChipID = sf_ReadID();	/* оƬID */

		switch (g_tSF.ChipID)
		{
			case SST25VF016B_ID:
				strcpy(g_tSF.ChipName, "SST25VF016B");
				g_tSF.TotalSize = 2 * 1024 * 1024;	/* ������ = 2M */
				g_tSF.PageSize = 4 * 1024;			/* ҳ���С = 4K */
				break;

			case MX25L1606E_ID:
				strcpy(g_tSF.ChipName, "MX25L1606E");
				g_tSF.TotalSize = 2 * 1024 * 1024;	/* ������ = 2M */
				g_tSF.PageSize = 4 * 1024;			/* ҳ���С = 4K */
				break;

			case W25Q64BV_ID:
				strcpy(g_tSF.ChipName, "W25Q64");
				g_tSF.TotalSize = 8 * 1024 * 1024;	/* ������ = 8M */
				g_tSF.PageSize = 4 * 1024;			/* ҳ���С = 4K */
				break;
			
			case W25Q128_ID:
				strcpy(g_tSF.ChipName, "W25Q128");
				g_tSF.TotalSize = 16 * 1024 * 1024;	/* ������ = 8M */
				g_tSF.PageSize = 4 * 1024;			/* ҳ���С = 4K */
				break;			

			default:
				strcpy(g_tSF.ChipName, "Unknow Flash");
				g_tSF.TotalSize = 2 * 1024 * 1024;
				g_tSF.PageSize = 4 * 1024;
				break;
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: sf_WriteEnable
*	����˵��: ����������дʹ������
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void sf_WriteEnable(void)
{
	sf_SetCS(0);									/* ʹ��Ƭѡ */
	g_spiLen = 0;
	g_spiTxBuf[g_spiLen++] = (CMD_WREN);								/* �������� */
	bsp_spiTransfer();
	sf_SetCS(1);									/* ����Ƭѡ */
}

#if 0
/*
*********************************************************************************************************
*	�� �� ��: sf_WriteStatus
*	����˵��: д״̬�Ĵ���
*	��    ��:  _ucValue : ״̬�Ĵ�����ֵ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void sf_WriteStatus(uint8_t _ucValue)
{
	if (g_tSF.ChipID == SST25VF016B_ID)
	{
		/* ��1������ʹ��д״̬�Ĵ��� */
		sf_SetCS(0);									/* ʹ��Ƭѡ */
		g_spiLen = 0;
		g_spiTxBuf[g_spiLen++] = (CMD_EWRSR);							/* ������� ����д״̬�Ĵ��� */
		bsp_spiTransfer();
		sf_SetCS(1);									/* ����Ƭѡ */

		/* ��2������д״̬�Ĵ��� */
		sf_SetCS(0);									/* ʹ��Ƭѡ */
		g_spiLen = 0;
		g_spiTxBuf[g_spiLen++] = (CMD_WRSR);							/* ������� д״̬�Ĵ��� */
		g_spiTxBuf[g_spiLen++] = (_ucValue);							/* �������ݣ�״̬�Ĵ�����ֵ */
		bsp_spiTransfer();
		sf_SetCS(1);									/* ����Ƭѡ */
	}
	else
	{
		sf_SetCS(0);									/* ʹ��Ƭѡ */
		g_spiLen = 0;
		g_spiTxBuf[g_spiLen++] = (CMD_WRSR);							/* ������� д״̬�Ĵ��� */
		g_spiTxBuf[g_spiLen++] = (_ucValue);							/* �������ݣ�״̬�Ĵ�����ֵ */
		bsp_spiTransfer();
		sf_SetCS(1);									/* ����Ƭѡ */
	}
}
#endif

/*
*********************************************************************************************************
*	�� �� ��: sf_WaitForWriteEnd
*	����˵��: ����ѭ����ѯ�ķ�ʽ�ȴ������ڲ�д�������
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void sf_WaitForWriteEnd(void)
{
	sf_SetCS(0);									/* ʹ��Ƭѡ */
	g_spiTxBuf[0] = (CMD_RDSR);							/* ������� ��״̬�Ĵ��� */
	g_spiLen = 2;
	bsp_spiTransfer();	
	sf_SetCS(1);									/* ����Ƭѡ */
	
	while(1)
	{
		sf_SetCS(0);									/* ʹ��Ƭѡ */
		g_spiTxBuf[0] = (CMD_RDSR);						/* ������� ��״̬�Ĵ��� */
		g_spiTxBuf[1] = 0;		/* �޹����� */
		g_spiLen = 2;
		bsp_spiTransfer();	
		sf_SetCS(1);									/* ����Ƭѡ */
		
		if ((g_spiRxBuf[1] & WIP_FLAG) != SET)	/* �ж�״̬�Ĵ�����æ��־λ */
		{
			break;
		}		
	}	
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
