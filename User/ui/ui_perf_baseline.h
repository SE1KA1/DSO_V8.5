/**
 * @file ui_perf_baseline.h
 * @brief 场测性能基线版本号与默认帧周期。
 */

/*
 * 阶段 0 — 性能对比基线（文档 + 默认参数锚点）
 * =================================================
 * 目的：后续优化前后用同一套「可声明」的配置做 Run-Time Stats / profile 对比。
 *
 * 手工核对清单（改实验条件后请更新 UI_PERF_BASELINE_RECORD_ID 并记下日期）：
 *   [ ] main.c：__HAL_RCC_DMA2D_CLK_ENABLE() 之后 DMA2D_Wait_Init()
 *   [ ] LCDConf_Lin_Template.c：DMA2D->CR |= START 之后为 DMA2D_Wait_TransferComplete（非忙等）
 *   [ ] LCDConf_Lin_Template.c：LCDCONF_PHYS_M2M_STRIPED（0=整屏 480×800 M2M 一次）
 *   [ ] profile_opts.h：MAINTASK_PROFILE_EMWIN_SEGMENTS（分段 profile + 串口打印）
 *   [ ] FreeRTOS：configGENERATE_RUN_TIME_STATS、TIM6 ulHighFrequencyTimerTicks 时基
 *
 * 记录方式：固定本 ID 后，抄串口 CpuStat 的 emWin%/IDLE%，以及 profile 的 TOTAL/[6a0]…（若开启）。
 */
#ifndef UI_PERF_BASELINE_H
/* 宏定义: UI_PERF_BASELINE_H - 头文件保护宏。 */
#define UI_PERF_BASELINE_H

#include <stdint.h>

/* 基线版本号：每次你刻意改「对比条件」时 +1，串口会打印便于对日志 */
#ifndef UI_PERF_BASELINE_RECORD_ID
/* 宏定义: UI_PERF_BASELINE_RECORD_ID - 内存地址或外设地址映射常量。 */
#define UI_PERF_BASELINE_RECORD_ID  1U
#endif

/* RUN 帧周期默认值（ms）；MainTask 未另行 #define MAINTASK_FRAME_PERIOD_MS 时使用 */
#ifndef UI_PERF_BASELINE_FRAME_PERIOD_MS
/* 宏定义: UI_PERF_BASELINE_FRAME_PERIOD_MS - 时间间隔、周期或时基相关配置常量。 */
#define UI_PERF_BASELINE_FRAME_PERIOD_MS  17U
#endif

/* 1=MainTask 启动时打印一行基线（仅一次）；正式发布可改为 0 */
#ifndef UI_PERF_BASELINE_BOOT_LOG
/* 宏定义: UI_PERF_BASELINE_BOOT_LOG - 内存地址或外设地址映射常量。 */
#define UI_PERF_BASELINE_BOOT_LOG  0
#endif

#endif /* UI_PERF_BASELINE_H */
