/**
 * @file y_t_stress_ab.h
 * @brief Y-T 显示/引擎场测 A/B 开关集合（单变量对比用）。
 */

/**
 * Y-T / LCD display A/B switches.
 * Use one switch at a time, then restore it before testing the next variable.
 */
#ifndef Y_T_STRESS_AB_H
/* 宏定义: Y_T_STRESS_AB_H - 头文件保护宏。 */
#define Y_T_STRESS_AB_H

/* 1 = do not enable D-Cache. Diagnostic only; not a production default. */
#ifndef Y_T_AB_DISABLE_DCACHE
/* 宏定义: Y_T_AB_DISABLE_DCACHE - Y_T_AB_DISABLE_DCACHE 的宏定义。 */
#define Y_T_AB_DISABLE_DCACHE  0
#endif

/* 1 = disable LCDConf DMA2D fill/copy paths and use CPU paths instead. */
#ifndef Y_T_AB_DISABLE_DMA2D_FILL_COPY
/* 宏定义: Y_T_AB_DISABLE_DMA2D_FILL_COPY - Y_T_AB_DISABLE_DMA2D_FILL_COPY 的宏定义。 */
#define Y_T_AB_DISABLE_DMA2D_FILL_COPY  0
#endif

/* 1 = wrap MainTask_Scope_RenderFrame with GUI_LOCK/GUI_UNLOCK. */
#ifndef Y_T_AB_WRAP_SCOPE_RENDER_WITH_GUI_LOCK
/* 宏定义: Y_T_AB_WRAP_SCOPE_RENDER_WITH_GUI_LOCK - Y_T_AB_WRAP_SCOPE_RENDER_WITH_GUI_LOCK 的宏定义。 */
#define Y_T_AB_WRAP_SCOPE_RENDER_WITH_GUI_LOCK  0
#endif

/* 1 = clean full D-Cache at MainTask_Scope_RenderFrame entry. */
#ifndef Y_T_AB_CLEAN_DCACHE_BEFORE_SCOPE_FRAME
/* 宏定义: Y_T_AB_CLEAN_DCACHE_BEFORE_SCOPE_FRAME - Y_T_AB_CLEAN_DCACHE_BEFORE_SCOPE_FRAME 的宏定义。 */
#define Y_T_AB_CLEAN_DCACHE_BEFORE_SCOPE_FRAME  0
#endif

/* 1 = after GUI/PlotBuf drawing, clean+invalidate the full logical VRAM frame
 * before LCDConf reads it for rotation. Diagnostic for logical framebuffer
 * coherency; keep as a single-variable A/B switch. */
#ifndef Y_T_AB_CLEANINV_LOGICAL_VRAM_BEFORE_LATCH
/* 宏定义: Y_T_AB_CLEANINV_LOGICAL_VRAM_BEFORE_LATCH - Y_T_AB_CLEANINV_LOGICAL_VRAM_BEFORE_LATCH 的宏定义。 */
#define Y_T_AB_CLEANINV_LOGICAL_VRAM_BEFORE_LATCH  0
#endif

/* 1 = override the display VRAM window at 0xC0000000 as non-cacheable.
 * Diagnostic for SDRAM write-through/posted-write visibility in the
 * LTDC/DMA2D/CPU display path. */
#ifndef Y_T_AB_NONCACHEABLE_DISPLAY_VRAM
/* 宏定义: Y_T_AB_NONCACHEABLE_DISPLAY_VRAM - Y_T_AB_NONCACHEABLE_DISPLAY_VRAM 的宏定义。 */
#define Y_T_AB_NONCACHEABLE_DISPLAY_VRAM  0
#endif

/* 1 = bypass RenderLatch partial-region bookkeeping and refresh the full
 * logical frame into the target physical framebuffer every scope frame.
 * PlotBuf composition still follows the frame plan, so this isolates the
 * partial Header/Body region sync path. */
#ifndef Y_T_AB_FORCE_FULL_ROTATE_EVERY_FRAME
/* 宏定义: Y_T_AB_FORCE_FULL_ROTATE_EVERY_FRAME - Y_T_AB_FORCE_FULL_ROTATE_EVERY_FRAME 的宏定义。 */
#define Y_T_AB_FORCE_FULL_ROTATE_EVERY_FRAME  0
#endif

/* 1 = let LTDC hardware reload CFBAR on vertical blanking (VBR) instead of
 * arming a line interrupt and issuing IMR from LTDC_IRQHandler. Diagnostic for
 * CFBAR reload timing / active framebuffer bookkeeping. */
#ifndef Y_T_AB_USE_LTDC_VBR_RELOAD
/* 宏定义: Y_T_AB_USE_LTDC_VBR_RELOAD - Y_T_AB_USE_LTDC_VBR_RELOAD 的宏定义。 */
#define Y_T_AB_USE_LTDC_VBR_RELOAD  0
#endif

/* 1 = after preparing the target physical framebuffer, copy it into the
 * other physical framebuffer too. Diagnostic for physical ping-pong content
 * mismatch / active-buffer bookkeeping. */
#ifndef Y_T_AB_MIRROR_PHYSICAL_FRAMEBUFFERS
/* 宏定义: Y_T_AB_MIRROR_PHYSICAL_FRAMEBUFFERS - Y_T_AB_MIRROR_PHYSICAL_FRAMEBUFFERS 的宏定义。 */
#define Y_T_AB_MIRROR_PHYSICAL_FRAMEBUFFERS  0
#endif

/* 1 = render and commit the first scope frame, then keep the scheduler alive
 * without further scope GUI/DMA2D/rotate/LTDC commits. Diagnostic for separating
 * dynamic render/cache/bus activity from static LTDC scanning. */
#ifndef Y_T_AB_FREEZE_SCOPE_AFTER_FIRST_FRAME
/* 宏定义: Y_T_AB_FREEZE_SCOPE_AFTER_FIRST_FRAME - Y_T_AB_FREEZE_SCOPE_AFTER_FIRST_FRAME 的宏定义。 */
#define Y_T_AB_FREEZE_SCOPE_AFTER_FIRST_FRAME  0
#endif

/* 1 = while scope rendering is frozen, periodically sample the LTDC-scanned
 * physical framebuffer and print whether its signature changes. */
#ifndef Y_T_AB_FREEZE_MONITOR_PHYSICAL_FRAME
/* 宏定义: Y_T_AB_FREEZE_MONITOR_PHYSICAL_FRAME - Y_T_AB_FREEZE_MONITOR_PHYSICAL_FRAME 的宏定义。 */
#define Y_T_AB_FREEZE_MONITOR_PHYSICAL_FRAME  0
#endif

/* 1 = skip the PA4/DAC/TIM5/DMA1 waveform-generator task.
 * Diagnostic for separating static LTDC scan flicker from Wavegen DMA/IO/power
 * activity. Restore to 0 if the A/B has no effect. */
#ifndef Y_T_AB_DISABLE_WAVEGEN_TASK
/* 宏定义: Y_T_AB_DISABLE_WAVEGEN_TASK - Y_T_AB_DISABLE_WAVEGEN_TASK 的宏定义。 */
#define Y_T_AB_DISABLE_WAVEGEN_TASK  0
#endif

/* 1 = compile out MainTask's periodic stack/LTDC/VRAM serial log.
 * Diagnostic for testing whether the 2 s printf/metric/sentinel burst starves
 * LTDC SDRAM reads while the scope frame is otherwise frozen. */
#ifndef Y_T_AB_DISABLE_MAINTASK_PERIODIC_LOG
/* 宏定义: Y_T_AB_DISABLE_MAINTASK_PERIODIC_LOG - Y_T_AB_DISABLE_MAINTASK_PERIODIC_LOG 的宏定义。 */
#define Y_T_AB_DISABLE_MAINTASK_PERIODIC_LOG  0
#endif

/* 1 = skip the DSO engine/acquisition task while keeping the UI task alive.
 * Diagnostic for separating static LTDC scan flicker from ADC/DMA/acquisition
 * background activity. Restore to 0 if the A/B has no effect. */
#ifndef Y_T_AB_DISABLE_DSO_ENGINE_TASK
/* 宏定义: Y_T_AB_DISABLE_DSO_ENGINE_TASK - Y_T_AB_DISABLE_DSO_ENGINE_TASK 的宏定义。 */
#define Y_T_AB_DISABLE_DSO_ENGINE_TASK  0
#endif

/* 1 = 不创建 MainTask（emWin GUI 任务）。
 * GUI_Init 卡死（云桥/FMC 冲突，LCD 空白）：MainTask 栈占 14336 words(≈57KB)，
 * 会挤爆 FreeRTOS 堆致 MX_LWIP_Init 的 tcpip 线程分配失败（link 灯不亮）。
 * PC 开关面板 GUI 不依赖 LCD 侧任务；故保持 1。 */
#ifndef Y_T_AB_DISABLE_GUI_TASK
#define Y_T_AB_DISABLE_GUI_TASK  1
#endif

/* 1 = keep DSOEng alive but prevent ADC/DMA acquisition from starting.
 * Diagnostic for separating DSOEng init/service from ADC DMA bus activity. */
#ifndef Y_T_AB_DISABLE_ACQ_START
/* 宏定义: Y_T_AB_DISABLE_ACQ_START - Y_T_AB_DISABLE_ACQ_START 的宏定义。 */
#define Y_T_AB_DISABLE_ACQ_START  0
#endif

/* 1 = skip D-Cache invalidation in ADC DMA half/full callbacks.
 * Diagnostic for separating ADC DMA traffic from callback cache-maintenance
 * bursts. Restore to 0 if the A/B has no effect. */
#ifndef Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE
/* 宏定义: Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE - Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE 的宏定义。 */
#define Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE  0
#endif

/* 1 = after DSO_Acq_Service(), skip DSO frame copy/process/publish loop body.
 * Diagnostic for separating raw ADC DMA activity from DSOEng CPU reads,
 * history writes, analysis, and UI publication. */
#ifndef Y_T_AB_DSO_ENGINE_ACQ_SERVICE_ONLY
/* 宏定义: Y_T_AB_DSO_ENGINE_ACQ_SERVICE_ONLY - Y_T_AB_DSO_ENGINE_ACQ_SERVICE_ONLY 的宏定义。 */
#define Y_T_AB_DSO_ENGINE_ACQ_SERVICE_ONLY  0
#endif

/* 1 = after starting ADC DMA streams, disable their NVIC IRQs while leaving DMA
 * circular transfers running. Diagnostic for separating raw DMA bus traffic
 * from DMA IRQ/HAL callback CPU load. */
#ifndef Y_T_AB_DISABLE_ACQ_DMA_IRQ_AFTER_START
/* 宏定义: Y_T_AB_DISABLE_ACQ_DMA_IRQ_AFTER_START - Y_T_AB_DISABLE_ACQ_DMA_IRQ_AFTER_START 的宏定义。 */
#define Y_T_AB_DISABLE_ACQ_DMA_IRQ_AFTER_START  0
#endif

/* 1 = keep ADC DMA NVIC IRQ enabled, but disable HT/TC interrupt sources after
 * starting circular DMA. Error interrupt sources stay enabled. Diagnostic for
 * moving acquisition progress tracking from DMA callbacks to task polling. */
#ifndef Y_T_AB_DISABLE_ACQ_DMA_HT_TC_IRQ_SOURCE
/* 宏定义: Y_T_AB_DISABLE_ACQ_DMA_HT_TC_IRQ_SOURCE - Y_T_AB_DISABLE_ACQ_DMA_HT_TC_IRQ_SOURCE 的宏定义。 */
#define Y_T_AB_DISABLE_ACQ_DMA_HT_TC_IRQ_SOURCE  1
#endif

#if (Y_T_AB_DISABLE_DCACHE != 0) && (Y_T_AB_CLEAN_DCACHE_BEFORE_SCOPE_FRAME != 0)
#warning "Y_T_AB: disable D-Cache and clean-before-GUI are redundant; use one for A/B."
#endif

#endif /* Y_T_STRESS_AB_H */
