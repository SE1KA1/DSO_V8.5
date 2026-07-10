/**
 * @file roll_history_ui.h
 * @brief Roll 模式 UI 侧辅助：时基切换后的环缓冲预填与最后一帧快照。
 *
 * 环数据由 DSO_Engine_Task 增量 feed（RollHistory_ServiceFeedOnce 为兼容空实现）。
 * MainTask 每帧在 UpdateWaveformData 之后调用 ApplyRollCoherentPrefill，避免 Roll 步进后空屏。
 */
#ifndef ROLL_HISTORY_UI_H
/* 宏定义: ROLL_HISTORY_UI_H - 头文件保护宏。 */
#define ROLL_HISTORY_UI_H

#include "dso/dso_ui_bridge.h"

/*
*********************************************************************************************************
*   函 数 名: RollHistory_Init
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void RollHistory_Init(void);

/**
 * 兼容旧 API：Roll ring feed 已迁至 DSO_Engine_Task，此处不再执行 ingest。
 */
void RollHistory_ServiceFeedOnce(void);

/**
 * Roll 时基变更后，从 ring 重建整屏 y_coords 写入 frame（引擎 roll 缓冲不变）。
 * frame->roll_ring_coherent_engine 或 roll_resync_in_progress 置位时跳过。
 */
void RollHistory_ApplyRollCoherentPrefill(DSO_UI_Frame *frame);

/**
 * 绘制完成后保存 Roll 模式最后一帧 y_coords，供稀疏/失败帧 hold 使用。
 * MainTask 在 scope 帧末尾、frame 有效时调用一次。
 */
void RollHistory_SnapshotLastDisplayed(const DSO_UI_Frame *frame);

#endif /* ROLL_HISTORY_UI_H */
