/**
 * @file bsp_i2c_wm8978.c
 * @brief 安富莱 BSP 通用驱动（WM8978 音频 CODEC）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : WM8978��ƵоƬ����ģ��
*	�ļ����� : bsp_wm8978.h
*	��    �� : V1.5
*	˵    �� : WM8978��ƵоƬ��STM32 I2S�ײ��������ڰ�����STM32-V5�������ϵ���ͨ����
*
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2013-02-01 armfly  ��ʽ����
*		V1.1    2013-06-12 armfly  �������Line ���벻�ܷ��������⡣�޸� wm8978_CfgAudioPath() ����
*		V1.2    2013-07-14 armfly  ��������Line����ӿ�����ĺ����� wm8978_SetLineGain()
*		V1.3    2015-10-18 armfly  ��ֲ��STM32F429���Ķ��ܴ�I2S�ӿ��޸�ΪSAI��Ƶ�ӿڡ�
*							-  wm8978_CfgAudioIF() �������ֳ��βΣ�����20bit
*		V1.4	2016-01-24 armfly   ����AUDIO_Play ������bug������δ��ʼ����ʹ���ˡ�
*		V1.5	2016-03-25 armfly   ����AUDIO_Record������bug������δ��ʼ����ʹ���ˡ����¼����Ҫ2�ε�����
*
*	Copyright (C), 2015-2020, ���������� www.armfly.com
*
*********************************************************************************************************
*/
#include "bsp.h"
#include <math.h>

/*
*********************************************************************************************************
*
*	��Ҫ��ʾ:
*	1��wm8978_ ��ͷ�ĺ����ǲ���WM8978�Ĵ���������WM8978�Ĵ�����ͨ��I2Cģ�����߽��е�
*	2��I2S_ ��ͷ�ĺ����ǲ���STM32  I2S��ؼĴ���
*	3��ʵ��¼�������Ӧ�ã���Ҫͬʱ����WM8978��STM32��I2S��
*	4�����ֺ����õ����βεĶ�����ST�̼����У����磺I2S_Standard_Phillips��I2S_Standard_MSB��I2S_Standard_LSB
*			  I2S_MCLKOutput_Enable��I2S_MCLKOutput_Disable
*			  I2S_AudioFreq_8K��I2S_AudioFreq_16K��I2S_AudioFreq_22K��I2S_AudioFreq_44K��I2S_AudioFreq_48
*			  I2S_Mode_MasterTx��I2S_Mode_MasterRx
*	5��ע���� pdf ָ���� wm8978.pdf �����ֲᣬwm8978de�Ĵ����ܶ࣬�õ��ļĴ�����ע��pdf�ļ���ҳ�룬���ڲ�ѯ
*
*********************************************************************************************************
*/

/* 
	������STM32-V7������---  SAI�ӿ� I2S���ߴ�����Ƶ���ݿ���

	PE4/SAI1_FS_A/DCMI_D4/NRF24L01_IRQ
	PE5/SAI1_SCK_B/DCMI_D6/AD7606_BUSY
	PE6/SAI1_SD_A/DCMI_D7/NRF905_CD		ADC ¼��
	PE3/SAI1_SD_B/NRF905_AM				DAC ����
	PE2/SAI1_MCLK_A


	STM32��SAI����Ϊ��ģʽ��SAIT_Block_A �� SAIT_Block_B ͬ��ģʽ����������SAIT_Block_B��Ϊ��ģ�����ʱ��.
	
	��ģ�� SAIT_Block_B �� SAI1_SD_B �������ڷ�������ģ�� SAIT_Block_A��SAI1_SD_A����¼����
	
	���ñ�׼I2SЭ�顣

    ��Ƶģ�������Ϊ��ڶ�����Ƶģ��ͬ��������������£�������λʱ�Ӻ�֡ͬ���źţ��Լ���ͨ��ʱռ���ⲿ���ŵ�����������Ϊ����һ��ģ��ͬ������Ƶģ�齫�ͷ��� SCK_x��
FS_x �� MCLK_x ���������� GPIO

*/

/* ����¼������������ */
#define REC_MEM_ADDR	SDRAM_APP_BUF
/* 宏定义: REC_MEM_SIZE - REC_MEM_SIZE 的宏定义。 */
#define REC_MEM_SIZE	(2 * 1024 * 1024)

/* 宏定义: EVAL_AUDIO_IRQ_PREPRIO - EVAL_AUDIO_IRQ_PREPRIO 的宏定义。 */
#define  EVAL_AUDIO_IRQ_PREPRIO  3
/* 宏定义: EVAL_AUDIO_IRQ_SUBRIO - EVAL_AUDIO_IRQ_SUBRIO 的宏定义。 */
#define  EVAL_AUDIO_IRQ_SUBRIO	 0

/*************** SAI ����ͨ��GPIO DMA���� ***************/
#define SAI_OUT                          SAI1_Block_A
/* 宏定义: SAI_OUT_CLK_ENABLE - SAI_OUT_CLK_ENABLE 的宏定义。 */
#define SAI_OUT_CLK_ENABLE()              __HAL_RCC_SAI1_CLK_ENABLE()

/* 宏定义: SAI_OUT_FS_GPIO_PORT - SAI_OUT_FS_GPIO_PORT 的宏定义。 */
#define SAI_OUT_FS_GPIO_PORT              GPIOE
/* 宏定义: SAI_OUT_FS_AF - SAI_OUT_FS_AF 的宏定义。 */
#define SAI_OUT_FS_AF                     GPIO_AF6_SAI1
/* 宏定义: SAI_OUT_FS_PIN - SAI_OUT_FS_PIN 的宏定义。 */
#define SAI_OUT_FS_PIN                    GPIO_PIN_4

/* 宏定义: SAI_OUT_SCK_GPIO_PORT - SAI_OUT_SCK_GPIO_PORT 的宏定义。 */
#define SAI_OUT_SCK_GPIO_PORT             GPIOE
/* 宏定义: SAI_OUT_SCK_AF - SAI_OUT_SCK_AF 的宏定义。 */
#define SAI_OUT_SCK_AF                    GPIO_AF6_SAI1
/* 宏定义: SAI_OUT_SCK_PIN - SAI_OUT_SCK_PIN 的宏定义。 */
#define SAI_OUT_SCK_PIN                   GPIO_PIN_5

/* 宏定义: SAI_OUT_SD_GPIO_PORT - SAI_OUT_SD_GPIO_PORT 的宏定义。 */
#define SAI_OUT_SD_GPIO_PORT              GPIOE
/* 宏定义: SAI_OUT_SD_AF - SAI_OUT_SD_AF 的宏定义。 */
#define SAI_OUT_SD_AF                     GPIO_AF6_SAI1
/* 宏定义: SAI_OUT_SD_PIN - SAI_OUT_SD_PIN 的宏定义。 */
#define SAI_OUT_SD_PIN                    GPIO_PIN_6

/* 宏定义: SAI_OUT_MCLK_GPIO_PORT - SAI_OUT_MCLK_GPIO_PORT 的宏定义。 */
#define SAI_OUT_MCLK_GPIO_PORT            GPIOE
/* 宏定义: SAI_OUT_MCLK_AF - SAI_OUT_MCLK_AF 的宏定义。 */
#define SAI_OUT_MCLK_AF                   GPIO_AF6_SAI1
/* 宏定义: SAI_OUT_MCLK_PIN - SAI_OUT_MCLK_PIN 的宏定义。 */
#define SAI_OUT_MCLK_PIN                  GPIO_PIN_2

/* 宏定义: SAI_OUT_MCLK_ENABLE - SAI_OUT_MCLK_ENABLE 的宏定义。 */
#define SAI_OUT_MCLK_ENABLE()             __HAL_RCC_GPIOE_CLK_ENABLE()
/* 宏定义: SAI_OUT_SCK_ENABLE - SAI_OUT_SCK_ENABLE 的宏定义。 */
#define SAI_OUT_SCK_ENABLE()              __HAL_RCC_GPIOE_CLK_ENABLE()
/* 宏定义: SAI_OUT_FS_ENABLE - SAI_OUT_FS_ENABLE 的宏定义。 */
#define SAI_OUT_FS_ENABLE()               __HAL_RCC_GPIOE_CLK_ENABLE()
/* 宏定义: SAI_OUT_SD_ENABLE - SAI_OUT_SD_ENABLE 的宏定义。 */
#define SAI_OUT_SD_ENABLE()               __HAL_RCC_GPIOE_CLK_ENABLE()

SAI_HandleTypeDef   SaiOutHandle;
DMA_HandleTypeDef   hSaiOutDma;

/* 宏定义: SAI_OUT_DMA_STREAM - SAI_OUT_DMA_STREAM 的宏定义。 */
#define SAI_OUT_DMA_STREAM				DMA2_Stream6
/* 宏定义: SAI_OUT_DMA_STREAM_IRQ - SAI_OUT_DMA_STREAM_IRQ 的宏定义。 */
#define SAI_OUT_DMA_STREAM_IRQ			DMA2_Stream6_IRQn
/* 宏定义: SAI_OUT_DMA_STREAM_IRQHandler - SAI_OUT_DMA_STREAM_IRQHandler 的宏定义。 */
#define SAI_OUT_DMA_STREAM_IRQHandler	DMA2_Stream6_IRQHandler

/* 宏定义: SAI_OUT_DMA_CLK_ENABLE - SAI_OUT_DMA_CLK_ENABLE 的宏定义。 */
#define SAI_OUT_DMA_CLK_ENABLE()		__HAL_RCC_DMA2_CLK_ENABLE()
/* 宏定义: SAI_OUT_DMA_REQUEST - SAI_OUT_DMA_REQUEST 的宏定义。 */
#define SAI_OUT_DMA_REQUEST				DMA_REQUEST_SAI1_A

/*************** SAI ¼��ͨ��GPIO DMA���� ***************/
#define SAI_IN                          SAI1_Block_B
/* 宏定义: SAI_IN_CLK_ENABLE - SAI_IN_CLK_ENABLE 的宏定义。 */
#define SAI_IN_CLK_ENABLE()              __HAL_RCC_SAI1_CLK_ENABLE()

/* 宏定义: SAI_IN_SD_GPIO_PORT - SAI_IN_SD_GPIO_PORT 的宏定义。 */
#define SAI_IN_SD_GPIO_PORT              GPIOE
/* 宏定义: SAI_IN_SD_AF - SAI_IN_SD_AF 的宏定义。 */
#define SAI_IN_SD_AF                     GPIO_AF6_SAI1
/* 宏定义: SAI_IN_SD_PIN - SAI_IN_SD_PIN 的宏定义。 */
#define SAI_IN_SD_PIN                    GPIO_PIN_3

/* 宏定义: SAI_IN_SD_ENABLE - SAI_IN_SD_ENABLE 的宏定义。 */
#define SAI_IN_SD_ENABLE()               __HAL_RCC_GPIOE_CLK_ENABLE()

SAI_HandleTypeDef   SaiInHandle;
DMA_HandleTypeDef   hSaiInDma;

/* 宏定义: SAI_IN_DMA_STREAM - SAI_IN_DMA_STREAM 的宏定义。 */
#define SAI_IN_DMA_STREAM				DMA2_Stream4
/* 宏定义: SAI_IN_DMA_STREAM_IRQ - SAI_IN_DMA_STREAM_IRQ 的宏定义。 */
#define SAI_IN_DMA_STREAM_IRQ			DMA2_Stream4_IRQn
/* 宏定义: SAI_IN_DMA_STREAM_IRQHandler - SAI_IN_DMA_STREAM_IRQHandler 的宏定义。 */
#define SAI_IN_DMA_STREAM_IRQHandler	DMA2_Stream4_IRQHandler

/* 宏定义: SAI_IN_DMA_CLK_ENABLE - SAI_IN_DMA_CLK_ENABLE 的宏定义。 */
#define SAI_IN_DMA_CLK_ENABLE()			__HAL_RCC_DMA2_CLK_ENABLE()
/* 宏定义: SAI_IN_DMA_REQUEST - SAI_IN_DMA_REQUEST 的宏定义。 */
#define SAI_IN_DMA_REQUEST				DMA_REQUEST_SAI1_B
	
/*************** SAI ¼��ͨ��GPIO DMA���壨End�� ***************/

typedef enum 
{
	DMA_BUFFER_NONE = 0,
	DMA_BUFFER_HALF,
	DMA_BUFFER_FULL,
}DMA_BUFFER_STATE_T;

/******** ������ȫ�ֱ��� ********/
#define PLAY_BUFF_SIZE       4096

static __IO DMA_BUFFER_STATE_T	s_PlayDmaState = DMA_BUFFER_NONE;

__IO int16_t s_PlayPointer = -1;
int16_t *s_PlayFileBuf;
uint32_t s_PlayPos;
uint32_t s_PlayFileLen;

/* Buffer location should aligned to cache line size (32 bytes) */
ALIGN_32BYTES (uint16_t s_PlayBuffCache[PLAY_BUFF_SIZE]);

/********* ¼����ȫ�ֱ��� *********/
#define REC_BUFF_SIZE       4096

static __IO DMA_BUFFER_STATE_T	s_RecDmaState = DMA_BUFFER_NONE;

int16_t *s_RecFileBuf;
uint32_t s_RecPos;
uint32_t s_RecFileLen;

/* Buffer location should aligned to cache line size (32 bytes) */
ALIGN_32BYTES (uint16_t s_RecBuffCache[REC_BUFF_SIZE]);


/* ���ڱ�ģ���ڲ�ʹ�õľֲ����� */
static uint16_t wm8978_ReadReg(uint8_t _ucRegAddr);
/*
*********************************************************************************************************
*   函 数 名: wm8978_WriteReg
*   功能说明: wm8978_WriteReg 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t wm8978_WriteReg(uint8_t _ucRegAddr, uint16_t _usValue);

/*
*********************************************************************************************************
*   函 数 名: wm8978_Reset
*   功能说明: wm8978_Reset 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void wm8978_Reset(void);

/*
	wm8978�Ĵ�������
	����WM8978��I2C���߽ӿڲ�֧�ֶ�ȡ��������˼Ĵ���ֵ�������ڴ��У���д�Ĵ���ʱͬ�����»��棬���Ĵ���ʱ
	ֱ�ӷ��ػ����е�ֵ��
	�Ĵ���MAP ��WM8978.pdf �ĵ�67ҳ���Ĵ�����ַ��7bit�� �Ĵ���������9bit
*/
static uint16_t wm8978_RegCash[] = {
	0x000, 0x000, 0x000, 0x000, 0x050, 0x000, 0x140, 0x000,
	0x000, 0x000, 0x000, 0x0FF, 0x0FF, 0x000, 0x100, 0x0FF,
	0x0FF, 0x000, 0x12C, 0x02C, 0x02C, 0x02C, 0x02C, 0x000,
	0x032, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,
	0x038, 0x00B, 0x032, 0x000, 0x008, 0x00C, 0x093, 0x0E9,
	0x000, 0x000, 0x000, 0x000, 0x003, 0x010, 0x010, 0x100,
	0x100, 0x002, 0x001, 0x001, 0x039, 0x039, 0x039, 0x039,
	0x001, 0x001
};


/*
*********************************************************************************************************
*	�� �� ��: wm8978_Init
*	����˵��: ����I2C GPIO�������I2C�����ϵ�WM8978�Ƿ�����
*	��    ��:  ��
*	�� �� ֵ: 1 ��ʾ��ʼ��������0��ʾ��ʼ��������
*********************************************************************************************************
*/
uint8_t wm8978_Init(void)
{
	uint8_t re;

	if (i2c_CheckDevice(WM8978_SLAVE_ADDRESS) == 0)	/* �������������STM32��GPIO��������ģ��I2Cʱ�� */
	{
		re = 1;
	}
	else
	{
		re = 0;
	}
	wm8978_Reset();			/* Ӳ����λWM8978���мĴ�����ȱʡ״̬ */
	return re;
}

/*
*********************************************************************************************************
*	�� �� ��: wm8978_SetEarVolume
*	����˵��: �޸Ķ����������
*	��    ��:  _ucLeftVolume ������������ֵ, 0-63
*			  _ucLRightVolume : ����������ֵ,0-63
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void wm8978_SetEarVolume(uint8_t _ucVolume)
{
	uint16_t regL;
	uint16_t regR;

	if (_ucVolume > 0x3F)
	{
		_ucVolume = 0x3F;
	}

	regL = _ucVolume;
	regR = _ucVolume;

	/*
		R52	LOUT1 Volume control
		R53	ROUT1 Volume control
	*/
	/* �ȸ�������������ֵ */
	wm8978_WriteReg(52, regL | 0x00);

	/* ��ͬ�������������������� */
	wm8978_WriteReg(53, regR | 0x100);	/* 0x180��ʾ ������Ϊ0ʱ�ٸ��£���������������ֵġ����ա��� */
}

/*
*********************************************************************************************************
*	�� �� ��: wm8978_SetSpkVolume
*	����˵��: �޸��������������
*	��    ��:  _ucLeftVolume ������������ֵ, 0-63
*			  _ucLRightVolume : ����������ֵ,0-63
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void wm8978_SetSpkVolume(uint8_t _ucVolume)
{
	uint16_t regL;
	uint16_t regR;

	if (_ucVolume > 0x3F)
	{
		_ucVolume = 0x3F;
	}

	regL = _ucVolume;
	regR = _ucVolume;

	/*
		R54	LOUT2 (SPK) Volume control
		R55	ROUT2 (SPK) Volume control
	*/
	/* �ȸ�������������ֵ */
	wm8978_WriteReg(54, regL | 0x00);

	/* ��ͬ�������������������� */
	wm8978_WriteReg(55, regR | 0x100);	/* ������Ϊ0ʱ�ٸ��£���������������ֵġ����ա��� */
}

/*
*********************************************************************************************************
*	�� �� ��: wm8978_ReadEarVolume
*	����˵��: ���ض���ͨ��������.
*	��    ��:  ��
*	�� �� ֵ: ��ǰ����ֵ
*********************************************************************************************************
*/
uint8_t wm8978_ReadEarVolume(void)
{
	return (uint8_t)(wm8978_ReadReg(52) & 0x3F );
}

/*
*********************************************************************************************************
*	�� �� ��: wm8978_ReadSpkVolume
*	����˵��: ����������ͨ��������.
*	��    ��:  ��
*	�� �� ֵ: ��ǰ����ֵ
*********************************************************************************************************
*/
uint8_t wm8978_ReadSpkVolume(void)
{
	return (uint8_t)(wm8978_ReadReg(54) & 0x3F );
}

/*
*********************************************************************************************************
*	�� �� ��: wm8978_OutMute
*	����˵��: �������.
*	��    ��:  _ucMute ��1�Ǿ�����0�ǲ�����.
*	�� �� ֵ: ��ǰ����ֵ
*********************************************************************************************************
*/
void wm8978_OutMute(uint8_t _ucMute)
{
	uint16_t usRegValue;

	if (_ucMute == 1) /* ���� */
	{
		usRegValue = wm8978_ReadReg(52); /* Left Mixer Control */
		usRegValue |= (1u << 6);
		wm8978_WriteReg(52, usRegValue);

		usRegValue = wm8978_ReadReg(53); /* Left Mixer Control */
		usRegValue |= (1u << 6);
		wm8978_WriteReg(53, usRegValue);

		usRegValue = wm8978_ReadReg(54); /* Right Mixer Control */
		usRegValue |= (1u << 6);
		wm8978_WriteReg(54, usRegValue);

		usRegValue = wm8978_ReadReg(55); /* Right Mixer Control */
		usRegValue |= (1u << 6);
		wm8978_WriteReg(55, usRegValue);
	}
	else	/* ȡ������ */
	{
		usRegValue = wm8978_ReadReg(52);
		usRegValue &= ~(1u << 6);
		wm8978_WriteReg(52, usRegValue);

		usRegValue = wm8978_ReadReg(53); /* Left Mixer Control */
		usRegValue &= ~(1u << 6);
		wm8978_WriteReg(53, usRegValue);

		usRegValue = wm8978_ReadReg(54);
		usRegValue &= ~(1u << 6);
		wm8978_WriteReg(54, usRegValue);

		usRegValue = wm8978_ReadReg(55); /* Left Mixer Control */
		usRegValue &= ~(1u << 6);
		wm8978_WriteReg(55, usRegValue);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: wm8978_SetMicGain
*	����˵��: ����MIC����
*	��    ��:  _ucGain ������ֵ, 0-63
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void wm8978_SetMicGain(uint8_t _ucGain)
{
	if (_ucGain > GAIN_MAX)
	{
		_ucGain = GAIN_MAX;
	}

	/* PGA ��������  R45�� R46   pdf 19ҳ
		Bit8	INPPGAUPDATE
		Bit7	INPPGAZCL		�����ٸ���
		Bit6	INPPGAMUTEL		PGA����
		Bit5:0	����ֵ��010000��0dB
	*/
	wm8978_WriteReg(45, _ucGain);
	wm8978_WriteReg(46, _ucGain | (1 << 8));
}

/*
*********************************************************************************************************
*	�� �� ��: wm8978_SetLineGain
*	����˵��: ����Line����ͨ��������
*	��    ��:  _ucGain ������ֵ, 0-7. 7���0��С�� ��˥���ɷŴ�
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void wm8978_SetLineGain(uint8_t _ucGain)
{
	uint16_t usRegValue;

	if (_ucGain > 7)
	{
		_ucGain = 7;
	}

	/*
		Mic �����ŵ��������� PGABOOSTL �� PGABOOSTR ����
		Aux �����ŵ������������� AUXL2BOOSTVO[2:0] �� AUXR2BOOSTVO[2:0] ����
		Line �����ŵ��������� LIP2BOOSTVOL[2:0] �� RIP2BOOSTVOL[2:0] ����
	*/
	/*	pdf 21ҳ��R47������������R48����������, MIC ������ƼĴ���
		R47 (R48���������ͬ)
		B8		PGABOOSTL	= 1,   0��ʾMIC�ź�ֱͨ�����棬1��ʾMIC�ź�+20dB���棨ͨ���Ծٵ�·��
		B7		= 0�� ����
		B6:4	L2_2BOOSTVOL = x�� 0��ʾ��ֹ��1-7��ʾ����-12dB ~ +6dB  ������˥��Ҳ���ԷŴ�
		B3		= 0�� ����
		B2:0`	AUXL2BOOSTVOL = x��0��ʾ��ֹ��1-7��ʾ����-12dB ~ +6dB  ������˥��Ҳ���ԷŴ�
	*/

	usRegValue = wm8978_ReadReg(47);
	usRegValue &= 0x8F;/* ��Bit6:4��0   1000 1111*/
	usRegValue |= (_ucGain << 4);
	wm8978_WriteReg(47, usRegValue);	/* д����������������ƼĴ��� */

	usRegValue = wm8978_ReadReg(48);
	usRegValue &= 0x8F;/* ��Bit6:4��0   1000 1111*/
	usRegValue |= (_ucGain << 4);
	wm8978_WriteReg(48, usRegValue);	/* д����������������ƼĴ��� */
}

/*
*********************************************************************************************************
*	�� �� ��: wm8978_PowerDown
*	����˵��: �ر�wm8978������͹���ģʽ
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void wm8978_PowerDown(void)
{
	wm8978_Reset();			/* Ӳ����λWM8978���мĴ�����ȱʡ״̬ */
}

/*
*********************************************************************************************************
*	�� �� ��: wm8978_CfgAudioIF
*	����˵��: ����WM8978����Ƶ�ӿ�(I2S)
*	��    ��:
*			  _usStandard : �ӿڱ�׼��I2S_Standard_Phillips, I2S_Standard_MSB �� I2S_Standard_LSB
*			  _ucWordLen : �ֳ���16��24��32��20bit��ʽ��
*			  _usMode : CPU I2S�Ĺ���ģʽ��I2S_Mode_MasterTx��I2S_Mode_MasterRx��
*						������������Ӳ����֧�� I2S_Mode_SlaveTx��I2S_Mode_SlaveRx ģʽ������ҪWM8978����
*						�ⲿ����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void wm8978_CfgAudioIF(uint16_t _usStandard, uint8_t _ucWordLen)
{
	uint16_t usReg;

	/* pdf 67ҳ���Ĵ����б� */

	/*	REG R4, ��Ƶ�ӿڿ��ƼĴ���
		B8		BCP	 = X, BCLK���ԣ�0��ʾ������1��ʾ����
		B7		LRCP = x, LRCʱ�Ӽ��ԣ�0��ʾ������1��ʾ����
		B6:5	WL = x�� �ֳ���00=16bit��01=20bit��10=24bit��11=32bit ���Ҷ���ģʽֻ�ܲ��������24bit)
		B4:3	FMT = x����Ƶ���ݸ�ʽ��00=�Ҷ��룬01=����룬10=I2S��ʽ��11=PCM
		B2		DACLRSWAP = x, ����DAC���ݳ�����LRCʱ�ӵ���߻����ұ�
		B1 		ADCLRSWAP = x������ADC���ݳ�����LRCʱ�ӵ���߻����ұ�
		B0		MONO	= 0��0��ʾ��������1��ʾ������������������Ч
	*/
	usReg = 0;
	if (_usStandard == I2S_STANDARD_PHILIPS)	/* I2S�����ֱ�׼ */
	{
		usReg |= (2 << 3);
	}
	else if (_usStandard == I2S_STANDARD_MSB)	/* MSB�����׼(�����) */
	{
		usReg |= (1 << 3);
	}
	else if (_usStandard == I2S_STANDARD_LSB)	/* LSB�����׼(�Ҷ���) */
	{
		usReg |= (0 << 3);
	}
	else	/* PCM��׼(16λͨ��֡�ϴ������֡ͬ������16λ����֡��չΪ32λͨ��֡) */
	{
		usReg |= (3 << 3);;
	}

	if (_ucWordLen == 24)
	{
		usReg |= (2 << 5);
	}
	else if (_ucWordLen == 32)
	{
		usReg |= (3 << 5);
	}
	else if (_ucWordLen == 20)
	{
		usReg |= (1 << 5);
	}	
	else
	{
		usReg |= (0 << 5);		/* 16bit */
	}
	wm8978_WriteReg(4, usReg);

	/* R5  pdf 57ҳ */


	/*
		R6��ʱ�Ӳ������ƼĴ���
		MS = 0,  WM8978����ʱ�ӣ���MCU�ṩMCLKʱ��
	*/
	wm8978_WriteReg(6, 0x000);
}

/*
*********************************************************************************************************
*	�� �� ��: wm8978_ReadReg
*	����˵��: ��cash�ж��ض���wm8978�Ĵ���
*	��    ��:  _ucRegAddr �� �Ĵ�����ַ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static uint16_t wm8978_ReadReg(uint8_t _ucRegAddr)
{
	return wm8978_RegCash[_ucRegAddr];
}

/*
*********************************************************************************************************
*	�� �� ��: wm8978_WriteReg
*	����˵��: дwm8978�Ĵ���
*	��    ��:  _ucRegAddr �� �Ĵ�����ַ
*			  _usValue ���Ĵ���ֵ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static uint8_t wm8978_WriteReg(uint8_t _ucRegAddr, uint16_t _usValue)
{
	uint8_t ucAck;

	/* ������ʼλ */
	i2c_Start();

	/* �����豸��ַ+��д����bit��0 = w�� 1 = r) bit7 �ȴ� */
	i2c_SendByte(WM8978_SLAVE_ADDRESS | I2C_WR);

	/* ���ACK */
	ucAck = i2c_WaitAck();
	if (ucAck == 1)
	{
		return 0;
	}

	/* ���Ϳ����ֽ�1 */
	i2c_SendByte(((_ucRegAddr << 1) & 0xFE) | ((_usValue >> 8) & 0x1));

	/* ���ACK */
	ucAck = i2c_WaitAck();
	if (ucAck == 1)
	{
		return 0;
	}

	/* ���Ϳ����ֽ�2 */
	i2c_SendByte(_usValue & 0xFF);

	/* ���ACK */
	ucAck = i2c_WaitAck();
	if (ucAck == 1)
	{
		return 0;
	}

	/* ����STOP */
	i2c_Stop();

	wm8978_RegCash[_ucRegAddr] = _usValue;
	return 1;
}

/*
*********************************************************************************************************
*	�� �� ��: wm8978_CfgInOut
*	����˵��: ����wm8978��Ƶͨ��
*	��    ��:
*			_InPath : ��Ƶ����ͨ������
*			_OutPath : ��Ƶ���ͨ������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void wm8978_CfgAudioPath(uint16_t _InPath, uint16_t _OutPath)
{
	uint16_t usReg;

	/* �鿴WM8978�����ֲ�� REGISTER MAP �½ڣ� ��67ҳ */

	if ((_InPath == IN_PATH_OFF) && (_OutPath == OUT_PATH_OFF))
	{
		wm8978_PowerDown();
		return;
	}

	/* --------------------------- ��1������������ͨ���������üĴ��� -----------------------*/
	/*
		R1 �Ĵ��� Power manage 1
		Bit8    BUFDCOPEN,  Output stage 1.5xAVDD/2 driver enable
 		Bit7    OUT4MIXEN, OUT4 mixer enable
		Bit6    OUT3MIXEN, OUT3 mixer enable
		Bit5    PLLEN	.����
		Bit4    MICBEN	,Microphone Bias Enable (MICƫ�õ�·ʹ��)
		Bit3    BIASEN	,Analogue amplifier bias control ��������Ϊ1ģ��Ŵ����Ź���
		Bit2    BUFIOEN , Unused input/output tie off buffer enable
		Bit1:0  VMIDSEL, ��������Ϊ��00ֵģ��Ŵ����Ź���
	*/
	usReg = (1 << 3) | (3 << 0);
	if (_OutPath & OUT3_4_ON) 	/* OUT3��OUT4ʹ�������GSMģ�� */
	{
		usReg |= ((1 << 7) | (1 << 6));
	}
	if ((_InPath & MIC_LEFT_ON) || (_InPath & MIC_RIGHT_ON))
	{
		usReg |= (1 << 4);
	}
	wm8978_WriteReg(1, usReg);	/* д�Ĵ��� */

	/*
		R2 �Ĵ��� Power manage 2
		Bit8	ROUT1EN,	ROUT1 output enable �������������ʹ��
		Bit7	LOUT1EN,	LOUT1 output enable �������������ʹ��
		Bit6	SLEEP, 		0 = Normal device operation   1 = Residual current reduced in device standby mode
		Bit5	BOOSTENR,	Right channel Input BOOST enable ����ͨ���Ծٵ�·ʹ��. �õ�PGA�Ŵ���ʱ����ʹ��
		Bit4	BOOSTENL,	Left channel Input BOOST enable
		Bit3	INPGAENR,	Right channel input PGA enable ����������PGAʹ��
		Bit2	INPGAENL,	Left channel input PGA enable
		Bit1	ADCENR,		Enable ADC right channel
		Bit0	ADCENL,		Enable ADC left channel
	*/
	usReg = 0;
	if (_OutPath & EAR_LEFT_ON)
	{
		usReg |= (1 << 7);
	}
	if (_OutPath & EAR_RIGHT_ON)
	{
		usReg |= (1 << 8);
	}
	if (_InPath & MIC_LEFT_ON)
	{
		usReg |= ((1 << 4) | (1 << 2));
	}
	if (_InPath & MIC_RIGHT_ON)
	{
		usReg |= ((1 << 5) | (1 << 3));
	}
	if (_InPath & LINE_ON)
	{
		usReg |= ((1 << 4) | (1 << 5));
	}
	if (_InPath & MIC_RIGHT_ON)
	{
		usReg |= ((1 << 5) | (1 << 3));
	}
	if (_InPath & ADC_ON)
	{
		usReg |= ((1 << 1) | (1 << 0));
	}
	wm8978_WriteReg(2, usReg);	/* д�Ĵ��� */

	/*
		R3 �Ĵ��� Power manage 3
		Bit8	OUT4EN,		OUT4 enable
		Bit7	OUT3EN,		OUT3 enable
		Bit6	LOUT2EN,	LOUT2 output enable
		Bit5	ROUT2EN,	ROUT2 output enable
		Bit4	0,
		Bit3	RMIXEN,		Right mixer enable
		Bit2	LMIXEN,		Left mixer enable
		Bit1	DACENR,		Right channel DAC enable
		Bit0	DACENL,		Left channel DAC enable
	*/
	usReg = 0;
	if (_OutPath & OUT3_4_ON)
	{
		usReg |= ((1 << 8) | (1 << 7));
	}
	if (_OutPath & SPK_ON)
	{
		usReg |= ((1 << 6) | (1 << 5));
	}
	if (_OutPath != OUT_PATH_OFF)
	{
		usReg |= ((1 << 3) | (1 << 2));
	}
	if (_InPath & DAC_ON)
	{
		usReg |= ((1 << 1) | (1 << 0));
	}
	wm8978_WriteReg(3, usReg);	/* д�Ĵ��� */

	/*
		R44 �Ĵ��� Input ctrl

		Bit8	MBVSEL, 		Microphone Bias Voltage Control   0 = 0.9 * AVDD   1 = 0.6 * AVDD
		Bit7	0
		Bit6	R2_2INPPGA,		Connect R2 pin to right channel input PGA positive terminal
		Bit5	RIN2INPPGA,		Connect RIN pin to right channel input PGA negative terminal
		Bit4	RIP2INPPGA,		Connect RIP pin to right channel input PGA amplifier positive terminal
		Bit3	0
		Bit2	L2_2INPPGA,		Connect L2 pin to left channel input PGA positive terminal
		Bit1	LIN2INPPGA,		Connect LIN pin to left channel input PGA negative terminal
		Bit0	LIP2INPPGA,		Connect LIP pin to left channel input PGA amplifier positive terminal
	*/
	usReg = 0 << 8;
	if (_InPath & LINE_ON)
	{
		usReg |= ((1 << 6) | (1 << 2));
	}
	if (_InPath & MIC_RIGHT_ON)
	{
		usReg |= ((1 << 5) | (1 << 4));
	}
	if (_InPath & MIC_LEFT_ON)
	{
		usReg |= ((1 << 1) | (1 << 0));
	}
	wm8978_WriteReg(44, usReg);	/* д�Ĵ��� */


	/*
		R14 �Ĵ��� ADC Control
		���ø�ͨ�˲�������ѡ�ģ� pdf 24��25ҳ,
		Bit8 	HPFEN,	High Pass Filter Enable��ͨ�˲���ʹ�ܣ�0��ʾ��ֹ��1��ʾʹ��
		BIt7 	HPFAPP,	Select audio mode or application mode ѡ����Ƶģʽ��Ӧ��ģʽ��0��ʾ��Ƶģʽ��
		Bit6:4	HPFCUT��Application mode cut-off frequency  000-111ѡ��Ӧ��ģʽ�Ľ�ֹƵ��
		Bit3 	ADCOSR,	ADC oversample rate select: 0=64x (lower power) 1=128x (best performance)
		Bit2   	0
		Bit1 	ADC right channel polarity adjust:  0=normal  1=inverted
		Bit0 	ADC left channel polarity adjust:  0=normal 1=inverted
	*/
	if (_InPath & ADC_ON)
	{
		usReg = (1 << 3) | (0 << 8) | (4 << 0);		/* ��ֹADC��ͨ�˲���, ���ý���Ƶ�� */
	}
	else
	{
		usReg = 0;
	}
	wm8978_WriteReg(14, usReg);	/* д�Ĵ��� */

	/* �����ݲ��˲�����notch filter������Ҫ�������ƻ�Ͳ����������������Х��.  ��ʱ�ر�
		R27��R28��R29��R30 ���ڿ����޲��˲�����pdf 26ҳ
		R7�� Bit7 NFEN = 0 ��ʾ��ֹ��1��ʾʹ��
	*/
	if (_InPath & ADC_ON)
	{
		usReg = (0 << 7);
		wm8978_WriteReg(27, usReg);	/* д�Ĵ��� */
		usReg = 0;
		wm8978_WriteReg(28, usReg);	/* д�Ĵ���,��0����Ϊ�Ѿ���ֹ������Ҳ�ɲ��� */
		wm8978_WriteReg(29, usReg);	/* д�Ĵ���,��0����Ϊ�Ѿ���ֹ������Ҳ�ɲ��� */
		wm8978_WriteReg(30, usReg);	/* д�Ĵ���,��0����Ϊ�Ѿ���ֹ������Ҳ�ɲ��� */
	}

	/* �Զ�������� ALC, R32  - 34  pdf 19ҳ */
	{
		usReg = 0;		/* ��ֹ�Զ�������� */
		wm8978_WriteReg(32, usReg);
		wm8978_WriteReg(33, usReg);
		wm8978_WriteReg(34, usReg);
	}

	/*  R35  ALC Noise Gate Control
		Bit3	NGATEN, Noise gate function enable
		Bit2:0	Noise gate threshold:
	*/
	usReg = (3 << 1) | (7 << 0);		/* ��ֹ�Զ�������� */
	wm8978_WriteReg(35, usReg);

	/*
		Mic �����ŵ��������� PGABOOSTL �� PGABOOSTR ����
		Aux �����ŵ������������� AUXL2BOOSTVO[2:0] �� AUXR2BOOSTVO[2:0] ����
		Line �����ŵ��������� LIP2BOOSTVOL[2:0] �� RIP2BOOSTVOL[2:0] ����
	*/
	/*	pdf 21ҳ��R47������������R48����������, MIC ������ƼĴ���
		R47 (R48���������ͬ)
		B8		PGABOOSTL	= 1,   0��ʾMIC�ź�ֱͨ�����棬1��ʾMIC�ź�+20dB���棨ͨ���Ծٵ�·��
		B7		= 0�� ����
		B6:4	L2_2BOOSTVOL = x�� 0��ʾ��ֹ��1-7��ʾ����-12dB ~ +6dB  ������˥��Ҳ���ԷŴ�
		B3		= 0�� ����
		B2:0`	AUXL2BOOSTVOL = x��0��ʾ��ֹ��1-7��ʾ����-12dB ~ +6dB  ������˥��Ҳ���ԷŴ�
	*/
	usReg = 0;
	if ((_InPath & MIC_LEFT_ON) || (_InPath & MIC_RIGHT_ON))
	{
		usReg |= (1 << 8);	/* MIC����ȡ+20dB */
	}
	if (_InPath & AUX_ON)
	{
		usReg |= (3 << 0);	/* Aux����̶�ȡ3���û��������е��� */
	}
	if (_InPath & LINE_ON)
	{
		usReg |= (3 << 4);	/* Line����̶�ȡ3���û��������е��� */
	}
	wm8978_WriteReg(47, usReg);	/* д����������������ƼĴ��� */
	wm8978_WriteReg(48, usReg);	/* д����������������ƼĴ��� */

	/* ����ADC�������ƣ�pdf 27ҳ
		R15 ����������ADC������R16����������ADC����
		Bit8 	ADCVU  = 1 ʱ�Ÿ��£�����ͬ����������������ADC����
		Bit7:0 	����ѡ�� 0000 0000 = ����
						   0000 0001 = -127dB
						   0000 0010 = -12.5dB  ��0.5dB ������
						   1111 1111 = 0dB  ����˥����
	*/
	usReg = 0xFF;
	wm8978_WriteReg(15, usReg);	/* ѡ��0dB���Ȼ��������� */
	usReg = 0x1FF;
	wm8978_WriteReg(16, usReg);	/* ͬ�������������� */

	/* ͨ�� wm8978_SetMicGain ��������mic PGA���� */

	/*	R43 �Ĵ���  AUXR �C ROUT2 BEEP Mixer Function
		B8:6 = 0

		B5	 MUTERPGA2INV,	Mute input to INVROUT2 mixer
		B4	 INVROUT2,  Invert ROUT2 output �����������������
		B3:1 BEEPVOL = 7;	AUXR input to ROUT2 inverter gain
		B0	 BEEPEN = 1;	Enable AUXR beep input

	*/
	usReg = 0;
	if (_OutPath & SPK_ON)
	{
		usReg |= (1 << 4);	/* ROUT2 ����, �������������� */
	}
	if (_InPath & AUX_ON)
	{
		usReg |= ((7 << 1) | (1 << 0));
	}
	wm8978_WriteReg(43, usReg);

	/* R49  Output ctrl
		B8:7	0
		B6		DACL2RMIX,	Left DAC output to right output mixer
		B5		DACR2LMIX,	Right DAC output to left output
		B4		OUT4BOOST,	0 = OUT4 output gain = -1; DC = AVDD / 2��1 = OUT4 output gain = +1.5��DC = 1.5 x AVDD / 2
		B3		OUT3BOOST,	0 = OUT3 output gain = -1; DC = AVDD / 2��1 = OUT3 output gain = +1.5��DC = 1.5 x AVDD / 2
		B2		SPKBOOST,	0 = Speaker gain = -1;  DC = AVDD / 2 ; 1 = Speaker gain = +1.5; DC = 1.5 x AVDD / 2
		B1		TSDEN,   Thermal Shutdown Enable  �������ȱ���ʹ�ܣ�ȱʡ1��
		B0		VROI,	Disabled Outputs to VREF Resistance
	*/
	usReg = 0;
	if (_InPath & DAC_ON)
	{
		usReg |= ((1 << 6) | (1 << 5));
	}
	if (_OutPath & SPK_ON)
	{
		usReg |=  ((1 << 2) | (1 << 1));	/* SPK 1.5x����,  �ȱ���ʹ�� */
	}
	if (_OutPath & OUT3_4_ON)
	{
		usReg |=  ((1 << 4) | (1 << 3));	/* BOOT3  BOOT4  1.5x���� */
	}
	wm8978_WriteReg(49, usReg);

	/*	REG 50    (50����������51�������������üĴ�������һ��) pdf 40ҳ
		B8:6	AUXLMIXVOL = 111	AUX����FM�������ź�����
		B5		AUXL2LMIX = 1		Left Auxilliary input to left channel
		B4:2	BYPLMIXVOL			����
		B1		BYPL2LMIX = 0;		Left bypass path (from the left channel input boost output) to left output mixer
		B0		DACL2LMIX = 1;		Left DAC output to left output mixer
	*/
	usReg = 0;
	if (_InPath & AUX_ON)
	{
		usReg |= ((7 << 6) | (1 << 5));
	}
	if ((_InPath & LINE_ON) || (_InPath & MIC_LEFT_ON) || (_InPath & MIC_RIGHT_ON))
	{
		usReg |= ((7 << 2) | (1 << 1));
	}
	if (_InPath & DAC_ON)
	{
		usReg |= (1 << 0);
	}
	wm8978_WriteReg(50, usReg);
	wm8978_WriteReg(51, usReg);

	/*	R56 �Ĵ���   OUT3 mixer ctrl
		B8:7	0
		B6		OUT3MUTE,  	0 = Output stage outputs OUT3 mixer;  1 = Output stage muted �C drives out VMID.
		B5:4	0
		B3		BYPL2OUT3,	OUT4 mixer output to OUT3  (����)
		B4		0
		B2		LMIX2OUT3,	Left ADC input to OUT3
		B1		LDAC2OUT3,	Left DAC mixer to OUT3
		B0		LDAC2OUT3,	Left DAC output to OUT3
	*/
	usReg = 0;
	if (_OutPath & OUT3_4_ON)
	{
		usReg |= (1 << 3);
	}
	wm8978_WriteReg(56, usReg);

	/* R57 �Ĵ���		OUT4 (MONO) mixer ctrl
		B8:7	0
		B6		OUT4MUTE,	0 = Output stage outputs OUT4 mixer  1 = Output stage muted �C drives outVMID.
		B5		HALFSIG,	0 = OUT4 normal output	1 = OUT4 attenuated by 6dB
		B4		LMIX2OUT4,	Left DAC mixer to OUT4
		B3		LDAC2UT4,	Left DAC to OUT4
		B2		BYPR2OUT4,	Right ADC input to OUT4
		B1		RMIX2OUT4,	Right DAC mixer to OUT4
		B0		RDAC2OUT4,	Right DAC output to OUT4
	*/
	usReg = 0;
	if (_OutPath & OUT3_4_ON)
	{
		usReg |= ((1 << 4) |  (1 << 1));
	}
	wm8978_WriteReg(57, usReg);


	/* R11, 12 �Ĵ��� DAC��������
		R11		Left DAC Digital Volume
		R12		Right DAC Digital Volume
	*/
	if (_InPath & DAC_ON)
	{
		wm8978_WriteReg(11, 255);
		wm8978_WriteReg(12, 255 | 0x100);
	}
	else
	{
		wm8978_WriteReg(11, 0);
		wm8978_WriteReg(12, 0 | 0x100);
	}

	/*	R10 �Ĵ��� DAC Control
		B8	0
		B7	0
		B6	SOFTMUTE,	Softmute enable:
		B5	0
		B4	0
		B3	DACOSR128,	DAC oversampling rate: 0=64x (lowest power) 1=128x (best performance)
		B2	AMUTE,		Automute enable
		B1	DACPOLR,	Right DAC output polarity
		B0	DACPOLL,	Left DAC output polarity:
	*/
	if (_InPath & DAC_ON)
	{
		wm8978_WriteReg(10, 0);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: wm8978_NotchFilter
*	����˵��: �����ݲ��˲�����notch filter������Ҫ�������ƻ�Ͳ����������������Х��
*	��    ��:  NFA0[13:0] and NFA1[13:0]
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void wm8978_NotchFilter(uint16_t _NFA0, uint16_t _NFA1)
{
	uint16_t usReg;

	/*  page 26
		A programmable notch filter is provided. This filter has a variable centre frequency and bandwidth,
		programmable via two coefficients, a0 and a1. a0 and a1 are represented by the register bits
		NFA0[13:0] and NFA1[13:0]. Because these coefficient values require four register writes to setup
		there is an NFU (Notch Filter Update) flag which should be set only when all four registers are setup.
	*/
	usReg = (1 << 7) | (_NFA0 & 0x3F);
	wm8978_WriteReg(27, usReg);	/* д�Ĵ��� */

	usReg = ((_NFA0 >> 7) & 0x3F);
	wm8978_WriteReg(28, usReg);	/* д�Ĵ��� */

	usReg = (_NFA1 & 0x3F);
	wm8978_WriteReg(29, usReg);	/* д�Ĵ��� */

	usReg = (1 << 8) | ((_NFA1 >> 7) & 0x3F);
	wm8978_WriteReg(30, usReg);	/* д�Ĵ��� */
}

/*
*********************************************************************************************************
*	�� �� ��: wm8978_CtrlGPIO1
*	����˵��: ����WM8978��GPIO1�������0��1
*	��    ��:  _ucValue ��GPIO1���ֵ��0��1
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void wm8978_CtrlGPIO1(uint8_t _ucValue)
{
	uint16_t usRegValue;

	/* R8�� pdf 62ҳ */
	if (_ucValue == 0) /* ���0 */
	{
		usRegValue = 6; /* B2:0 = 110 */
	}
	else
	{
		usRegValue = 7; /* B2:0 = 111 */
	}
	wm8978_WriteReg(8, usRegValue);
}

/*
*********************************************************************************************************
*	�� �� ��: wm8978_Reset
*	����˵��: ��λwm8978�����еļĴ���ֵ�ָ���ȱʡֵ
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void wm8978_Reset(void)
{
	/* wm8978�Ĵ���ȱʡֵ */
	const uint16_t reg_default[] = {
	0x000, 0x000, 0x000, 0x000, 0x050, 0x000, 0x140, 0x000,
	0x000, 0x000, 0x000, 0x0FF, 0x0FF, 0x000, 0x100, 0x0FF,
	0x0FF, 0x000, 0x12C, 0x02C, 0x02C, 0x02C, 0x02C, 0x000,
	0x032, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,
	0x038, 0x00B, 0x032, 0x000, 0x008, 0x00C, 0x093, 0x0E9,
	0x000, 0x000, 0x000, 0x000, 0x003, 0x010, 0x010, 0x100,
	0x100, 0x002, 0x001, 0x001, 0x039, 0x039, 0x039, 0x039,
	0x001, 0x001
	};
	uint8_t i;

	wm8978_WriteReg(0x00, 0);

	for (i = 0; i < sizeof(reg_default) / 2; i++)
	{
		wm8978_RegCash[i] = reg_default[i];
	}
}

/*
*********************************************************************************************************
*	                     ����Ĵ����Ǻ�STM32 SAI��Ƶ�ӿ�Ӳ����ص�
*********************************************************************************************************
*/
/*
*********************************************************************************************************
*	�� �� ��: AUDIO_MakeSine16bit
*	����˵��: �������Ҳ����飬���ڲ��ŵ���Ƶ
*	��    ��: _outbuf : �����������int16_t
*			  _sin_freq : ����Ĳ���Ƶ��
*			  _sample_freq : codec����Ƶ��
*			  _sample_count : ����������˫������ÿ����2�ֽڣ�ÿ������4�ֽڡ�
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void AUDIO_MakeSine16bit(int16_t *_outbuf, uint32_t _sin_freq,  uint32_t _sample_freq, uint32_t _count)
{
#if 0	/* �������� */
	for (int i = 0; i < _size / 4; i++)
	{
		_outbuf[2 * i] = 0x5555;
		_outbuf[2 * i + 1] = 0x0505;
	}
#else
/* 宏定义: PI - PI 的宏定义。 */
	#define PI 3.14159
	#define AM 3200			/* ���ɵĲ���������� (0-32767) */
	
	uint32_t i;
	double rd;
	double aa, bb;
	int16_t dac;
	
	for (i = 0; i < _count; i++)
	{
		rd = i * _sin_freq * PI / _sample_freq;
		aa = AM * sin(rd);
		
		rd = i * (_sin_freq / 2) * PI / _sample_freq;
		bb = AM * sin(rd);
		
		dac = (int16_t)aa;
		_outbuf[2 * i] = dac;
		
		dac = (int16_t)bb;		/* ��������������ͬ */
		_outbuf[2 * i + 1] = dac;
	}
	
	s_RecPos = _count;	/* ���沨���������������ڻط� */
#endif
}

/*
*********************************************************************************************************
*	�� �� ��: AUDIO_Init
*	����˵��: ����GPIO���ź��ж�ͨ������codecӦ��
*	��    ��: _ucMode : 1 ��ʾ������2��ʾ¼���� 3��ʾ��¼�߷�(��δ֧��)
*			 _usStandard : δ�á��̶���I2S PHILIP��ʽ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void AUDIO_Init(uint8_t _ucMode, uint16_t _usStandard, uint32_t _uiWordLen, uint32_t _uiAudioFreq)
{	
	uint32_t tmpdiv;
	
//		���ݲ���Ƶ��ѡ��ʱ��Դ, ������Ƶ����Եõ�׼ȷ��λʱ��  ===== ��ʱδ��
//		#define SAI_ClockPLLSAI             ((uint32_t)11289600)
//		#define SAI_ClockPLLI2S             ((uint32_t)49152000)

//	*/
//	if (_uiAudioFreq == SAI_AudioFreq_44_1k || _uiAudioFreq == SAI_AudioFreq_22_05k
//		|| _uiAudioFreq == SAI_AudioFreq_11_025k)
//	{
//		SAI_ClockSrcFreq = SAI_ClockPLLSAI;
//		
//		/* ����SAI_Block_A�� SAI_Block_B��ʱ��Դ */
//		RCC_SAIBlockACLKConfig(RCC_SAIACLKSource_PLLSAI);
//		RCC_SAIBlockBCLKConfig(RCC_SAIACLKSource_PLLSAI);		
//	}
//	else	/*192k, 96k, 48k, 32k, 16k, 8k */
//	{
//  }

	
	tmpdiv = 11289600 / (_uiAudioFreq * 256);
	
	/* ����ʱ��Դ */
	{
		RCC_PeriphCLKInitTypeDef RCC_PeriphCLKInitStruct;

		/* Configure PLLSAI prescalers */
		/* PLL2SAI_VCO: VCO_429M
		SAI_CLK(first level) = PLLSAI_VCO/PLLSAIP = 429/38 = 11.289 Mhz */
		RCC_PeriphCLKInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SAI1;
		RCC_PeriphCLKInitStruct.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLL2;
		RCC_PeriphCLKInitStruct.PLL2.PLL2P = 38;
		RCC_PeriphCLKInitStruct.PLL2.PLL2Q = 1;
		RCC_PeriphCLKInitStruct.PLL2.PLL2R = 1;
		RCC_PeriphCLKInitStruct.PLL2.PLL2N = 429;
		RCC_PeriphCLKInitStruct.PLL2.PLL2M = 25;

		if(HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphCLKInitStruct) != HAL_OK)
		{
			Error_Handler(__FILE__, __LINE__);
		}
	}
	
	/* Initialize SAI */
	if (_ucMode == 1 || _ucMode == 3)
	{
		__HAL_SAI_RESET_HANDLE_STATE(&SaiOutHandle);

		SaiOutHandle.Instance = SAI_OUT;

		__HAL_SAI_DISABLE(&SaiOutHandle);

		SaiOutHandle.Init.AudioMode      = SAI_MODEMASTER_TX;		/* ������ģʽ���� */
		SaiOutHandle.Init.Synchro        = SAI_ASYNCHRONOUS;		/* ����Ϊ�첽��ʹ�ñ�ģ��� FS, SCK,MCLK */
		SaiOutHandle.Init.OutputDrive    = SAI_OUTPUTDRIVE_ENABLE;
		SaiOutHandle.Init.NoDivider      = SAI_MASTERDIVIDER_ENABLE;
		
		SaiOutHandle.Init.Mckdiv       	 = tmpdiv;
		
		SaiOutHandle.Init.FIFOThreshold  = SAI_FIFOTHRESHOLD_1QF;
		SaiOutHandle.Init.AudioFrequency = _uiAudioFreq;			// SAI_AUDIO_FREQUENCY_22K;
		SaiOutHandle.Init.Protocol       = SAI_FREE_PROTOCOL;		/* ����Э�飬��AC97����SPDIF */
		SaiOutHandle.Init.DataSize       = _uiWordLen;				/* �����ֳ� */
		SaiOutHandle.Init.FirstBit       = SAI_FIRSTBIT_MSB;		/* bit���򣬸�bit�ȴ� */
		SaiOutHandle.Init.ClockStrobing  = SAI_CLOCKSTROBING_FALLINGEDGE;

		/* Configure SAI_Block_x Frame 
			Frame Length : 32
			Frame active Length: 16
			FS Definition : Start frame + Channel Side identification
			FS Polarity: FS active Low
			FS Offset: FS asserted one bit before the first bit of slot 0 */ 
		/*
			��֡����ӦΪ 8 �� 256 ֮���һ������ 2
			�� n ���ݵ���������Ϊ��ȷ����Ƶ֡��ÿ��λʱ�Ӱ��������� MCLK ���壬������ȷ���������ڵ��ⲿ DAC/ADC ��ȷ������	
			
		*/		
		SaiOutHandle.FrameInit.FrameLength       = 32;
		SaiOutHandle.FrameInit.ActiveFrameLength = 16;
		SaiOutHandle.FrameInit.FSDefinition      = SAI_FS_CHANNEL_IDENTIFICATION;	 /* FS����Ϊ�������� */		
		SaiOutHandle.FrameInit.FSPolarity        = SAI_FS_ACTIVE_LOW;
		SaiOutHandle.FrameInit.FSOffset          = SAI_FS_BEFOREFIRSTBIT;

		/* ���� SAI Block_x Slot */
		SaiOutHandle.SlotInit.FirstBitOffset = 0;
		SaiOutHandle.SlotInit.SlotSize       = SAI_SLOTSIZE_16B;	// SAI_SLOTSIZE_DATASIZE;
		SaiOutHandle.SlotInit.SlotNumber     = 2;
		SaiOutHandle.SlotInit.SlotActive     = (SAI_SLOTACTIVE_0 | SAI_SLOTACTIVE_1);	// SAI_SLOTACTIVE_0;	//

		if(HAL_OK != HAL_SAI_Init(&SaiOutHandle))
		{
			Error_Handler(__FILE__, __LINE__);
		}

		/* Enable SAI to generate clock used by audio driver */
		__HAL_SAI_ENABLE(&SaiOutHandle);
	}
  
	if (_ucMode == 1)	/* ���� */
	{
		;
	}
	else if (_ucMode == 2)	/* ¼�� */
	{
		;
	}
	else	/* ���� + ¼�� */
	{
		__HAL_SAI_RESET_HANDLE_STATE(&SaiInHandle);

		SaiInHandle.Instance = SAI_IN;

		__HAL_SAI_DISABLE(&SaiInHandle);

		SaiInHandle.Init.AudioMode      = SAI_MODESLAVE_RX;		/* ���ô�ģʽ���� */
		SaiInHandle.Init.Synchro        = SAI_SYNCHRONOUS;		/* ����Ϊ�첽 */
		SaiInHandle.Init.OutputDrive    = SAI_OUTPUTDRIVE_DISABLE;
		SaiInHandle.Init.NoDivider      = SAI_MASTERDIVIDER_DISABLE;	// SAI_MASTERDIVIDER_ENABLE;
		
		SaiInHandle.Init.Mckdiv       	 = tmpdiv;
		
		SaiInHandle.Init.FIFOThreshold  = SAI_FIFOTHRESHOLD_1QF;
		SaiInHandle.Init.AudioFrequency = _uiAudioFreq;			// SAI_AUDIO_FREQUENCY_22K;
		SaiInHandle.Init.Protocol       = SAI_FREE_PROTOCOL;		/* ����Э�飬��AC97����SPDIF */
		SaiInHandle.Init.DataSize       = _uiWordLen;				/* �����ֳ� */
		SaiInHandle.Init.FirstBit       = SAI_FIRSTBIT_MSB;		/* bit���򣬸�bit�ȴ� */
		SaiInHandle.Init.ClockStrobing  = SAI_CLOCKSTROBING_RISINGEDGE;

		/* Configure SAI_Block_x Frame 
			Frame Length : 32
			Frame active Length: 16
			FS Definition : Start frame + Channel Side identification
			FS Polarity: FS active Low
			FS Offset: FS asserted one bit before the first bit of slot 0 */ 
		/*
			��֡����ӦΪ 8 �� 256 ֮���һ������ 2
			�� n ���ݵ���������Ϊ��ȷ����Ƶ֡��ÿ��λʱ�Ӱ��������� MCLK ���壬������ȷ���������ڵ��ⲿ DAC/ADC ��ȷ������	
		*/		
		SaiInHandle.FrameInit.FrameLength       = 32;
		SaiInHandle.FrameInit.ActiveFrameLength = 16;
		SaiInHandle.FrameInit.FSDefinition      = SAI_FS_CHANNEL_IDENTIFICATION;	 /* FS����Ϊ�������� */		
		SaiInHandle.FrameInit.FSPolarity        = SAI_FS_ACTIVE_LOW;
		SaiInHandle.FrameInit.FSOffset          = SAI_FS_BEFOREFIRSTBIT;

		/* ���� SAI Block_x Slot */
		SaiInHandle.SlotInit.FirstBitOffset = 0;
		SaiInHandle.SlotInit.SlotSize       = SAI_SLOTSIZE_16B;	// SAI_SLOTSIZE_DATASIZE;
		SaiInHandle.SlotInit.SlotNumber     = 2;
		SaiInHandle.SlotInit.SlotActive     = (SAI_SLOTACTIVE_0 | SAI_SLOTACTIVE_1);	// SAI_SLOTACTIVE_0;	//

		if (HAL_OK != HAL_SAI_Init(&SaiInHandle))
		{
			Error_Handler(__FILE__, __LINE__);
		}

		/* Enable SAI to generate clock used by audio driver */
		__HAL_SAI_ENABLE(&SaiInHandle);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: HAL_SAI_TxCpltCallback
*	����˵��: Tx Transfer completed callbacks.  DMA������ɻص�����.
*	��    ��: hsai : SAI_HandleTypeDef �ṹ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{
	s_PlayDmaState = DMA_BUFFER_FULL;
}

/*
*********************************************************************************************************
*	�� �� ��: HAL_SAI_TxHalfCpltCallback
*	����˵��: Tx Transfer Half completed callbacks.  DMA���͵�1��ʱ�Ļص�����.
*	��    ��: hsai : SAI_HandleTypeDef �ṹ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
	s_PlayDmaState = DMA_BUFFER_HALF;
}

/*********************************************************************************************************
*	�� �� ��: AUDIO_Play
*	����˵��: IS2Э�����������I2S���ݰ���DMAģʽ���Զ�ֹͣ
*	��    ��: pBuffer : ��������
*			  Size : ��������. ÿͨ��16bit��˫ͨ����ÿ������ռ4�ֽڡ�
*	�� �� ֵ: ��
*********************************************************************************************************
*/
uint32_t AUDIO_Play(int16_t* pBuffer, uint32_t Size)
{		
	s_PlayFileBuf = pBuffer;	/* ����ָ�� */
	s_PlayFileLen = Size;		/* �����ļ����� */
	
	/* Initialize the data buffer */
	for (int i = 0; i < PLAY_BUFF_SIZE; i++)
	{
		s_PlayBuffCache[i] = s_PlayFileBuf[i];
	}

	s_PlayPos = PLAY_BUFF_SIZE;		/* �´λط�λ�� */
	
	s_PlayPointer = -1;
	
	if (HAL_OK != HAL_SAI_Transmit_DMA(&SaiOutHandle, (uint8_t *)s_PlayBuffCache, PLAY_BUFF_SIZE))
	{
		Error_Handler(__FILE__, __LINE__);
	}
	return 0;
}

/*
*********************************************************************************************************
*	�� �� ��: AUDIO_Poll
*	����˵��: ����¼������ѯ����������whileѭ��ִ��. ���ڽ��ļ����������ݰ��Ƶ�32�ֽڶ����DMA������.
*	��    ��: pBuffer : ��������
*			  Size : ��������. ÿͨ��16bit��˫ͨ����ÿ������ռ4�ֽڡ�
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void AUDIO_Poll(void)
{
	{
		uint8_t file_end = 0;
		
		/* ������һ�룬����׼���õ�1���������� */
		if (s_PlayDmaState == DMA_BUFFER_HALF)
		{
			s_PlayDmaState = DMA_BUFFER_NONE;
			
			/* ��cashe����ȡ���µ�sram���� */
//			SCB_CleanDCache_by_Addr((uint32_t *)s_PlayBuffCache, PLAY_BUFF_SIZE);
			SCB_CleanInvalidateDCache();
			
			/* ��DMA�ڴ�����ת�Ƶ��ļ������� */
			for (int i = 0; i < PLAY_BUFF_SIZE / 2; i++)
			{
				if (s_PlayPos < s_PlayFileLen)
				{
					s_PlayBuffCache[i] = s_PlayFileBuf[s_PlayPos++];
				}
				else
				{
					s_PlayBuffCache[i] = 0;		/* ���㲹0���� */
					file_end = 1;
				}
			}
			
			if (s_PlayDmaState != DMA_BUFFER_NONE)
			{
				/* ����������������ݣ��������ڴ˴� */
				Error_Handler(__FILE__, __LINE__);
			}			
		}

		/* ������ϣ�����׼���õ�2����������. DMAѭ��ִ�� */
		if (s_PlayDmaState == DMA_BUFFER_FULL)
		{
			s_PlayDmaState = DMA_BUFFER_NONE;
			
			/* ��cashe����ȡ���µ�sram���� */
//			SCB_CleanDCache_by_Addr((uint32_t *)s_PlayBuffCache + PLAY_BUFF_SIZE, PLAY_BUFF_SIZE);
			SCB_CleanInvalidateDCache();
			
			/* ��DMA�ڴ�����ת�Ƶ��ļ������� */
			for (int i = 0; i < PLAY_BUFF_SIZE / 2; i++)
			{
				if (s_PlayPos < s_PlayFileLen)
				{
					s_PlayBuffCache[i + PLAY_BUFF_SIZE / 2] = s_PlayFileBuf[s_PlayPos++];
				}
				else
				{
					s_PlayBuffCache[i] = 0;		/* ���㲹0���� */
					file_end = 1;
				}
			}

			if (s_PlayDmaState != DMA_BUFFER_NONE)
			{
				/* ����������������ݣ��������ڴ˴� */
				Error_Handler(__FILE__, __LINE__);
			}			
		}		
		
		if (file_end == 1)
		{
			AUDIO_Stop();
		}		
	}	
	
	/* ����¼�� */
	{
		uint8_t file_full = 0;
		
		if (s_RecDmaState == DMA_BUFFER_HALF)
		{
			s_RecDmaState = DMA_BUFFER_NONE;
			
			/* ��cashe����ˢ�µ�sram */
//			SCB_InvalidateDCache_by_Addr((uint32_t *)s_RecBuffCache, REC_BUFF_SIZE);
			SCB_CleanInvalidateDCache();
			
			/* ��DMA�ڴ�����ת�Ƶ��ļ������� */
			for (int i = 0; i < REC_BUFF_SIZE / 2; i++)
			{
				if (s_RecPos < s_RecFileLen)
				{
					s_RecFileBuf[s_RecPos++] = s_RecBuffCache[i];
				}
				else
				{
					file_full = 1;
				}
			}

			if (s_RecDmaState != DMA_BUFFER_NONE)
			{
				/* ����������������ݣ��������ڴ˴� */
				Error_Handler(__FILE__, __LINE__);
			}				
		}

		if (s_RecDmaState == DMA_BUFFER_FULL)
		{
			s_RecDmaState = DMA_BUFFER_NONE;
			
			/* ��cashe����ˢ�µ�sram */
//			SCB_InvalidateDCache_by_Addr((uint32_t *)s_RecBuffCache + REC_BUFF_SIZE, REC_BUFF_SIZE);
			SCB_CleanInvalidateDCache();
			
			/* ��DMA�ڴ�����ת�Ƶ��ļ������� */
			for (int i = 0; i < REC_BUFF_SIZE / 2; i++)
			{
				if (s_RecPos < s_RecFileLen)
				{
					s_RecFileBuf[s_RecPos++] = s_RecBuffCache[i + REC_BUFF_SIZE / 2];
				}
				else
				{
					file_full = 1;
				}
			}
			
			if (s_RecDmaState != DMA_BUFFER_NONE)
			{
				/* ����������������ݣ��������ڴ˴� */
				Error_Handler(__FILE__, __LINE__);
			}			
		}		
		
		if (file_full == 1)
		{
			AUDIO_Stop();
		}		
	}
}

/*
*********************************************************************************************************
*	�� �� ��: HAL_SAI_RxCpltCallback
*	����˵��: Rx Transfer completed callbacks. DMA���ջ��������ص�����
*	��    ��: hsai : SAI_HandleTypeDef structure
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai)
{
	s_RecDmaState = DMA_BUFFER_FULL;
}

/*
*********************************************************************************************************
*	�� �� ��: HAL_SAI_RxHalfCpltCallback
*	����˵��:  Rx Transfer Half completed callbacks. DMA���ջ��������1��ʱ�Ļص�����
*	��    ��: hsai : SAI_HandleTypeDef structure
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
	s_RecDmaState = DMA_BUFFER_HALF;
}

/*
*********************************************************************************************************
*	�� �� ��: AUDIO_Record
*	����˵��: ��ʼ����������DMA���䷽ʽ�� ��������Ҫѭ��ִ��AUDIO_Poll()���������ݵ�DMA������.
*	��    ��: pBuffer : ��Ƶ�ļ���������PCM����
*			  Size : ���������� ����16bit˫����������4�ֽ�1��������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
uint32_t AUDIO_Record(int16_t* pBuffer, uint32_t Size)
{	
	s_RecFileBuf = pBuffer;		/* ����ָ�� */
	s_RecFileLen = Size;		/* �����ļ����� */
	
	s_RecPos = 0;
	
	s_RecDmaState = DMA_BUFFER_NONE;
	
	/* Start the PDM data reception process */
	if (HAL_OK != HAL_SAI_Receive_DMA(&SaiInHandle, (uint8_t*)s_RecBuffCache, REC_BUFF_SIZE))
	{
		Error_Handler(__FILE__, __LINE__);
	}

	return 0;
}

/*
*********************************************************************************************************
*	�� �� ��: AUDIO_GetRecordSampleCount
*	����˵��: ����ϴ�¼������������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
uint32_t AUDIO_GetRecordSampleCount(void)
{
	return s_RecPos;
}

/*
*********************************************************************************************************
*	�� �� ��: AUDIO_Pause
*	����˵��: ��ͣ���š���δ���á�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void AUDIO_Pause(void)
{    
	;
}

/*
*********************************************************************************************************
*	�� �� ��: AUDIO_Resume
*	����˵��: �ָ����š���δ���á�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void AUDIO_Resume(uint32_t Cmd)
{    
	;
}

/*
*********************************************************************************************************
*	�� �� ��: AUDIO_Resume
*	����˵��: ֹͣ¼���Ͳ��š��ر�DMA���ر�DMA�ն�. ��ֹSAI�豸.
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void AUDIO_Stop(void)
{
	if (hSaiOutDma.Instance == SAI_OUT_DMA_STREAM)
	{
		/* ֹͣ���� */
		HAL_NVIC_DisableIRQ(SAI_OUT_DMA_STREAM_IRQ);
		HAL_DMA_DeInit(&hSaiOutDma);		
	}
	
	if (SaiOutHandle.Instance == SAI_OUT)
	{	
		__HAL_SAI_DISABLE(&SaiOutHandle);
	}
	
	if (hSaiInDma.Instance == SAI_IN_DMA_STREAM)
	{
		/* ֹͣ���� */
		HAL_NVIC_DisableIRQ(SAI_IN_DMA_STREAM_IRQ);
		HAL_DMA_DeInit(&hSaiInDma);		
	}
	
	if (SaiInHandle.Instance == SAI_IN)
	{	
		__HAL_SAI_DISABLE(&SaiInHandle);
	}	
}

/*
*********************************************************************************************************
*	�� �� ��: AUDIO_SetVolume
*	����˵��: ����������������������������ͬʱ����.
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void AUDIO_SetVolume(uint8_t Volume)
{
	/* ����������������ͬ���� */
	wm8978_SetEarVolume(Volume);
	wm8978_SetSpkVolume(Volume);
}


/*-----------------------------------------------------------------------------
                    Audio MAL Interface Control Functions
  ----------------------------------------------------------------------------*/

/*
*********************************************************************************************************
*	�� �� ��: HAL_SAI_MspInit
*	����˵��: ����SAIģ�顣���������õ�SAI��¼���õ�SAI��
*	��    ��: hsai
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_SAI_MspInit(SAI_HandleTypeDef *hsai)
{
	GPIO_InitTypeDef  GPIO_Init;

	if (hsai->Instance == SAI_OUT)
	{
		/* Configure DMA used for SAI1 */
		SAI_OUT_DMA_CLK_ENABLE();
		
		/* Enable SAI1 clock */
		SAI_OUT_CLK_ENABLE();

		/* Configure GPIOs used for SAI1 */
		SAI_OUT_MCLK_ENABLE();
		SAI_OUT_SCK_ENABLE();
		SAI_OUT_FS_ENABLE();
		SAI_OUT_SD_ENABLE();

		GPIO_Init.Mode      = GPIO_MODE_AF_PP;
		GPIO_Init.Pull      = GPIO_NOPULL;
		GPIO_Init.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;

		GPIO_Init.Alternate = SAI_OUT_FS_AF;
		GPIO_Init.Pin       = SAI_OUT_FS_PIN;
		HAL_GPIO_Init(SAI_OUT_FS_GPIO_PORT, &GPIO_Init);
		
		GPIO_Init.Alternate = SAI_OUT_SCK_AF;
		GPIO_Init.Pin       = SAI_OUT_SCK_PIN;
		HAL_GPIO_Init(SAI_OUT_SCK_GPIO_PORT, &GPIO_Init);
		
		GPIO_Init.Alternate = SAI_OUT_SD_AF;
		GPIO_Init.Pin       = SAI_OUT_SD_PIN;
		HAL_GPIO_Init(SAI_OUT_SD_GPIO_PORT, &GPIO_Init);
		
		GPIO_Init.Alternate = SAI_OUT_MCLK_AF;
		GPIO_Init.Pin       = SAI_OUT_MCLK_PIN;
		HAL_GPIO_Init(SAI_OUT_MCLK_GPIO_PORT, &GPIO_Init);
	
		hSaiOutDma.Init.Request             = SAI_OUT_DMA_REQUEST;
		hSaiOutDma.Init.Direction           = DMA_MEMORY_TO_PERIPH;
		hSaiOutDma.Init.PeriphInc           = DMA_PINC_DISABLE;
		hSaiOutDma.Init.MemInc              = DMA_MINC_ENABLE;
		hSaiOutDma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
		hSaiOutDma.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
		hSaiOutDma.Init.Mode                = DMA_CIRCULAR;
		hSaiOutDma.Init.Priority            = DMA_PRIORITY_HIGH;
		hSaiOutDma.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
		hSaiOutDma.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
		hSaiOutDma.Init.MemBurst            = DMA_MBURST_SINGLE;
		hSaiOutDma.Init.PeriphBurst         = DMA_PBURST_SINGLE;

		/* Select the DMA instance to be used for the transfer : DMA2_Stream6 */
		hSaiOutDma.Instance                 = SAI_OUT_DMA_STREAM;

		/* Associate the DMA handle */
		__HAL_LINKDMA(hsai, hdmatx, hSaiOutDma);

		/* Deinitialize the Stream for new transfer */
		HAL_DMA_DeInit(&hSaiOutDma);

		/* Configure the DMA Stream */
		if (HAL_OK != HAL_DMA_Init(&hSaiOutDma))
		{
			Error_Handler(__FILE__, __LINE__);
		}

		HAL_NVIC_SetPriority(SAI_OUT_DMA_STREAM_IRQ, 0x01, 0);
		HAL_NVIC_EnableIRQ(SAI_OUT_DMA_STREAM_IRQ);		
	}

	else if (hsai->Instance == SAI_IN)
	{
		/* Configure DMA used for SAI1 */
		SAI_IN_DMA_CLK_ENABLE();
		
		/* Enable SAI1 clock */
		SAI_IN_CLK_ENABLE();

		/* Configure GPIOs used for SAI1 */
		SAI_IN_SD_ENABLE();

		GPIO_Init.Mode      = GPIO_MODE_AF_PP;
		GPIO_Init.Pull      = GPIO_NOPULL;
		GPIO_Init.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;

		GPIO_Init.Alternate = SAI_IN_SD_AF;
		GPIO_Init.Pin       = SAI_IN_SD_PIN;
		HAL_GPIO_Init(SAI_IN_SD_GPIO_PORT, &GPIO_Init);
		
		hSaiInDma.Init.Request             = SAI_IN_DMA_REQUEST;
		hSaiInDma.Init.Direction           = DMA_PERIPH_TO_MEMORY;
		hSaiInDma.Init.PeriphInc           = DMA_PINC_DISABLE;
		hSaiInDma.Init.MemInc              = DMA_MINC_ENABLE;
		hSaiInDma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;	/* 16bit */
		hSaiInDma.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;	/* 16bit */
		hSaiInDma.Init.Mode                = DMA_CIRCULAR;
		hSaiInDma.Init.Priority            = DMA_PRIORITY_HIGH;
		hSaiInDma.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;	// DMA_FIFOMODE_ENABLE; DMA_FIFOMODE_DISABLE
		hSaiInDma.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
		hSaiInDma.Init.MemBurst            = DMA_MBURST_SINGLE;
		hSaiInDma.Init.PeriphBurst         = DMA_PBURST_SINGLE;

		/* Select the DMA instance to be used for the transfer : DMA2_Stream6 */
		hSaiInDma.Instance                 = SAI_IN_DMA_STREAM;

		/* Associate the DMA handle */
		__HAL_LINKDMA(hsai, hdmarx, hSaiInDma);

		/* Deinitialize the Stream for new transfer */
		HAL_DMA_DeInit(&hSaiInDma);

		/* Configure the DMA Stream */
		if (HAL_OK != HAL_DMA_Init(&hSaiInDma))
		{
			Error_Handler(__FILE__, __LINE__);
		}

		HAL_NVIC_SetPriority(SAI_IN_DMA_STREAM_IRQ, 0x01, 0);
		HAL_NVIC_EnableIRQ(SAI_IN_DMA_STREAM_IRQ);			
	}
}

/*
*********************************************************************************************************
*	�� �� ��: SAI_OUT_DMA_STREAM_IRQHandler
*	����˵��: ����DMA�ն�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SAI_OUT_DMA_STREAM_IRQHandler(void)
{
	HAL_DMA_IRQHandler(SaiOutHandle.hdmatx);
}

/*
*********************************************************************************************************
*	�� �� ��: SAI_IN_DMA_STREAM_IRQHandler
*	����˵��: ¼��DMA�ն�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SAI_IN_DMA_STREAM_IRQHandler(void)
{
	HAL_DMA_IRQHandler(SaiInHandle.hdmarx);
}


/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
