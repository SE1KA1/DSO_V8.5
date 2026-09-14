/**
 * @file bsp_spi_ads1256.c
 * @brief 安富莱 BSP 通用驱动（ADS1256 ADC）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : ADS1256 ����ģ��(8ͨ����PGA��24λADC)
*	�ļ����� : bsp_ads1256.c
*	��    �� : V1.0
*	˵    �� : ADS1256ģ���CPU֮�����SPI�ӿڡ�����������֧������SPI�ӿڡ�
*			  ͨ�����л���
*
*	�޸ļ�¼ :
*		�汾��  ����         ����     ˵��
*		V1.0    2014-01-01  armfly  ��ʽ����
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

#define SOFT_SPI		/* ������б�ʾʹ��GPIOģ��SPI�ӿ� */
//#define HARD_SPI		/* ������б�ʾʹ��CPU��Ӳ��SPI�ӿ� */

/*
    ADS1256ģ��    STM32-V7������(ʾ�����ӿ�)
      +5V   <------  5.0V      5V����
      GND   -------  GND       ��
      DRDY  ------>  PC6       ׼������
      CS    <------  PC7       SPI_CS
      DIN   <------  PG10      SPI_MOSI
      DOUT  ------>  PA5       SPI_MISO
      SCLK  <------  PA4       SPIʱ��
      GND   -------  GND       ��
      PDWN  <------  PB7       �������
      RST   <------  PC3       ��λ�ź�
      NC   �ս�
      NC   �ս�
*/

/*
	ADS1256��������:
	1��ģ�ⲿ�ֹ���5V;
	2��SPI���ֽӿڵ�ƽ��3.3V
	3��PGA���÷�Χ�� 1��2��4��8��16��32��64��
	4���ο���ѹ2.5V (�Ƽ�ȱʡ�ģ����õģ�
	5�������ѹ��Χ��PGA = 1 ʱ, ����������5V
	6. �Զ�У׼ ����������PGA,BUFʹ�ܡ����ݲ�����ʱ����������У׼)
	7. ����Ļ��������������ú͹رգ�һ��ѡ���ã�


	�ⲿ����Ƶ�� = 7.68MHz, 
		ʱ��Ƶ�� tCLK = 1/7.68M = 0.13uS
		����������� tDATA =  1 / 30K = 0.033mS  (��30Ksps����)
	
	��SPI��ʱ���ٶ�Ҫ��: (ads1256.pdf page 6)
		��� 4��tCLK = 0.52uS
		���� 10��tDATA = 0.3mS (�� 30Ksps ����)
		
		SCL�ߵ�ƽ�͵͵�ƽ����ʱ����С 200ns
	
	RREG, WREG, RDATA ����֮����Ҫ�ӳ� 4 * tCLK = 0.52uS;
	RDATAC, RESET, SYNC ����֮����Ҫ�ӳ� 24 * tCLK = 3.12uS;
	
	ʵ�ʲ��ԣ���3.3V�ϵ��, ��ʱ�����κ����ã�ADS125��DRDY ���߼���ʼ��������źţ�2.6us��,33.4�ͣ�Ƶ��30KHz��
*/

/*
	���Լ�¼
	(1) ���üĴ���ʱ��SCK���쵼��оƬ����ÿ�ζ��յ����ݡ�ԭ��: ���͵����ڵ��ֽ�֮����Ҫ�ӳ�һС��ʱ��.
	(2) ������λCPUʱ��ż������оƬ����������쳣��
*/

#ifdef SOFT_SPI		/* ����SPI */
	/* ����GPIO�˿� */	
	#define SCK_CLK_ENABLE() 	__HAL_RCC_GPIOA_CLK_ENABLE()
/* 宏定义: SCK_GPIO - SCK_GPIO 的宏定义。 */
	#define SCK_GPIO			GPIOA
/* 宏定义: SCK_PIN - SCK_PIN 的宏定义。 */
	#define SCK_PIN				GPIO_PIN_4
/* 宏定义: SCK_1 - SCK_1 的宏定义。 */
	#define SCK_1()				SCK_GPIO->BSRRL = SCK_PIN
/* 宏定义: SCK_0 - SCK_0 的宏定义。 */
	#define SCK_0()				SCK_GPIO->BSRRH = SCK_PIN	

/* 宏定义: DIN_CLK_ENABLE - DIN_CLK_ENABLE 的宏定义。 */
	#define DIN_CLK_ENABLE() 	__HAL_RCC_GPIOG_CLK_ENABLE()
/* 宏定义: DIN_GPIO - DIN_GPIO 的宏定义。 */
	#define DIN_GPIO			GPIOG
/* 宏定义: DIN_PIN - DIN_PIN 的宏定义。 */
	#define DIN_PIN				GPIO_PIN_10
/* 宏定义: DIN_1 - DIN_1 的宏定义。 */
	#define DIN_1()				DIN_GPIO->BSRRL = DIN_PIN
/* 宏定义: DIN_0 - DIN_0 的宏定义。 */
	#define DIN_0()				DIN_GPIO->BSRRH = DIN_PIN		

/* 宏定义: CS_CLK_ENABLE - CS_CLK_ENABLE 的宏定义。 */
	#define CS_CLK_ENABLE() 	__HAL_RCC_GPIOC_CLK_ENABLE()
/* 宏定义: CS_GPIO - CS_GPIO 的宏定义。 */
	#define CS_GPIO				GPIOC
/* 宏定义: CS_PIN - CS_PIN 的宏定义。 */
	#define CS_PIN				GPIO_PIN_7
/* 宏定义: CS_1 - CS_1 的宏定义。 */
	#define CS_1()				CS_GPIO->BSRRL = CS_PIN
/* 宏定义: CS_0 - CS_0 的宏定义。 */
	#define CS_0()				CS_GPIO->BSRRH = CS_PIN		

/* 宏定义: DOUT_CLK_ENABLE - DOUT_CLK_ENABLE 的宏定义。 */
	#define DOUT_CLK_ENABLE() 	__HAL_RCC_GPIOA_CLK_ENABLE()
/* 宏定义: DOUT_GPIO - DOUT_GPIO 的宏定义。 */
	#define DOUT_GPIO			GPIOA
/* 宏定义: DOUT_PIN - DOUT_PIN 的宏定义。 */
	#define DOUT_PIN			GPIO_PIN_5
/* 宏定义: DOUT_IS_HIGH - DOUT_IS_HIGH 的宏定义。 */
	#define DOUT_IS_HIGH()		((DOUT_GPIO->IDR & DOUT_PIN) != 0)

/* 宏定义: DRDY_CLK_ENABLE - DRDY_CLK_ENABLE 的宏定义。 */
	#define DRDY_CLK_ENABLE() 	__HAL_RCC_GPIOC_CLK_ENABLE()
/* 宏定义: DRDY_GPIO - DRDY_GPIO 的宏定义。 */
	#define DRDY_GPIO			GPIOC
/* 宏定义: DRDY_PIN - DRDY_PIN 的宏定义。 */
	#define DRDY_PIN			GPIO_PIN_6
/* 宏定义: DRDY_IS_LOW - DRDY_IS_LOW 的宏定义。 */
	#define DRDY_IS_LOW()		((DRDY_GPIO->IDR & DRDY_PIN) == 0)
/* 宏定义: DRDY_IRQn - DRDY_IRQn 的宏定义。 */
	#define DRDY_IRQn 			EXTI9_5_IRQn
/* 宏定义: DRDY_IRQHandler - DRDY_IRQHandler 的宏定义。 */
	#define DRDY_IRQHandler		EXTI9_5_IRQHandler	

	/* PDWN  <------  PB7       ������� */
	#define PWDN_CLK_ENABLE() 	__HAL_RCC_GPIOB_CLK_ENABLE()
/* 宏定义: PWDN_GPIO - PWDN_GPIO 的宏定义。 */
	#define PWDN_GPIO			GPIOB
/* 宏定义: PWDN_PIN - PWDN_PIN 的宏定义。 */
	#define PWDN_PIN			GPIO_PIN_7
/* 宏定义: PWDN_1 - PWDN_1 的宏定义。 */
	#define PWDN_1()			PWDN_GPIO->BSRRL = PWDN_PIN
/* 宏定义: PWDN_0 - PWDN_0 的宏定义。 */
	#define PWDN_0()			PWDN_GPIO->BSRRH = PWDN_PIN		
	
	/*  RST   <------  PC3       ��λ�ź�	 */
	#define RST_CLK_ENABLE() 	__HAL_RCC_GPIOC_CLK_ENABLE()
/* 宏定义: RST_GPIO - RST_GPIO 的宏定义。 */
	#define RST_GPIO			GPIOC
/* 宏定义: RST_PIN - RST_PIN 的宏定义。 */
	#define RST_PIN				GPIO_PIN_3
/* 宏定义: RST_1 - RST_1 的宏定义。 */
	#define RST_1()				RST_GPIO->BSRRL = RST_PIN
/* 宏定义: RST_0 - RST_0 的宏定义。 */
	#define RST_0()				RST_GPIO->BSRRH = RST_PIN		
#endif

#ifdef HARD_SPI		/* Ӳ��SPI */
	;
#endif

/* 宏定义: RCC_DRDY - RCC_DRDY 的宏定义。 */
#define RCC_DRDY             RCC_AHB1Periph_GPIOC
/* 宏定义: PORT_DRDY - PORT_DRDY 的宏定义。 */
#define PORT_DRDY            GPIOC
/* 宏定义: PIN_DRDY - PIN_DRDY 的宏定义。 */
#define PIN_DRDY             GPIO_PIN_6
/* 宏定义: EXTI_PortSourceGPIOx - EXTI_PortSourceGPIOx 的宏定义。 */
#define EXTI_PortSourceGPIOx EXTI_PortSourceGPIOC
/* 宏定义: EXTI_PinSourcex - EXTI_PinSourcex 的宏定义。 */
#define EXTI_PinSourcex      EXTI_PinSource6
/* 宏定义: EXTI_Linex - EXTI_Linex 的宏定义。 */
#define EXTI_Linex           EXTI_Line6
/* 宏定义: EXTI_IRQn - EXTI_IRQn 的宏定义。 */
#define EXTI_IRQn			 EXTI9_5_IRQn;

/* �Ĵ������壺 Table 23. Register Map --- ADS1256�����ֲ��30ҳ */
enum
{
	/* �Ĵ�����ַ�� �����Ǹ�λ��ȱʡֵ */
	REG_STATUS = 0,	// x1H
	REG_MUX    = 1, // 01H
	REG_ADCON  = 2, // 20H
	REG_DRATE  = 3, // F0H
	REG_IO     = 4, // E0H
	REG_OFC0   = 5, // xxH
	REG_OFC1   = 6, // xxH
	REG_OFC2   = 7, // xxH
	REG_FSC0   = 8, // xxH
	REG_FSC1   = 9, // xxH
	REG_FSC2   = 10, // xxH
};

/* ����壺 TTable 24. Command Definitions --- ADS1256�����ֲ��34ҳ */
enum
{
	CMD_WAKEUP  = 0x00,	// Completes SYNC and Exits Standby Mode 0000  0000 (00h)
	CMD_RDATA   = 0x01, // Read Data 0000  0001 (01h)
	CMD_RDATAC  = 0x03, // Read Data Continuously 0000   0011 (03h)
	CMD_SDATAC  = 0x0F, // Stop Read Data Continuously 0000   1111 (0Fh)
	CMD_RREG    = 0x10, // Read from REG rrr 0001 rrrr (1xh)
	CMD_WREG    = 0x50, // Write to REG rrr 0101 rrrr (5xh)
	CMD_SELFCAL = 0xF0, // Offset and Gain Self-Calibration 1111    0000 (F0h)
	CMD_SELFOCAL= 0xF1, // Offset Self-Calibration 1111    0001 (F1h)
	CMD_SELFGCAL= 0xF2, // Gain Self-Calibration 1111    0010 (F2h)
	CMD_SYSOCAL = 0xF3, // System Offset Calibration 1111   0011 (F3h)
	CMD_SYSGCAL = 0xF4, // System Gain Calibration 1111    0100 (F4h)
	CMD_SYNC    = 0xFC, // Synchronize the A/D Conversion 1111   1100 (FCh)
	CMD_STANDBY = 0xFD, // Begin Standby Mode 1111   1101 (FDh)
	CMD_RESET   = 0xFE, // Reset to Power-Up Values 1111   1110 (FEh)
};

/*
*********************************************************************************************************
*   函 数 名: ADS1256_Send8Bit
*   功能说明: ADS1256_Send8Bit 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ADS1256_Send8Bit(uint8_t _data);
/*
*********************************************************************************************************
*   函 数 名: ADS1256_Recive8Bit
*   功能说明: ADS1256_Recive8Bit 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t ADS1256_Recive8Bit(void);
/*
*********************************************************************************************************
*   函 数 名: ADS1256_WaitDRDY
*   功能说明: ADS1256_WaitDRDY 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ADS1256_WaitDRDY(void);
/*
*********************************************************************************************************
*   函 数 名: ADS1256_ResetHard
*   功能说明: ADS1256_ResetHard 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ADS1256_ResetHard(void);
/*
*********************************************************************************************************
*   函 数 名: ADS1256_DelaySCLK
*   功能说明: ADS1256_DelaySCLK 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ADS1256_DelaySCLK(void);
/*
*********************************************************************************************************
*   函 数 名: ADS1256_DelayDATA
*   功能说明: ADS1256_DelayDATA 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ADS1256_DelayDATA(void);

/*
*********************************************************************************************************
*   函 数 名: ADS1256_WriteCmd
*   功能说明: ADS1256_WriteCmd 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ADS1256_WriteCmd(uint8_t _cmd);
/*
*********************************************************************************************************
*   函 数 名: ADS1256_WriteReg
*   功能说明: ADS1256_WriteReg 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ADS1256_WriteReg(uint8_t _RegID, uint8_t _RegValue);
/*
*********************************************************************************************************
*   函 数 名: ADS1256_ReadReg
*   功能说明: ADS1256_ReadReg 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t ADS1256_ReadReg(uint8_t _RegID);
/*
*********************************************************************************************************
*   函 数 名: ADS1256_ReadData
*   功能说明: ADS1256_ReadData 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static int32_t ADS1256_ReadData(void);
/*
*********************************************************************************************************
*   函 数 名: ADS1256_SetChannal
*   功能说明: ADS1256_SetChannal 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ADS1256_SetChannal(uint8_t _ch);
//static void ADS1256_SetDiffChannal(uint8_t _ch);

ADS1256_VAR_T g_tADS1256;
static const uint8_t s_tabDataRate[ADS1256_DRATE_MAX] = 
{
	0xF0,		/* ��λʱȱʡֵ */
	0xE0,
	0xD0,
	0xC0,
	0xB0,
	0xA1,
	0x92,
	0x82,
	0x72,
	0x63,
	0x53,
	0x43,
	0x33,
	0x20,
	0x13,
	0x03
};

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitADS1256
*	����˵��: ����STM32��GPIO��SPI�ӿڣ��������� ADS1256
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitADS1256(void)
{
#ifdef SOFT_SPI
	GPIO_InitTypeDef gpio_init;
	
	RST_1();
	PWDN_1();
	CS_1();
	SCK_0();		/* SPI���߿���ʱ�������ǵ͵�ƽ */
	DIN_1();

	/* ��GPIOʱ�� */
	SCK_CLK_ENABLE();
	DIN_CLK_ENABLE();
	CS_CLK_ENABLE();
	DOUT_CLK_ENABLE();
	DRDY_CLK_ENABLE();
	PWDN_CLK_ENABLE();
	RST_CLK_ENABLE();

	/* ���ü����������IO */
	gpio_init.Mode = GPIO_MODE_OUTPUT_PP;		/* ����������� */
	gpio_init.Pull = GPIO_NOPULL;				/* ���������費ʹ�� */
	gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;  	/* GPIO�ٶȵȼ� */		
	
	gpio_init.Pin = SCK_PIN;	
	HAL_GPIO_Init(SCK_GPIO, &gpio_init);	

	gpio_init.Pin = DIN_PIN;	
	HAL_GPIO_Init(DIN_GPIO, &gpio_init);	
	
	gpio_init.Pin = CS_PIN;	
	HAL_GPIO_Init(CS_GPIO, &gpio_init);	

	gpio_init.Pin = PWDN_PIN;	
	HAL_GPIO_Init(PWDN_GPIO, &gpio_init);	

	/* DRDY ����Ϊ���� */
	gpio_init.Mode = GPIO_MODE_INPUT;			/* �������� */
	gpio_init.Pull = GPIO_NOPULL;				/* ���������費ʹ�� */
	gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;  	/* GPIO�ٶȵȼ� */
	
	gpio_init.Pin = DRDY_PIN;	
	HAL_GPIO_Init(DRDY_GPIO, &gpio_init);	

	gpio_init.Pin = DOUT_PIN;	
	HAL_GPIO_Init(DOUT_GPIO, &gpio_init);	
#endif
		
	//ADS1256_CfgADC(ADS1256_GAIN_1, ADS1256_1000SPS);	/* ����ADC������ ����1:1, ����������� 1KHz */
}

/*
*********************************************************************************************************
*	�� �� ��: ADS1256_CfgADC
*	����˵��: ����ADC����������������������
*	��    ��: _gain : ����
*			  _drate : ����������� 
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ADS1256_CfgADC(ADS1256_GAIN_E _gain, ADS1256_DRATE_E _drate)
{	
	g_tADS1256.Gain = _gain;
	g_tADS1256.DataRate = _drate;
	
	ADS1256_StopScan();			/* ��ͣCPU�ж� */
	
	ADS1256_ResetHard();		/* Ӳ����λ */

	ADS1256_WaitDRDY();

	{
		uint8_t buf[4];		/* �ݴ�ADS1256 �Ĵ������ò�����֮������д4���Ĵ��� */
		
		/* ״̬�Ĵ�������
			Bits 7-4 ID3, ID2, ID1, ID0  Factory Programmed Identification Bits (Read Only)
		
			Bit 3 ORDER: Data Output Bit Order
				0 = Most Significant Bit First (default)
				1 = Least Significant Bit First
			Input data  is always shifted in most significant byte and bit first. Output data is always shifted out most significant
			byte first. The ORDER bit only controls the bit order of the output data within the byte.

			Bit 2 ACAL : Auto-Calibration
				0 = Auto-Calibration Disabled (default)
				1 = Auto-Calibration Enabled
			When Auto-Calibration is enabled, self-calibration begins at the completion of the WREG command that changes
			the PGA (bits 0-2 of ADCON register), DR (bits 7-0 in the DRATE register) or BUFEN (bit 1 in the STATUS register)
			values.

			Bit 1 BUFEN: Analog Input Buffer Enable
				0 = Buffer Disabled (default)
				1 = Buffer Enabled

			Bit 0 DRDY :  Data Ready (Read Only)	
				This bit duplicates the state of the DRDY pin.

			ACAL=1ʹ����У׼���ܡ��� PGA��BUFEEN, DRATE�ı�ʱ��������У׼
		*/
		buf[0] = (0 << 3) | (1 << 2) | (1 << 1);
		//ADS1256_WriteReg(REG_STATUS, (0 << 3) | (1 << 2) | (1 << 1));
		
		buf[1] = 0x08;	/* ����λ0��ʾAINP�� AIN0,  ����λ8��ʾ AINN �̶��� AINCOM */

		/*	ADCON: A/D Control Register (Address 02h)
			Bit 7 Reserved, always 0 (Read Only)
			Bits 6-5 CLK1, CLK0 : D0/CLKOUT Clock Out Rate Setting
				00 = Clock Out OFF
				01 = Clock Out Frequency = fCLKIN (default)
				10 = Clock Out Frequency = fCLKIN/2
				11 = Clock Out Frequency = fCLKIN/4
				When not using CLKOUT, it is recommended that it be turned off. These bits can only be reset using the RESET pin.

			Bits 4-2 SDCS1, SCDS0: Sensor Detect Current Sources
				00 = Sensor Detect OFF (default)
				01 = Sensor Detect Current = 0.5 �� A
				10 = Sensor Detect Current = 2 �� A
				11 = Sensor Detect Current = 10�� A
				The Sensor Detect Current Sources can be activated to verify  the integrity of an external sensor supplying a signal to the
				ADS1255/6. A shorted sensor produces a very small signal while an open-circuit sensor produces a very large signal.

			Bits 2-0 PGA2, PGA1, PGA0: Programmable Gain Amplifier Setting
				000 = 1 (default)
				001 = 2
				010 = 4
				011 = 8
				100 = 16
				101 = 32
				110 = 64
				111 = 64
		*/
		buf[2] = (0 << 5) | (0 << 2) | (_gain << 1);
		//ADS1256_WriteReg(REG_ADCON, (0 << 5) | (0 << 2) | (GAIN_1 << 1));	/* ѡ��1;1����, ��������5V */

		/* ��Ϊ�л�ͨ���Ͷ����ݺ�ʱ 123uS, ���ɨ���ж�ģʽ����ʱ��������� = DRATE_1000SPS */
		buf[3] = s_tabDataRate[_drate];	// DRATE_10SPS;	/* ѡ������������� */
		
		CS_0();	/* SPIƬѡ = 0 */
		ADS1256_Send8Bit(CMD_WREG | 0);	/* д�Ĵ���������, �����ͼĴ�����ַ */
		ADS1256_Send8Bit(0x03);			/* �Ĵ������� - 1, �˴�3��ʾд4���Ĵ��� */
		
		ADS1256_Send8Bit(buf[0]);	/* ����״̬�Ĵ��� */
		ADS1256_Send8Bit(buf[1]);	/* ��������ͨ������ */
		ADS1256_Send8Bit(buf[2]);	/* ����ADCON���ƼĴ��������� */
		ADS1256_Send8Bit(buf[3]);	/* ��������������� */
		
		CS_1();	/* SPIƬѡ = 1 */		
	}

	bsp_DelayUS(50);	
}

/*
*********************************************************************************************************
*	�� �� ��: ADS1256_DelaySCLK
*	����˵��: CLK֮����ӳ٣�ʱ���ӳ�. ����STM32F407  168M��Ƶ
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void ADS1256_DelaySCLK(void)
{
	uint16_t i;

	/* 
		ȡ 5 ʱ��ʵ��ߵ�ƽ200ns, �͵�ƽ250ns <-- ���ȶ� 
		ȡ 10 ���ϣ��������������� �͵�ƽ400ns �߶�400ns <--- �ȶ�
	*/
	for (i = 0; i < 10; i++);
}

/*
*********************************************************************************************************
*	�� �� ��: ADS1256_DelayDATA
*	����˵��: ��ȡDOUT֮ǰ���ӳ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void ADS1256_DelayDATA(void)
{
	/*  
		Delay from last SCLK edge for DIN to first SCLK rising edge for DOUT: RDATA, RDATAC,RREG Commands 
		��С 50 ��tCLK = 50 * 0.13uS = 6.5uS
	*/
	bsp_DelayUS(10);	/* ��С�ӳ� 6.5uS, �˴�ȡ10us */
}

/*
*********************************************************************************************************
*	�� �� ��: ADS1256_ResetHard
*	����˵��: Ӳ����λ ADS1256оƬ.�͵�ƽ��λ�����4��ʱ�ӣ�Ҳ���� 4x0.13uS = 0.52uS
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void ADS1256_ResetHard(void)
{
	/* ADS1256�����ֲ��7ҳ */
	RST_0();			/* ��λ */
	bsp_DelayUS(5);
	RST_1();

	//PWDN_0();			/* ������� ͬ��*/
	//bsp_DelayUS(2);	
	//PWDN_1();			/* �˳����� */
	
	bsp_DelayUS(5);
	
	//ADS1256_WaitDRDY();	/* �ȴ� DRDY��Ϊ0, �˹���ʵ��: 630us */
}

/*
*********************************************************************************************************
*	�� �� ��: ADS1256_Send8Bit
*	����˵��: ��SPI���߷���8��bit���ݡ� ����CS���ơ�
*	��    ��: _data : ����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void ADS1256_Send8Bit(uint8_t _data)
{
	uint8_t i;

	/* �������Ͷ���ֽ�ʱ����Ҫ�ӳ�һ�� */
	ADS1256_DelaySCLK();
	ADS1256_DelaySCLK();

	/*��ADS1256 Ҫ�� SCL�ߵ�ƽ�͵͵�ƽ����ʱ����С 200ns  */
	for(i = 0; i < 8; i++)
	{
		if (_data & 0x80)
		{
			DIN_1();
		}
		else
		{
			DIN_0();
		}
		SCK_1();				
		ADS1256_DelaySCLK();		
		_data <<= 1;		
		SCK_0();			/* <----  ADS1256 ����SCK�½��ز���DIN����, ���ݱ���ά�� 50nS */
		ADS1256_DelaySCLK();		
	}
}

/*
*********************************************************************************************************
*	�� �� ��: ADS1256_Recive8Bit
*	����˵��: ��SPI���߽���8��bit���ݡ� ����CS���ơ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static uint8_t ADS1256_Recive8Bit(void)
{
	uint8_t i;
	uint8_t read = 0;

	ADS1256_DelaySCLK();
	/*��ADS1256 Ҫ�� SCL�ߵ�ƽ�͵͵�ƽ����ʱ����С 200ns  */
	for (i = 0; i < 8; i++)
	{
		SCK_1();
		ADS1256_DelaySCLK();
		read = read<<1;
		SCK_0();
		if (DOUT_IS_HIGH())
		{
			read++;
		}		
		ADS1256_DelaySCLK();
	}
	return read;
}

/*
*********************************************************************************************************
*	�� �� ��: ADS1256_WriteReg
*	����˵��: дָ���ļĴ���
*	��    ��:  _RegID : �Ĵ���ID
*			  _RegValue : �Ĵ���ֵ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void ADS1256_WriteReg(uint8_t _RegID, uint8_t _RegValue)
{
	CS_0();	/* SPIƬѡ = 0 */
	ADS1256_Send8Bit(CMD_WREG | _RegID);	/* д�Ĵ���������, �����ͼĴ�����ַ */
	ADS1256_Send8Bit(0x00);		/* �Ĵ������� - 1, �˴�д1���Ĵ��� */
	
	ADS1256_Send8Bit(_RegValue);	/* ���ͼĴ���ֵ */
	CS_1();	/* SPIƬѡ = 1 */
}

/*
*********************************************************************************************************
*	�� �� ��: ADS1256_ReadReg
*	����˵��: дָ���ļĴ���
*	��    ��:  _RegID : �Ĵ���ID
*			  _RegValue : �Ĵ���ֵ��
*	�� �� ֵ: �����ļĴ���ֵ��
*********************************************************************************************************
*/
static uint8_t ADS1256_ReadReg(uint8_t _RegID)
{
	uint8_t read;

	CS_0();	/* SPIƬѡ = 0 */
	ADS1256_Send8Bit(CMD_RREG | _RegID);	/* д�Ĵ���������, �����ͼĴ�����ַ */
	ADS1256_Send8Bit(0x00);	/* �Ĵ������� - 1, �˴���1���Ĵ��� */
	
	ADS1256_DelayDATA();	/* �����ӳٲ��ܶ�ȡоƬ�������� */
	
	read = ADS1256_Recive8Bit();	/* ���Ĵ���ֵ */
	CS_1();	/* SPIƬѡ = 1 */

	return read;
}

/*
*********************************************************************************************************
*	�� �� ��: ADS1256_WriteCmd
*	����˵��: ���͵��ֽ�����
*	��    ��:  _cmd : ����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void ADS1256_WriteCmd(uint8_t _cmd)
{
	CS_0();	/* SPIƬѡ = 0 */
	ADS1256_Send8Bit(_cmd);
	CS_1();	/* SPIƬѡ = 1 */
}

/*
*********************************************************************************************************
*	�� �� ��: ADS1256_ReadChipID
*	����˵��: ��оƬID, ��״̬�Ĵ����еĸ�4bit
*	��    ��: ��
*	�� �� ֵ: 8bit״̬�Ĵ���ֵ�ĸ�4λ
*********************************************************************************************************
*/
uint8_t ADS1256_ReadChipID(void)
{
	uint8_t id;

	ADS1256_WaitDRDY();
	id = ADS1256_ReadReg(REG_STATUS);
	return (id >> 4);
}

/*
*********************************************************************************************************
*	�� �� ��: ADS1256_SetChannal
*	����˵��: ����ͨ���š���·���á�AIN- �̶��ӵأ�ACOM).
*	��    ��: _ch : ͨ���ţ� 0-7
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void ADS1256_SetChannal(uint8_t _ch)
{
	/*
	Bits 7-4 PSEL3, PSEL2, PSEL1, PSEL0: Positive Input Channel (AINP) Select
		0000 = AIN0 (default)
		0001 = AIN1
		0010 = AIN2 (ADS1256 only)
		0011 = AIN3 (ADS1256 only)
		0100 = AIN4 (ADS1256 only)
		0101 = AIN5 (ADS1256 only)
		0110 = AIN6 (ADS1256 only)
		0111 = AIN7 (ADS1256 only)
		1xxx = AINCOM (when PSEL3 = 1, PSEL2, PSEL1, PSEL0 are ��don��t care��)

		NOTE: When using an ADS1255 make sure to only select the available inputs.

	Bits 3-0 NSEL3, NSEL2, NSEL1, NSEL0: Negative Input Channel (AINN)Select
		0000 = AIN0
		0001 = AIN1 (default)
		0010 = AIN2 (ADS1256 only)
		0011 = AIN3 (ADS1256 only)
		0100 = AIN4 (ADS1256 only)
		0101 = AIN5 (ADS1256 only)
		0110 = AIN6 (ADS1256 only)
		0111 = AIN7 (ADS1256 only)
		1xxx = AINCOM (when NSEL3 = 1, NSEL2, NSEL1, NSEL0 are ��don��t care��)
	*/
	if (_ch > 7)
	{
		return;
	}
	ADS1256_WriteReg(REG_MUX, (_ch << 4) | (1 << 3));	/* Bit3 = 1, AINN �̶��� AINCOM */
}

/*
*********************************************************************************************************
*	�� �� ��: ADS1256_SetDiffChannal
*	����˵��: ���ò��ͨ���š���·���á�
*	��    ��: _ch : ͨ����,0-3����4��
*	�� �� ֵ: 8bit״̬�Ĵ���ֵ�ĸ�4λ
*********************************************************************************************************
*/
#if 0
/*
*********************************************************************************************************
*   函 数 名: ADS1256_SetDiffChannal
*   功能说明: ADS1256_SetDiffChannal 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ADS1256_SetDiffChannal(uint8_t _ch)
{
	/*
	Bits 7-4 PSEL3, PSEL2, PSEL1, PSEL0: Positive Input Channel (AINP) Select
		0000 = AIN0 (default)
		0001 = AIN1
		0010 = AIN2 (ADS1256 only)
		0011 = AIN3 (ADS1256 only)
		0100 = AIN4 (ADS1256 only)
		0101 = AIN5 (ADS1256 only)
		0110 = AIN6 (ADS1256 only)
		0111 = AIN7 (ADS1256 only)
		1xxx = AINCOM (when PSEL3 = 1, PSEL2, PSEL1, PSEL0 are ��don��t care��)

		NOTE: When using an ADS1255 make sure to only select the available inputs.

	Bits 3-0 NSEL3, NSEL2, NSEL1, NSEL0: Negative Input Channel (AINN)Select
		0000 = AIN0
		0001 = AIN1 (default)
		0010 = AIN2 (ADS1256 only)
		0011 = AIN3 (ADS1256 only)
		0100 = AIN4 (ADS1256 only)
		0101 = AIN5 (ADS1256 only)
		0110 = AIN6 (ADS1256 only)
		0111 = AIN7 (ADS1256 only)
		1xxx = AINCOM (when NSEL3 = 1, NSEL2, NSEL1, NSEL0 are ��don��t care��)
	*/
	if (_ch == 0)
	{
		ADS1256_WriteReg(REG_MUX, (0 << 4) | 1);	/* ������� AIN0�� AIN1 */
	}
	else if (_ch == 1)
	{
		ADS1256_WriteReg(REG_MUX, (2 << 4) | 3);	/* ������� AIN2�� AIN3 */
	}
	else if (_ch == 2)
	{
		ADS1256_WriteReg(REG_MUX, (4 << 4) | 5);	/* ������� AIN4�� AIN5 */
	}
	else if (_ch == 3)
	{
		ADS1256_WriteReg(REG_MUX, (6 << 4) | 7);	/* ������� AIN6�� AIN7 */
	}
}
#endif

/*
*********************************************************************************************************
*	�� �� ��: ADS1256_WaitDRDY
*	����˵��: �ȴ��ڲ�������ɡ� ��У׼ʱ��ϳ�����Ҫ�ȴ���
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void ADS1256_WaitDRDY(void)
{
	uint32_t i;

	for (i = 0; i < 40000000; i++)
	{
		if (DRDY_IS_LOW())
		{
			break;
		}
	}
	if (i >= 40000000)
	{
		printf("ADS1256_WaitDRDY() Time Out ...\r\n");		/* �������. �����Ŵ� */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: ADS1256_ReadData
*	����˵��: ��ADC����
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static int32_t ADS1256_ReadData(void)
{
	uint32_t read = 0;

	CS_0();	/* SPIƬѡ = 0 */

	ADS1256_Send8Bit(CMD_RDATA);	/* �����ݵ����� */
	
	ADS1256_DelayDATA();	/* �����ӳٲ��ܶ�ȡоƬ�������� */

	/* �����������3���ֽڣ����ֽ���ǰ */
	read = ADS1256_Recive8Bit() << 16;
	read += ADS1256_Recive8Bit() << 8;
	read += ADS1256_Recive8Bit() << 0;

	CS_1();	/* SPIƬѡ = 1 */
	
	/* ����������չ��24λ�з�������չΪ32λ�з����� */
	if (read & 0x800000)
	{
		read += 0xFF000000;
	}
	
	return (int32_t)read;
}

/*
*********************************************************************************************************
*	�� �� ��: ADS1256_ReadAdc
*	����˵��: ��ָ��ͨ����ADC����
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
int32_t ADS1256_ReadAdc(uint8_t _ch)
{
	/* ADS1256 �����ֲ��21ҳ */
	
#if 0	/* ����30Ksps �������� */
	int32_t read;
	
	while (DRDY_IS_LOW());	/* �ȴ� DRDY �� */			
	while (!DRDY_IS_LOW());	/* �ȴ� DRDY �� */			
	
	ADS1256_SetChannal(_ch);	/* �л�ģ��ͨ�� */	
	bsp_DelayUS(5);
	
	ADS1256_WriteCmd(CMD_SYNC);
	bsp_DelayUS(5);
	
	ADS1256_WriteCmd(CMD_WAKEUP);  /* ��������£����ʱ�� DRDY �Ѿ�Ϊ�� */
	bsp_DelayUS(25);
			
	read =  (int32_t)ADS1256_ReadData();

	while (DRDY_IS_LOW());	/* �ȴ� DRDY �� */			
	while (!DRDY_IS_LOW());	/* �ȴ� DRDY �� */			
	
	read =  (int32_t)ADS1256_ReadData();

	return read;
#else	
	//while (DRDY_IS_LOW());
		
	/* ADS1256 �����ֲ��21ҳ */
	ADS1256_WaitDRDY();		/* �ȴ� DRDY = 0 */
	
	ADS1256_SetChannal(_ch);	/* �л�ģ��ͨ�� */	
	bsp_DelayUS(5);
	
	ADS1256_WriteCmd(CMD_SYNC);
	bsp_DelayUS(5);
	
	ADS1256_WriteCmd(CMD_WAKEUP);
	bsp_DelayUS(25);
	
	//ADS1256_WaitDRDY();		/* �ȴ� DRDY = 0 */
	
	return (int32_t)ADS1256_ReadData();
#endif	
}

/*
*********************************************************************************************************
*	����ĺ�������DRDY�жϹ���ģʽ
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*	�� �� ��: ADS1256_StartScan
*	����˵��: �� DRDY���� ��PH9 �����ó��ⲿ�жϴ�����ʽ�� �жϷ��������ɨ��8��ͨ�������ݡ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ADS1256_StartScan(void)
{
	/* PC6 �ⲿ�жϣ�BUSY 
		���� BUSY ��Ϊ�ж�����ڣ��½��ش��� */
	{
		GPIO_InitTypeDef   GPIO_InitStructure;
		
		DRDY_CLK_ENABLE();	/* ��GPIOʱ�� */

		GPIO_InitStructure.Mode = GPIO_MODE_INPUT | GPIO_MODE_IT_FALLING;	/* �ж��½��ش��� */
		GPIO_InitStructure.Pull = GPIO_NOPULL;
		GPIO_InitStructure.Pin = DRDY_PIN;
		HAL_GPIO_Init(DRDY_GPIO, &GPIO_InitStructure);	

		HAL_NVIC_SetPriority(DRDY_IRQn, 2, 0);
		HAL_NVIC_EnableIRQ(DRDY_IRQn);	
	}
	
	/* ��ʼɨ��ǰ, ������������ */	
	{
		uint8_t i;
		
		g_tADS1256.Channel = 0;
		
		for (i = 0; i < 8; i++)
		{
			g_tADS1256.AdcNow[i] = 0;
		}	
	}
}

/*
*********************************************************************************************************
*	�� �� ��: ADS1256_StopScan
*	����˵��: ֹͣ DRDY �ж�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ADS1256_StopScan(void)
{
	/* ��ֹ�ⲿ�ж� */
	HAL_NVIC_DisableIRQ(DRDY_IRQn);	
}

/*
*********************************************************************************************************
*	�� �� ��: ADS1256_GetAdc
*	����˵��: �ӻ�������ȡADC��������������ṹ�����жϷ���������ġ�
*	��    ��: _ch ͨ���� (0 - 7)
*	�� �� ֵ: ADC�ɼ�������з�������
*********************************************************************************************************
*/
int32_t ADS1256_GetAdc(uint8_t _ch)
{
	int32_t iTemp;
	
	if (_ch > 7)
	{
		return 0;
	}
	
	DISABLE_INT();  			/* ���ж� */

	iTemp = g_tADS1256.AdcNow[_ch];

	ENABLE_INT();  				/* ���ж� */		
	
	return iTemp;
}

/*
*********************************************************************************************************
*	�� �� ��: ADS1256_ISR
*	����˵��: ��ʱ�ɼ��жϷ������
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ADS1256_ISR(void)
{
	/* ��ȡ�ɼ��ṹ��������ȫ�ֱ��� */					
	ADS1256_SetChannal(g_tADS1256.Channel);	/* �л�ģ��ͨ�� */	
	bsp_DelayUS(5);
	
	ADS1256_WriteCmd(CMD_SYNC);
	bsp_DelayUS(5);
	
	ADS1256_WriteCmd(CMD_WAKEUP);
	bsp_DelayUS(25);
	
	if (g_tADS1256.Channel == 0)
	{
		g_tADS1256.AdcNow[7] = ADS1256_ReadData();	/* ע�Ᵽ�������һ��ͨ�������� */
	}
	else
	{
		g_tADS1256.AdcNow[g_tADS1256.Channel-1] = ADS1256_ReadData();	/* ע�Ᵽ�������һ��ͨ�������� */
	}
				
	if (++g_tADS1256.Channel >= 8)
	{
		g_tADS1256.Channel = 0;
	}
}
	
/*
*********************************************************************************************************
*	�� �� ��: EXTI9_5_IRQHandler
*	����˵��: �ⲿ�жϷ������.  �˳���ִ��ʱ��Լ 123uS
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
#ifndef EXTI9_5_ISR_MOVE_OUT		/* bsp.h �ж�����У���ʾ�������Ƶ� stam32f4xx_it.c�� �����ظ����� */
void DRDY_IRQHandler(void)
{
	HAL_GPIO_EXTI_IRQHandler(DRDY_PIN);
}

/*
*********************************************************************************************************
*	�� �� ��: EXTI9_5_IRQHandler
*	����˵��: �ⲿ�жϷ��������ڡ�PI6 / AD7606_BUSY �½����жϴ���
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == DRDY_PIN)
	{
		ADS1256_ISR();
	}
}

#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
