/**
 * @file ui_trigger_ui.c
 * @brief 触发设置页的 UI 镜像与绘制；选项通过 UI_TrigUi_PushAcquireBackedFields 写入 DSO pending。
 *
 * 状态在 FreeRTOS critical 内读写，供 DSO_Engine_Task 与 MainTask 并发访问。
 * 边沿类型固定为 Edge（产品约束）；源/边沿/模式/电平预设与 dso_acquire_control 对齐。
 */
#include "ui_trigger_ui.h"
#include "ui_theme.h"
#include "ui_nav.h"
#include "dso/dso_acquire_control.h"
#include "dso/dso_acq.h"
#include "dso/dso_vertical.h"
#include "dso/dso_ui_bridge.h"
#include "GUI.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

static UI_TrigUiSource_t       s_src      = UI_TRIG_SRC_CH1;
static UI_TrigUiSlope_t        s_slope_ui = UI_TRIG_SLOPE_RISE;
static uint8_t                 s_mode; /* DSO_TriggerMode */
static UI_TrigUiLevelPreset_t  s_lvl      = UI_TRIG_LVL_P125;
static int                     s_focus_row;

/*
*********************************************************************************************************
*   函 数 名: UI_TriggerCtrl_OnSourceChanged
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
__weak void UI_TriggerCtrl_OnSourceChanged(UI_TrigUiSource_t s)
{
    (void)s;
}

/* ── 临界区：DSO 任务与 GUI 并发读写镜像 -------------------------------- */

UI_TrigUiSource_t UI_TrigUi_GetSource(void)
{
    UI_TrigUiSource_t v;
    taskENTER_CRITICAL();
    v = s_src;
    taskEXIT_CRITICAL();
    return v;
}

/*
*********************************************************************************************************
*   函 数 名: UI_TrigUi_GetSlope
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
UI_TrigUiSlope_t UI_TrigUi_GetSlope(void)
{
    UI_TrigUiSlope_t v;
    taskENTER_CRITICAL();
    v = s_slope_ui;
    taskEXIT_CRITICAL();
    return v;
}

/*
*********************************************************************************************************
*   函 数 名: UI_TrigUi_GetTriggerMode
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_TriggerMode UI_TrigUi_GetTriggerMode(void)
{
    uint8_t m;
    taskENTER_CRITICAL();
    m = s_mode;
    taskEXIT_CRITICAL();
    return (DSO_TriggerMode)m;
}

/*
*********************************************************************************************************
*   函 数 名: UI_TrigUi_GetLevelPreset
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
UI_TrigUiLevelPreset_t UI_TrigUi_GetLevelPreset(void)
{
    UI_TrigUiLevelPreset_t v;
    taskENTER_CRITICAL();
    v = s_lvl;
    taskEXIT_CRITICAL();
    return v;
}

/*
*********************************************************************************************************
*   函 数 名: UI_TrigUi_GetFocusRow
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
int UI_TrigUi_GetFocusRow(void)
{
    int r;
    taskENTER_CRITICAL();
    r = s_focus_row;
    taskEXIT_CRITICAL();
    return r;
}

/*
*********************************************************************************************************
*   函 数 名: UI_TrigUi_SetSourceFromDso
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_TrigUi_SetSourceFromDso(UI_TrigUiSource_t v, int request_redraw)
{
    if (v != UI_TRIG_SRC_CH2) {
        v = UI_TRIG_SRC_CH1;
    }
    taskENTER_CRITICAL();
    s_src = v;
    taskEXIT_CRITICAL();
    if (request_redraw != 0) {
        UI_Nav_MarkDirty();
    }
}

/*
*********************************************************************************************************
*   函 数 名: UI_TrigUi_SetSlopeFromDso
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_TrigUi_SetSlopeFromDso(UI_TrigUiSlope_t v, int request_redraw)
{
    if ((unsigned)v > (unsigned)UI_TRIG_SLOPE_BOTH) {
        return;
    }
    taskENTER_CRITICAL();
    s_slope_ui = v;
    taskEXIT_CRITICAL();
    if (request_redraw != 0) {
        UI_Nav_MarkDirty();
    }
}

/*
*********************************************************************************************************
*   函 数 名: UI_TrigUi_SetModeFromDso
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_TrigUi_SetModeFromDso(DSO_TriggerMode m, int request_redraw)
{
    if ((unsigned)m > (unsigned)DSO_UI_TRIG_SINGLE) {
        return;
    }
    taskENTER_CRITICAL();
    s_mode = (uint8_t)m;
    taskEXIT_CRITICAL();
    if (request_redraw != 0) {
        UI_Nav_MarkDirty();
    }
}

/*
*********************************************************************************************************
*   函 数 名: UI_TrigUi_SetLevelPresetFromDso
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_TrigUi_SetLevelPresetFromDso(UI_TrigUiLevelPreset_t p, int request_redraw)
{
    if ((unsigned)p > (unsigned)UI_TRIG_LVL_OFF) {
        return;
    }
    taskENTER_CRITICAL();
    s_lvl = p;
    taskEXIT_CRITICAL();
    if (request_redraw != 0) {
        UI_Nav_MarkDirty();
    }
}

/*
*********************************************************************************************************
*   函 数 名: UI_TrigUi_SetLevelPresetFromDsoVolts
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_TrigUi_SetLevelPresetFromDsoVolts(float level_v, int request_redraw)
{
    UI_TrigUiLevelPreset_t p;

    if (level_v >= 1.05f) {
        p = UI_TRIG_LVL_P125;
    } else {
        p = UI_TRIG_LVL_000;
    }
    UI_TrigUi_SetLevelPresetFromDso(p, request_redraw);
}

/*
*********************************************************************************************************
*   函 数 名: apply_slope_to_dso_with
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void apply_slope_to_dso_with(UI_TrigUiSlope_t sl)
{
    if (sl == UI_TRIG_SLOPE_FALL) {
        DSO_AcquireCtrl_SetTriggerSlope(DSO_TRIGGER_SLOPE_FALLING);
    } else if (sl == UI_TRIG_SLOPE_BOTH) {
        DSO_AcquireCtrl_SetTriggerSlope(DSO_TRIGGER_SLOPE_BOTH);
    } else {
        DSO_AcquireCtrl_SetTriggerSlope(DSO_TRIGGER_SLOPE_RISING);
    }
}

/*
*********************************************************************************************************
*   函 数 名: apply_mode_to_dso_with
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void apply_mode_to_dso_with(uint8_t mode_u8)
{
    DSO_TriggerMode m = (DSO_TriggerMode)mode_u8;

    DSO_AcquireCtrl_SetTriggerMode(m);
    if (m == DSO_UI_TRIG_SINGLE) {
        DSO_AcquireCtrl_RearmSingle();
        DSO_AcquireCtrl_SetRunRequested(true);
    }
}

/*
*********************************************************************************************************
*   函 数 名: apply_level_to_dso_with
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void apply_level_to_dso_with(UI_TrigUiLevelPreset_t lv)
{
    switch (lv) {
    case UI_TRIG_LVL_P125:
        DSO_AcquireCtrl_SetTriggerLevelEnabled(true);
        DSO_AcquireCtrl_SetTriggerLevelVolts(1.25f);
        break;
    case UI_TRIG_LVL_000:
        DSO_AcquireCtrl_SetTriggerLevelEnabled(true);
        DSO_AcquireCtrl_SetTriggerLevelVolts(0.0f);
        break;
    case UI_TRIG_LVL_OFF:
        DSO_AcquireCtrl_SetTriggerLevelEnabled(false);
        break;
    default:
        break;
    }
}

/*
*********************************************************************************************************
*   函 数 名: UI_TrigUi_PushAcquireBackedFields
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_TrigUi_PushAcquireBackedFields(void)
{
    UI_TrigUiSlope_t       sl;
    uint8_t                mode_u8;
    UI_TrigUiLevelPreset_t lv;

    taskENTER_CRITICAL();
    sl      = s_slope_ui;
    mode_u8 = s_mode;
    lv      = s_lvl;
    taskEXIT_CRITICAL();

    apply_slope_to_dso_with(sl);
    apply_mode_to_dso_with(mode_u8);
    apply_level_to_dso_with(lv);
}

/*
*********************************************************************************************************
*   函 数 名: sync_from_dso
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void sync_from_dso(void)
{
    DSO_AcquireCtrlInfo acq;

    DSO_AcquireCtrl_GetInfo(&acq);
    if (acq.trigger_source == DSO_UI_TRIG_SRC_CH2) {
        UI_TrigUi_SetSourceFromDso(UI_TRIG_SRC_CH2, 0);
    } else {
        UI_TrigUi_SetSourceFromDso(UI_TRIG_SRC_CH1, 0);
    }
    UI_TrigUi_SetModeFromDso(acq.trigger_mode, 0);
    if (acq.trigger_slope == DSO_TRIGGER_SLOPE_FALLING) {
        UI_TrigUi_SetSlopeFromDso(UI_TRIG_SLOPE_FALL, 0);
    } else if (acq.trigger_slope == DSO_TRIGGER_SLOPE_BOTH) {
        UI_TrigUi_SetSlopeFromDso(UI_TRIG_SLOPE_BOTH, 0);
    } else {
        UI_TrigUi_SetSlopeFromDso(UI_TRIG_SLOPE_RISE, 0);
    }
    if (acq.trigger_level_enabled) {
        UI_TrigUi_SetLevelPresetFromDsoVolts(acq.trigger_level_v, 0);
    } else {
        UI_TrigUi_SetLevelPresetFromDso(UI_TRIG_LVL_OFF, 0);
    }
}

/*
*********************************************************************************************************
*   函 数 名: UI_Trigger_OnScreenShown
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Trigger_OnScreenShown(void)
{
    taskENTER_CRITICAL();
    s_focus_row = 0;
    taskEXIT_CRITICAL();
    sync_from_dso();
}

/*
*********************************************************************************************************
*   函 数 名: UI_Trigger_SyncFrameState
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Trigger_SyncFrameState(const DSO_UI_Frame *frame)
{
    UI_TrigUiSource_t next;
    UI_TrigUiLevelPreset_t next_level;
    int changed = 0;

    if (frame == NULL) {
        return;
    }

    next = (frame->trig_source == DSO_UI_TRIG_SRC_CH2)
         ? UI_TRIG_SRC_CH2
         : UI_TRIG_SRC_CH1;

    if (UI_TrigUi_GetSource() != next) {
        UI_TrigUi_SetSourceFromDso(next, 0);
        changed = 1;
    }

    if (frame->trig_level_enabled != 0u) {
        next_level = (frame->trig_level_v >= 1.05f) ? UI_TRIG_LVL_P125 : UI_TRIG_LVL_000;
    } else {
        next_level = UI_TRIG_LVL_OFF;
    }
    if (UI_TrigUi_GetLevelPreset() != next_level) {
        UI_TrigUi_SetLevelPresetFromDso(next_level, 0);
        changed = 1;
    }

    if (changed && (UI_Nav_GetScreen() == UI_SCR_TRIGGER)) {
        UI_Nav_MarkDirty();
    }
}

/*
*********************************************************************************************************
*   函 数 名: normalize_focus_row
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static int normalize_focus_row(int row, int dir)
{
    if (row < 0) {
        row = 6;
    } else if (row > 6) {
        row = 0;
    }

    if (row == 1) {
        row = (dir < 0) ? 0 : 2;
    }

    return row;
}

/*
*********************************************************************************************************
*   函 数 名: UI_Trigger_Ec1Detent
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Trigger_Ec1Detent(int dir)
{
    taskENTER_CRITICAL();
    if (dir > 0) {
        s_focus_row = normalize_focus_row(s_focus_row + 1, dir);
    } else if (dir < 0) {
        s_focus_row = normalize_focus_row(s_focus_row - 1, dir);
    }
    taskEXIT_CRITICAL();
    UI_Nav_MarkDirty();
}

/*
*********************************************************************************************************
*   函 数 名: cycle_src_user
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void cycle_src_user(int dir)
{
    UI_TrigUiSource_t supported[2];
    uint32_t supported_count = 0u;
    uint32_t idx = 0u;
    UI_TrigUiSource_t cur;
    UI_TrigUiSource_t next;

    supported[supported_count++] = UI_TRIG_SRC_CH1;
    if (DSO_Acq_IsCh2Available()) {
        supported[supported_count++] = UI_TRIG_SRC_CH2;
    }

    taskENTER_CRITICAL();
    cur = s_src;
    taskEXIT_CRITICAL();

    while ((idx < supported_count) && (supported[idx] != cur)) {
        idx++;
    }
    if (idx >= supported_count) {
        idx = 0u;
    }

    if (supported_count > 1u) {
        if (dir < 0) {
            idx = (idx == 0u) ? (supported_count - 1u) : (idx - 1u);
        } else {
            idx = (idx + 1u) % supported_count;
        }
    }

    next = supported[idx];
    taskENTER_CRITICAL();
    s_src = next;
    taskEXIT_CRITICAL();
    UI_TriggerCtrl_OnSourceChanged(next);
}

/*
*********************************************************************************************************
*   函 数 名: cycle_slope_user
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void cycle_slope_user(void)
{
    UI_TrigUiSlope_t next;
    taskENTER_CRITICAL();
    next = (UI_TrigUiSlope_t)(((int)s_slope_ui + 1) % 3);
    s_slope_ui = next;
    taskEXIT_CRITICAL();
    apply_slope_to_dso_with(next);
}

/*
*********************************************************************************************************
*   函 数 名: cycle_mode_user
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void cycle_mode_user(void)
{
    uint8_t next;
    taskENTER_CRITICAL();
    next = (uint8_t)(((int)s_mode + 1) % 3);
    s_mode = next;
    taskEXIT_CRITICAL();
    apply_mode_to_dso_with(next);
}

/*
*********************************************************************************************************
*   函 数 名: cycle_level_user
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void cycle_level_user(void)
{
    UI_TrigUiLevelPreset_t next;
    taskENTER_CRITICAL();
    next = (UI_TrigUiLevelPreset_t)(((int)s_lvl + 1) % 3);
    s_lvl = next;
    taskEXIT_CRITICAL();
    apply_level_to_dso_with(next);
}

/*
*********************************************************************************************************
*   函 数 名: cycle_probe_user
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void cycle_probe_user(DSO_ChannelId ch)
{
    DSO_VerticalInfo vi;
    DSO_ProbeAtten next;

    DSO_Vertical_GetInfoForChannel(ch, &vi);
    next = (vi.probe == DSO_PROBE_X10) ? DSO_PROBE_X1 : DSO_PROBE_X10;
    DSO_Vertical_SetProbeForChannel(ch, next);
}

/*
*********************************************************************************************************
*   函 数 名: UI_Trigger_Ec2Detent
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Trigger_Ec2Detent(int dir)
{
    int row;
    taskENTER_CRITICAL();
    row = s_focus_row;
    taskEXIT_CRITICAL();

    switch (row) {
    case 0:
        cycle_src_user(dir);
        break;
    case 2:
        cycle_slope_user();
        break;
    case 3:
        cycle_mode_user();
        break;
    case 4:
        cycle_level_user();
        break;
    case 5:
        cycle_probe_user(DSO_CHANNEL_1);
        break;
    case 6:
        cycle_probe_user(DSO_CHANNEL_2);
        break;
    default:
        break;
    }
    UI_Nav_MarkDirty();
}

/*
*********************************************************************************************************
*   函 数 名: fmt_src
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fmt_src(char *b, unsigned n)
{
    static const char *t[] = { "CH1", "CH2" };
    UI_TrigUiSource_t src = UI_TrigUi_GetSource();

    if (src != UI_TRIG_SRC_CH2) {
        src = UI_TRIG_SRC_CH1;
    }
    (void)snprintf(b, n, "%s  v", t[(int)src]);
}

/*
*********************************************************************************************************
*   函 数 名: fmt_type
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fmt_type(char *b, unsigned n)
{
    (void)snprintf(b, n, "Edge");
}

/*
*********************************************************************************************************
*   函 数 名: fmt_slope
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fmt_slope(char *b, unsigned n)
{
    static const char *t[] = { "Rising ^", "Falling v", "Both ^v" };
    (void)snprintf(b, n, "%s  v", t[(int)UI_TrigUi_GetSlope()]);
}

/*
*********************************************************************************************************
*   函 数 名: fmt_mode
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fmt_mode(char *b, unsigned n)
{
    static const char *t[] = { "Auto", "Normal", "Single" };
    (void)snprintf(b, n, "%s  v", t[(int)UI_TrigUi_GetTriggerMode() % 3]);
}

/*
*********************************************************************************************************
*   函 数 名: fmt_level
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fmt_level(char *b, unsigned n)
{
    static const char *t[] = { "+1.25V", "0.00V", "OFF" };
    (void)snprintf(b, n, "%s  v", t[(int)UI_TrigUi_GetLevelPreset()]);
}

/*
*********************************************************************************************************
*   函 数 名: fmt_probe_ch1
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fmt_probe_ch1(char *b, unsigned n)
{
    DSO_VerticalInfo vi;

    DSO_Vertical_GetInfoForChannel(DSO_CHANNEL_1, &vi);
    (void)snprintf(b, n, "%s  v", (vi.probe == DSO_PROBE_X10) ? "10x" : "1x");
}

/*
*********************************************************************************************************
*   函 数 名: fmt_probe_ch2
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fmt_probe_ch2(char *b, unsigned n)
{
    DSO_VerticalInfo vi;

    DSO_Vertical_GetInfoForChannel(DSO_CHANNEL_2, &vi);
    (void)snprintf(b, n, "%s  v", (vi.probe == DSO_PROBE_X10) ? "10x" : "1x");
}

typedef void (*fmt_fn)(char *, unsigned);

/*
*********************************************************************************************************
*   函 数 名: draw_trig_row
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void draw_trig_row(int y, int row_idx, const char *label, fmt_fn fmt, int read_only)
{
    char val[40];
    int  w;
    int  vx   = 300;
    int  pad  = 4;
    int  fh;
    int  focus;

    fmt(val, (unsigned)sizeof(val));
    focus = UI_TrigUi_GetFocusRow();
    GUI_SetFont(&GUI_Font24_ASCII);
    GUI_SetColor(UI_CLR_TEXT_MID);
    GUI_SetTextMode(GUI_TM_TRANS);
    GUI_DispStringAt(label, UI_MARGIN_X + 4, y);

    fh = GUI_GetFontSizeY();
    w  = GUI_GetStringDistX(val);
    if (!read_only && (row_idx == focus)) {
        GUI_SetColor(UI_CLR_SEL);
        GUI_FillRoundedRect(vx - pad, y - 2, vx + w + pad, y + fh + 2, 4);
        GUI_SetColor(UI_CLR_ACCENT);
        GUI_DrawRoundedRect(vx - pad, y - 2, vx + w + pad, y + fh + 2, 4);
        GUI_SetColor(UI_CLR_TEXT_HI);
    } else if (read_only) {
        GUI_SetColor(UI_CLR_TEXT_MID);
    } else {
        GUI_SetColor(UI_CLR_TEXT_HI);
    }
    GUI_DispStringAt(val, vx, y);
}

/*
*********************************************************************************************************
*   函 数 名: UI_Trigger_DrawContent
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Trigger_DrawContent(void)
{
    int y = UI_CONTENT_TOP + 8;

    draw_trig_row(y, 0, "Trig source:", fmt_src, 0);
    y += 44;
    draw_trig_row(y, 1, "Trig type:", fmt_type, 1);
    y += 44;
    draw_trig_row(y, 2, "Trig edge:", fmt_slope, 0);
    y += 44;
    draw_trig_row(y, 3, "Trig mode:", fmt_mode, 0);
    y += 44;
    draw_trig_row(y, 4, "Trig level:", fmt_level, 0);
    y += 44;
    draw_trig_row(y, 5, "CH1 probe:", fmt_probe_ch1, 0);
    y += 44;
    draw_trig_row(y, 6, "CH2 probe:", fmt_probe_ch2, 0);
}
