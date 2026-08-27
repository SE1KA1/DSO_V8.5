/**
 * @file ui_switch_panel.c
 * @brief 开关面板页绘制与 EC1 导航。
 *
 * 布局（800x480 逻辑坐标）：
 *   - 顶栏 / 底栏：由 ui_screens.c 的 screen_switch 调用 draw_header/footer
 *   - LED 行：3 个圆点（LED1 绿心跳 / LED2 蓝电源 / LED3 红报警）
 *   - 10-BIT DIP：2 行 × 5 列（SW1..SW10）
 *   - 4-BIT DIP：1 行 × 4 列（SW11..SW14）
 *   - 选中开关：橙色外框高亮 + 序号反色
 */
#include "ui_switch_panel.h"
#include "ui_theme.h"
#include "dso/dso_switch_panel.h"
#include "ui_nav.h"
#include "GUI.h"
#include <stdio.h>

/* 焦点 0..13 对应 SW1..SW14 */
static int s_focus = 0;

/* ---- 几何常量 ---- */
#define SW_W        126
#define SW_H        56
#define SW_GAP      10
#define LED_R       10

static void switch_panel_textmode(void)
{
    GUI_SetTextMode(GUI_TM_TRANS);
}

/* 居中绘制字符串 */
static void disp_center(int x, int y, const char *s)
{
    GUI_DispStringHCenterAt(s, x, y);
}

/* 绘制一个 LED 圆点 */
static void draw_led(int x, int y, int idx, const char *label, uint32_t on_clr)
{
    uint8_t on = DSO_Switch_GetLed(idx);
    GUI_SetColor(on ? on_clr : UI_CLR_TEXT_DIM);
    GUI_FillCircle(x, y, LED_R);
    GUI_SetColor(UI_CLR_TEXT_DIM);
    GUI_DrawCircle(x, y, LED_R + 1);
    GUI_SetFont(&GUI_Font13_ASCII);
    GUI_SetColor(on ? UI_CLR_TEXT_HI : UI_CLR_TEXT_DIM);
    switch_panel_textmode();
    disp_center(x, y + LED_R + 12, label);
    GUI_SetFont(&GUI_Font13_ASCII);
    disp_center(x, y + LED_R + 26, "LED");
    /* 序号 */
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", idx + 1);
        GUI_SetColor(UI_CLR_TEXT_HI);
        GUI_SetFont(&GUI_Font8_ASCII);
        switch_panel_textmode();
        disp_center(x, y - LED_R - 14, buf);
    }
}

/* 绘制一个拨码开关（竖式 2 段：上 ON 绿 / 下 OFF 灰） */
static void draw_switch(int x, int y, int idx, int selected)
{
    char label[16];
    uint8_t on = DSO_Switch_Get(idx);

    /* 外框：选中橙色高亮，否则卡片描边 */
    if (selected) {
        GUI_SetColor(UI_CLR_ACCENT);
        GUI_FillRoundedRect(x - 2, y - 2, x + SW_W + 1, y + SW_H + 1, 4);
        GUI_SetColor(0xFFA030u);  /* 橙色高亮 */
        GUI_DrawRoundedRect(x - 2, y - 2, x + SW_W + 1, y + SW_H + 1, 4);
    } else {
        GUI_SetColor(UI_CLR_CARD);
        GUI_FillRoundedRect(x, y, x + SW_W - 1, y + SW_H - 1, 4);
        GUI_SetColor(UI_CLR_CARD_BR);
        GUI_DrawRoundedRect(x, y, x + SW_W - 1, y + SW_H - 1, 4);
    }

    /* 上下两段：上 ON / 下 OFF */
    int mid = y + SW_H / 2;
    if (on) {
        GUI_SetColor(UI_CLR_RUN_FG);            /* 绿色 = ON 上半 */
        GUI_FillRect(x + 3, y + 3, x + SW_W - 4, mid - 1);
        GUI_SetColor(0x182820u);
        GUI_FillRect(x + 3, mid + 1, x + SW_W - 4, y + SW_H - 4);
    } else {
        GUI_SetColor(0x182820u);
        GUI_FillRect(x + 3, y + 3, x + SW_W - 4, mid - 1);
        GUI_SetColor(UI_CLR_RUN_BG);
        GUI_FillRect(x + 3, mid + 1, x + SW_W - 4, y + SW_H - 4);
    }
    GUI_SetColor(UI_CLR_DIV);
    GUI_DrawHLine(mid, x + 3, x + SW_W - 4);

    /* 分界 + 状态标记 */
    GUI_SetColor(selected ? 0xFFA030u : UI_CLR_TEXT_MID);
    GUI_SetFont(&GUI_Font8_ASCII);
    switch_panel_textmode();
    disp_center(x + SW_W / 2, mid - 12, on ? "ON" : "  ");
    disp_center(x + SW_W / 2, mid + 5,  on ? "  " : "OFF");

    /* 序号 SWn */
    snprintf(label, sizeof(label), "SW%d", idx + 1);
    GUI_SetFont(&GUI_Font13_ASCII);
    GUI_SetColor(selected ? 0xFFA030u : UI_CLR_TEXT_HI);
    switch_panel_textmode();
    disp_center(x + SW_W / 2, y + SW_H + 4, label);
}

void UI_Switch_OnScreenShown(void)
{
    s_focus = 0;
}

int UI_Switch_GetFocus(void)
{
    return s_focus;
}

void UI_Switch_Ec1Detent(int dir)
{
    s_focus += dir;
    if (s_focus < 0)        s_focus = SW_TOTAL - 1;
    if (s_focus >= SW_TOTAL) s_focus = 0;
    UI_Nav_MarkDirty();
}

void UI_Switch_Ec1Click(void)
{
    DSO_Switch_Toggle(s_focus);
    UI_Nav_MarkDirty();
}

void UI_Switch_Draw(uint32_t tick_ms)
{
    int i;
    (void)tick_ms;
    switch_panel_textmode();

    /* ---- LED 区 ---- */
    GUI_SetFont(&GUI_Font16_ASCII);
    GUI_SetColor(UI_CLR_TEXT_HI);
    switch_panel_textmode();
    disp_center(UI_DISP_W / 2, UI_CONTENT_TOP + 6, "STATUS LED");

    {
        const int ly = UI_CONTENT_TOP + 52;
        const int lx0 = UI_DISP_W / 2 - 2 * 150;
        draw_led(lx0 + 0,    ly, 0, "HEART", UI_CLR_RUN_FG);    /* 绿 */
        draw_led(lx0 + 150,  ly, 1, "POWER", 0x44AAFFu);         /* 蓝 */
        draw_led(lx0 + 300,  ly, 2, "ALARM", UI_CLR_CH1);        /* 红 */
    }

    /* ---- 10-BIT DIP ---- */
    GUI_SetColor(UI_CLR_TEXT_MID);
    GUI_SetFont(&GUI_Font13_ASCII);
    switch_panel_textmode();
    disp_center(UI_DISP_W / 2, 160, "10-BIT DIP");

    {
        const int cols = 5;
        const int y0 = 182;
        const int x0 = (UI_DISP_W - (cols * SW_W + (cols - 1) * SW_GAP)) / 2;
        for (i = 0; i < SW_DIP10_COUNT; i++) {
            int r = i / cols;
            int c = i % cols;
            draw_switch(x0 + c * (SW_W + SW_GAP), y0 + r * (SW_H + 26), i, i == s_focus);
        }
    }

    /* ---- 4-BIT DIP ---- */
    GUI_SetColor(UI_CLR_TEXT_MID);
    GUI_SetFont(&GUI_Font13_ASCII);
    switch_panel_textmode();
    disp_center(UI_DISP_W / 2, 320, "4-BIT DIP");

    {
        const int cols = 4;
        const int y0 = 342;
        const int x0 = (UI_DISP_W - (cols * SW_W + (cols - 1) * SW_GAP)) / 2;
        for (i = 0; i < SW_DIP4_COUNT; i++) {
            int c = i % cols;
            draw_switch(x0 + c * (SW_W + SW_GAP), y0, SW_DIP10_COUNT + i,
                        (SW_DIP10_COUNT + i) == s_focus);
        }
    }
}
