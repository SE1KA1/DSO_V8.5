/**
 * @file UI_WaveformCtrl.c
 * @brief 实现 UI_WaveformCtrl.h：2-slot 帧环、DSO_Frame→DSO_UI_Frame 转换、旋钮/按键参数路由。
 *
 * 帧环协议：
 *   生产者 DSO_Engine_Task → DSO_UI_SendFrame(DSO_Frame*)
 *   消费者 MainTask         → UI_Waveform_CopyUIFrame(DSO_UI_Frame*)
 *   pending_count 满时覆盖最旧槽，保证 UI 始终拿到最新一帧。
 *   stream_active=0 时引擎仍可运行，但 UI 侧可选择不拉帧（菜单页）。
 */
#include "UI_WaveformCtrl.h"
#include "ui_measure_units.h"
#include "dso/dso_process.h"      /* DSO_Frame — only needed for DSO_UI_SendFrame conversion */
#include "dso/dso_vertical.h"
#include "dso/dso_timebase.h"
#include "dso/dso_acquire_control.h"
#include "dso/dso_acq.h"
#include "dso/dso_10bit_lane_cal.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

/* ────────────────────────────────────────────────────────────────
 * 2-slot 接收环 + 1 份 display 快照（ReadDisplayUIFrame 不消费新帧标志）。
 * critical section 内 memcpy，典型耗时数 µs。
 * ──────────────────────────────────────────────────────────────── */
static DSO_UI_Frame s_recv_frames[2];
static DSO_UI_Frame s_display_frame;
static volatile uint8_t s_write_idx = 0;
static volatile uint8_t s_read_idx = 0;
static volatile uint8_t s_pending_count = 0;

/* UI-only state */
static volatile int        s_stream_active = 1;
static UI_Waveform_Solo_t  s_solo          = UI_WAVEFORM_SOLO_BOTH;
/** Last offsets used in DUAL before entering solo (restored when returning to DUAL). */
static float               s_dual_saved_ch1_off_v;
static float               s_dual_saved_ch2_off_v;
static UI_CursorMode_t     s_cursor_mode   = UI_CURSOR_OFF;
static float s_cur_t1 = 0.3f, s_cur_t2 = 0.7f;
static float s_cur_v1 = 0.3f, s_cur_v2 = 0.7f;

/* ── DSO_Frame → DSO_UI_Frame conversion ─────────────────────── */

static void convert_frame(const DSO_Frame *src, DSO_UI_Frame *dst)
{
    /* Waveform display */
    memcpy(dst->y_coords, src->y_coords, sizeof(dst->y_coords));
    memcpy(dst->y_coords_ch2, src->y_coords_ch2, sizeof(dst->y_coords_ch2));
    dst->waveform_column_envelope_ch1 = src->waveform_column_envelope_ch1;
    dst->waveform_column_envelope_ch2 = src->waveform_column_envelope_ch2;
    memcpy(dst->y_column_env_top_ch1, src->y_column_env_top_ch1, sizeof(dst->y_column_env_top_ch1));
    memcpy(dst->y_column_env_bot_ch1, src->y_column_env_bot_ch1, sizeof(dst->y_column_env_bot_ch1));
    memcpy(dst->y_column_env_top_ch2, src->y_column_env_top_ch2, sizeof(dst->y_column_env_top_ch2));
    memcpy(dst->y_column_env_bot_ch2, src->y_column_env_bot_ch2, sizeof(dst->y_column_env_bot_ch2));

    /* Measurement CH1 */
    dst->freq_hz    = src->freq_hz;
    dst->freq_valid = src->freq_valid;
    dst->vpp        = src->vpp;
    dst->vmin       = src->vmin;
    dst->vmax       = src->vmax;
    dst->vrms       = src->vrms;
    dst->duty_cycle = src->duty_cycle;
    dst->triggered  = src->triggered;

    /* Measurement CH2 */
    dst->ch2_freq_hz    = src->ch2_freq_hz;
    dst->ch2_freq_valid = src->ch2_freq_valid;
    dst->ch2_vpp        = src->ch2_vpp;
    dst->ch2_vmin       = src->ch2_vmin;
    dst->ch2_vmax       = src->ch2_vmax;
    dst->ch2_vrms       = src->ch2_vrms;
    dst->ch2_duty_cycle = src->ch2_duty_cycle;
    dst->ch2_present    = src->ch2_present;

    /* External frequency counter */
    dst->ext_freq_millihz = src->ext_freq_millihz;
    dst->ext_freq_valid   = src->ext_freq_valid;
    dst->ext_freq_state   = src->ext_freq_state;
    dst->ext_freq_mode    = src->ext_freq_mode;

    /* Timebase */
    dst->tb_sec_div_ns          = src->tb_sec_div_ns;
    dst->tb_mode                 = src->tb_mode;
    dst->tb_time_per_pixel_ns    = src->tb_time_per_pixel_ns;
    dst->effective_sample_rate_hz = src->effective_sample_rate_hz;
    dst->sample_rate             = src->sample_rate;
    dst->ch2_sample_rate         = src->ch2_sample_rate;

    /* Vertical */
    dst->vert_mv_per_div       = src->vert_mv_per_div;
    dst->ch2_vert_mv_per_div   = src->ch2_vert_mv_per_div;
    dst->vert_scale_min_v      = src->vert_scale_min_v;
    dst->vert_scale_max_v      = src->vert_scale_max_v;
    dst->ch2_vert_scale_min_v  = src->ch2_vert_scale_min_v;
    dst->ch2_vert_scale_max_v  = src->ch2_vert_scale_max_v;

    /* Trigger */
    dst->trig_mode         = src->trig_mode;
    dst->trig_source       = src->trig_source;
    dst->trig_level_v      = src->trig_level_v;
    dst->trig_level_enabled = src->trig_level_enabled;

    /* ADC metadata */
    dst->adc_bits  = src->adc_bits;
    dst->adc_lsb_v = src->adc_lsb_v;

    /* Render quality */
    dst->recon_mode      = src->recon_mode;
    dst->display_quality = src->display_quality;

    /* Calibration */
    dst->cal_state               = src->cal_state;
    dst->cal_progress_pct        = src->cal_progress_pct;
    dst->ch1_interleave_cal_valid = src->ch1_interleave_cal_valid;

    /* Roll */
    dst->roll_ring_coherent_engine = src->roll_ring_coherent_engine;
    dst->roll_resync_in_progress   = src->roll_resync_in_progress;
    dst->roll_fill_ch1             = src->roll_fill_ch1;
    dst->roll_fill_ch2             = src->roll_fill_ch2;

    /* ETS */
    dst->ets_active  = src->ets_active;
    dst->ets_building = src->ets_building;

    dst->live_record_paused = src->live_record_paused;
}

/* ── DSO_UI_SendFrame (called from DSO engine task) ──────────── */

void DSO_UI_SendFrame(const DSO_Frame *frame)
{
    DSO_UI_Frame ui;

    if (frame == NULL) {
        return;
    }
    if (s_stream_active == 0) {
        return;
    }
    convert_frame(frame, &ui);
    taskENTER_CRITICAL();
    if (s_pending_count >= 2u) {
        s_read_idx = (uint8_t)((s_read_idx + 1u) % 2u);
        s_pending_count--;
    }
    s_recv_frames[s_write_idx] = ui;
    s_write_idx = (uint8_t)((s_write_idx + 1u) % 2u);
    s_pending_count++;
    taskEXIT_CRITICAL();
}

/* ── Init ────────────────────────────────────────────────────── */

void UI_Waveform_Init(void)
{
    memset(s_recv_frames, 0, sizeof(s_recv_frames));
    memset(&s_display_frame, 0, sizeof(s_display_frame));
    s_write_idx = 0;
    s_read_idx = 0;
    s_pending_count = 0;
    s_solo          = UI_WAVEFORM_SOLO_BOTH;
    s_cursor_mode   = UI_CURSOR_OFF;
    /* Must match DSO_Vertical_Init defaults; UI_Waveform_Init runs before DSO engine inits vertical. */
    s_dual_saved_ch1_off_v = -0.4f;
    s_dual_saved_ch2_off_v =  0.4f;
}

/* ── Frame interface ─────────────────────────────────────────── */

bool UI_Waveform_HasNewFrame(void)
{
    return s_pending_count != 0u;
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_CopyUIFrame
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool UI_Waveform_CopyUIFrame(DSO_UI_Frame *out)
{
    bool got;

    taskENTER_CRITICAL();
    got = (s_pending_count != 0u);
    if (got) {
        s_display_frame = s_recv_frames[s_read_idx];
        s_read_idx = (uint8_t)((s_read_idx + 1u) % 2u);
        s_pending_count--;
    }
    taskEXIT_CRITICAL();

    if (got && (out != NULL)) {
        *out = s_display_frame;
    }
    return got;
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_ReadDisplayUIFrame
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_ReadDisplayUIFrame(DSO_UI_Frame *out)
{
    if (out == NULL) {
        return;
    }
    taskENTER_CRITICAL();
    *out = s_display_frame;
    taskEXIT_CRITICAL();
}

/* ── RUN / STOP ──────────────────────────────────────────────── */

bool UI_Waveform_IsRunning(void)
{
    return DSO_AcquireCtrl_IsRunRequested();
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_ToggleRunStop
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_ToggleRunStop(void)
{
    if (DSO_AcquireCtrl_IsRunRequested()) {
        DSO_AcquireCtrl_SetRunRequested(false);
    } else {
        DSO_AcquireCtrl_RearmSingle();
        DSO_AcquireCtrl_SetRunRequested(true);
    }
}

/* ── Display mode ────────────────────────────────────────────── */

UI_Waveform_DisplayMode_t UI_Waveform_GetDisplayMode(void)
{
    return (s_display_frame.tb_mode == DSO_UI_TB_MODE_ROLL)
           ? UI_WAVEFORM_MODE_ROLL
           : UI_WAVEFORM_MODE_YT;
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_ToggleDisplayMode
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_ToggleDisplayMode(void)
{
    uint8_t idx   = DSO_Timebase_GetIndex();
    uint8_t count = DSO_Timebase_GetCount();
    DSO_TimebaseMode cur_mode = DSO_Timebase_GetMode();

    if (cur_mode == DSO_UI_TB_MODE_ROLL) {
        for (int i = (int)idx - 1; i >= 0; i--) {
            const DSO_TimebaseEntry *e = DSO_Timebase_GetEntryAt((uint8_t)i);
            if (e != NULL && e->mode != DSO_UI_TB_MODE_ROLL) {
                DSO_Timebase_SetIndex((uint8_t)i);
                return;
            }
        }
    } else {
        for (uint8_t i = idx + 1u; i < count; i++) {
            const DSO_TimebaseEntry *e = DSO_Timebase_GetEntryAt(i);
            if (e != NULL && e->mode == DSO_UI_TB_MODE_ROLL) {
                DSO_Timebase_SetIndex(i);
                return;
            }
        }
    }
}

/* ── Solo ────────────────────────────────────────────────────── */

UI_Waveform_Solo_t UI_Waveform_GetSolo(void)       { return s_solo; }

/*
*********************************************************************************************************
*   函 数 名: ui_waveform_apply_solo_vertical
*   功能说明: 在 UI 波形控制层完成帧环读写、显示状态、档位、触发、游标或通道操作。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ui_waveform_apply_solo_vertical(UI_Waveform_Solo_t prev, UI_Waveform_Solo_t next)
{
    if (prev == next) {
        return;
    }

    if (prev == UI_WAVEFORM_SOLO_BOTH && next != UI_WAVEFORM_SOLO_BOTH) {
        s_dual_saved_ch1_off_v = DSO_Vertical_GetOffsetForChannel(DSO_CHANNEL_1);
        s_dual_saved_ch2_off_v = DSO_Vertical_GetOffsetForChannel(DSO_CHANNEL_2);
    }

    if (next == UI_WAVEFORM_SOLO_BOTH) {
        DSO_Vertical_SetOffsetForChannel(DSO_CHANNEL_1, s_dual_saved_ch1_off_v);
        DSO_Vertical_SetOffsetForChannel(DSO_CHANNEL_2, s_dual_saved_ch2_off_v);
    } else if (next == UI_WAVEFORM_SOLO_CH1) {
        /* Midscale (0 V offset) at graticule center for the solo trace */
        DSO_Vertical_SetOffsetForChannel(DSO_CHANNEL_1, 0.0f);
    } else if (next == UI_WAVEFORM_SOLO_CH2) {
        DSO_Vertical_SetOffsetForChannel(DSO_CHANNEL_2, 0.0f);
    }
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_SetSolo
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_SetSolo(UI_Waveform_Solo_t solo)
{
    UI_Waveform_Solo_t prev;

    if (solo > UI_WAVEFORM_SOLO_CH2) {
        return;
    }
    prev = s_solo;
    if (solo == s_solo) {
        return;
    }
    s_solo = solo;
    ui_waveform_apply_solo_vertical(prev, s_solo);
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_CycleSolo
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_CycleSolo(void)
{
    UI_Waveform_Solo_t prev = s_solo;

    s_solo = (UI_Waveform_Solo_t)(((int)s_solo + 1) % 3);
    ui_waveform_apply_solo_vertical(prev, s_solo);
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_GetSoloLabel
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const char *UI_Waveform_GetSoloLabel(void)
{
    switch (s_solo) {
    case UI_WAVEFORM_SOLO_CH1: return "CH1";
    case UI_WAVEFORM_SOLO_CH2: return "CH2";
    default:                   return "DUAL";
    }
}

/* ── Channel vertical offset ─────────────────────────────────── */

#define OFFSET_STEP_V  0.05f

/** EC1 position: CH2 solo adjusts CH2; otherwise CH1 (DUAL or CH1 solo). */
static DSO_ChannelId vertical_pos_channel_ec1(void)
{
    return (s_solo == UI_WAVEFORM_SOLO_CH2) ? DSO_CHANNEL_2 : DSO_CHANNEL_1;
}

/** EC2 position: CH1 solo adjusts CH1 (CH2 hidden); otherwise CH2. */
static DSO_ChannelId vertical_pos_channel_ec2(void)
{
    return (s_solo == UI_WAVEFORM_SOLO_CH1) ? DSO_CHANNEL_1 : DSO_CHANNEL_2;
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_CH1_MoveUp
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_CH1_MoveUp(void)
{
    DSO_ChannelId ch = vertical_pos_channel_ec1();
    float cur        = DSO_Vertical_GetOffsetForChannel(ch);

    DSO_Vertical_SetOffsetForChannel(ch, cur + OFFSET_STEP_V);
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_CH1_MoveDown
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_CH1_MoveDown(void)
{
    DSO_ChannelId ch = vertical_pos_channel_ec1();
    float cur        = DSO_Vertical_GetOffsetForChannel(ch);

    DSO_Vertical_SetOffsetForChannel(ch, cur - OFFSET_STEP_V);
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_CH2_MoveUp
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_CH2_MoveUp(void)
{
    DSO_ChannelId ch = vertical_pos_channel_ec2();
    float cur        = DSO_Vertical_GetOffsetForChannel(ch);

    DSO_Vertical_SetOffsetForChannel(ch, cur + OFFSET_STEP_V);
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_CH2_MoveDown
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_CH2_MoveDown(void)
{
    DSO_ChannelId ch = vertical_pos_channel_ec2();
    float cur        = DSO_Vertical_GetOffsetForChannel(ch);

    DSO_Vertical_SetOffsetForChannel(ch, cur - OFFSET_STEP_V);
}

/* ── V/div and ms/div ────────────────────────────────────────── */

float UI_Waveform_GetVoltPerDiv(void)
{
    return (float)s_display_frame.vert_mv_per_div / 1000.0f;
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_GetVoltPerDivCh2
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
float UI_Waveform_GetVoltPerDivCh2(void)
{
    return (float)s_display_frame.ch2_vert_mv_per_div / 1000.0f;
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_GetTimePerDiv
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
float UI_Waveform_GetTimePerDiv(void)
{
    return (float)s_display_frame.tb_sec_div_ns / 1e6f;
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_CycleVoltPerDiv
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_CycleVoltPerDiv(void)
{
    uint8_t idx   = DSO_Vertical_GetIndex();
    uint8_t count = DSO_Vertical_GetCount();
    uint8_t next  = (uint8_t)((idx + 1u) % count);
    DSO_Vertical_SetIndexForChannel(DSO_CHANNEL_1, next);
    DSO_Vertical_SetIndexForChannel(DSO_CHANNEL_2, next);
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_CycleTimePerDiv
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_CycleTimePerDiv(void)
{
    uint8_t idx   = DSO_Timebase_GetIndex();
    uint8_t count = DSO_Timebase_GetCount();
    DSO_Timebase_SetIndex((uint8_t)((idx + 1u) % count));
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_StepVoltPerDiv
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_StepVoltPerDiv(int delta)
{
    int idx   = (int)DSO_Vertical_GetIndex() + delta;
    int count = (int)DSO_Vertical_GetCount();
    if (idx < 0) idx = 0;
    if (idx >= count) idx = count - 1;
    DSO_Vertical_SetIndexForChannel(DSO_CHANNEL_1, (uint8_t)idx);
    DSO_Vertical_SetIndexForChannel(DSO_CHANNEL_2, (uint8_t)idx);
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_StepTimePerDiv
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_StepTimePerDiv(int delta)
{
    int idx   = (int)DSO_Timebase_GetIndex() + delta;
    int count = (int)DSO_Timebase_GetCount();
    if (idx < 0) idx = 0;
    if (idx >= count) idx = count - 1;
    DSO_Timebase_SetIndex((uint8_t)idx);
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_FormatScaleStr
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_FormatScaleStr(char *buf, uint32_t buf_size)
{
    uint32_t len;
    int      n;
    float v_per_div   = UI_Waveform_GetVoltPerDiv();
    float sec_div_ns  = (float)s_display_frame.tb_sec_div_ns;

    if (!buf || buf_size < 24u) {
        return;
    }

    if (v_per_div >= 1.0f) {
        n = snprintf(buf, buf_size, "%.0fV/div  ", (double)v_per_div);
    } else {
        n = snprintf(buf, buf_size, "%.0fmV/div ", (double)(v_per_div * 1000.0f));
    }
    if (n < 0 || (uint32_t)n >= buf_size) {
        if (buf_size > 0u) {
            buf[buf_size - 1u] = '\0';
        }
        return;
    }
    len = (uint32_t)n;

    if (sec_div_ns >= 1e9f) {
        (void)snprintf(buf + len, buf_size - len, "%.0fs/div", (double)(sec_div_ns / 1e9f));
    } else if (sec_div_ns >= 1e6f) {
        (void)snprintf(buf + len, buf_size - len, "%.0fms/div", (double)(sec_div_ns / 1e6f));
    } else if (sec_div_ns >= 1e3f) {
        (void)snprintf(buf + len, buf_size - len, "%.0fus/div", (double)(sec_div_ns / 1e3f));
    } else {
        (void)snprintf(buf + len, buf_size - len, "%.0fns/div", (double)sec_div_ns);
    }
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_FormatScaleStrCh2
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_FormatScaleStrCh2(char *buf, uint32_t buf_size)
{
    uint32_t len;
    int      n;
    float v_per_div   = UI_Waveform_GetVoltPerDivCh2();
    float sec_div_ns  = (float)s_display_frame.tb_sec_div_ns;

    if (!buf || buf_size < 24u) {
        return;
    }

    if (v_per_div >= 1.0f) {
        n = snprintf(buf, buf_size, "%.0fV/div  ", (double)v_per_div);
    } else {
        n = snprintf(buf, buf_size, "%.0fmV/div ", (double)(v_per_div * 1000.0f));
    }
    if (n < 0 || (uint32_t)n >= buf_size) {
        if (buf_size > 0u) {
            buf[buf_size - 1u] = '\0';
        }
        return;
    }
    len = (uint32_t)n;

    if (sec_div_ns >= 1e9f) {
        (void)snprintf(buf + len, buf_size - len, "%.0fs/div", (double)(sec_div_ns / 1e9f));
    } else if (sec_div_ns >= 1e6f) {
        (void)snprintf(buf + len, buf_size - len, "%.0fms/div", (double)(sec_div_ns / 1e6f));
    } else if (sec_div_ns >= 1e3f) {
        (void)snprintf(buf + len, buf_size - len, "%.0fus/div", (double)(sec_div_ns / 1e3f));
    } else {
        (void)snprintf(buf + len, buf_size - len, "%.0fns/div", (double)sec_div_ns);
    }
}

/* ── Sample rate ─────────────────────────────────────────────── */

float UI_Waveform_GetEffectiveSampleRateHz(void)
{
    /* Roll: match CH2-style ADC/stream rate in the status bar (not 1/time_per_pixel "column Hz"). */
    if (s_display_frame.tb_mode == DSO_UI_TB_MODE_ROLL) {
        if (s_display_frame.sample_rate > 0u) {
            return (float)s_display_frame.sample_rate;
        }
    }
    return (s_display_frame.effective_sample_rate_hz > 1.0f)
         ? s_display_frame.effective_sample_rate_hz
         : (float)s_display_frame.sample_rate;
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_GetCh2SampleRateHz
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
float UI_Waveform_GetCh2SampleRateHz(void)
{
    return (float)s_display_frame.ch2_sample_rate;
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_FormatSampleRateHz
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_FormatSampleRateHz(float hz, char *buf, uint32_t buf_size)
{
    if (!buf || buf_size < 8u) {
        return;
    }
    if (hz <= 1.0f) {
        (void)snprintf(buf, buf_size, "Fs --");
        return;
    }
    if (hz >= 1e6f) {
        (void)snprintf(buf, buf_size, "Fs %.2fM", (double)(hz / 1e6f));
    } else if (hz >= 1e3f) {
        (void)snprintf(buf, buf_size, "Fs %.1fk", (double)(hz / 1e3f));
    } else {
        (void)snprintf(buf, buf_size, "Fs %.0f", (double)hz);
    }
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_RequestInterleaveCalibration
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_RequestInterleaveCalibration(void)
{
    DSO_10bitLaneCal_RequestRun();
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_CancelInterleaveCalibration
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_CancelInterleaveCalibration(void)
{
    DSO_10bitLaneCal_Cancel();
}

/* ── Measurements ────────────────────────────────────────────── */

void UI_Waveform_GetMeasurements(UI_Waveform_Meas_t *m)
{
    if (m == NULL) {
        return;
    }

    m->freq_hz   = s_display_frame.freq_hz;
    m->freq_valid = s_display_frame.freq_valid;
    m->vpp_v     = UI_Measure_CodeDeltaToVolts((float)s_display_frame.vpp,
                                                &s_display_frame);
    m->vrms_v    = UI_Measure_CodeDeltaToVolts(s_display_frame.vrms,
                                                &s_display_frame);
    m->duty      = s_display_frame.duty_cycle;
    m->triggered = s_display_frame.triggered;
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_GetMeasurementsCh2
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_GetMeasurementsCh2(UI_Waveform_Meas_t *m)
{
    if (m == NULL) {
        return;
    }

    if (!s_display_frame.ch2_present) {
        m->freq_hz    = 0.0f;
        m->freq_valid = false;
        m->vpp_v      = 0.0f;
        m->vrms_v     = 0.0f;
        m->duty       = -1.0f;
        m->triggered  = s_display_frame.triggered;
        return;
    }

    m->freq_hz    = s_display_frame.ch2_freq_hz;
    m->freq_valid = s_display_frame.ch2_freq_valid;
    m->vpp_v      = UI_Measure_CodeDeltaToVolts((float)s_display_frame.ch2_vpp,
                                                &s_display_frame);
    m->vrms_v     = UI_Measure_CodeDeltaToVolts(s_display_frame.ch2_vrms,
                                                &s_display_frame);
    m->duty       = s_display_frame.ch2_duty_cycle;
    m->triggered  = s_display_frame.triggered;
}

/* ── Trigger ─────────────────────────────────────────────────── */

UI_TrigMode_t UI_Waveform_GetTrigMode(void)
{
    switch (s_display_frame.trig_mode) {
    case DSO_UI_TRIG_NORMAL:  return UI_TRIG_MODE_NORMAL;
    case DSO_UI_TRIG_SINGLE:  return UI_TRIG_MODE_SINGLE;
    default:                  return UI_TRIG_MODE_AUTO;
    }
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_CycleTrigMode
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_CycleTrigMode(void)
{
    DSO_TriggerMode cur  = (DSO_TriggerMode)s_display_frame.trig_mode;
    DSO_TriggerMode next = (DSO_TriggerMode)(((int)cur + 1) % 3);

    DSO_AcquireCtrl_SetTriggerMode(next);

    if (next == DSO_UI_TRIG_SINGLE) {
        DSO_AcquireCtrl_RearmSingle();
        DSO_AcquireCtrl_SetRunRequested(true);
    }
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_GetTrigModeLabel
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const char *UI_Waveform_GetTrigModeLabel(void)
{
    switch (UI_Waveform_GetTrigMode()) {
    case UI_TRIG_MODE_AUTO:   return "Auto";
    case UI_TRIG_MODE_NORMAL: return "Norm";
    case UI_TRIG_MODE_SINGLE: return "Sing";
    default:                  return "?";
    }
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_GetTrigLevel
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
float UI_Waveform_GetTrigLevel(void)
{
    return s_display_frame.trig_level_v;
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_AdjustTrigLevel
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_AdjustTrigLevel(int delta_steps)
{
    float cur  = s_display_frame.trig_level_v;
    float step = 0.05f;

    DSO_AcquireCtrl_SetTriggerLevelEnabled(true);
    DSO_AcquireCtrl_SetTriggerLevelVolts(cur + (float)delta_steps * step);
}

/* ── Cursors ─────────────────────────────────────────────────── */

void UI_Waveform_SetCursorMode(UI_CursorMode_t mode)
{
    if (mode <= UI_CURSOR_BOTH) {
        s_cursor_mode = mode;
    }
}

UI_CursorMode_t UI_Waveform_GetCursorMode(void) { return s_cursor_mode; }

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_CycleCursorMode
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_CycleCursorMode(void)
{
    s_cursor_mode = (UI_CursorMode_t)(((int)s_cursor_mode + 1) % 4);
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_GetTimeCursors
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_GetTimeCursors(float *t1, float *t2)
{
    if (t1) *t1 = s_cur_t1;
    if (t2) *t2 = s_cur_t2;
}

/*
*********************************************************************************************************
*   函 数 名: UI_Waveform_GetVoltCursors
*   功能说明: 处理波形 UI 帧缓存、运行状态、通道显示、档位、触发或游标控制。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Waveform_GetVoltCursors(float *v1, float *v2)
{
    if (v1) *v1 = s_cur_v1;
    if (v2) *v2 = s_cur_v2;
}

/*
*********************************************************************************************************
*   函 数 名: clamp_01
*   功能说明: 在 UI 波形控制层完成帧环读写、显示状态、档位、触发、游标或通道操作。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void clamp_01(float *v)
{
    if (*v < 0.0f) *v = 0.0f;
    if (*v > 1.0f) *v = 1.0f;
}

void UI_Waveform_AdjustTimeCursor1(int delta) { s_cur_t1 += (float)delta * 0.01f; clamp_01(&s_cur_t1); }
void UI_Waveform_AdjustTimeCursor2(int delta) { s_cur_t2 += (float)delta * 0.01f; clamp_01(&s_cur_t2); }
void UI_Waveform_AdjustVoltCursor1(int delta) { s_cur_v1 += (float)delta * 0.01f; clamp_01(&s_cur_v1); }
void UI_Waveform_AdjustVoltCursor2(int delta) { s_cur_v2 += (float)delta * 0.01f; clamp_01(&s_cur_v2); }

/* ── Stream gate ─────────────────────────────────────────────── */

void UI_Waveform_SetStreamActive(int active) { s_stream_active = active ? 1 : 0; }
int  UI_Waveform_IsStreamActive(void)        { return s_stream_active; }
