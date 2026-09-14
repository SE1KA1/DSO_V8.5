/**
 * @file bsp_cpu_flash.c
 * @brief 安富莱 BSP 通用驱动（内部 Flash）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : cpu�ڲ�falsh����ģ��(for F4)
*	�ļ����� : bsp_cpu_flash.c
*	��    �� : V1.0
*	˵    �� : �ṩ��дCPU�ڲ�Flash�ĺ���
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2013-02-01 armfly  ��ʽ����
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/*
*********************************************************************************************************
*	�� �� ��: bsp_GetSector
*	����˵��: ���ݵ�ַ���������׵�ַ
*	��    ��:  ��
*	�� �� ֵ: �����׵�ַ
*********************************************************************************************************
*/
uint32_t bsp_GetSector(uint32_t Address)
{
	uint32_t sector = 0;

	if((Address < ADDR_FLASH_SECTOR_1) && (Address >= ADDR_FLASH_SECTOR_0))
	{
		sector = FLASH_Sector_0;
	}
	else if((Address < ADDR_FLASH_SECTOR_2) && (Address >= ADDR_FLASH_SECTOR_1))
	{
		sector = FLASH_Sector_1;
	}
	else if((Address < ADDR_FLASH_SECTOR_3) && (Address >= ADDR_FLASH_SECTOR_2))
	{
		sector = FLASH_Sector_2;
	}
	else if((Address < ADDR_FLASH_SECTOR_4) && (Address >= ADDR_FLASH_SECTOR_3))
	{
		sector = FLASH_Sector_3;
	}
	else if((Address < ADDR_FLASH_SECTOR_5) && (Address >= ADDR_FLASH_SECTOR_4))
	{
		sector = FLASH_Sector_4;
	}
	else if((Address < ADDR_FLASH_SECTOR_6) && (Address >= ADDR_FLASH_SECTOR_5))
	{
		sector = FLASH_Sector_5;
	}
	else if((Address < ADDR_FLASH_SECTOR_7) && (Address >= ADDR_FLASH_SECTOR_6))
	{
		sector = FLASH_Sector_6;
	}
	else if((Address < ADDR_FLASH_SECTOR_8) && (Address >= ADDR_FLASH_SECTOR_7))
	{
		sector = FLASH_Sector_7;
	}
	else if((Address < ADDR_FLASH_SECTOR_9) && (Address >= ADDR_FLASH_SECTOR_8))
	{
		sector = FLASH_Sector_8;
	}
	else if((Address < ADDR_FLASH_SECTOR_10) && (Address >= ADDR_FLASH_SECTOR_9))
	{
		sector = FLASH_Sector_9;
	}
	else if((Address < ADDR_FLASH_SECTOR_11) && (Address >= ADDR_FLASH_SECTOR_10))
	{
		sector = FLASH_Sector_10;
	}
	else	/*(Address < FLASH_END_ADDR) && (Address >= ADDR_FLASH_SECTOR_11))*/
	{
		sector = FLASH_Sector_11;
	}

	return sector;
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_ReadCpuFlash
*	����˵��: ��ȡCPU Flash������
*	��    ��:  _ucpDst : Ŀ�껺����
*			 _ulFlashAddr : ��ʼ��ַ
*			 _ulSize : ���ݴ�С����λ���ֽڣ�
*	�� �� ֵ: 0=�ɹ���1=ʧ��
*********************************************************************************************************
*/
uint8_t bsp_ReadCpuFlash(uint32_t _ulFlashAddr, uint8_t *_ucpDst, uint32_t _ulSize)
{
	uint32_t i;

	/* ���ƫ�Ƶ�ַ����оƬ�������򲻸�д��������� */
	if (_ulFlashAddr + _ulSize > FLASH_BASE_ADDR + FLASH_SIZE)
	{
		return 1;
	}

	/* ����Ϊ0ʱ����������,������ʼ��ַΪ���ַ����� */
	if (_ulSize == 0)
	{
		return 1;
	}

	for (i = 0; i < _ulSize; i++)
	{
		*_ucpDst++ = *(uint8_t *)_ulFlashAddr++;
	}

	return 0;
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_CmpCpuFlash
*	����˵��: �Ƚ�Flashָ����ַ������.
*	��    ��: _ulFlashAddr : Flash��ַ
*			 _ucpBuf : ���ݻ�����
*			 _ulSize : ���ݴ�С����λ���ֽڣ�
*	�� �� ֵ:
*			FLASH_IS_EQU		0   Flash���ݺʹ�д���������ȣ�����Ҫ������д����
*			FLASH_REQ_WRITE		1	Flash����Ҫ������ֱ��д
*			FLASH_REQ_ERASE		2	Flash��Ҫ�Ȳ���,��д
*			FLASH_PARAM_ERR		3	������������
*********************************************************************************************************
*/
uint8_t bsp_CmpCpuFlash(uint32_t _ulFlashAddr, uint8_t *_ucpBuf, uint32_t _ulSize)
{
	uint32_t i;
	uint8_t ucIsEqu;	/* ��ȱ�־ */
	uint8_t ucByte;

	/* ���ƫ�Ƶ�ַ����оƬ�������򲻸�д��������� */
	if (_ulFlashAddr + _ulSize > FLASH_BASE_ADDR + FLASH_SIZE)
	{
		return FLASH_PARAM_ERR;		/*��������������*/
	}

	/* ����Ϊ0ʱ������ȷ */
	if (_ulSize == 0)
	{
		return FLASH_IS_EQU;		/* Flash���ݺʹ�д���������� */
	}

	ucIsEqu = 1;			/* �ȼ��������ֽںʹ�д���������ȣ���������κ�һ������ȣ�������Ϊ 0 */
	for (i = 0; i < _ulSize; i++)
	{
		ucByte = *(uint8_t *)_ulFlashAddr;

		if (ucByte != *_ucpBuf)
		{
			if (ucByte != 0xFF)
			{
				return FLASH_REQ_ERASE;		/* ��Ҫ��������д */
			}
			else
			{
				ucIsEqu = 0;	/* ����ȣ���Ҫд */
			}
		}

		_ulFlashAddr++;
		_ucpBuf++;
	}

	if (ucIsEqu == 1)
	{
		return FLASH_IS_EQU;	/* Flash���ݺʹ�д���������ȣ�����Ҫ������д���� */
	}
	else
	{
		return FLASH_REQ_WRITE;	/* Flash����Ҫ������ֱ��д */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_WriteCpuFlash
*	����˵��: д���ݵ�CPU �ڲ�Flash��
*	��    ��: _ulFlashAddr : Flash��ַ
*			 _ucpSrc : ���ݻ�����
*			 _ulSize : ���ݴ�С����λ���ֽڣ�
*	�� �� ֵ: 0-�ɹ���1-���ݳ��Ȼ��ַ�����2-дFlash����(����Flash������)
*********************************************************************************************************
*/
uint8_t bsp_WriteCpuFlash(uint32_t _ulFlashAddr, uint8_t *_ucpSrc, uint32_t _ulSize)
{
	uint32_t i;
	uint8_t ucRet;

	/* ���ƫ�Ƶ�ַ����оƬ�������򲻸�д��������� */
	if (_ulFlashAddr + _ulSize > FLASH_BASE_ADDR + FLASH_SIZE)
	{
		return 1;
	}

	/* ����Ϊ0ʱ����������  */
	if (_ulSize == 0)
	{
		return 0;
	}

	ucRet = bsp_CmpCpuFlash(_ulFlashAddr, _ucpSrc, _ulSize);

	if (ucRet == FLASH_IS_EQU)
	{
		return 0;
	}

	__set_PRIMASK(1);  		/* ���ж� */

	/* FLASH ���� */
	FLASH_Unlock();

  	/* Clear pending flags (if any) */
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                  FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR);

	/* ��Ҫ���� */
	if (ucRet == FLASH_REQ_ERASE)
	{
		FLASH_EraseSector(bsp_GetSector(_ulFlashAddr), VoltageRange_3);
	}

	/* ���ֽ�ģʽ��̣�Ϊ���Ч�ʣ����԰��ֱ�̣�һ��д��4�ֽڣ� */
	for (i = 0; i < _ulSize; i++)
	{
		FLASH_ProgramByte(_ulFlashAddr++, *_ucpSrc++);
	}

  	/* Flash ��������ֹдFlash���ƼĴ��� */
  	FLASH_Lock();

  	__set_PRIMASK(0);  		/* ���ж� */

	return 0;
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
