/**
 * @file dma2d_wait.h
 * @brief DMA2D 硬件忙等待与超时辅助 API。
 */

#ifndef DMA2D_WAIT_H
/* 宏定义: DMA2D_WAIT_H - 头文件保护宏。 */
#define DMA2D_WAIT_H

#include "FreeRTOS.h"

/*
 * portMAX_DELAY 时实际使用的超时（毫秒）。超时后会 ABORT 并清标志，避免 MainTask 永久阻塞。
 * 可在包含本头文件前 #define 覆盖。
 */
#ifndef DMA2D_WAIT_TIMEOUT_MS
/* 宏定义: DMA2D_WAIT_TIMEOUT_MS - DMA2D_WAIT_TIMEOUT_MS 的宏定义。 */
#define DMA2D_WAIT_TIMEOUT_MS  500u
#endif

/* 须在使用 DMA2D 且调用 Wait 前执行一次（例如 main 里 RCC 开 DMA2D 后） */
void DMA2D_Wait_Init(void);

/* 在 DMA2D->CR |= DMA2D_CR_START 之后调用：TC/TE/CE 等中断内 give，本任务阻塞让出 CPU */
void DMA2D_Wait_TransferComplete(TickType_t xTicksToWait);

/* 因等待超时而执行硬件恢复的次数（调试用，永不递减） */
uint32_t DMA2D_Wait_GetTimeoutRecoverCount(void);

/* 单一 DMA2D 外设上串行化「Wait→配置→START→Wait」；允许多层嵌套（递归），与 emWin/PlotBuf 同任务内交错调用兼容 */
void DMA2D_Bus_RecursiveInit(void);
/*
*********************************************************************************************************
*   函 数 名: DMA2D_Bus_RecursiveLock
*   功能说明: DMA2D_Bus_RecursiveLock 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DMA2D_Bus_RecursiveLock(void);
/*
*********************************************************************************************************
*   函 数 名: DMA2D_Bus_RecursiveUnlock
*   功能说明: DMA2D_Bus_RecursiveUnlock 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DMA2D_Bus_RecursiveUnlock(void);

#endif /* DMA2D_WAIT_H */
