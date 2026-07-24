/**
  ******************************************************************************
  * @file    Templates/Src/stm32h7xx_it.c 
  * @author  MCD Application Team
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and 
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2017 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_it.h"
#include "stm32h7xx_hal.h"
#include "bsp.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

volatile uint32_t g_fault_cfsr_full;
volatile uint32_t g_fault_mmfsr;
volatile uint32_t g_fault_mmfar;
volatile uint32_t g_fault_bfsr;
volatile uint32_t g_fault_ufsr;
volatile uint32_t g_fault_mm_iaccviol;
volatile uint32_t g_fault_mm_daccviol;
volatile uint32_t g_fault_mm_mstkerr;
volatile uint32_t g_fault_mm_munstkerr;
volatile uint32_t g_fault_mm_mmarvalid;
volatile uint32_t g_fault_exc_return;
volatile uint32_t g_fault_ctx_offset;
volatile uint32_t g_fault_psp_at_entry;
volatile uint32_t g_fault_stacked_lr;
volatile uint32_t g_fault_stacked_pc;
volatile uint32_t g_fault_stacked_xpsr;
/* 对照：若 g_fault_ctx_offset=0 但下列 FPU 偏移处 PC 落在 Flash，则异常帧解析偏移有误 */
volatile uint32_t g_fault_stacked_lr_fpu;
volatile uint32_t g_fault_stacked_pc_fpu;
volatile uint32_t g_fault_stacked_xpsr_fpu;
volatile uint32_t g_fault_raw_ctx[8];
volatile char     g_fault_task_name[12];

extern ETH_HandleTypeDef heth;

/*
 * 排障备忘（需与当前构建的 Listings/output.map 对照，重编后地址会变）：
 * - 典型堆叠 LR 0x08104C73 曾落在 STemWin GUIDRV_Lin_16 _FillRect (约 0x08104C55) 内。
 * - FreeRTOS heap（ucHeap）在 map 中约 0x24053D28 起 0x18000 字节；PSP 落在该段内与 xTaskCreate 自 heap_4 分配 TCB+栈 一致。
 * - 片内 MPU/Cache：User/bsp/bsp.c 中 MPU_Config()。
 * Keil 写断点（阶段 3）：Debug -> Breakpoints -> New -> Access Breakpoint，选 Write，地址填
 *   GUI 设备表根等（在 output.map 搜 GUI__apDevice / GUI_pContext，重编后地址会变）；若每帧合法写太多，先缩小范围或先做
 *   User/bsp/y_t_stress_ab.h 中 D-Cache / DMA2D / GUI_LOCK 单项 A/B。
 */
/* 进入异常时硬件把 EXC_RETURN 放在 lr。普通 C 函数序言会改 lr，故必须在无序言的 __asm 入口里用 MOV r0,lr 再转 C。 */
/* PSP 上整数异常帧起点相对当前 PSP 的偏移：无 FPU 为 0；有扩展 FPU 帧时为 0x48（S0–S15、FPSCR、对齐） */
static uint32_t memmanage_intctx_offset(uint32_t exc_return)
{
  /* 非法/未捕获的 exc（如 0）不得套 FPU 判据，否则会误判为 0x48 导致 LR/PC 全错 */
  if ((exc_return & 0xFF000000UL) != 0xFF000000UL) {
    return 0U;
  }
  if ((exc_return & 0x10UL) == 0UL && (exc_return & 0xFUL) != 0xFUL) {
    return 0x48U;
  }
  return 0U;
}

/** @addtogroup STM32H7xx_HAL_Examples
  * @{
  */

/** @addtogroup Templates
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
void MemManage_Handler_Continue(uint32_t exc_return);
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M7 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief   This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
__asm void MemManage_Handler(void)
{
  PRESERVE8
  IMPORT MemManage_Handler_Continue
  MOV r0, lr
  BL MemManage_Handler_Continue
}

/*
*********************************************************************************************************
*   函 数 名: MemManage_Handler_Continue
*   功能说明: MemManage_Handler_Continue 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void MemManage_Handler_Continue(uint32_t exc_return)
{
  uint32_t psp = __get_PSP();
  uint32_t off = memmanage_intctx_offset(exc_return);
  volatile uint32_t *ctx = (volatile uint32_t *)(psp + off);

  g_fault_exc_return = exc_return;
  g_fault_ctx_offset = off;
  g_fault_psp_at_entry = psp;
  /* 标准 8 字：R0..R12, LR, PC, xPSR；LR @ +0x14，PC @ +0x18（off 为 FPU 扩展前缀长度） */
  g_fault_stacked_lr   = ctx[5];
  g_fault_stacked_pc   = ctx[6];
  g_fault_stacked_xpsr = ctx[7];
  {
    uint32_t i;
    for (i = 0U; i < 8U; i++) {
      g_fault_raw_ctx[i] = ctx[i];
    }
  }
  {
    volatile uint32_t *ctx_fpu = (volatile uint32_t *)(psp + 0x48U);
    g_fault_stacked_lr_fpu   = ctx_fpu[5];
    g_fault_stacked_pc_fpu   = ctx_fpu[6];
    g_fault_stacked_xpsr_fpu = ctx_fpu[7];
  }
  {
    TaskHandle_t th = xTaskGetCurrentTaskHandle();
    const char *tn = (th != NULL) ? pcTaskGetName(th) : "?";
    (void)strncpy(g_fault_task_name, tn, sizeof(g_fault_task_name) - 1U);
    g_fault_task_name[sizeof(g_fault_task_name) - 1U] = '\0';
  }

  g_fault_cfsr_full = SCB->CFSR;
  g_fault_mmfsr     = SCB->CFSR & 0xFFUL;
  g_fault_bfsr      = (SCB->CFSR >> 8) & 0xFFUL;
  g_fault_ufsr      = (SCB->CFSR >> 16) & 0xFFFFUL;
  g_fault_mmfar     = SCB->MMFAR;

  g_fault_mm_iaccviol  = (SCB->CFSR & SCB_CFSR_IACCVIOL_Msk) ? 1U : 0U;
  g_fault_mm_daccviol  = (SCB->CFSR & SCB_CFSR_DACCVIOL_Msk) ? 1U : 0U;
  g_fault_mm_mstkerr   = (SCB->CFSR & SCB_CFSR_MSTKERR_Msk) ? 1U : 0U;
  g_fault_mm_munstkerr = (SCB->CFSR & SCB_CFSR_MUNSTKERR_Msk) ? 1U : 0U;
  g_fault_mm_mmarvalid = (SCB->CFSR & SCB_CFSR_MMARVALID_Msk) ? 1U : 0U;

  __DSB();
  while (1) {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}

/******************************************************************************/
/*                 STM32H7xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32h7xx.s).                                               */
/******************************************************************************/

/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval None
  */
void ETH_IRQHandler(void)
{
  HAL_ETH_IRQHandler(&heth);
}

/*void PPP_IRQHandler(void)
{
}*/


/**
  * @}
  */ 

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
