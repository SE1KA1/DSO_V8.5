/**
 * @file debug_uart_lock.h
 * @brief 串口 printf 互斥锁：多任务并发日志时防交错。
 */

#ifndef DEBUG_UART_LOCK_H
/* 宏定义: DEBUG_UART_LOCK_H - 头文件保护宏。 */
#define DEBUG_UART_LOCK_H

/*
 * 串行化多任务对调试串口 printf 的并发访问，减轻行首字符被 « 撕开 » 的乱码。
 * 调度器未启动时为空操作；禁止在 ISR 内调用 Lock/Unlock（亦勿在 ISR 内 printf）。
 */
void DebugUartLock(void);
/*
*********************************************************************************************************
*   函 数 名: DebugUartUnlock
*   功能说明: DebugUartUnlock 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DebugUartUnlock(void);

#endif /* DEBUG_UART_LOCK_H */
