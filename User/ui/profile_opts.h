/**
 * @file profile_opts.h
 * @brief MainTask/LCD 旋转 profiling 编译选项聚合。
 */

/*
 * 与 MainTask_profile、LCDConf ManualRotate 细分统计共用。
 * 置 1：分段打印 + ManualRotate 内 [6a] 干活 / [6b] VSYNC 等待 拆开（须 RTOS TIM6 + configGENERATE_RUN_TIME_STATS）。
 * LCDConf 与 MainTask 须看到同一值：请只在此文件改，勿仅在 MainTask.c #define。
 * 若串口只有 CpuStat 两行、无 [MTP] 与 MainTask profile：检查 Keil「预处理器定义」是否写了
 * MAINTASK_PROFILE_EMWIN_SEGMENTS=0（会覆盖本文件）；应删掉或改为 1。
 */
#ifndef PROFILE_OPTS_H
/* 宏定义: PROFILE_OPTS_H - 头文件保护宏。 */
#define PROFILE_OPTS_H

#ifndef MAINTASK_PROFILE_EMWIN_SEGMENTS
/* 0=关闭 MainTask 分段串口打印；调试性能时再改为 1（须 RTOS 运行时统计） */
#define MAINTASK_PROFILE_EMWIN_SEGMENTS  0
#endif

#endif /* PROFILE_OPTS_H */
