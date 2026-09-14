/**
 * @file bsp_tft_429.c
 * @brief 安富莱 BSP 通用驱动（F429 TFT）驱动实现；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : STM32F429�ڲ�LCD��������
*	�ļ����� : bsp_tft_429.c
*	��    �� : V1.0
*	˵    �� : STM32F429 �ڲ�LCD�ӿڵ�Ӳ�����ó���
*	�޸ļ�¼ :
*		�汾��  ����       ����    ˵��
*		V1.0    2014-05-05 armfly ���� STM32F429 �ڲ�LCD�ӿڣ� ����ST�����Ӹ��ģ���Ҫ�������ǰ���㶨�壬ֱ��
*							      �� LTDC_Layer1 �� LTDC_Layer2, ����2���ṹ��ָ��
*		V1.1	2015-11-19 armfly 
*						1. ��ͼ�����滻ΪDMA2DӲ����������߻�ͼЧ��
*						2. ͳһ�����������ú������Զ�ʶ���������
*
*	Copyright (C), 2015-2020, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"
#include "fonts.h"

typedef uint32_t LCD_COLOR;

/* 宏定义: LCD429_FRAME_BUFFER - LCD429_FRAME_BUFFER 的宏定义。 */
#define LCD429_FRAME_BUFFER		SDRAM_LCD_BUF1

/* ƫ�Ƶ�ַ���㹫ʽ::
   Maximum width x Maximum Length x Maximum Pixel size (ARGB8888) in bytes
   => 640 x 480 x 4 =  1228800 or 0x12C000 */
#define BUFFER_OFFSET			SDRAM_LCD_SIZE		// (uint32_t)(g_LcdHeight * g_LcdWidth * 2)

static LTDC_HandleTypeDef   hltdc_F;
static DMA2D_HandleTypeDef  hdma2d;

uint32_t s_CurrentFrameBuffer;
uint8_t s_CurrentLayer;

/*
*********************************************************************************************************
*   函 数 名: LCD429_ConfigLTDC
*   功能说明: LCD429_ConfigLTDC 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void LCD429_ConfigLTDC(void);

/*
*********************************************************************************************************
*   函 数 名: LCD429_LayerInit
*   功能说明: LCD429_LayerInit 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void LCD429_LayerInit(void);

/*
*********************************************************************************************************
*   函 数 名: LCD429_SetPixelFormat
*   功能说明: LCD429_SetPixelFormat 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void LCD429_SetPixelFormat(uint32_t PixelFormat);

/*
*********************************************************************************************************
*   函 数 名: LCD429_InitDMA2D
*   功能说明: LCD429_InitDMA2D 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void LCD429_InitDMA2D(void);
/*
*********************************************************************************************************
*   函 数 名: LCD_LL_GetPixelformat
*   功能说明: LCD_LL_GetPixelformat 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static inline uint32_t LCD_LL_GetPixelformat(uint32_t LayerIndex);
/*
*********************************************************************************************************
*   函 数 名: DMA2D_CopyBuffer
*   功能说明: DMA2D_CopyBuffer 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DMA2D_CopyBuffer(uint32_t LayerIndex, void * pSrc, void * pDst, uint32_t xSize, uint32_t ySize, uint32_t OffLineSrc, uint32_t OffLineDst);
/*
*********************************************************************************************************
*   函 数 名: DMA2D_FillBuffer
*   功能说明: DMA2D_FillBuffer 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void DMA2D_FillBuffer(uint32_t LayerIndex, void * pDst, uint32_t xSize, uint32_t ySize, uint32_t OffLine, uint32_t ColorIndex);

/*
*********************************************************************************************************
*	�� �� ��: LCD429_InitHard
*	����˵��: ��ʼ��LCD
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_InitHard(void)
{
	LCD429_ConfigLTDC();			/* ����429 CPU�ڲ�LTDC */
	
	LCD429_InitDMA2D();

	LCD429_SetLayer(LCD_LAYER_1);	/* �л���ǰ���� */
//	LCD429_SetPixelFormat(LTDC_PIXEL_FORMAT_RGB565);
//	LCD429_ClrScr(CL_BLACK);		/* ��������ʾȫ�� */	
	
	LCD429_QuitWinMode();
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_ConfigLTDC
*	����˵��: ����LTDC
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void LCD429_ConfigLTDC(void)
{
	/* ����LCD��ص�GPIO */
	{
		/* GPIOs Configuration */
		/*
		+------------------------+-----------------------+----------------------------+
		+                       LCD pins assignment                                   +
		+------------------------+-----------------------+----------------------------+
		|  LCD429_TFT R0 <-> PI.15  |  LCD429_TFT G0 <-> PJ.07 |  LCD429_TFT B0 <-> PJ.12      |
		|  LCD429_TFT R1 <-> PJ.00  |  LCD429_TFT G1 <-> PJ.08 |  LCD429_TFT B1 <-> PJ.13      |
		|  LCD429_TFT R2 <-> PJ.01  |  LCD429_TFT G2 <-> PJ.09 |  LCD429_TFT B2 <-> PJ.14      |
		|  LCD429_TFT R3 <-> PJ.02  |  LCD429_TFT G3 <-> PJ.10 |  LCD429_TFT B3 <-> PJ.15      |
		|  LCD429_TFT R4 <-> PJ.03  |  LCD429_TFT G4 <-> PJ.11 |  LCD429_TFT B4 <-> PK.03      |
		|  LCD429_TFT R5 <-> PJ.04  |  LCD429_TFT G5 <-> PK.00 |  LCD429_TFT B5 <-> PK.04      |
		|  LCD429_TFT R6 <-> PJ.05  |  LCD429_TFT G6 <-> PK.01 |  LCD429_TFT B6 <-> PK.05      |
		|  LCD429_TFT R7 <-> PJ.06  |  LCD429_TFT G7 <-> PK.02 |  LCD429_TFT B7 <-> PK.06      |
		-------------------------------------------------------------------------------
		|  LCD429_TFT HSYNC <-> PI.12  | LCDTFT VSYNC <->  PI.13 |
		|  LCD429_TFT CLK   <-> PI.14  | LCD429_TFT DE   <->  PK.07 |
		-----------------------------------------------------
		*/		
		GPIO_InitTypeDef GPIO_Init_Structure;

		/*##-1- Enable peripherals and GPIO Clocks #################################*/  
		/* Enable the LTDC Clock */
		__HAL_RCC_LTDC_CLK_ENABLE();
		/* Enable GPIOs clock */
		__HAL_RCC_GPIOI_CLK_ENABLE();
		__HAL_RCC_GPIOJ_CLK_ENABLE();
		__HAL_RCC_GPIOK_CLK_ENABLE();

		/*** LTDC Pins configuration ***/
		/* GPIOI configuration */
		GPIO_Init_Structure.Pin       = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15; 
		GPIO_Init_Structure.Mode      = GPIO_MODE_AF_PP;
		GPIO_Init_Structure.Pull      = GPIO_NOPULL;
		GPIO_Init_Structure.Speed     = GPIO_SPEED_FREQ_HIGH;
		GPIO_Init_Structure.Alternate = GPIO_AF14_LTDC;  
		HAL_GPIO_Init(GPIOI, &GPIO_Init_Structure);

		/* GPIOJ configuration */  
		GPIO_Init_Structure.Pin       = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | \
									  GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | \
									  GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | \
									  GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15; 
		GPIO_Init_Structure.Mode      = GPIO_MODE_AF_PP;
		GPIO_Init_Structure.Pull      = GPIO_NOPULL;
		GPIO_Init_Structure.Speed     = GPIO_SPEED_FREQ_HIGH;
		GPIO_Init_Structure.Alternate = GPIO_AF14_LTDC;  
		HAL_GPIO_Init(GPIOJ, &GPIO_Init_Structure);  

		/* GPIOK configuration */  
		GPIO_Init_Structure.Pin       = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | \
									  GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7; 
		GPIO_Init_Structure.Mode      = GPIO_MODE_AF_PP;
		GPIO_Init_Structure.Pull      = GPIO_NOPULL;
		GPIO_Init_Structure.Speed     = GPIO_SPEED_FREQ_HIGH;
		GPIO_Init_Structure.Alternate = GPIO_AF14_LTDC;  
		HAL_GPIO_Init(GPIOK, &GPIO_Init_Structure);  	
	}
	
	/* LTDC Initialization -------------------------------------------------------*/
	{	
		LTDC_LayerCfgTypeDef      pLayerCfg;
		uint16_t Width, Height, HSYNC_W, HBP, HFP, VSYNC_W, VBP, VFP;
		RCC_PeriphCLKInitTypeDef  PeriphClkInitStruct;

		/* ֧��6����� */
		switch (g_LcdType)
		{
			case LCD_35_480X320:	/* 3.5�� 480 * 320 */	
				Width = 480;
				Height = 272;
				HSYNC_W = 10;
				HBP = 20;
				HFP = 20;
				VSYNC_W = 20;
				VBP = 20;
				VFP = 20;
				break;
			
			case LCD_43_480X272:		/* 4.3�� 480 * 272 */			
				Width = 480;
				Height = 272;

				HSYNC_W = 40;
				HBP = 2;
				HFP = 2;
				VSYNC_W = 9;
				VBP = 2;
				VFP = 2;
		
				/* AMPIRE480272 LCD clock configuration */
				/* LCD clock configuration */
				/* PLL3_VCO Input = HSE_VALUE/PLL3M = 5 Mhz */
				/* PLL3_VCO Output = PLL3_VCO Input * PLL3N = 800 Mhz */
				/* PLLLCDCLK = PLL3_VCO Output/PLL3R = 800/83 = 9.63 Mhz */
				/* LTDC clock frequency = PLLLCDCLK = 9.63 Mhz */
				PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
				PeriphClkInitStruct.PLL3.PLL3M = 5;
				PeriphClkInitStruct.PLL3.PLL3N = 48;
				PeriphClkInitStruct.PLL3.PLL3P = 2;
				PeriphClkInitStruct.PLL3.PLL3Q = 5;
				PeriphClkInitStruct.PLL3.PLL3R = 10;				
				HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);     			
				break;
			
			case LCD_50_480X272:		/* 5.0�� 480 * 272 */
				Width = 480;
				Height = 272;
			
				HSYNC_W = 40;
				HBP = 2;
				HFP = 2;
				VSYNC_W = 9;
				VBP = 2;
				VFP = 2;			
				break;
			
			case LCD_50_800X480:		/* 5.0�� 800 * 480 */
			case LCD_70_800X480:		/* 7.0�� 800 * 480 */					
				Width = 800;
				Height = 480;

				HSYNC_W = 96;	/* =10ʱ����ʾ��λ��20ʱ���������Ե�,80ʱȫ��OK */
				HBP = 10;
				HFP = 10;
				VSYNC_W = 2;
				VBP = 10;
				VFP = 10;			

				/* AMPIRE640480 typical PCLK is 25 MHz so the PLL3R is configured to provide this clock */ 
				/* AMPIRE640480 LCD clock configuration */
				/* PLL3_VCO Input = HSE_VALUE/PLL3M = 5 Mhz */
				/* PLL3_VCO Output = PLL3_VCO Input * PLL3N = 800 Mhz */
				/* PLLLCDCLK = PLL3_VCO Output/PLL3R = 800/32 = 25Mhz */
				/* LTDC clock frequency = PLLLCDCLK = 25 Mhz */    
				PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
				PeriphClkInitStruct.PLL3.PLL3M = 5;
				PeriphClkInitStruct.PLL3.PLL3N = 48;
				PeriphClkInitStruct.PLL3.PLL3P = 2;
				PeriphClkInitStruct.PLL3.PLL3Q = 5;
				PeriphClkInitStruct.PLL3.PLL3R = 10; 
				HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);     			
				break;
			
			case LCD_70_1024X600:		/* 7.0�� 1024 * 600 */
				/* ʵ������ʱ�� = 53.7M */
				Width = 1024;
				Height = 600;

				HSYNC_W = 2;	/* =10ʱ����ʾ��λ��20ʱ���������Ե�,80ʱȫ��OK */
				HBP = 157;
				HFP = 160;
				VSYNC_W = 2;
				VBP = 20;
				VFP = 12;		
			
				PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
				PeriphClkInitStruct.PLL3.PLL3M = 5;
				PeriphClkInitStruct.PLL3.PLL3N = 48;
				PeriphClkInitStruct.PLL3.PLL3P = 2;
				PeriphClkInitStruct.PLL3.PLL3Q = 5;
				PeriphClkInitStruct.PLL3.PLL3R = 10;
				HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct); 			
				break;
			
			default:
				Width = 800;
				Height = 480;

				HSYNC_W = 80;	/* =10ʱ����ʾ��λ��20ʱ���������Ե�,80ʱȫ��OK */
				HBP = 10;
				HFP = 10;
				VSYNC_W = 10;
				VBP = 10;
				VFP = 10;		
			
				PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
					PeriphClkInitStruct.PLL3.PLL3M = 5;
				PeriphClkInitStruct.PLL3.PLL3N = 48;
				PeriphClkInitStruct.PLL3.PLL3P = 2;
				PeriphClkInitStruct.PLL3.PLL3Q = 5;
				PeriphClkInitStruct.PLL3.PLL3R = 10;  
				HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct); 			
				break;
		}		

		g_LcdHeight = Height;
		g_LcdWidth = Width;
		
		/* �����źż��� */	
		/* Initialize the horizontal synchronization polarity as active low */
		hltdc_F.Init.HSPolarity = LTDC_HSPOLARITY_AL;	/* HSYNC �͵�ƽ��Ч */
		/* Initialize the vertical synchronization polarity as active low */ 
		hltdc_F.Init.VSPolarity = LTDC_VSPOLARITY_AL; 	/* VSYNC �͵�ƽ��Ч */
		/* Initialize the data enable polarity as active low */ 
		hltdc_F.Init.DEPolarity = LTDC_DEPOLARITY_AL; 	/* DE �͵�ƽ��Ч */
		/* Initialize the pixel clock polarity as input pixel clock */  
		hltdc_F.Init.PCPolarity = LTDC_PCPOLARITY_IPC;

		/* The AMPIRE LCD 480x272 is selected */
		/* Timing Configuration */    
		hltdc_F.Init.HorizontalSync = (HSYNC_W - 1);
		hltdc_F.Init.VerticalSync = (VSYNC_W - 1);
		hltdc_F.Init.AccumulatedHBP = (HSYNC_W + HBP - 1);
		hltdc_F.Init.AccumulatedVBP = (VSYNC_W + VBP - 1);  
		hltdc_F.Init.AccumulatedActiveH = (Height + VSYNC_W + VBP - 1);
		hltdc_F.Init.AccumulatedActiveW = (Width + HSYNC_W + HBP - 1);
		hltdc_F.Init.TotalHeigh = (Height + VSYNC_W + VBP + VFP - 1);
		hltdc_F.Init.TotalWidth = (Width + HSYNC_W + HBP + HFP - 1); 

		/* Configure R,G,B component values for LCD background color : all black background */
		hltdc_F.Init.Backcolor.Blue = 0;
		hltdc_F.Init.Backcolor.Green = 0;
		hltdc_F.Init.Backcolor.Red = 0;

		hltdc_F.Instance = LTDC;

		/* Layer1 Configuration ------------------------------------------------------*/

		/* Windowing configuration */ 
		/* In this case all the active display area is used to display a picture then :
		Horizontal start = horizontal synchronization + Horizontal back porch = 43 
		Vertical start   = vertical synchronization + vertical back porch     = 12
		Horizontal stop = Horizontal start + window width -1 = 43 + 480 -1 
		Vertical stop   = Vertical start + window height -1  = 12 + 272 -1      */
		pLayerCfg.WindowX0 = 0;
		pLayerCfg.WindowX1 = Width;
		pLayerCfg.WindowY0 = 0;
		pLayerCfg.WindowY1 = Height;

		/* Pixel Format configuration*/ 
		pLayerCfg.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;

		/* Start Address configuration : frame buffer is located at FLASH memory */
		pLayerCfg.FBStartAdress = LCD429_FRAME_BUFFER;	

		/* Alpha constant (255 == totally opaque) */
		pLayerCfg.Alpha = 255;

		/* Default Color configuration (configure A,R,G,B component values) : no background color */
		pLayerCfg.Alpha0 = 0; /* fully transparent */
		pLayerCfg.Backcolor.Blue = 0;
		pLayerCfg.Backcolor.Green = 0;
		pLayerCfg.Backcolor.Red = 0;

		/* Configure blending factors */
		pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
		pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;

		/* Configure the number of lines and number of pixels per line */
		pLayerCfg.ImageWidth  = Width;
		pLayerCfg.ImageHeight = Height;

		/* Configure the LTDC */  
		if (HAL_LTDC_Init(&hltdc_F) != HAL_OK)
		{
			/* Initialization Error */
			Error_Handler(__FILE__, __LINE__);
		}

		/* Configure the Layer,  0 �ǵ�1�㣬 1�ǵ�2�� */
		if (HAL_LTDC_ConfigLayer(&hltdc_F, &pLayerCfg, LTDC_LAYER_1) != HAL_OK)
		{
			/* Initialization Error */
			Error_Handler(__FILE__, __LINE__);
		}  

		if (HAL_LTDC_ConfigLayer(&hltdc_F, &pLayerCfg, LTDC_LAYER_2) != HAL_OK)
		{
			/* Initialization Error */
			Error_Handler(__FILE__, __LINE__);
		}  		
	}  
	
	/* Set LTDC Interrupt to the lowest priority */
	HAL_NVIC_SetPriority(LTDC_IRQn, 0xE, 0);

	/* Enable LTDC Interrupt */
	HAL_NVIC_EnableIRQ(LTDC_IRQn);	
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_GetChipDescribe
*	����˵��: ��ȡLCD����оƬ���������ţ�������ʾ
*	��    ��: char *_str : �������ַ�������˻�����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_GetChipDescribe(char *_str)
{
	strcpy(_str, "STM32H743");
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_SetLayer
*	����˵��: �л��㡣ֻ�Ǹ��ĳ���������Ա��ں���Ĵ��������ؼĴ�����Ӳ��֧��2�㡣
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_SetLayer(uint8_t _ucLayer)
{
	if (_ucLayer == LCD_LAYER_1)
	{
		s_CurrentFrameBuffer = LCD429_FRAME_BUFFER;
		s_CurrentLayer = LCD_LAYER_1;
	}
	else if (_ucLayer == LCD_LAYER_2)
	{
		s_CurrentFrameBuffer = LCD429_FRAME_BUFFER + BUFFER_OFFSET;
		s_CurrentLayer = LCD_LAYER_2;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_SetTransparency
*	����˵��: ���õ�ǰ���͸������
*	��    ��: ͸���ȣ� ֵ�� 0x00 - 0xFF
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_SetTransparency(uint8_t transparency)
{
	HAL_LTDC_SetAlpha(&hltdc_F, transparency, s_CurrentLayer);	/* ����ˢ�� */
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_SetPixelFormat
*	����˵��: ���õ�ǰ������ظ�ʽ
*	��    ��: ���ظ�ʽ:
*                      LTDC_PIXEL_FORMAT_ARGB8888
*                      LTDC_PIXEL_FORMAT_ARGB8888_RGB888
*                      LTDC_PIXEL_FORMAT_ARGB8888_RGB565
*                      LTDC_PIXEL_FORMAT_ARGB8888_ARGB1555
*                      LTDC_PIXEL_FORMAT_ARGB8888_ARGB4444
*                      LTDC_PIXEL_FORMAT_ARGB8888_L8
*                      LTDC_PIXEL_FORMAT_ARGB8888_AL44
*                      LTDC_PIXEL_FORMAT_ARGB8888_AL88
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_SetPixelFormat(uint32_t PixelFormat)
{
	HAL_LTDC_SetPixelFormat(&hltdc_F, PixelFormat, s_CurrentLayer);
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_SetDispWin
*	����˵��: ������ʾ���ڣ����봰����ʾģʽ��
*	��    ��:  
*		_usX : ˮƽ����
*		_usY : ��ֱ����
*		_usHeight: ���ڸ߶�
*		_usWidth : ���ڿ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_SetDispWin(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth)
{
	HAL_LTDC_SetWindowSize_NoReload(&hltdc_F, _usWidth, _usHeight, s_CurrentLayer);	/* ���������� */
	HAL_LTDC_SetWindowPosition(&hltdc_F, _usX, _usY, s_CurrentLayer);		/* �������� */
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_QuitWinMode
*	����˵��: �˳�������ʾģʽ����Ϊȫ����ʾģʽ
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_QuitWinMode(void)
{
	HAL_LTDC_SetWindowSize_NoReload(&hltdc_F, g_LcdWidth, g_LcdHeight, s_CurrentLayer);	/* ���������� */
	HAL_LTDC_SetWindowPosition(&hltdc_F, 0, 0, s_CurrentLayer);		/* �������� */	
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_DispOn
*	����˵��: ����ʾ
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_DispOn(void)
{
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_DispOff
*	����˵��: �ر���ʾ
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_DispOff(void)
{
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_ClrScr
*	����˵��: �����������ɫֵ����
*	��    ��: _usColor : ����ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_ClrScr(uint16_t _usColor)
{
	LCD429_FillRect(0, 0, g_LcdHeight, g_LcdWidth, _usColor);
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_PutPixel
*	����˵��: ��1������
*	��    ��:
*			_usX,_usY : ��������
*			_usColor  : ������ɫ ( RGB = 565 ��ʽ)
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_PutPixel(uint16_t _usX, uint16_t _usY, uint16_t _usColor)
{
	uint16_t *p;
	uint32_t index = 0;
	
	p = (uint16_t *)s_CurrentFrameBuffer;
		
	if (g_LcdDirection == 0)		/* ���� */
	{
		index = (uint32_t)_usY * g_LcdWidth + _usX;
	}
	else if (g_LcdDirection == 1)	/* ����180��*/
	{
		index = (uint32_t)(g_LcdHeight - _usY - 1) * g_LcdWidth + (g_LcdWidth - _usX - 1);
	}
	else if (g_LcdDirection == 2)	/* ���� */
	{
		index = (uint32_t)(g_LcdWidth - _usX - 1) * g_LcdHeight + _usY;
	}
	else if (g_LcdDirection == 3)	/* ����180�� */
	{
		index = (uint32_t)_usX * g_LcdHeight + g_LcdHeight - _usY - 1;
	}	
	
	if (index < g_LcdHeight * g_LcdWidth)
	{
		p[index] = _usColor;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_GetPixel
*	����˵��: ��ȡ1������
*	��    ��:
*			_usX,_usY : ��������
*			_usColor  : ������ɫ
*	�� �� ֵ: RGB��ɫֵ
*********************************************************************************************************
*/
uint16_t LCD429_GetPixel(uint16_t _usX, uint16_t _usY)
{
	uint16_t usRGB;
	uint16_t *p;
	uint32_t index = 0;
	
	p = (uint16_t *)s_CurrentFrameBuffer;

	if (g_LcdDirection == 0)		/* ���� */
	{
		index = (uint32_t)_usY * g_LcdWidth + _usX;
	}
	else if (g_LcdDirection == 1)	/* ����180��*/
	{
		index = (uint32_t)(g_LcdHeight - _usY - 1) * g_LcdWidth + (g_LcdWidth - _usX - 1);
	}
	else if (g_LcdDirection == 2)	/* ���� */
	{
		index = (uint32_t)(g_LcdWidth - _usX - 1) * g_LcdHeight + _usY;
	}
	else if (g_LcdDirection == 3)	/* ����180�� */
	{
		index = (uint32_t)_usX * g_LcdHeight + g_LcdHeight - _usY - 1;
	}
	
	usRGB = p[index];

	return usRGB;
}


/*
*********************************************************************************************************
*	�� �� ��: LCD429_DrawLine
*	����˵��: ���� Bresenham �㷨����2��仭һ��ֱ�ߡ�
*	��    ��:
*			_usX1, _usY1 : ��ʼ������
*			_usX2, _usY2 : ��ֹ��Y����
*			_usColor     : ��ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_DrawLine(uint16_t _usX1 , uint16_t _usY1 , uint16_t _usX2 , uint16_t _usY2 , uint16_t _usColor)
{
	int32_t dx , dy ;
	int32_t tx , ty ;
	int32_t inc1 , inc2 ;
	int32_t d , iTag ;
	int32_t x , y ;

	/* ���� Bresenham �㷨����2��仭һ��ֱ�� */

	LCD429_PutPixel(_usX1 , _usY1 , _usColor);

	/* ��������غϣ���������Ķ�����*/
	if ( _usX1 == _usX2 && _usY1 == _usY2 )
	{
		return;
	}

	iTag = 0 ;
	/* dx = abs ( _usX2 - _usX1 ); */
	if (_usX2 >= _usX1)
	{
		dx = _usX2 - _usX1;
	}
	else
	{
		dx = _usX1 - _usX2;
	}

	/* dy = abs ( _usY2 - _usY1 ); */
	if (_usY2 >= _usY1)
	{
		dy = _usY2 - _usY1;
	}
	else
	{
		dy = _usY1 - _usY2;
	}

	if ( dx < dy )   /*���dyΪ�Ƴ������򽻻��ݺ����ꡣ*/
	{
		uint16_t temp;

		iTag = 1 ;
		temp = _usX1; _usX1 = _usY1; _usY1 = temp;
		temp = _usX2; _usX2 = _usY2; _usY2 = temp;
		temp = dx; dx = dy; dy = temp;
	}
	tx = _usX2 > _usX1 ? 1 : -1 ;    /* ȷ������1���Ǽ�1 */
	ty = _usY2 > _usY1 ? 1 : -1 ;
	x = _usX1 ;
	y = _usY1 ;
	inc1 = 2 * dy ;
	inc2 = 2 * ( dy - dx );
	d = inc1 - dx ;
	while ( x != _usX2 )     /* ѭ������ */
	{
		if ( d < 0 )
		{
			d += inc1 ;
		}
		else
		{
			y += ty ;
			d += inc2 ;
		}
		if ( iTag )
		{
			LCD429_PutPixel ( y , x , _usColor) ;
		}
		else
		{
			LCD429_PutPixel ( x , y , _usColor) ;
		}
		x += tx ;
	}	
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_DrawHLine
*	����˵��: ����һ��ˮƽ��. ʹ��STM32F429 DMA2DӲ������.
*	��    ��:
*			_usX1, _usY1 : ��ʼ������
*			_usLen       : �ߵĳ���
*			_usColor     : ��ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_DrawHLine(uint16_t _usX, uint16_t _usY, uint16_t _usLen , uint16_t _usColor)
{
#if 0
	LCD429_FillRect(_usX, _usY, 1, _usLen, _usColor);
#else	
	uint16_t i;
	
	for (i = 0; i < _usLen; i++)
	{	
		LCD429_PutPixel(_usX + i , _usY , _usColor);
	}
#endif	
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_DrawVLine
*	����˵��: ����һ����ֱ�ߡ� ʹ��STM32F429 DMA2DӲ������.
*	��    ��:
*			_usX, _usY : ��ʼ������
*			_usLen       : �ߵĳ���
*			_usColor     : ��ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_DrawVLine(uint16_t _usX , uint16_t _usY , uint16_t _usLen , uint16_t _usColor)
{
#if 0
	LCD429_FillRect(_usX, _usY, _usLen, 1, _usColor);
#else	
	uint16_t i;
	
	for (i = 0; i < _usLen; i++)
	{	
		LCD429_PutPixel(_usX, _usY + i, _usColor);
	}
#endif	
}
/*
*********************************************************************************************************
*	�� �� ��: LCD429_DrawPoints
*	����˵��: ���� Bresenham �㷨������һ��㣬������Щ�����������������ڲ�����ʾ��
*	��    ��:
*			x, y     : ��������
*			_usColor : ��ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_DrawPoints(uint16_t *x, uint16_t *y, uint16_t _usSize, uint16_t _usColor)
{
	uint16_t i;

	for (i = 0 ; i < _usSize - 1; i++)
	{
		LCD429_DrawLine(x[i], y[i], x[i + 1], y[i + 1], _usColor);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_DrawRect
*	����˵��: ����ˮƽ���õľ��Ρ�
*	��    ��:
*			_usX,_usY: �������Ͻǵ�����
*			_usHeight : ���εĸ߶�
*			_usWidth  : ���εĿ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_DrawRect(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor)
{
	/*
	 ---------------->---
	|(_usX��_usY)        |
	V                    V  _usHeight
	|                    |
	 ---------------->---
		  _usWidth
	*/
	LCD429_DrawHLine(_usX, _usY, _usWidth, _usColor);
	LCD429_DrawVLine(_usX +_usWidth - 1, _usY, _usHeight, _usColor);
	LCD429_DrawHLine(_usX, _usY + _usHeight - 1, _usWidth, _usColor);
	LCD429_DrawVLine(_usX, _usY, _usHeight, _usColor);
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_FillRect
*	����˵��: ��һ����ɫֵ���һ�����Ρ�ʹ��STM32F429�ڲ�DMA2DӲ�����ơ�
*	��    ��:
*			_usX,_usY: �������Ͻǵ�����
*			_usHeight : ���εĸ߶�
*			_usWidth  : ���εĿ���
*			_usColor  : ��ɫ����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_FillRect(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor)
{
	/* ʹ��DMA2DӲ�������� */
	uint32_t  Xaddress = 0;
	uint16_t  OutputOffset;
	uint16_t  NumberOfLine;
	uint16_t  PixelPerLine;	
		
	if (g_LcdDirection == 0)		/* ���� */
	{
		Xaddress = s_CurrentFrameBuffer + 2 * (g_LcdWidth * _usY + _usX);	
		OutputOffset = g_LcdWidth - _usWidth;
		NumberOfLine = _usHeight;
		PixelPerLine = _usWidth;
	}
	else if (g_LcdDirection == 1)	/* ����180��*/
	{
		Xaddress = s_CurrentFrameBuffer + 2 * (g_LcdWidth * (g_LcdHeight - _usHeight - _usY) + g_LcdWidth - _usX - _usWidth);	
		OutputOffset = g_LcdWidth - _usWidth;
		NumberOfLine = _usHeight;
		PixelPerLine = _usWidth;
	}
	else if (g_LcdDirection == 2)	/* ���� */
	{
		Xaddress = s_CurrentFrameBuffer + 2 * (g_LcdHeight * (g_LcdWidth - _usX -  _usWidth) + _usY);	
		OutputOffset = g_LcdHeight - _usHeight;
		NumberOfLine = _usWidth;
		PixelPerLine = _usHeight;
	}
	else if (g_LcdDirection == 3)	/* ����180�� */
	{
		Xaddress = s_CurrentFrameBuffer + 2 * (g_LcdHeight * _usX + g_LcdHeight - _usHeight - _usY);	
		OutputOffset = g_LcdHeight - _usHeight;
		NumberOfLine = _usWidth;
		PixelPerLine = _usHeight;		
	}	

	//DMA2D_FillBuffer(uint32_t LayerIndex, void * pDst, uint32_t xSize, uint32_t ySize, uint32_t OffLine, uint32_t ColorIndex) 
	DMA2D_FillBuffer(s_CurrentLayer, (void *)Xaddress, PixelPerLine, NumberOfLine, OutputOffset, _usColor);
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_DrawCircle
*	����˵��: ����һ��Բ���ʿ�Ϊ1������
*	��    ��:
*			_usX,_usY  : Բ�ĵ�����
*			_usRadius  : Բ�İ뾶
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_DrawCircle(uint16_t _usX, uint16_t _usY, uint16_t _usRadius, uint16_t _usColor)
{
	int32_t  D;			/* Decision Variable */
	uint32_t  CurX;		/* ��ǰ X ֵ */
	uint32_t  CurY;		/* ��ǰ Y ֵ */

	D = 3 - (_usRadius << 1);
	CurX = 0;
	CurY = _usRadius;

	while (CurX <= CurY)
	{
		LCD429_PutPixel(_usX + CurX, _usY + CurY, _usColor);
		LCD429_PutPixel(_usX + CurX, _usY - CurY, _usColor);
		LCD429_PutPixel(_usX - CurX, _usY + CurY, _usColor);
		LCD429_PutPixel(_usX - CurX, _usY - CurY, _usColor);
		LCD429_PutPixel(_usX + CurY, _usY + CurX, _usColor);
		LCD429_PutPixel(_usX + CurY, _usY - CurX, _usColor);
		LCD429_PutPixel(_usX - CurY, _usY + CurX, _usColor);
		LCD429_PutPixel(_usX - CurY, _usY - CurX, _usColor);

		if (D < 0)
		{
			D += (CurX << 2) + 6;
		}
		else
		{
			D += ((CurX - CurY) << 2) + 10;
			CurY--;
		}
		CurX++;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_DrawBMP
*	����˵��: ��LCD����ʾһ��BMPλͼ��λͼ����ɨ����򣺴����ң����ϵ���
*	��    ��:  
*			_usX, _usY : ͼƬ������
*			_usHeight  ��ͼƬ�߶�
*			_usWidth   ��ͼƬ����
*			_ptr       ��ͼƬ����ָ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_DrawBMP(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t *_ptr)
{
	uint16_t i, k, y;
	const uint16_t *p;

	p = _ptr;
	y = _usY;
	for (i = 0; i < _usHeight; i++)
	{
		for (k = 0; k < _usWidth; k++)
		{
			LCD429_PutPixel(_usX + k, y, *p++);
		}
		
		y++;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_SetDirection
*	����˵��: ������ʾ����ʾ���򣨺��� ������
*	��    ��: ��ʾ������� 0 ��������, 1=����180�ȷ�ת, 2=����, 3=����180�ȷ�ת
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_SetDirection(uint8_t _dir)
{
	uint16_t temp;
	
	if (_dir == 0 || _dir == 1)		/* ������ ����180�� */
	{
		if (g_LcdWidth < g_LcdHeight)
		{
			temp = g_LcdWidth;
			g_LcdWidth = g_LcdHeight;
			g_LcdHeight = temp;			
		}
	}
	else if (_dir == 2 || _dir == 3)	/* ����, ����180��*/
	{
		if (g_LcdWidth > g_LcdHeight)
		{
			temp = g_LcdWidth;
			g_LcdWidth = g_LcdHeight;
			g_LcdHeight = temp;			
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_BeginDraw
*	����˵��: ˫����������ģʽ����ʼ��ͼ������ǰ��ʾ�������������������Ƶ�����һ����������
*			 ����� LCD429_EndDraw�����ɶ�ʹ�á� ʵ��Ч�������á�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_BeginDraw(void)
{
//	uint16_t *src;
//	uint16_t *dst;
//		
//	if (s_CurrentFrameBuffer == LCD429_FRAME_BUFFER)
//	{
//		src = (uint16_t *)LCD429_FRAME_BUFFER;
//		dst =  (uint16_t *)(LCD429_FRAME_BUFFER + BUFFER_OFFSET);
//		
//		s_CurrentFrameBuffer = LCD429_FRAME_BUFFER + BUFFER_OFFSET;
//	}
//	else
//	{
//		src = (uint16_t *)(LCD429_FRAME_BUFFER + BUFFER_OFFSET);
//		dst = (uint16_t *)LCD429_FRAME_BUFFER;
//		
//		s_CurrentFrameBuffer = LCD429_FRAME_BUFFER;
//	}
//	
//	_DMA_Copy(src, dst, g_LcdHeight, g_LcdWidth, 0, 0);
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_EndDraw
*	����˵��: APP�����˻�������ͼ�������л�Ӳ����ʾ��
*			 ����� LCD429_BeginDraw�����ɶ�ʹ�á�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void LCD429_EndDraw(void)
{
	//LTDC_LayerAddress(LTDC_Layer1, s_CurrentFrameBuffer);
}

/*
*********************************************************************************************************
*	�� �� ��: _GetBufferSize
*	����˵��: ��
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
uint32_t _GetBufferSize(uint8_t LayerIndex)
{
	return g_LcdWidth * g_LcdHeight;
}

/*
*********************************************************************************************************
*	�� �� ��: LCD429_InitDMA2D
*	����˵��: ����DMA2D
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void LCD429_InitDMA2D(void)
{
	/* Enable peripheral */
	__HAL_RCC_DMA2D_CLK_ENABLE();   
	
	/* Configure the DMA2D default mode */ 
	hdma2d.Init.Mode         = DMA2D_R2M;
	hdma2d.Init.ColorMode    = DMA2D_INPUT_RGB565;
	hdma2d.Init.OutputOffset = 0x0;     

	hdma2d.Instance          = DMA2D; 

	if (HAL_DMA2D_Init(&hdma2d) != HAL_OK)
	{
		Error_Handler(__FILE__, __LINE__);
	}
 }

 /**
  * @brief  Return Pixel format for a given layer
  * @param  LayerIndex : Layer Index 
  * @retval Status ( 0 : 0k , 1: error)
  */
static inline uint32_t LCD_LL_GetPixelformat(uint32_t LayerIndex)
{
	if (LayerIndex == 0)
	{
		return LTDC_PIXEL_FORMAT_RGB565;
	} 
	else
	{
		return LTDC_PIXEL_FORMAT_RGB565;  // LTDC_PIXEL_FORMAT_ARGB1555;
	} 
}

/**
  * @brief  Return Pixel format for a given layer
  * @param  LayerIndex : Layer Index 
  * @retval Status ( 0 : 0k , 1: error)
  */
void DMA2D_CopyBuffer(uint32_t LayerIndex, void * pSrc, void * pDst, uint32_t xSize, uint32_t ySize, uint32_t OffLineSrc, uint32_t OffLineDst)
{
	uint32_t PixelFormat;

	PixelFormat = LCD_LL_GetPixelformat(LayerIndex);
	DMA2D->CR      = 0x00000000UL | (1 << 9);  

	/* Set up pointers */
	DMA2D->FGMAR   = (uint32_t)pSrc;                       
	DMA2D->OMAR    = (uint32_t)pDst;                       
	DMA2D->FGOR    = OffLineSrc;                      
	DMA2D->OOR     = OffLineDst; 

	/* Set up pixel format */  
	DMA2D->FGPFCCR = PixelFormat;  

	/*  Set up size */
	DMA2D->NLR     = (uint32_t)(xSize << 16) | (uint16_t)ySize; 

	DMA2D->CR     |= DMA2D_CR_START;   

	/* Wait until transfer is done */
	while (DMA2D->CR & DMA2D_CR_START) 
	{
	}
}

/**
  * @brief  Fill Buffer
  * @param  LayerIndex : Layer Index
  * @param  pDst:        pointer to destination
  * @param  xSize:       X size
  * @param  ySize:       Y size
  * @param  OffLine:     offset after each line
  * @param  ColorIndex:  color to be used.           
  * @retval None.
  */
static void DMA2D_FillBuffer(uint32_t LayerIndex, void * pDst, uint32_t xSize, uint32_t ySize, uint32_t OffLine, uint32_t ColorIndex) 
{
	uint32_t PixelFormat;

	PixelFormat = LCD_LL_GetPixelformat(LayerIndex);

	/* Set up mode  Register to memory and TCIE */
	DMA2D->CR      = 0x00030000UL | (1 << 9);        
	DMA2D->OCOLR   = ColorIndex;                     

	/* Set up pointers   Destination address */
	DMA2D->OMAR    = (uint32_t)pDst;                      

	/* Set up offsets  Destination line offset */
	DMA2D->OOR     = OffLine;                        

	/* Set up pixel format */
	DMA2D->OPFCCR  = PixelFormat;                    

	/*  Set up size */
	DMA2D->NLR     = (uint32_t)(xSize << 16) | (uint16_t)ySize;

	DMA2D->CR     |= DMA2D_CR_START; 

	/* Wait until transfer is done */
	while (DMA2D->CR & DMA2D_CR_START) 
	{
	}
}

/*
*********************************************************************************************************
*   函 数 名: LTDC_IRQHandler
*   功能说明: LTDC_IRQHandler 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void LTDC_IRQHandler(void)
{
  HAL_LTDC_IRQHandler(&hltdc_F);
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
