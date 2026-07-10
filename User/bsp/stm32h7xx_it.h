/**
  ******************************************************************************
  * @file    Templates/Inc/stm32h7xx_it.h 
  * @author  MCD Application Team
  * @version V1.2.0
  * @date    29-December-2017
  * @brief   This file contains the headers of the interrupt handlers.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32H7xx_IT_H
/* 宏定义: __STM32H7xx_IT_H - 头文件保护宏。 */
#define __STM32H7xx_IT_H

#ifdef __cplusplus
 extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

/* MemManage 瞬间快照（在 Handler 入口读取）；Watch: g_fault_mmfar、g_fault_mmfsr（MMFSR 位定义见 ARMv7-M CFSR[7:0]） */
extern volatile uint32_t g_fault_cfsr_full;
extern volatile uint32_t g_fault_mmfsr;
extern volatile uint32_t g_fault_mmfar;
/* CFSR 子区（整段 CFSR 亦可拆 Watch） */
extern volatile uint32_t g_fault_bfsr;
extern volatile uint32_t g_fault_ufsr;
/* MMFSR 常用位 0/1 便于看窗口 */
extern volatile uint32_t g_fault_mm_iaccviol;
extern volatile uint32_t g_fault_mm_daccviol;
extern volatile uint32_t g_fault_mm_mstkerr;
extern volatile uint32_t g_fault_mm_munstkerr;
extern volatile uint32_t g_fault_mm_mmarvalid;
/* Handler 内的 LR=EXC_RETURN；0x10 为 0 且低位有效时常表示已压 FPU 扩展帧，整数上下文在 PSP+0x48 起 */
extern volatile uint32_t g_fault_exc_return;
extern volatile uint32_t g_fault_ctx_offset;
/* 进入 MemManage 瞬间的 PSP；以及任务栈上异常帧内的 LR/PC（已按 g_fault_ctx_offset 校正 FPU 扩展帧） */
extern volatile uint32_t g_fault_psp_at_entry;
extern volatile uint32_t g_fault_stacked_lr;
extern volatile uint32_t g_fault_stacked_pc;
extern volatile uint32_t g_fault_stacked_xpsr;
extern volatile uint32_t g_fault_stacked_lr_fpu;
extern volatile uint32_t g_fault_stacked_pc_fpu;
extern volatile uint32_t g_fault_stacked_xpsr_fpu;
extern volatile uint32_t g_fault_raw_ctx[8];
extern volatile char     g_fault_task_name[12];

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32H7xx_IT_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
