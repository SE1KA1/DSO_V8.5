/**
 * @file dso_integration.h
 * @brief DSO 引擎集成开关：USE_DSO_ENGINE 选择真实采集或假数据任务。
 */

#ifndef DSO_INTEGRATION_H
/* 宏定义: DSO_INTEGRATION_H - 头文件保护宏。 */
#define DSO_INTEGRATION_H

/*
 * DSO 引擎与 UI 的运行时集成开关（User/dso/ 波形管线）。
 *
 * USE_DSO_ENGINE = 1（默认）：
 *   main.c 创建 DSO_Engine_Task，走真实 ADC/TIM/DMA 采集链路。
 * USE_DSO_ENGINE = 0：
 *   创建 ADC_FakeData_Task，构造假 DSO_Frame 供无探头时的 UI 联调。
 *
 * 无论哪种模式，MainTask 均通过 UI_WaveformCtrl 同一套帧接口收帧。
 */
#ifndef USE_DSO_ENGINE
/* 宏定义: USE_DSO_ENGINE - USE_DSO_ENGINE 的宏定义。 */
#define USE_DSO_ENGINE  1
#endif

#endif /* DSO_INTEGRATION_H */
