/**
 * @file bsp.c
 * @brief 板级支持包入口：MPU、I/D Cache、400MHz 时钟、HAL_Init、基础外设初始化。
 */

/*
*********************************************************************************************************
*
*	ģ������ : BSPģ��(For STM32H7)
*	�ļ����� : bsp.c
*	��    �� : V1.0
*	˵    �� : ����Ӳ���ײ�������������ļ���ÿ��c�ļ����� #include "bsp.h" ���������е���������ģ�顣
*			   bsp = Borad surport packet �弶֧�ְ�
*	�޸ļ�¼ :
*		�汾��  ����         ����       ˵��
*		V1.0    2018-07-29  Eric2013   ��ʽ����
*
*	Copyright (C), 2018-2030, ���������� www.armfly.com
*
*********************************************************************************************************
*/
#include "bsp.h"
#include "y_t_stress_ab.h"



/*
*********************************************************************************************************
*	                                   ��������
*********************************************************************************************************
*/
static void SystemClock_Config(void);
/*
*********************************************************************************************************
*   函 数 名: CPU_CACHE_Enable
*   功能说明: CPU_CACHE_Enable 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void CPU_CACHE_Enable(void);
/*
*********************************************************************************************************
*   函 数 名: MPU_Config
*   功能说明: MPU_Config 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void MPU_Config(void);


/*
*********************************************************************************************************
*	                                   ��������
*********************************************************************************************************
*/
static void SystemClock_Config(void);
/*
*********************************************************************************************************
*   函 数 名: CPU_CACHE_Enable
*   功能说明: CPU_CACHE_Enable 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void CPU_CACHE_Enable(void);
/*
*********************************************************************************************************
*   函 数 名: MPU_Config
*   功能说明: MPU_Config 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void MPU_Config(void);

/*
*********************************************************************************************************
*	�� �� ��: System_Init
*	����˵��: ϵͳ��ʼ������Ҫ��MPU��Cache��ϵͳʱ������
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void System_Init(void)
{
	
	/* ����MPU */
	MPU_Config();
	
	/* ʹ��L1 Cache */
	CPU_CACHE_Enable();

	/* 
       STM32H7xx HAL ���ʼ������ʱϵͳ�õĻ���H7�Դ���64MHz��HSIʱ��:
	   - ���ú���HAL_InitTick����ʼ���δ�ʱ���ж�1ms��
	   - ����NVIV���ȼ�����Ϊ4��
	 */
	HAL_Init();

	/* 
       ����ϵͳʱ�ӵ�400MHz
       - �л�ʹ��HSE��
       - �˺��������ȫ�ֱ���SystemCoreClock������������HAL_InitTick��
    */
	SystemClock_Config();

	/* 
	   Event Recorder��
	   - �����ڴ���ִ��ʱ�������MDK5.25�������ϰ汾��֧�֣�IAR��֧�֡�
	   - Ĭ�ϲ���������Ҫʹ�ܴ�ѡ���ؿ�V7�������û��ֲ��8��
	*/	
#if Enable_EventRecorder == 1  
	/* ��ʼ��EventRecorder������ */
	EventRecorderInitialize(EventRecordAll, 1U);
	EventRecorderStart();
#endif

	Bsp_DisableUnalignedFaultTrap();
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_Init
*	����˵��: ��ʼ�����е�Ӳ���豸���ú�������CPU�Ĵ���������ļĴ�������ʼ��һЩȫ�ֱ�����ֻ��Ҫ����һ��
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void Bsp_DisableUnalignedFaultTrap(void)
{
	SCB->CCR &= ~(uint32_t)SCB_CCR_UNALIGN_TRP_Msk;
	__DSB();
	__ISB();
}

/*
*********************************************************************************************************
*   函 数 名: bsp_Init
*   功能说明: bsp_Init 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void bsp_Init(void)
{
	bsp_InitDWT();      /* ��ʼ��CM7�ں˵�ʱ�����ڼ����� */
	/* LCD/SDRAMʵ����ʹ��LTDC��չIO�����ñ��걾��ť��ʼ������IO��ͻ */
	bsp_InitUart();		/* ��ʼ������ */
	/* LCD/SDRAMʵ�����FMC��չIO��ʼ����������SDRAM���ų�ͻ */
	bsp_InitLed();    	/* ��ʼ��LED */	
}

/*
*********************************************************************************************************
*	�� �� ��: SystemClock_Config
*	����˵��: ��ʼ��ϵͳʱ��
*            	System Clock source            = PLL (HSE)
*            	SYSCLK(Hz)                     = 400000000 (CPU Clock)
*           	HCLK(Hz)                       = 200000000 (AXI and AHBs Clock)
*            	AHB Prescaler                  = 2
*            	D1 APB3 Prescaler              = 2 (APB3 Clock  100MHz)
*            	D2 APB1 Prescaler              = 2 (APB1 Clock  100MHz)
*            	D2 APB2 Prescaler              = 2 (APB2 Clock  100MHz)
*            	D3 APB4 Prescaler              = 2 (APB4 Clock  100MHz)
*            	HSE Frequency(Hz)              = 25000000
*           	PLL_M                          = 5
*            	PLL_N                          = 160
*            	PLL_P                          = 2
*            	PLL_Q                          = 4
*            	PLL_R                          = 2
*            	VDD(V)                         = 3.3
*            	Flash Latency(WS)              = 4
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void SystemClock_Config(void)
{
	RCC_ClkInitTypeDef RCC_ClkInitStruct;
	RCC_OscInitTypeDef RCC_OscInitStruct;
	HAL_StatusTypeDef ret = HAL_OK;

	/* ��סSCU(Supply configuration update) */
	MODIFY_REG(PWR->CR3, PWR_CR3_SCUEN, 0);

	/* 
      1��оƬ�ڲ���LDO��ѹ������ĵ�ѹ��Χ����ѡVOS1��VOS2��VOS3����ͬ��Χ��Ӧ��ͬ��Flash���ٶȣ�
         ���鿴�ο��ֲ��Table 12�ı��
      2������ѡ��ʹ��VOS1����ѹ��Χ1.15V - 1.26V��
    */
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

	/* ʹ��HSE����ѡ��HSE��ΪPLLʱ��Դ */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
	RCC_OscInitStruct.HSIState = RCC_HSI_OFF;
	RCC_OscInitStruct.CSIState = RCC_CSI_OFF;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
		
	RCC_OscInitStruct.PLL.PLLM = 5;
	RCC_OscInitStruct.PLL.PLLN = 160;
	RCC_OscInitStruct.PLL.PLLP = 2;
	RCC_OscInitStruct.PLL.PLLR = 2;
	RCC_OscInitStruct.PLL.PLLQ = 4;		
		
	RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
	RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;	
	ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
	if(ret != HAL_OK)
	{
        Error_Handler(__FILE__, __LINE__);
	}

	/* 
       ѡ��PLL�������Ϊϵͳʱ��
       ����RCC_CLOCKTYPE_SYSCLKϵͳʱ��
       ����RCC_CLOCKTYPE_HCLK ʱ�ӣ���ӦAHB1��AHB2��AHB3��AHB4����
       ����RCC_CLOCKTYPE_PCLK1ʱ�ӣ���ӦAPB1����
       ����RCC_CLOCKTYPE_PCLK2ʱ�ӣ���ӦAPB2����
       ����RCC_CLOCKTYPE_D1PCLK1ʱ�ӣ���ӦAPB3����
       ����RCC_CLOCKTYPE_D3PCLK1ʱ�ӣ���ӦAPB4����     
    */
	RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 | \
								 RCC_CLOCKTYPE_PCLK2  | RCC_CLOCKTYPE_D3PCLK1);

	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;  
	RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2; 
	RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2; 
	RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2; 
	
	/* �˺��������SystemCoreClock������������HAL_InitTick */
	ret = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);
	if(ret != HAL_OK)
	{
        Error_Handler(__FILE__, __LINE__);
	}

    /*
      ʹ��IO�ĸ���ģʽ��Ҫʹ��IO������������������������ 
      ��1��ʹ��CSI clock
      ��2��ʹ��SYSCFG clock
      ��3��ʹ��I/O������Ԫ�� ����SYSCFG_CCCSR�Ĵ�����bit0
    */
	__HAL_RCC_CSI_ENABLE() ;

	__HAL_RCC_SYSCFG_CLK_ENABLE() ;

	HAL_EnableCompensationCell();

   /* AXI SRAM��ʱ�����ϵ��Զ�ʹ�ܵģ���D2���SRAM1��SRAM2��SRAM3Ҫ����ʹ�� */	
#if 0
	__HAL_RCC_D2SRAM1_CLK_ENABLE();
	__HAL_RCC_D2SRAM2_CLK_ENABLE();
	__HAL_RCC_D2SRAM3_CLK_ENABLE();
#endif
}

/*
*********************************************************************************************************
*	�� �� ��: Error_Handler
*	��    ��: file : Դ�����ļ����ơ��ؼ��� __FILE__ ��ʾԴ�����ļ�����
*			  line �������кš��ؼ��� __LINE__ ��ʾԴ�����к�
*	�� �� ֵ: ��
*		Error_Handler(__FILE__, __LINE__);
*********************************************************************************************************
*/
void Error_Handler(char *file, uint32_t line)
{
	if (line == 0)
	{
		return;
	}

	printf("\r\n[ERR] Error_Handler file=%s line=%lu\r\n",
	       (file != 0) ? file : "?",
	       (unsigned long)line);

	while(1)
	{
	}
}

/*
*********************************************************************************************************
*	�� �� ��: MPU_Config
*	����˵��: ����MPU
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void MPU_Config( void )
{
	MPU_Region_InitTypeDef MPU_InitStruct;

	/* ��ֹ MPU */
	HAL_MPU_Disable();

	/* ����AXI SRAM��MPU����ΪWrite back, Read allocate��Write allocate */
	MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
	MPU_InitStruct.BaseAddress      = 0x24000000;
	MPU_InitStruct.Size             = MPU_REGION_SIZE_512KB;
	MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.IsBufferable     = MPU_ACCESS_BUFFERABLE;
	MPU_InitStruct.IsCacheable      = MPU_ACCESS_CACHEABLE;
	MPU_InitStruct.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
	MPU_InitStruct.Number           = MPU_REGION_NUMBER0;
	MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL1;
	MPU_InitStruct.SubRegionDisable = 0x00;
	MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;

	HAL_MPU_ConfigRegion(&MPU_InitStruct);
	
	
	/* ����FMC��չIO��MPU����ΪDevice����Strongly Ordered */
	MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
	MPU_InitStruct.BaseAddress      = 0x60000000;
	MPU_InitStruct.Size             = ARM_MPU_REGION_SIZE_64KB;	
	MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.IsBufferable     = MPU_ACCESS_BUFFERABLE;
	MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;	/* ������MPU_ACCESS_CACHEABLE�������2��CS��WE�ź� */
	MPU_InitStruct.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
	MPU_InitStruct.Number           = MPU_REGION_NUMBER1;
	MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL0;
	MPU_InitStruct.SubRegionDisable = 0x00;
	MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;
	
	HAL_MPU_ConfigRegion(&MPU_InitStruct);

	/* Region 2: 全体 SDRAM (0xC0000000, 32MB) — Write-Through, No Write-Allocate
	 * TEX=0, C=1, B=0
	 * 覆盖所有 SDRAM，包含物理缓冲区（LTDC 直接读 SDRAM）和其他用途。
	 * 物理缓冲区 (0xC0100000 / 0xC0200000) 使用此属性：
	 *   CPU 旋转写直达 SDRAM → LTDC 无需任何 CleanDCache 即可读到最新数据 */
	MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
	MPU_InitStruct.BaseAddress      = 0xC0000000;
	MPU_InitStruct.Size             = MPU_REGION_SIZE_32MB;
	MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;  /* B=0 */
	MPU_InitStruct.IsCacheable      = MPU_ACCESS_CACHEABLE;       /* C=1 */
	MPU_InitStruct.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
	MPU_InitStruct.Number           = MPU_REGION_NUMBER2;
	MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL0;             /* TEX=0, C=1, B=0 = WT */
	MPU_InitStruct.SubRegionDisable = 0x00;
	MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;
	HAL_MPU_ConfigRegion(&MPU_InitStruct);

#if (Y_T_AB_NONCACHEABLE_DISPLAY_VRAM != 0)
	/* Region 3: display VRAM diagnostic override, 0xC0000000..0xC03FFFFF.
	 * Higher MPU region number overrides the 32MB SDRAM WT region above.
	 * Covers logical VRAM plus both physical LTDC framebuffers. */
	MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
	MPU_InitStruct.BaseAddress      = 0xC0000000;
	MPU_InitStruct.Size             = MPU_REGION_SIZE_4MB;
	MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
	MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
	MPU_InitStruct.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
	MPU_InitStruct.Number           = MPU_REGION_NUMBER3;
	MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL1;
	MPU_InitStruct.SubRegionDisable = 0x00;
	MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;
	HAL_MPU_ConfigRegion(&MPU_InitStruct);
#endif

	/*
	 * PlotBuf / 波形缓冲在 emWin GUI_ALLOC 中动态分配，基址随构建与堆变化，启动时无法在 MPU_Config 写死「夹心」区。
	 * 若将来把波形缓冲改为链接器固定段，可在此增加更高区号（如 Region4）压过 Region0 的一小段：中间 RW、两侧 NO_ACCESS。
	 * 勿用整段 0x2407xxxx 对 emWin BSS 全只读：会与 WM 正常写全局冲突。
	 */

	/* 默认路径：逻辑缓冲与物理缓冲使用 Region 2 的 Write-Through。
	 * 当前排查路径可通过 Region 3 将显示 VRAM 临时覆盖为 Non-Cacheable，
	 * 用于验证 WT SDRAM 可见性是否导致偶发屏闪。 */

	/*ʹ�� MPU */
	HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/*
*********************************************************************************************************
*	�� �� ��: CPU_CACHE_Enable
*	����˵��: ʹ��L1 Cache
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void CPU_CACHE_Enable(void)
{
	/* ʹ�� I-Cache */
	SCB_EnableICache();

	/* ʹ�� D-Cache（Y_T_AB_DISABLE_DCACHE=1 时跳过，仅作 A/B 定位） */
#if (Y_T_AB_DISABLE_DCACHE == 0)
	SCB_EnableDCache();
#endif

	Bsp_DisableUnalignedFaultTrap();
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_RunPer10ms
*	����˵��: �ú���ÿ��10ms��Systick�жϵ���1�Ρ���� bsp_timer.c�Ķ�ʱ�жϷ������һЩ����ʱ��Ҫ���ϸ��
*			������Է��ڴ˺��������磺����ɨ�衢���������п��Ƶȡ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_RunPer10ms(void)
{
	/* LCDʵ����δʹ�ð�����ѯ�� */
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_RunPer1ms
*	����˵��: �ú���ÿ��1ms��Systick�жϵ���1�Ρ���� bsp_timer.c�Ķ�ʱ�жϷ������һЩ��Ҫ�����Դ��������
*			 ���Է��ڴ˺��������磺��������ɨ�衣
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_RunPer1ms(void)
{
	
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_Idle
*	����˵��: ����ʱִ�еĺ�����һ����������for��whileѭ������������Ҫ���� CPU_IDLE() �������ñ�������
*			 ������ȱʡΪ�ղ������û��������ι��������CPU��������ģʽ�Ĺ��ܡ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_Idle(void)
{
	/* --- ι�� */

	/* --- ��CPU�������ߣ���Systick��ʱ�жϻ��ѻ��������жϻ��� */

	/* ���� emWin ͼ�ο⣬���Բ���ͼ�ο���Ҫ����ѯ���� */
	//GUI_Exec();

	/* ���� uIP Э�飬���Բ���uip��ѯ���� */
	//TOUCH_CapScan();
}

/*
*********************************************************************************************************
*	�� �� ��: HAL_Delay
*	����˵��: �ض�������ӳٺ������滻HAL�еĺ�������ΪHAL�е�ȱʡ����������Systick�жϣ������USB��SD��
*             �ж������ӳٺ��������������Ҳ����ͨ������HAL_NVIC_SetPriority����Systick�ж�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
/* ��ǰ����ʹ��stm32h7xx_hal.cĬ�Ϸ�ʽʵ�֣�δʹ�������ض���ĺ��� */
#if 0
/*
*********************************************************************************************************
*   函 数 名: HAL_Delay
*   功能说明: HAL_Delay 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void HAL_Delay(uint32_t Delay)
{
	bsp_DelayUS(Delay * 1000);
}
#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/

