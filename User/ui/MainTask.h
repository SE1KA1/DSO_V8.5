#ifndef __MAINTASK_H
/* 宏定义: __MAINTASK_H - 头文件保护宏。 */
#define __MAINTASK_H

/**
 * @file MainTask.h
 * @brief emWin 主 UI 任务入口（FreeRTOS 任务名 "emWin"）。
 *
 * MainTask 负责示波器界面绘制与页面调度，与 DSO_Engine_Task 通过
 * UI_WaveformCtrl 的双缓冲帧环（DSO_UI_SendFrame → UI_Waveform_CopyUIFrame）
 * 解耦：引擎产帧、UI 消费帧，二者各自独立周期运行。
 *
 * 创建位置：User/main.c，优先级 2，栈 14336 words。
 * 详见 MainTask.c 文件头注释与 docs/architecture/02-runtime-task-flow.md。
 */
void MainTask(void);

#endif
