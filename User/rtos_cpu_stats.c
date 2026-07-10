/**
 * @file rtos_cpu_stats.c
 * @brief FreeRTOS 运行时 CPU 占用统计：TIM6 时基与统计任务。
 */

/*
 * FreeRTOS 运行时间统计：串口周期性打印各任务 CPU%（printf → UART，见 bsp_uart_fifo fputc）
 */
#include "rtos_cpu_stats.h"

#if RTOS_CPU_STATS_ENABLE

#include "includes.h"
#include "debug_uart_lock.h"
#include "dso/dso_serial_debug.h"
#include "ui/lcd_rotate_request.h"
#include <stdio.h>

/*
 * vTaskGetRunTimeStats / vTaskList 内部用 sprintf 连续追加且**不检查缓冲区长度**，
 * 1024 易溢出破坏其后全局变量，串口上出现乱码后整机异常（与日志中 g_lcdconf_vsync 行后乱码吻合）。
 */
#define CPU_STATS_BUF_BYTES  4096U

static char s_rtos_stats_buf[CPU_STATS_BUF_BYTES];

/*
*********************************************************************************************************
*   函 数 名: RtosCpuStats_TimerInit
*   功能说明: RtosCpuStats_TimerInit 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void RtosCpuStats_TimerInit(void)
{
    vSetupSysInfoTest();
}

/*
*********************************************************************************************************
*   函 数 名: RtosCpuStats_Task
*   功能说明: RtosCpuStats_Task 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void RtosCpuStats_Task(void *arg)
{
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(RTOS_CPU_STATS_PRINT_PERIOD_MS));
        DebugUartLock();
        printf("\r\n======== FreeRTOS Run-Time Stats (TIM6 50us base) ========\r\n");
        printf("g_lcdconf_vsync_timeout_cnt = %lu  (LTDC VSYNC 等待超时次数，底板噪声时可能增加)\r\n",
               (unsigned long)g_lcdconf_vsync_timeout_cnt);
        vTaskGetRunTimeStats(s_rtos_stats_buf);
        printf("%s", s_rtos_stats_buf);
        printf("-------- Task State List --------\r\n");
        vTaskList(s_rtos_stats_buf);
        printf("%s", s_rtos_stats_buf);
        printf("============================================================\r\n\r\n");
        DebugUartUnlock();
    }
}

/*
*********************************************************************************************************
*   函 数 名: RtosCpuStats_TaskCreate
*   功能说明: RtosCpuStats_TaskCreate 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void RtosCpuStats_TaskCreate(void)
{
    if (DSO_SerialDebug_IsPhaseOnlyModeEnabled()) {
        return;
    }

    /* 优先级 1：与 DSOEng 同级；栈需容纳 vTaskGetRunTimeStats 内 sprintf + 微库开销 */
    if (xTaskCreate(RtosCpuStats_Task, "CpuStat", 2048, NULL, 1, NULL) != pdPASS) {
        DebugUartLock();
        printf("RtosCpuStats_TaskCreate failed\r\n");
        DebugUartUnlock();
    }
}

#endif /* RTOS_CPU_STATS_ENABLE */
