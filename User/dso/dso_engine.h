/**
 * @file dso_engine.h
 * @brief DSO 主引擎任务 API 与 g_dso_engine_task 句柄。
 */

#ifndef DSO_ENGINE_H
/* 宏定义: DSO_ENGINE_H - 头文件保护宏。 */
#define DSO_ENGINE_H

#include "FreeRTOS.h"
#include "dso_process.h"
#include "dso_ui_bridge.h"
#include "task.h"

/*
*********************************************************************************************************
*   函 数 名: DSO_Engine_Task
*   功能说明: DSO 主引擎 FreeRTOS 任务入口，负责采集服务、处理、时基渲染和 UI 帧发布。
*   形    参: param - FreeRTOS 任务参数，当前未使用。
*   返 回 值: 无；任务创建后在内部循环运行。
*********************************************************************************************************
*/
void DSO_Engine_Task(void *param);

/*
*********************************************************************************************************
*   函 数 名: DSO_UI_SendFrame
*   功能说明: 将 DSO 引擎生成的帧发布到 UI 双缓冲队列。
*   形    参: frame - 引擎侧填充完成的 DSO_Frame 指针。
*   返 回 值: 无。
*********************************************************************************************************
*/
extern void DSO_UI_SendFrame(const DSO_Frame *frame);

/* DSO 引擎任务句柄；用于栈水位、运行状态和调试诊断。 */
extern TaskHandle_t g_dso_engine_task;

#endif /* DSO_ENGINE_H */
