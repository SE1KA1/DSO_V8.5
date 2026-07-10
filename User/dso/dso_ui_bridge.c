/**
 * @file dso_ui_bridge.c
 * @brief UI bridge 辅助函数与帧元数据默认值。
 */

/*
 * dso_ui_bridge.c — Narrow interface between UI and DSO core.
 *
 * Provides UI-friendly types, constants, and helper functions.
 * Frame double-buffer and transfer live in UI_WaveformCtrl.c.
 */

#include "dso/dso_ui_bridge.h"
#include "dso/dso_vertical.h"
#include <stddef.h>

/* ── Vertical info ──────────────────────────────────────────────── */

DSO_UI_VerticalInfo DSO_UI_GetVerticalInfo(int channel)
{
    DSO_VerticalInfo vi;
    DSO_UI_VerticalInfo ui;

    DSO_Vertical_GetInfoForChannel((DSO_ChannelId)channel, &vi);
    ui.mv_per_div  = vi.mv_per_div;
    ui.offset_v    = vi.offset_v;
    ui.probe       = (uint8_t)vi.probe;
    ui.coupling    = (uint8_t)vi.coupling;
    ui.scale_min_v = vi.scale_min_v;
    ui.scale_max_v = vi.scale_max_v;
    return ui;
}

/* ── Measurement helpers ────────────────────────────────────────── */

float DSO_UI_FrameAdcLsbV(const DSO_UI_Frame *frame)
{
    if ((frame == NULL) || (frame->adc_lsb_v <= 0.0f)) {
        return 0.0f;
    }
    return frame->adc_lsb_v;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_UI_CodeDeltaToVolts
*   功能说明: 在 DSO 引擎与 UI 之间转换或发布波形帧数据。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
float DSO_UI_CodeDeltaToVolts(float code_delta, float lsb_v)
{
    return code_delta * lsb_v;
}