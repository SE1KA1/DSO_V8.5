/**
 * @file debug_uart_lock.c
 * @brief DebugUartLock/Unlock 实现。
 */

#include "debug_uart_lock.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

static SemaphoreHandle_t s_debug_uart_mutex;

/*
*********************************************************************************************************
*   函 数 名: DebugUartLock
*   功能说明: DebugUartLock 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DebugUartLock(void)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return;
    }
    if (xPortIsInsideInterrupt() != pdFALSE) {
        return;
    }
    if (s_debug_uart_mutex == NULL) {
        s_debug_uart_mutex = xSemaphoreCreateMutex();
    }
    if (s_debug_uart_mutex != NULL) {
        (void)xSemaphoreTake(s_debug_uart_mutex, portMAX_DELAY);
    }
}

/*
*********************************************************************************************************
*   函 数 名: DebugUartUnlock
*   功能说明: DebugUartUnlock 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DebugUartUnlock(void)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return;
    }
    if (xPortIsInsideInterrupt() != pdFALSE) {
        return;
    }
    if (s_debug_uart_mutex != NULL) {
        (void)xSemaphoreGive(s_debug_uart_mutex);
    }
}
