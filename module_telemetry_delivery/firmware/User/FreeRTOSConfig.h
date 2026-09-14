/**
 * @file FreeRTOSConfig.h
 * @brief FreeRTOS 内核配置：堆大小、优先级、钩子、运行时统计等。
 */

/*
 * FreeRTOS Kernel V10.2.0
 * Copyright (C) 2019 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */


#ifndef FREERTOS_CONFIG_H
/* 宏定义: FREERTOS_CONFIG_H - 头文件保护宏。 */
#define FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * Application specific definitions.
 *
 * These definitions should be adjusted for your particular hardware and
 * application requirements.
 *
 * THESE PARAMETERS ARE DESCRIBED WITHIN THE 'CONFIGURATION' SECTION OF THE
 * FreeRTOS API DOCUMENTATION AVAILABLE ON THE FreeRTOS.org WEB SITE.
 *
 * See http://www.freertos.org/a00110.html
 *----------------------------------------------------------*/
 
 /* Ensure stdint is only used by the compiler, and not the assembler. */
#if defined(__ICCARM__) || defined(__CC_ARM) || defined(__GNUC__)
 #include <stdint.h>
 extern volatile uint32_t ulHighFrequencyTimerTicks;
#endif


/* 宏定义: configUSE_PREEMPTION - configUSE_PREEMPTION 的宏定义。 */
#define configENABLE_FPU                         0
#define configUSE_PREEMPTION					1
/* 宏定义: configUSE_PORT_OPTIMISED_TASK_SELECTION - configUSE_PORT_OPTIMISED_TASK_SELECTION 的宏定义。 */
#define configUSE_PORT_OPTIMISED_TASK_SELECTION	1
/* 宏定义: configUSE_QUEUE_SETS - configUSE_QUEUE_SETS 的宏定义。 */
#define configUSE_QUEUE_SETS					1
/* 宏定义: configUSE_IDLE_HOOK - configUSE_IDLE_HOOK 的宏定义。 */
#define configUSE_IDLE_HOOK						0
/* 宏定义: configUSE_TICK_HOOK - configUSE_TICK_HOOK 的宏定义。 */
#define configUSE_TICK_HOOK						0
/* 宏定义: configCPU_CLOCK_HZ - configCPU_CLOCK_HZ 的宏定义。 */
#define configCPU_CLOCK_HZ						( 400000000 )
/* 宏定义: configTICK_RATE_HZ - configTICK_RATE_HZ 的宏定义。 */
#define configTICK_RATE_HZ						( 1000 )
/* 宏定义: configMAX_PRIORITIES - configMAX_PRIORITIES 的宏定义。 */
#define configMAX_PRIORITIES					( 7 )
/* 宏定义: configMINIMAL_STACK_SIZE - configMINIMAL_STACK_SIZE 的宏定义。 */
#define configMINIMAL_STACK_SIZE				( ( unsigned short ) 130 )
/* emWin/DSO 任务栈自 heap_4 分配；增大栈深后需同步加大堆，避免 xTaskCreate/pvPortMalloc 失败。
 * 148KB 时 LwIP tcpip 线程创建耗尽堆（FMC16 任务加入后）；AXI SRAM 实测余 359KB，加到 160KB。 */
#define configTOTAL_HEAP_SIZE					( ( size_t ) ( 160 * 1024 ) )
/* 宏定义: configMAX_TASK_NAME_LEN - configMAX_TASK_NAME_LEN 的宏定义。 */
#define configMAX_TASK_NAME_LEN					( 10 )
/* 宏定义: configUSE_TRACE_FACILITY - configUSE_TRACE_FACILITY 的宏定义。 */
#define configUSE_TRACE_FACILITY				1
/* 宏定义: configUSE_16_BIT_TICKS - configUSE_16_BIT_TICKS 的宏定义。 */
#define configUSE_16_BIT_TICKS					0
/* 宏定义: configIDLE_SHOULD_YIELD - configIDLE_SHOULD_YIELD 的宏定义。 */
#define configIDLE_SHOULD_YIELD					1
/* 宏定义: configUSE_MUTEXES - configUSE_MUTEXES 的宏定义。 */
#define configUSE_MUTEXES						1
/* 宏定义: configQUEUE_REGISTRY_SIZE - configQUEUE_REGISTRY_SIZE 的宏定义。 */
#define configQUEUE_REGISTRY_SIZE				8
/* 宏定义: configCHECK_FOR_STACK_OVERFLOW - configCHECK_FOR_STACK_OVERFLOW 的宏定义。 */
#define configCHECK_FOR_STACK_OVERFLOW			2
/* 宏定义: configUSE_RECURSIVE_MUTEXES - configUSE_RECURSIVE_MUTEXES 的宏定义。 */
#define configUSE_RECURSIVE_MUTEXES				1
/* 宏定义: configUSE_MALLOC_FAILED_HOOK - configUSE_MALLOC_FAILED_HOOK 的宏定义。 */
#define configUSE_MALLOC_FAILED_HOOK			1
/* 宏定义: configUSE_APPLICATION_TASK_TAG - configUSE_APPLICATION_TASK_TAG 的宏定义。 */
#define configUSE_APPLICATION_TASK_TAG			0
/* 宏定义: configUSE_COUNTING_SEMAPHORES - configUSE_COUNTING_SEMAPHORES 的宏定义。 */
#define configUSE_COUNTING_SEMAPHORES			1

/* The full demo always has tasks to run so the tick will never be turned off.
The blinky demo will use the default tickless idle implementation to turn the
tick off. */
#define configUSE_TICKLESS_IDLE					0

/* Run time stats gathering definitions. */
/* This demo makes use of one or more example stats formatting functions.  These
format the raw data provided by the uxTaskGetSystemState() function in to human
readable ASCII form.  See the notes in the implementation of vTaskList() within
FreeRTOS/Source/tasks.c for limitations. */
#define configUSE_TRACE_FACILITY	                1
/* 宏定义: configGENERATE_RUN_TIME_STATS - configGENERATE_RUN_TIME_STATS 的宏定义。
 * 精简工程不编译 rtos_cpu_stats.c（无 TIM6 计数源），故置 0，避免
 * ulHighFrequencyTimerTicks 链接缺失。 */
#define configGENERATE_RUN_TIME_STATS               0
/* 宏定义: configUSE_STATS_FORMATTING_FUNCTIONS - configUSE_STATS_FORMATTING_FUNCTIONS 的宏定义。 */
#define configUSE_STATS_FORMATTING_FUNCTIONS        0

/* Co-routine definitions. */
#define configUSE_CO_ROUTINES 			0
/* 宏定义: configMAX_CO_ROUTINE_PRIORITIES - configMAX_CO_ROUTINE_PRIORITIES 的宏定义。 */
#define configMAX_CO_ROUTINE_PRIORITIES ( 2 )

/* Software timer definitions. */
#define configUSE_TIMERS				1
/* 宏定义: configTIMER_TASK_PRIORITY - configTIMER_TASK_PRIORITY 的宏定义。 */
#define configTIMER_TASK_PRIORITY		( configMAX_PRIORITIES - 1 )
/* 宏定义: configTIMER_QUEUE_LENGTH - configTIMER_QUEUE_LENGTH 的宏定义。 */
#define configTIMER_QUEUE_LENGTH		5
/* 宏定义: configTIMER_TASK_STACK_DEPTH - configTIMER_TASK_STACK_DEPTH 的宏定义。 */
#define configTIMER_TASK_STACK_DEPTH	( configMINIMAL_STACK_SIZE * 2 )

/* Set the following definitions to 1 to include the API function, or zero
to exclude the API function. */
#define INCLUDE_vTaskPrioritySet		1
/* 宏定义: INCLUDE_uxTaskPriorityGet - INCLUDE_uxTaskPriorityGet 的宏定义。 */
#define INCLUDE_uxTaskPriorityGet		1
/* 宏定义: INCLUDE_vTaskDelete - INCLUDE_vTaskDelete 的宏定义。 */
#define INCLUDE_vTaskDelete				1
/* 宏定义: INCLUDE_vTaskCleanUpResources - INCLUDE_vTaskCleanUpResources 的宏定义。 */
#define INCLUDE_vTaskCleanUpResources	1
/* 宏定义: INCLUDE_vTaskSuspend - INCLUDE_vTaskSuspend 的宏定义。 */
#define INCLUDE_vTaskSuspend			1
/* 宏定义: INCLUDE_vTaskDelayUntil - INCLUDE_vTaskDelayUntil 的宏定义。 */
#define INCLUDE_vTaskDelayUntil			1
/* 宏定义: INCLUDE_vTaskDelay - INCLUDE_vTaskDelay 的宏定义。 */
#define INCLUDE_vTaskDelay				1
/* 宏定义: INCLUDE_eTaskGetState - INCLUDE_eTaskGetState 的宏定义。 */
#define INCLUDE_eTaskGetState			1
/* 宏定义: INCLUDE_xTimerPendFunctionCall - INCLUDE_xTimerPendFunctionCall 的宏定义。 */
#define INCLUDE_xTimerPendFunctionCall	1
/* 宏定义: INCLUDE_uxTaskGetStackHighWaterMark - INCLUDE_uxTaskGetStackHighWaterMark 的宏定义。 */
#define INCLUDE_uxTaskGetStackHighWaterMark	1

/* Cortex-M specific definitions. */
#ifdef __NVIC_PRIO_BITS
	/* __BVIC_PRIO_BITS will be specified when CMSIS is being used. */
	#define configPRIO_BITS       		__NVIC_PRIO_BITS
#else
	#define configPRIO_BITS       		4        /* 15 priority levels */
#endif

/* The lowest interrupt priority that can be used in a call to a "set priority"
function. */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY			15

/* The highest interrupt priority that can be used by any interrupt service
routine that makes calls to interrupt safe FreeRTOS API functions.  DO NOT CALL
INTERRUPT SAFE FREERTOS API FUNCTIONS FROM ANY INTERRUPT THAT HAS A HIGHER
PRIORITY THAN THIS! (higher priorities are lower numeric values. */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY	1

/* Interrupt priorities used by the kernel port layer itself.  These are generic
to all Cortex-M ports, and do not rely on any particular library functions. */
#define configKERNEL_INTERRUPT_PRIORITY 		( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
/* !!!! configMAX_SYSCALL_INTERRUPT_PRIORITY must not be set to zero !!!!
See http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html. */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 	( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* Definitions that map the FreeRTOS port interrupt handlers to their CMSIS
standard names. */
#define xPortPendSVHandler PendSV_Handler
/* 宏定义: vPortSVCHandler - vPortSVCHandler 的宏定义。 */
#define vPortSVCHandler SVC_Handler
/* 宏定义: xPortSysTickHandler - xPortSysTickHandler 的宏定义。 */
#define xPortSysTickHandler SysTick_Handler

/* Normal assert() semantics without relying on the provision of an assert.h
header file. */
#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for(;;);  }	

#endif /* FREERTOS_CONFIG_H */

