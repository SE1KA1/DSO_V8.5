/**
 * @file ui_screens.c
 * @brief 欢迎页、主菜单、通道/触发/测量/系统设置等子页面的 emWin 绘制实现。
 *
 * 配色与布局常量来自 ui_theme.h，与 MainTask 示波器页视觉一致。
 * 各子页的 EC 交互由 ui_nav.c 分发到 UI_Channel_* / UI_Trigger_* / UI_System_*。
 */
#include "GUI.h"
#include "ui_screens.h"
#include "ui_theme.h"
#include "ui_nav.h"
#include "ui_app_config.h"
#include "UI_WaveformCtrl.h"
#include "ui_measure_units.h"
#include "ui_channel_ctrl.h"

#include "ui_freq_format.h"
#include "ui_trigger_ui.h"
#include "dso/dso_vertical.h"
#include "dso/dso_config.h"
#include "dso/dso_10bit_lane_cal.h"
#include "ui_system_setup.h"
#include "ui_switch_panel.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "dso/dso_ui_bridge.h"

/*
*********************************************************************************************************
*   函 数 名: ui_text_prepare
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ui_text_prepare(void)
{
    GUI_SetTextMode(GUI_TM_TRANS);
}

/* 避免 GUI_DispStringHCenterAt 在旋转后/字宽取整上与 MainTask 手写坐标不一致 */
static void disp_str_cx(const char *s, int cx, int y)
{
    int w;
    ui_text_prepare();
    w = GUI_GetStringDistX(s);
    GUI_DispStringAt(s, cx - (w / 2), y);
}

/*
*********************************************************************************************************
*   函 数 名: fill_bg
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fill_bg(void)
{
    GUI_SetColor(UI_CLR_BG);
    GUI_FillRect(0, 0, UI_DISP_W - 1, UI_DISP_H - 1);
    ui_text_prepare();
}

/* 与 MainTask DrawHeader 相同的顶栏分层与分割线，仅替换标题与右上文案 */
static void draw_header_bar(const char *title, const char *right_tag)
{
    ui_text_prepare();

    GUI_SetColor(UI_CLR_HDR_TOP);
    GUI_FillRect(0, 0, UI_DISP_W - 1, UI_HDR_TOP);
    GUI_SetColor(UI_CLR_HDR_BOT);
    GUI_FillRect(0, UI_HDR_TOP + 1, UI_DISP_W - 1, UI_HDR_H_COMPACT - 1);

    GUI_SetColor(UI_CLR_DIV);
    GUI_DrawHLine(UI_HDR_TOP, 0, UI_DISP_W - 1);
    GUI_DrawHLine(UI_HDR_H_COMPACT, 0, UI_DISP_W - 1);

    GUI_SetFont(&GUI_Font20_ASCII);
    GUI_SetColor(UI_CLR_TEXT_HI);
    {
        int ty = (UI_HDR_TOP > 22) ? ((UI_HDR_TOP - GUI_GetFontDistY()) / 2) : 1;
        disp_str_cx(title, UI_DISP_W / 2, ty);
    }

    if (right_tag != NULL && right_tag[0] != '\0') {
        GUI_SetFont(&GUI_Font13_ASCII);
        GUI_SetColor(UI_CLR_TEXT_MID);
        {
            int tw = GUI_GetStringDistX(right_tag);
            int rty = (UI_HDR_TOP > 16) ? ((UI_HDR_TOP - GUI_GetFontDistY()) / 2) : 3;
            GUI_DispStringAt(right_tag, UI_DISP_W - 12 - tw, rty);
        }
    }
}

/*
*********************************************************************************************************
*   函 数 名: draw_footer_bar
*   功能说明: 完成当前 UI 模块的绘制、格式化或局部状态辅助处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void draw_footer_bar(const char *hint)
{
    int y0 = UI_DISP_H - UI_FOOTER_H;
    GUI_SetColor(UI_CLR_HDR_BOT);
    GUI_FillRect(0, y0, UI_DISP_W - 1, UI_DISP_H - 1);
    GUI_SetColor(UI_CLR_DIV);
    GUI_DrawHLine(y0, 0, UI_DISP_W - 1);
    GUI_SetFont(&GUI_Font8_ASCII);
    GUI_SetColor(UI_CLR_TEXT_DIM);
    ui_text_prepare();
    GUI_DispStringAt(hint, UI_MARGIN_X, y0 + 5);
}

/* 圆角半径与 MainTask 徽章一致用 4，避免过圆角在低密度屏上发糊 */
static void draw_menu_card(int x, int y, int w, int h, int selected, const char *line1, const char *line2)
{
    if (selected) {
        GUI_SetColor(UI_CLR_SEL);
        GUI_FillRoundedRect(x, y, x + w - 1, y + h - 1, 4);
        GUI_SetColor(UI_CLR_ACCENT);
        GUI_DrawRoundedRect(x, y, x + w - 1, y + h - 1, 4);
        GUI_SetColor(UI_CLR_TEXT_HI);
    } else {
        GUI_SetColor(UI_CLR_BG_PLOT);
        GUI_FillRoundedRect(x, y, x + w - 1, y + h - 1, 4);
        GUI_SetColor(UI_CLR_CARD_BR);
        GUI_DrawRoundedRect(x, y, x + w - 1, y + h - 1, 4);
        GUI_SetColor(UI_CLR_TEXT_MID);
    }

    ui_text_prepare();
    GUI_SetFont(&GUI_Font20_ASCII);
    GUI_DispStringAt(line1, x + 10, y + 6);
    if (line2 != NULL && line2[0] != '\0') {
        GUI_SetFont(&GUI_Font13_ASCII);
        GUI_SetColor(selected ? UI_CLR_TEXT_HI : UI_CLR_TEXT_DIM);
        GUI_DispStringAt(line2, x + 10, y + 32);
    }
}

/*
*********************************************************************************************************
*   函 数 名: screen_welcome
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void screen_welcome(uint32_t tick_ms)
{
    const int cx = UI_DISP_W / 2;

    (void)tick_ms;
    fill_bg();

    GUI_SetFont(&GUI_Font16_ASCII);
    GUI_SetColor(UI_CLR_TEXT_HI);
    disp_str_cx("DUAL CHANNEL OSCILLOSCOPE", cx, UI_DISP_H / 2 - 68);

    GUI_SetFont(&GUI_Font13_ASCII);
    GUI_SetColor(UI_CLR_CH1);
    disp_str_cx("WELCOME", cx, UI_DISP_H / 2 - 32);

    GUI_SetColor(UI_CLR_TEXT_MID);
    disp_str_cx("STM32H743  @400MHz", cx, UI_DISP_H / 2 + 4);

    GUI_SetFont(&GUI_Font8_ASCII);
    GUI_SetColor(UI_CLR_TEXT_DIM);
    disp_str_cx("DSO UI  |  emWin", cx, UI_DISP_H / 2 + 34);

    GUI_SetColor(UI_CLR_TEXT_MID);
    disp_str_cx("EC1: enter oscilloscope", cx, UI_DISP_H / 2 + 56);
    GUI_SetFont(&GUI_Font8_ASCII);
    GUI_SetColor(UI_CLR_TEXT_DIM);
    disp_str_cx("Side MENU: EC1 hold | Wave: EC2 tap | EC2 hold: Scale/Pos", cx, UI_DISP_H / 2 + 74);
}

/*
*********************************************************************************************************
*   函 数 名: screen_main_menu
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void screen_main_menu(uint32_t tick_ms)
{
    const int card_h = 44;
    const int gap    = 5;
    const int x      = UI_MARGIN_X;
    const int w      = UI_DISP_W - UI_MARGIN_X - UI_MARGIN_XR;
    int y            = UI_CONTENT_TOP + 2;
    int sel          = UI_Nav_GetMenuFocus();

    (void)tick_ms;
    fill_bg();
    draw_header_bar("MAIN MENU", UI_Waveform_IsRunning() ? "RUN" : "STOP");
#if UI_DEMO_AUTO_ADVANCE
    draw_footer_bar("EC1:OK EC2:BACK | scope:EC1=T/pos EC2=V/pos hold=solo/YT btn=RUN/mode");
#else
    draw_footer_bar("EC1:OK EC2:BACK | scope:EC1=T/pos EC2=V/pos hold=solo/YT btn=RUN/mode");
#endif

    draw_menu_card(x, y, w, card_h, sel == 0, "Oscilloscope", "Waveform display");
    y += card_h + gap;
    draw_menu_card(x, y, w, card_h, sel == 1, "Channel setup", "V/div  offset  ON/OFF  DC/AC/GND");
    y += card_h + gap;
    draw_menu_card(x, y, w, card_h, sel == 2, "Trigger", "Edge  mode  level  probe 1x/10x");
    y += card_h + gap;
    draw_menu_card(x, y, w, card_h, sel == 3, "Auto measure", "All params CH1 / CH2");
    y += card_h + gap;
    draw_menu_card(x, y, w, card_h, sel == 4, "System", "ADC 8-bit/10-bit  factory  EC nav");
    y += card_h + gap;
    draw_menu_card(x, y, w, card_h, sel == 5, "Switch panel", "Cloud LED + 14-bit DIP  EC1 nav");
}

/*
*********************************************************************************************************
*   函 数 名: ch_row
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ch_row(int x_name, int x_val, int y, const char *name, const char *value)
{
    ui_text_prepare();
    GUI_SetFont(&GUI_Font13_ASCII);
    GUI_SetColor(UI_CLR_TEXT_MID);
    GUI_DispStringAt(name, x_name, y);
    GUI_SetColor(UI_CLR_TEXT_HI);
    GUI_DispStringAt(value, x_val, y);
}

/*
*********************************************************************************************************
*   函 数 名: fmt_volts_signed
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fmt_volts_signed(float v, char *buf, unsigned n)
{
    if (!buf || n < 6u) {
        return;
    }
    if (v >= 1.0f || v <= -1.0f) {
        (void)snprintf(buf, n, "%+.2f V", (double)v);
    } else {
        (void)snprintf(buf, n, "%+.0f mV", (double)(v * 1000.0f));
    }
}

/*
*********************************************************************************************************
*   函 数 名: fmt_volts_mag
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fmt_volts_mag(float v, char *buf, unsigned n)
{
    float a = v < 0.0f ? -v : v;
    if (!buf || n < 6u) {
        return;
    }
    if (a >= 1.0f) {
        (void)snprintf(buf, n, "%.2f V", (double)v);
    } else {
        (void)snprintf(buf, n, "%.0f mV", (double)(v * 1000.0f));
    }
}

/*
*********************************************************************************************************
*   函 数 名: fmt_freq_hz
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fmt_freq_hz(float hz, int valid, char *buf, unsigned n)
{
    if (!buf || n < 4u) {
        return;
    }
    if (!valid || hz <= 0.0f) {
        (void)snprintf(buf, n, "--");
        return;
    }
    if (hz >= 1e6f) {
        (void)snprintf(buf, n, "%.2f MHz", (double)(hz / 1e6f));
    } else if (hz >= 1e3f) {
        (void)snprintf(buf, n, "%.2f kHz", (double)(hz / 1e3f));
    } else {
        (void)snprintf(buf, n, "%.1f Hz", (double)hz);
    }
}

/*
*********************************************************************************************************
*   函 数 名: fmt_period
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fmt_period(float hz, int valid, char *buf, unsigned n)
{
    double t;
    if (!buf || n < 4u) {
        return;
    }
    if (!valid || hz <= 0.0f) {
        (void)snprintf(buf, n, "--");
        return;
    }
    t = 1.0 / (double)hz;
    if (t >= 1.0) {
        (void)snprintf(buf, n, "%.3f s", t);
    } else if (t >= 1e-3) {
        (void)snprintf(buf, n, "%.3f ms", t * 1e3);
    } else if (t >= 1e-6) {
        (void)snprintf(buf, n, "%.2f us", t * 1e6);
    } else {
        (void)snprintf(buf, n, "%.0f ns", t * 1e9);
    }
}

/*
*********************************************************************************************************
*   函 数 名: fmt_duty
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fmt_duty(float d, char *buf, unsigned n)
{
    if (!buf || n < 6u) {
        return;
    }
    if (d < 0.0f || d > 1.0f) {
        (void)snprintf(buf, n, "--");
    } else {
        (void)snprintf(buf, n, "%.1f %%", (double)(d * 100.0f));
    }
}

/* ── 通道页：EC 焦点 0..3 = CH1 ON / CH2 ON / CH1 耦合 / CH2 耦合 ─── */
static int s_ch_ui_focus;

static const char *coupling_label(DSO_CouplingMode mode)
{
    if (mode == DSO_COUPLING_AC) {
        return "AC";
    }
    if (mode == DSO_COUPLING_GND) {
        return "GND";
    }
    return "DC";
}

void UI_Channel_OnScreenShown(void)
{
    s_ch_ui_focus = 0;
}

void UI_Channel_Ec1Detent(int dir)
{
    int next = s_ch_ui_focus;

    if (dir > 0) {
        next = (next + 1) % 4;
    } else if (dir < 0) {
        next = (next + 3) % 4;
    }
    s_ch_ui_focus = next;
    UI_Nav_MarkDirty();
}

static void cycle_coupling_user(DSO_ChannelId ch)
{
    DSO_VerticalInfo vi;
    DSO_CouplingMode next;

    DSO_Vertical_GetInfoForChannel(ch, &vi);
    next = (DSO_CouplingMode)(((int)vi.coupling + 1) % 3);
    DSO_Vertical_SetCouplingForChannel(ch, next);
}

void UI_Channel_Ec2Detent(int dir)
{
    DSO_ChannelId ch;
    bool          cur;

    (void)dir;
    if (s_ch_ui_focus <= 1) {
        ch  = (s_ch_ui_focus == 0) ? DSO_CHANNEL_1 : DSO_CHANNEL_2;
        cur = UI_ChannelCtrl_GetEnabled(ch);
        UI_ChannelCtrl_SetEnabled(ch, !cur);
    } else {
        ch = (s_ch_ui_focus == 2) ? DSO_CHANNEL_1 : DSO_CHANNEL_2;
        cycle_coupling_user(ch);
    }
    UI_Nav_MarkDirty();
}

/*
*********************************************************************************************************
*   函 数 名: fmt_vdiv_readout
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fmt_vdiv_readout(const DSO_VerticalInfo *vi, char *buf, unsigned n)
{
    if (!buf || n < 8u) {
        return;
    }
    if (vi->mv_per_div >= 1000u) {
        (void)snprintf(buf, n, "%.2fV/div", (double)vi->mv_per_div / 1000.0);
    } else {
        (void)snprintf(buf, n, "%.0fmV/div", (double)vi->mv_per_div);
    }
}

/*
*********************************************************************************************************
*   函 数 名: fmt_offset_readout
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fmt_offset_readout(const DSO_VerticalInfo *vi, char *buf, unsigned n)
{
    if (!buf || n < 8u) {
        return;
    }
    (void)snprintf(buf, n, "%+.2f V", (double)vi->offset_v);
}

/*
*********************************************************************************************************
*   函 数 名: fmt_real_probe_vdiv
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fmt_real_probe_vdiv(const DSO_VerticalInfo *vi, int ch_on, char *buf, unsigned n)
{
    float mult;
    float r;
    if (!buf || n < 6u) {
        return;
    }
    if (!ch_on) {
        (void)snprintf(buf, n, "--");
        return;
    }
    mult = (vi->probe == DSO_PROBE_X10) ? 10.0f : 1.0f;
    r    = ((float)vi->mv_per_div / 1000.0f) * mult;
    (void)snprintf(buf, n, "%.2fV/div", (double)r);
}

/*
*********************************************************************************************************
*   函 数 名: fmt_ch_status
*   功能说明: 维护触发设置页面的显示状态、用户调节和 DSO 触发参数同步。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void fmt_ch_status(unsigned ch_idx, const DSO_UI_Frame *fr, int ch_on, char *buf, unsigned n)
{
    if (!buf || n < 8u) {
        return;
    }
    if (!ch_on) {
        (void)snprintf(buf, n, "--");
        return;
    }
    if (ch_idx == 2u && !fr->ch2_present) {
        (void)snprintf(buf, n, "--");
        return;
    }
    if (ch_idx == 1u) {
        if (fr->cal_state == DSO_UI_CAL_COLLECTING) {
            (void)snprintf(buf, n, "Cal..");
        } else if (fr->cal_state == DSO_UI_CAL_FAILED) {
            (void)snprintf(buf, n, "Fault");
        } else {
            (void)snprintf(buf, n, "Normal");
        }
    } else {
        (void)snprintf(buf, n, "Normal");
    }
}

/*
*********************************************************************************************************
*   函 数 名: ch24_pair_row
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void ch24_pair_row(int y, const char *lab,
                          const char *v1, const char *v2,
                          GUI_COLOR c1, GUI_COLOR c2)
{
    const int xl = UI_MARGIN_X + 4;
    const int xr = UI_DISP_W / 2 + 14;
    GUI_SetFont(&GUI_Font24_ASCII);
    GUI_SetColor(UI_CLR_TEXT_MID);
    ui_text_prepare();
    GUI_DispStringAt(lab, xl, y);
    GUI_SetColor(c1);
    GUI_DispStringAt(v1, xl + 200, y);
    GUI_SetColor(UI_CLR_TEXT_MID);
    GUI_DispStringAt(lab, xr, y);
    GUI_SetColor(c2);
    GUI_DispStringAt(v2, xr + 200, y);
}

static void draw_option_cell(int x, int y, const char *text, int selected,
                             GUI_COLOR badge_bg, GUI_COLOR ch_fg)
{
    char s[16];
    int wlab;
    int box_pad = 4;
    int fh;

    (void)snprintf(s, sizeof(s), "%s  v", text);
    GUI_SetFont(&GUI_Font24_ASCII);
    fh = GUI_GetFontSizeY();
    if (selected) {
        wlab = GUI_GetStringDistX(s);
        GUI_SetColor(badge_bg);
        GUI_FillRoundedRect(x - box_pad, y - 2, x + wlab + box_pad, y + fh + 2, 4);
        GUI_SetColor(ch_fg);
        GUI_DrawRoundedRect(x - box_pad, y - 2, x + wlab + box_pad, y + fh + 2, 4);
        GUI_SetColor(UI_CLR_TEXT_HI);
    } else {
        GUI_SetColor(ch_fg);
    }
    ui_text_prepare();
    GUI_DispStringAt(s, x, y);
}

/*
*********************************************************************************************************
*   函 数 名: screen_channel
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void screen_channel(uint32_t tick_ms)
{
    DSO_VerticalInfo vi1, vi2;
    DSO_UI_Frame        fr;
    char             vdiv1[24], vdiv2[24];
    char             ofs1[20], ofs2[20];
    char             st1[16], st2[16];
    char             real1[20], real2[20];
    char             cpl1[8], cpl2[8];
    const int        mid = UI_DISP_W / 2;
    int              y   = UI_CONTENT_TOP + 8;
    const int        y_bot = UI_DISP_H - UI_FOOTER_H - 8;

    (void)tick_ms;
    DSO_Vertical_GetInfoForChannel(DSO_CHANNEL_1, &vi1);
    DSO_Vertical_GetInfoForChannel(DSO_CHANNEL_2, &vi2);
    UI_Waveform_ReadDisplayUIFrame(&fr);

    fmt_vdiv_readout(&vi1, vdiv1, (unsigned)sizeof(vdiv1));
    fmt_vdiv_readout(&vi2, vdiv2, (unsigned)sizeof(vdiv2));
    fmt_offset_readout(&vi1, ofs1, (unsigned)sizeof(ofs1));
    fmt_offset_readout(&vi2, ofs2, (unsigned)sizeof(ofs2));
    fmt_ch_status(1u, &fr, UI_ChannelCtrl_GetEnabled(DSO_CHANNEL_1) ? 1 : 0, st1, (unsigned)sizeof(st1));
    fmt_ch_status(2u, &fr, UI_ChannelCtrl_GetEnabled(DSO_CHANNEL_2) ? 1 : 0, st2, (unsigned)sizeof(st2));
    fmt_real_probe_vdiv(&vi1, UI_ChannelCtrl_GetEnabled(DSO_CHANNEL_1) ? 1 : 0, real1, (unsigned)sizeof(real1));
    fmt_real_probe_vdiv(&vi2, UI_ChannelCtrl_GetEnabled(DSO_CHANNEL_2) ? 1 : 0, real2, (unsigned)sizeof(real2));
    (void)snprintf(cpl1, sizeof(cpl1), "%s", coupling_label(vi1.coupling));
    (void)snprintf(cpl2, sizeof(cpl2), "%s", coupling_label(vi2.coupling));

    fill_bg();
    draw_header_bar("CHANNEL", "DSO");
    draw_footer_bar("EC1: focus  EC2: ON/OFF or DC/AC/GND  |  BACK: scope");

    GUI_SetColor(UI_CLR_DIV);
    GUI_DrawVLine(mid, UI_CONTENT_TOP + 4, y_bot);

    /* CH1 红 / CH2 绿（与 ui_theme UI_CLR_CH1 / UI_CLR_CH2 一致） */
    GUI_SetFont(&GUI_Font24_ASCII);
    GUI_SetColor(UI_CLR_CH1);
    ui_text_prepare();
    GUI_DispStringAt("CH1", UI_MARGIN_X + 4, y);
    GUI_SetColor(UI_CLR_CH2);
    GUI_DispStringAt("CH2", mid + 14, y);
    y += 42;

    draw_option_cell(UI_MARGIN_X + 4, y,
                     UI_ChannelCtrl_GetEnabled(DSO_CHANNEL_1) ? "ON" : "OFF",
                     s_ch_ui_focus == 0,
                     UI_CLR_CH1_BADGE, UI_CLR_CH1);
    draw_option_cell(mid + 14, y,
                     UI_ChannelCtrl_GetEnabled(DSO_CHANNEL_2) ? "ON" : "OFF",
                     s_ch_ui_focus == 1,
                     UI_CLR_CH2_BADGE, UI_CLR_CH2);
    y += 46;

    GUI_SetFont(&GUI_Font24_ASCII);
    GUI_SetColor(UI_CLR_TEXT_MID);
    ui_text_prepare();
    GUI_DispStringAt("Coupling:", UI_MARGIN_X + 4, y);
    GUI_DispStringAt("Coupling:", mid + 14, y);
    y += 42;
    draw_option_cell(UI_MARGIN_X + 4, y, cpl1, s_ch_ui_focus == 2,
                     UI_CLR_CH1_BADGE, UI_CLR_CH1);
    draw_option_cell(mid + 14, y, cpl2, s_ch_ui_focus == 3,
                     UI_CLR_CH2_BADGE, UI_CLR_CH2);
    y += 46;

    ch24_pair_row(y, "V/div:", vdiv1, vdiv2, UI_CLR_CH1, UI_CLR_CH2);
    y += 44;
    ch24_pair_row(y, "Offset:", ofs1, ofs2, UI_CLR_CH1, UI_CLR_CH2);
    y += 44;
    ch24_pair_row(y, "Status:", st1, st2, UI_CLR_CH1, UI_CLR_CH2);
    y += 44;
    ch24_pair_row(y, "Real:", real1, real2, UI_CLR_CH1, UI_CLR_CH2);
}

/*
*********************************************************************************************************
*   函 数 名: screen_trigger
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void screen_trigger(uint32_t tick_ms)
{
    /* 正文与 EC 逻辑在 ui_trigger_ui.c；ASCII 24 与通道页同量级 */
    (void)tick_ms;
    fill_bg();
    draw_header_bar("TRIGGER", "SETUP");
    draw_footer_bar("EC1: row 0-6  EC2: option/probe 1x10x  |  BACK: scope");
    UI_Trigger_DrawContent();
}

/*
*********************************************************************************************************
*   函 数 名: meas_side_row
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void meas_side_row(int xn, int xv, int y, const char *name, const char *val, GUI_COLOR clr_val)
{
    GUI_SetFont(&GUI_Font24_ASCII);
    GUI_SetColor(UI_CLR_TEXT_MID);
    ui_text_prepare();
    GUI_DispStringAt(name, xn, y);
    GUI_SetColor(clr_val);
    GUI_DispStringAt(val, xv, y);
}

/*
*********************************************************************************************************
*   函 数 名: screen_measure
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void screen_measure(uint32_t tick_ms)
{
    /* 左 CH1 / 右 CH2：来自 DSO 帧（测量页与示波器页均开流）；上升时间 UI_Port_FormatRiseTimeStr */
    static const char *names[] = {
        "Frequency", "Period", "Vpp", "Vmax", "Vmin", "Duty", "Rise time"
    };
    DSO_UI_Frame fr;
    char v1[7][28], v2[7][28];
    const int n     = 7;
    int i;
    const int ytop  = UI_CONTENT_TOP + 52;
    const int dy    = 44;
    const int mid   = UI_DISP_W / 2;
    const int x1n   = UI_MARGIN_X + 6;
    const int x1v   = 236;
    const int x2n   = mid + 12;
    const int x2v   = mid + 242;

    (void)tick_ms;
    UI_Waveform_ReadDisplayUIFrame(&fr);

    UI_FreqFormat_ExtMilliHz(fr.ext_freq_millihz,
                             fr.ext_freq_valid,
                             fr.ext_freq_state,
                             v1[0],
                             sizeof(v1[0]));
    UI_FreqFormat_ExtPeriod(fr.ext_freq_millihz,
                            fr.ext_freq_valid,
                            fr.ext_freq_state,
                            v1[1],
                            sizeof(v1[1]));
    fmt_volts_mag(UI_Measure_CodeDeltaToVolts((float)fr.vpp, &fr), v1[2], (unsigned)sizeof(v1[2]));
    fmt_volts_signed(DSO_UI_CodeDeltaToVolts((float)fr.vmax, DSO_UI_FrameAdcLsbV(&fr)), v1[3], (unsigned)sizeof(v1[3]));
    fmt_volts_signed(DSO_UI_CodeDeltaToVolts((float)fr.vmin, DSO_UI_FrameAdcLsbV(&fr)), v1[4], (unsigned)sizeof(v1[4]));
    fmt_duty(fr.duty_cycle, v1[5], (unsigned)sizeof(v1[5]));
    UI_Port_FormatRiseTimeStr(1u, v1[6], (unsigned)sizeof(v1[6]));

    if (fr.ch2_present) {
        fmt_freq_hz(fr.ch2_freq_hz, fr.ch2_freq_valid ? 1 : 0, v2[0], (unsigned)sizeof(v2[0]));
        fmt_period(fr.ch2_freq_hz, fr.ch2_freq_valid ? 1 : 0, v2[1], (unsigned)sizeof(v2[1]));
        fmt_volts_mag(UI_Measure_CodeDeltaToVolts((float)fr.ch2_vpp, &fr), v2[2], (unsigned)sizeof(v2[2]));
        fmt_volts_signed(DSO_UI_CodeDeltaToVolts((float)fr.ch2_vmax, DSO_UI_FrameAdcLsbV(&fr)), v2[3], (unsigned)sizeof(v2[3]));
        fmt_volts_signed(DSO_UI_CodeDeltaToVolts((float)fr.ch2_vmin, DSO_UI_FrameAdcLsbV(&fr)), v2[4], (unsigned)sizeof(v2[4]));
        fmt_duty(fr.ch2_duty_cycle, v2[5], (unsigned)sizeof(v2[5]));
        UI_Port_FormatRiseTimeStr(2u, v2[6], (unsigned)sizeof(v2[6]));
    } else {
        for (i = 0; i < n; i++) {
            (void)snprintf(v2[i], sizeof(v2[i]), "--");
        }
    }

    if (!UI_ChannelCtrl_GetEnabled(DSO_CHANNEL_1)) {
        for (i = 0; i < n; i++) {
            (void)snprintf(v1[i], sizeof(v1[i]), "--");
        }
    }
    if (!UI_ChannelCtrl_GetEnabled(DSO_CHANNEL_2)) {
        for (i = 0; i < n; i++) {
            (void)snprintf(v2[i], sizeof(v2[i]), "--");
        }
    }

    fill_bg();
    draw_header_bar("AUTO MEASURE", "LIVE");
    draw_footer_bar("From DSO frame  |  rise: UI_Port");

    GUI_SetColor(UI_CLR_DIV);
    GUI_DrawVLine(mid, UI_CONTENT_TOP + 4, UI_DISP_H - UI_FOOTER_H - 10);

    GUI_SetColor(UI_CLR_CH1_BADGE);
    GUI_FillRoundedRect(16, UI_CONTENT_TOP + 6, mid - 10, UI_CONTENT_TOP + 44, 4);
    GUI_SetColor(UI_CLR_CH1);
    GUI_DrawRoundedRect(16, UI_CONTENT_TOP + 6, mid - 10, UI_CONTENT_TOP + 44, 4);
    GUI_SetFont(&GUI_Font24_ASCII);
    ui_text_prepare();
    GUI_DispStringAt("CH1", 22, UI_CONTENT_TOP + 14);

    GUI_SetColor(UI_CLR_CH2_BADGE);
    GUI_FillRoundedRect(mid + 10, UI_CONTENT_TOP + 6, UI_DISP_W - 16, UI_CONTENT_TOP + 44, 4);
    GUI_SetColor(UI_CLR_CH2);
    GUI_DrawRoundedRect(mid + 10, UI_CONTENT_TOP + 6, UI_DISP_W - 16, UI_CONTENT_TOP + 44, 4);
    GUI_DispStringAt("CH2", mid + 18, UI_CONTENT_TOP + 14);

    for (i = 0; i < n; i++) {
        int y = ytop + i * dy;
        meas_side_row(x1n, x1v, y, names[i], v1[i], UI_CLR_CH1);
        meas_side_row(x2n, x2v, y, names[i], v2[i], UI_CLR_CH2);
    }
}

/*
*********************************************************************************************************
*   函 数 名: screen_system
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void screen_system(uint32_t tick_ms)
{
    const int y0   = UI_CONTENT_TOP + 10;
    const int xn   = UI_MARGIN_X + 14;
    const int xv   = 280;
    const int dd_x = 248;
    const int dd_w = 220;
    const int dd_h = 30;
    int y;
    uint8_t row = UI_System_GetFocusRow();
    const char *fmt_lbl = UI_System_AdcFormatLabel();

    (void)tick_ms;
    fill_bg();
    draw_header_bar("SYSTEM SETUP", "EC nav");
    draw_footer_bar("EC1: row  EC2: 10-bit / 8-bit (3L)  *=pending  |  BACK: scope");

    GUI_SetColor(UI_CLR_BG_PLOT);
    GUI_FillRoundedRect(UI_MARGIN_X, y0 - 4, UI_DISP_W - UI_MARGIN_XR, UI_DISP_H - UI_FOOTER_H - 10, 4);
    GUI_SetColor(UI_CLR_ACCENT);
    GUI_DrawRoundedRect(UI_MARGIN_X, y0 - 4, UI_DISP_W - UI_MARGIN_XR, UI_DISP_H - UI_FOOTER_H - 10, 4);

    y = y0 + 8;
    GUI_SetFont(&GUI_Font16_ASCII);
    GUI_SetColor(UI_CLR_TEXT_HI);
    ui_text_prepare();
    GUI_DispStringAt("Acquisition", xn, y);
    y += 28;
    GUI_SetFont(&GUI_Font13_ASCII);
    GUI_SetColor(UI_CLR_TEXT_MID);
    ui_text_prepare();
    GUI_DispStringAt("ADC resolution", xn, y + 4);

    /* 下拉框外观：当前项 + 三角标 */
    if (row == 0u) {
        GUI_SetColor(UI_CLR_SEL);
        GUI_DrawRoundedRect(dd_x - 3, y - 2, dd_x + dd_w + 2, y + dd_h + 1, 4);
        GUI_SetColor(UI_CLR_ACCENT);
        GUI_DrawRoundedRect(dd_x - 3, y - 2, dd_x + dd_w + 2, y + dd_h + 1, 4);
    } else {
        GUI_SetColor(UI_CLR_CARD_BR);
        GUI_DrawRoundedRect(dd_x - 2, y - 1, dd_x + dd_w + 1, y + dd_h, 4);
    }
    GUI_SetColor(UI_CLR_BG_PLOT);
    GUI_FillRoundedRect(dd_x, y, dd_x + dd_w - 1, y + dd_h - 1, 3);
    GUI_SetFont(&GUI_Font13_ASCII);
    GUI_SetColor(UI_CLR_TEXT_HI);
    ui_text_prepare();
    GUI_DispStringAt(fmt_lbl, dd_x + 8, y + 7);
    GUI_SetColor(UI_CLR_TEXT_MID);
    GUI_DispStringAt("v", dd_x + dd_w - 18, y + 7);

    y += 48;
    GUI_SetFont(&GUI_Font16_ASCII);
    GUI_SetColor(UI_CLR_TEXT_HI);
    ui_text_prepare();
    GUI_DispStringAt("Maintenance", xn, y);
    y += 28;
    GUI_SetFont(&GUI_Font13_ASCII);
    GUI_SetColor(row == 1u ? UI_CLR_TEXT_HI : UI_CLR_TEXT_MID);
    ui_text_prepare();
    GUI_DispStringAt("Restore factory defaults", xn, y);
    if (row == 1u) {
        GUI_SetColor(UI_CLR_ACCENT);
        GUI_DispStringAt(">", xv, y);
    } else {
        GUI_SetColor(UI_CLR_TEXT_DIM);
        GUI_DispStringAt(">", xv, y);
    }
}

/* 开关面板页：LED + 拨码开关 */
static void screen_switch(uint32_t tick_ms)
{
    fill_bg();
    draw_header_bar("SWITCH PANEL", "EC nav");
    draw_footer_bar("EC1: select  EC1 click: toggle  |  BACK: scope");
    UI_Switch_Draw(tick_ms);
}

/*
*********************************************************************************************************
*   函 数 名: UI_Screen_Draw
*   功能说明: 绘制和调度 UI 页面内容。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Screen_Draw(UI_ScreenId_t id, uint32_t tick_ms)
{
    ui_text_prepare();
    switch (id) {
    case UI_SCR_WELCOME:
        screen_welcome(tick_ms);
        break;
    case UI_SCR_MAIN_MENU:
        screen_main_menu(tick_ms);
        break;
    case UI_SCR_SCOPE:
        (void)tick_ms;
        fill_bg();
        break;
    case UI_SCR_CHANNEL:
        screen_channel(tick_ms);
        break;
    case UI_SCR_TRIGGER:
        screen_trigger(tick_ms);
        break;
    case UI_SCR_MEASURE:
        screen_measure(tick_ms);
        break;
    case UI_SCR_SYSTEM:
        screen_system(tick_ms);
        break;
    case UI_SCR_SWITCH:
        screen_switch(tick_ms);
        break;
    default:
        fill_bg();
        draw_header_bar("UNKNOWN", "");
        break;
    }
}

/* ── UI_Port：弱默认实现，可在其它 .c 中覆盖以接 BSP / 存储 ───────── */

__weak const char *UI_Port_TrigSourceLabel(void)
{
    return "CH1";
}

/*
*********************************************************************************************************
*   函 数 名: UI_Port_TrigCouplingLabel
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
__weak const char *UI_Port_TrigCouplingLabel(void)
{
    return "DC";
}

/*
*********************************************************************************************************
*   函 数 名: UI_Port_FormatRiseTimeStr
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
__weak void UI_Port_FormatRiseTimeStr(unsigned ch_idx, char *buf, unsigned buf_size)
{
    (void)ch_idx;
    if (buf != NULL && buf_size > 0u) {
        (void)snprintf(buf, (size_t)buf_size, "---");
    }
}
