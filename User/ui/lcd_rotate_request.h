/**
 * @file lcd_rotate_request.h
 * @brief RenderLatch 管线 API：Prepare/Commit、dirty 标记、ManualRotate。
 */

/*
 * 与 LCDConf_Lin_Template.c 中 RenderLatch 区域版本配合：
 * 应用侧重绘 Header/Body 后标记 dirty，下一帧 PreparePhysical 只把过期区域
 * 同步到即将提交的物理后台缓冲。
 *
 * ── Render Latch（示波器帧管线顺序，勿打乱）────────────────────────────
 * 1) GUI_MULTIBUF_Begin … GUI_MULTIBUF_End：写入逻辑横屏缓冲（IndexDst）。
 * 2) PlotBuf：DMA2D_PlotBuf_EndFrame → DMA2D_PlotBuf_PreDisplaySync。
 * 3) LCDConf_RenderLatch_PreparePhysical：逻辑(+Plot)→物理「后台」缓冲（非 LTDC 当前扫描块）。
 * 4) （可选）taskYIELD：LCDCONF_RENDER_LATCH_YIELD_BEFORE_COMMIT=1 时在 MainTask 两阶段之间让出。
 * 5) LCDConf_RenderLatch_Commit：写 LTDC Shadow CFBAR + LCDConf_WaitVsyncReload（VFP 锁存）。
 * 示波器帧须在 MainTask_Scope_RenderFrame 开头调用 LCDConf_RenderLatch_MarkScopeFrameFuStart()，
 * 使 fu 统计覆盖「整帧 UI 绘制 + Prepare + Commit」；否则 avg_fu_delta 仅覆盖 Prepare→Commit 窄窗，易恒为 0。
 * ManualRotateToPhysical() ≡ Prepare + Commit（无 yield）。
 * NUM_BUFFERS>1 且走 _CustomCopyBuffer 的路径另行完整旋转，不经上述拆分。
 *
 * B2（独立任务队列、VSYNC 后再起大块 DMA2D）：未实现；仅当可接受额外显示延迟时再评估。
 */
#ifndef LCD_ROTATE_REQUEST_H
/* 宏定义: LCD_ROTATE_REQUEST_H - 头文件保护宏。 */
#define LCD_ROTATE_REQUEST_H

#include <stdint.h>

#ifndef USE_ROTATE_HEADER_STATIC_SKIP
/* 宏定义: USE_ROTATE_HEADER_STATIC_SKIP - 编译期开关，用于启用或关闭对应功能。 */
#define USE_ROTATE_HEADER_STATIC_SKIP  1
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t use_plotbuf;
} LCD_RenderLatchFramePlan;

/** LTDC 行中断未及时到达时 WaitVsync 超时恢复次数（底板噪声时可能偶发递增） */
extern volatile uint32_t g_lcdconf_vsync_timeout_cnt;
/** LTDC FIFO underrun 次数；持续递增时说明显存读带宽/仲裁余量不足。 */
extern volatile uint32_t g_lcdconf_ltdc_fu_cnt;

/** 示波器帧开头调用：锁定本帧 fu 统计起点（至 Commit 结束），避免 avg_fu_delta 只见 Prepare→Commit 窄窗。 */
void LCDConf_RenderLatch_MarkScopeFrameFuStart(void);
/** Render Latch 阶段一：按 frame plan 将逻辑帧/PlotBuf 同步到物理后台缓冲。 */
void LCDConf_RenderLatch_PreparePhysical(const LCD_RenderLatchFramePlan *plan);
/** Render Latch 阶段二：CFBAR + VSYNC 等待；内部累计 DWT 周期与 fu 采样供周期日志取平均。 */
void LCDConf_RenderLatch_Commit(void);
/**
 * 取出自上次调用以来的 Render Latch 帧计数与各均值（CPU 周期数、fu 计数差），并清零累计器。
 * out_sum_fu_delta：该间隔内各帧 fu_d 之和（稀疏 underrun 时 avg 可能为 0 但 sum 可见）。
 */
void LCDConf_RenderLatchMetrics_FlushForLog(uint32_t *out_frames,
                                            uint32_t *out_avg_prep_cy,
                                            uint32_t *out_avg_commit_cy,
                                            uint32_t *out_avg_fu_delta,
                                            uint64_t *out_sum_fu_delta);

void LCDConf_MarkHeaderDirty(void);
void LCDConf_MarkBodyDirty(void);
void LCDConf_MarkFullDirty(void);

/* 兼容旧命名：等价于 LCDConf_MarkHeaderDirty()。 */
void LCDConf_RequestHeaderRotate(void);

/** 兼容旧命名：等价于 LCDConf_MarkBodyDirty()。 */
void LCDConf_RequestBodyRotate(void);

/**
 * 兼容旧命名：等价于 LCDConf_MarkFullDirty()，并保留全屏重刷路径的入口。
 */
void LCDConf_RequestFullScreenRotate(void);

void ManualRotateToPhysical(void);

#ifdef __cplusplus
}
#endif

#endif /* LCD_ROTATE_REQUEST_H */
