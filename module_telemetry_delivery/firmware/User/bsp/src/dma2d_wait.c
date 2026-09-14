/**
 * @file dma2d_wait.c
 * @brief DMA2D 等待实现，供 PlotBuf 与 emWin 填色同步。
 */

/*
 * DMA2D 传输完成：中断 + 信号量阻塞，替代 while(DMA2D->CR & START) 空转，降低 emWin 任务 CPU 统计占比。
 * 对 portMAX_DELAY 使用有界超时并在超时后 ABORT，避免 SDRAM/总线异常时 MainTask 永久卡死。
 */
#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "dma2d_wait.h"

static SemaphoreHandle_t   s_dma2d_tc_sem;
static SemaphoreHandle_t   s_dma2d_bus_mtx;
static volatile uint32_t  s_timeout_recover_count;

/* 宏定义: DMA2D_IFCR_CLEAR_ALL - DMA2D_IFCR_CLEAR_ALL 的宏定义。 */
#define DMA2D_IFCR_CLEAR_ALL  (DMA2D_IFCR_CTEIF | DMA2D_IFCR_CTCIF | DMA2D_IFCR_CTWIF | \
                             DMA2D_IFCR_CAECIF | DMA2D_IFCR_CCTCIF | DMA2D_IFCR_CCEIF)

/*
*********************************************************************************************************
*   函 数 名: dma2d_disable_irq_bits
*   功能说明: dma2d_disable_irq_bits 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void dma2d_disable_irq_bits(void)
{
    DMA2D->CR &= ~(uint32_t)(DMA2D_CR_TCIE | DMA2D_CR_TEIE | DMA2D_CR_TWIE |
                             DMA2D_CR_CAEIE | DMA2D_CR_CTCIE | DMA2D_CR_CEIE);
}

/*
*********************************************************************************************************
*   函 数 名: dma2d_recover_after_stall
*   功能说明: dma2d_recover_after_stall 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void dma2d_recover_after_stall(void)
{
    dma2d_disable_irq_bits();
    DMA2D->IFCR = DMA2D_IFCR_CLEAR_ALL;

    if ((DMA2D->CR & DMA2D_CR_START) != 0U) {
        DMA2D->CR |= DMA2D_CR_ABORT;
        for (volatile uint32_t n = 0U;
             n < 500000U && (DMA2D->CR & DMA2D_CR_START) != 0U;
             n++) {
        }
    }

    DMA2D->IFCR = DMA2D_IFCR_CLEAR_ALL;
}

/*
*********************************************************************************************************
*   函 数 名: DMA2D_Wait_GetTimeoutRecoverCount
*   功能说明: DMA2D_Wait_GetTimeoutRecoverCount 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DMA2D_Wait_GetTimeoutRecoverCount(void)
{
    return s_timeout_recover_count;
}

/*
*********************************************************************************************************
*   函 数 名: DMA2D_Bus_RecursiveInit
*   功能说明: DMA2D_Bus_RecursiveInit 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DMA2D_Bus_RecursiveInit(void)
{
    if (s_dma2d_bus_mtx == NULL) {
        s_dma2d_bus_mtx = xSemaphoreCreateRecursiveMutex();
    }
}

/*
*********************************************************************************************************
*   函 数 名: DMA2D_Bus_RecursiveLock
*   功能说明: DMA2D_Bus_RecursiveLock 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DMA2D_Bus_RecursiveLock(void)
{
    if (s_dma2d_bus_mtx == NULL) {
        DMA2D_Bus_RecursiveInit();
    }
    (void)xSemaphoreTakeRecursive(s_dma2d_bus_mtx, portMAX_DELAY);
}

/*
*********************************************************************************************************
*   函 数 名: DMA2D_Bus_RecursiveUnlock
*   功能说明: DMA2D_Bus_RecursiveUnlock 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DMA2D_Bus_RecursiveUnlock(void)
{
    if (s_dma2d_bus_mtx != NULL) {
        (void)xSemaphoreGiveRecursive(s_dma2d_bus_mtx);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DMA2D_Wait_Init
*   功能说明: DMA2D_Wait_Init 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DMA2D_Wait_Init(void)
{
    if (s_dma2d_tc_sem == NULL) {
        s_dma2d_tc_sem = xSemaphoreCreateBinary();
    }
    DMA2D_Bus_RecursiveInit();
    /* 清 stale 标志 */
    DMA2D->IFCR = DMA2D_IFCR_CLEAR_ALL;
    HAL_NVIC_SetPriority(DMA2D_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA2D_IRQn);
}

/*
*********************************************************************************************************
*   函 数 名: DMA2D_Wait_TransferComplete
*   功能说明: DMA2D_Wait_TransferComplete 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DMA2D_Wait_TransferComplete(TickType_t xTicksToWait)
{
    TickType_t wait_ticks = xTicksToWait;

    if (s_dma2d_tc_sem == NULL) {
        DMA2D_Wait_Init();
    }

    if (wait_ticks == portMAX_DELAY) {
        wait_ticks = pdMS_TO_TICKS(DMA2D_WAIT_TIMEOUT_MS);
        if (wait_ticks < (TickType_t)1) {
            wait_ticks = (TickType_t)1;
        }
    }

    /* 排空残留 token（传输极快完成、或 ISR 与任务交错时） */
    while (xSemaphoreTake(s_dma2d_tc_sem, 0) == pdTRUE) {
    }

    DMA2D->IFCR = DMA2D_IFCR_CTCIF;

    /* 已结束则不必等（避免误 Take 到旧 token：正常路径 ISR 与 START 同步） */
    if ((DMA2D->CR & DMA2D_CR_START) == 0U) {
        return;
    }

    DMA2D->CR |= DMA2D_CR_TCIE;
    if (xSemaphoreTake(s_dma2d_tc_sem, wait_ticks) != pdTRUE) {
        s_timeout_recover_count++;
        dma2d_recover_after_stall();
        while (xSemaphoreTake(s_dma2d_tc_sem, 0) == pdTRUE) {
        }
    }
    DMA2D->CR &= ~(uint32_t)DMA2D_CR_TCIE;
}

/*
*********************************************************************************************************
*   函 数 名: DMA2D_IRQHandler
*   功能说明: DMA2D_IRQHandler 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DMA2D_IRQHandler(void)
{
    uint32_t isr = DMA2D->ISR;
    BaseType_t hpw = pdFALSE;
    int        wake = 0;

    if ((isr & DMA2D_ISR_TCIF) != 0U) {
        DMA2D->IFCR = DMA2D_IFCR_CTCIF;
        wake        = 1;
    }
    if ((isr & DMA2D_ISR_TEIF) != 0U) {
        DMA2D->IFCR = DMA2D_IFCR_CTEIF;
        wake        = 1;
    }
    if ((isr & DMA2D_ISR_CEIF) != 0U) {
        DMA2D->IFCR = DMA2D_IFCR_CCEIF;
        wake        = 1;
    }
    if ((isr & DMA2D_ISR_CAEIF) != 0U) {
        DMA2D->IFCR = DMA2D_IFCR_CAECIF;
        wake        = 1;
    }

    if (wake != 0) {
        DMA2D->CR &= ~(uint32_t)DMA2D_CR_TCIE;
        if (s_dma2d_tc_sem != NULL) {
            (void)xSemaphoreGiveFromISR(s_dma2d_tc_sem, &hpw);
        }
    }

    portYIELD_FROM_ISR(hpw);
}
