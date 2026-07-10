/**
 * @file ui_app_config.h
 * @brief UI 层编译期配置：帧率、Demo 模式、PlotBuf A/B、EC 轮询、诊断开关。
 *
 * 排障原则：一次只改一个宏，对比后再恢复（与 y_t_stress_ab.h 场测开关配合）。
 *
 * 模块关系：
 *   MainTask      — MAINTASK_* 帧周期、PlotBuf 旁路、栈/LTDC 周期日志
 *   ui_ec_poll    — UI_EC_POLL_* 编码器任务周期与优先级
 *   LCDConf       — LCDCONF_RENDER_LATCH_* / PHYS_STRIPED_* 旋转提交时序
 *   UI Demo       — UI_APP_DEMO_CYCLE_SCREENS / UI_DEMO_* 多页演示
 */
#ifndef UI_APP_CONFIG_H
/* 宏定义: UI_APP_CONFIG_H - 头文件保护宏。 */
#define UI_APP_CONFIG_H

#ifndef UI_APP_DEMO_CYCLE_SCREENS
/** 1=MainTask 多页 Demo（欢迎/菜单/示波器/通道/触发/测量/系统）；0=仅示波器循环 */
#define UI_APP_DEMO_CYCLE_SCREENS  1
#endif

#ifndef UI_DEMO_SCREEN_MS
/** 自动切页间隔（须 UI_DEMO_AUTO_ADVANCE=1） */
#define UI_DEMO_SCREEN_MS  10000u
#endif

#ifndef UI_DEMO_AUTO_ADVANCE
/** 1=按 UI_DEMO_SCREEN_MS 自动轮播子界面 */
#define UI_DEMO_AUTO_ADVANCE  0
#endif

/* MainTask 栈/LTDC/VRAM 周期串口日志间隔；0=关闭 */
#ifndef MAINTASK_STACK_LOG_INTERVAL_MS
/* 宏定义: MAINTASK_STACK_LOG_INTERVAL_MS - 时间间隔、周期或时基相关配置常量。 */
#define MAINTASK_STACK_LOG_INTERVAL_MS  2000U
#endif

/* RUN 模式帧周期：25 ms ≈ 40 fps（减轻 RenderLatch/LTDC 争用与偶发闪屏） */
#ifndef MAINTASK_FRAME_PERIOD_MS
/* 宏定义: MAINTASK_FRAME_PERIOD_MS - 时间间隔、周期或时基相关配置常量。 */
#define MAINTASK_FRAME_PERIOD_MS  25U
#endif

/* FAST 时基专用帧周期；默认与 MAINTASK_FRAME_PERIOD_MS 相同，可用 -D 单独覆盖 */
#ifndef MAINTASK_FAST_FRAME_PERIOD_MS
/* 宏定义: MAINTASK_FAST_FRAME_PERIOD_MS - 时间间隔、周期或时基相关配置常量。 */
#define MAINTASK_FAST_FRAME_PERIOD_MS  MAINTASK_FRAME_PERIOD_MS
#endif

#ifndef MAINTASK_SCOPE_USE_FAST_PERIOD_MAX_SEC_DIV_NS
/** sec/div ≤ 此值（ns）且 tb_mode=FAST 时启用 FAST 帧周期 */
#define MAINTASK_SCOPE_USE_FAST_PERIOD_MAX_SEC_DIV_NS  5000000u
#endif

/* 1=每 PlotBuf 帧强制 MarkBodyDirty；仅诊断，正常运行应为 0 */
#ifndef MAINTASK_PLOTBUF_ROTATE_BODY_EVERY_FRAME
/* 宏定义: MAINTASK_PLOTBUF_ROTATE_BODY_EVERY_FRAME - 当前模块使用的配置常量。 */
#define MAINTASK_PLOTBUF_ROTATE_BODY_EVERY_FRAME  0
#endif

/* 1=PreparePhysical 与 Commit 之间 taskYIELD，让 DSO/EC 任务插入 */
#ifndef LCDCONF_RENDER_LATCH_YIELD_BEFORE_COMMIT
/* 宏定义: LCDCONF_RENDER_LATCH_YIELD_BEFORE_COMMIT - 当前模块使用的配置常量。 */
#define LCDCONF_RENDER_LATCH_YIELD_BEFORE_COMMIT  1
#endif

/* 1=物理 M2M 分条拷贝条间 yield */
#ifndef LCDCONF_PHYS_STRIPED_INTER_YIELD
/* 宏定义: LCDCONF_PHYS_STRIPED_INTER_YIELD - 当前模块使用的配置常量。 */
#define LCDCONF_PHYS_STRIPED_INTER_YIELD  0
#endif

/* 1=旁路 PlotBuf，波形改走纯 emWin GUI 路径（对比 DMA2D 嫌疑时用） */
#ifndef MAINTASK_AB_DISABLE_PLOTBUF_WAVEFORM
/* 宏定义: MAINTASK_AB_DISABLE_PLOTBUF_WAVEFORM - 编译期开关，用于启用或关闭对应功能。 */
#define MAINTASK_AB_DISABLE_PLOTBUF_WAVEFORM  0
#endif

/* 非示波器页 MainTask idle 轮询间隔 */
#ifndef UI_DEMO_IDLE_POLL_MS
/* 宏定义: UI_DEMO_IDLE_POLL_MS - 时间间隔、周期或时基相关配置常量。 */
#define UI_DEMO_IDLE_POLL_MS  8u
#endif

/* EC 轮询任务（ui_ec_poll_task.c） */
#ifndef UI_EC_POLL_MS
/* 宏定义: UI_EC_POLL_MS - 时间间隔、周期或时基相关配置常量。 */
#define UI_EC_POLL_MS  8u
#endif

#ifndef UI_EC_POLL_TASK_PRIORITY
/* 宏定义: UI_EC_POLL_TASK_PRIORITY - 当前模块使用的配置常量。 */
#define UI_EC_POLL_TASK_PRIORITY  3U
#endif

#ifndef UI_EC_POLL_TASK_STACK_WORDS
/* 宏定义: UI_EC_POLL_TASK_STACK_WORDS - 当前模块使用的配置常量。 */
#define UI_EC_POLL_TASK_STACK_WORDS  2048U
#endif

#ifndef UI_ENC_PULSES_PER_DETENT
/** EC11 每格脉冲数（与 ui_nav.c 正交解码配合） */
#define UI_ENC_PULSES_PER_DETENT  4
#endif

/* MainTask PlotSampleIndexToLocalX 默认有效采样率（Hz） */
#ifndef UI_SCOPE_EFFECTIVE_FS_HZ
/* 宏定义: UI_SCOPE_EFFECTIVE_FS_HZ - 当前模块使用的配置常量。 */
#define UI_SCOPE_EFFECTIVE_FS_HZ  80000.0f
#endif

#ifndef UI_WAVEFORM_VERTICAL_HEADROOM_CH1
/* 宏定义: UI_WAVEFORM_VERTICAL_HEADROOM_CH1 - 当前模块使用的配置常量。 */
#define UI_WAVEFORM_VERTICAL_HEADROOM_CH1  0.70f
#endif

#ifndef UI_WAVEFORM_VERTICAL_HEADROOM_CH2
/* 宏定义: UI_WAVEFORM_VERTICAL_HEADROOM_CH2 - 当前模块使用的配置常量。 */
#define UI_WAVEFORM_VERTICAL_HEADROOM_CH2  0.86f
#endif

#endif /* UI_APP_CONFIG_H */
