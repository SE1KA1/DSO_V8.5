/**
 * @file ui_trigger_ui.h
 * @brief 触发页 UI 镜像：源/边沿/模式/电平 preset 与 DSO pending 对接。
 */

#ifndef UI_TRIGGER_UI_H
/* 宏定义: UI_TRIGGER_UI_H - 头文件保护宏。 */
#define UI_TRIGGER_UI_H

/**
 * 触发页「下拉」选项的 UI 层单一数据源。
 *
 * DSO 完善后建议用法（无需改 dso_acquire_control / engine 结构即可对接）：
 *  - 从 UI 读用户选择：`UI_TrigUi_Get*()`，在 DSO 内据此配置硬件/策略。
 *  - 硬件或协议栈回读与 UI 对齐：`UI_TrigUi_Set*FromDso(..., request_redraw!=0)` 只写 UI 镜像，
 *    不向 DSO 寄存器回写，避免与引擎状态机打架。
 *  - 需要把当前 UI 里已支持的项一次性写入 AcquireCtrl pending：`UI_TrigUi_PushAcquireBackedFields()`
 *    （建议在已与引擎协调的上下文调用，例如 GUI 任务或引擎安全点）。
 *
 * 线程：状态用 FreeRTOS critical 短临界区保护；DSO 任务与 GUI 并发时仍建议由 GUI
 * 聚合后再调 Set*FromDso，或保证调用顺序符合你们引擎约定。
 */

#include "dso/dso_acquire_control.h"
#include "dso/dso_ui_bridge.h"
#include <stdint.h>

typedef enum {
    UI_TRIG_SRC_CH1 = 0,
    UI_TRIG_SRC_CH2
} UI_TrigUiSource_t;

typedef enum {
    UI_TRIG_SLOPE_RISE = 0,
    UI_TRIG_SLOPE_FALL,
    UI_TRIG_SLOPE_BOTH
} UI_TrigUiSlope_t;

typedef enum {
    UI_TRIG_LVL_P125 = 0,
    UI_TRIG_LVL_000,
    UI_TRIG_LVL_OFF
} UI_TrigUiLevelPreset_t;

/* --- 页面生命周期 / 绘制 / EC（原有） -------------------------------- */
void UI_Trigger_OnScreenShown(void);
/*
*********************************************************************************************************
*   函 数 名: UI_Trigger_Ec1Detent
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Trigger_Ec1Detent(int dir);
/*
*********************************************************************************************************
*   函 数 名: UI_Trigger_Ec2Detent
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Trigger_Ec2Detent(int dir);
/*
*********************************************************************************************************
*   函 数 名: UI_Trigger_DrawContent
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Trigger_DrawContent(void);

/* --- 读模型：供 DSO / 其它模块查询当前 UI 选项 ------------------------ */
UI_TrigUiSource_t        UI_TrigUi_GetSource(void);
/*
*********************************************************************************************************
*   函 数 名: UI_TrigUi_GetSlope
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
UI_TrigUiSlope_t         UI_TrigUi_GetSlope(void);
/*
*********************************************************************************************************
*   函 数 名: UI_TrigUi_GetTriggerMode
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_TriggerMode          UI_TrigUi_GetTriggerMode(void);
/*
*********************************************************************************************************
*   函 数 名: UI_TrigUi_GetLevelPreset
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
UI_TrigUiLevelPreset_t   UI_TrigUi_GetLevelPreset(void);
/** 当前高亮行 0..6（含 CH1/CH2 probe），仅展示用 */
int                      UI_TrigUi_GetFocusRow(void);

/**
 * 将 UI 中与 AcquireCtrl 相关的项写入 pending（沿、模式、电平档位）。
 * 不修改触发源/类型；触发源仍走 bridge，类型固定为 Edge。
 */
void UI_TrigUi_PushAcquireBackedFields(void);

/* --- DSO / 后端 -> UI：仅更新镜像，默认不向 AcquireCtrl 回写 ---------- */
void UI_TrigUi_SetSourceFromDso(UI_TrigUiSource_t v, int request_redraw);
/*
*********************************************************************************************************
*   函 数 名: UI_TrigUi_SetSlopeFromDso
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_TrigUi_SetSlopeFromDso(UI_TrigUiSlope_t v, int request_redraw);
/*
*********************************************************************************************************
*   函 数 名: UI_TrigUi_SetModeFromDso
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_TrigUi_SetModeFromDso(DSO_TriggerMode m, int request_redraw);
/*
*********************************************************************************************************
*   函 数 名: UI_TrigUi_SetLevelPresetFromDso
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_TrigUi_SetLevelPresetFromDso(UI_TrigUiLevelPreset_t p, int request_redraw);
/** 按已启用的手动触发电平粗映射到两档预设（>=1.05 -> +1.25；其余 -> 0.00）。 */
void UI_TrigUi_SetLevelPresetFromDsoVolts(float level_v, int request_redraw);
/*
*********************************************************************************************************
*   函 数 名: UI_Trigger_SyncFrameState
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Trigger_SyncFrameState(const DSO_UI_Frame *frame);

/* --- UI 旋钮侧 weak：应用层接路由 -------------------------------------- */
void UI_TriggerCtrl_OnSourceChanged(UI_TrigUiSource_t s);

#endif /* UI_TRIGGER_UI_H */
