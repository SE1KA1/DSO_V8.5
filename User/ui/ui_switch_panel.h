/**
 * @file ui_switch_panel.h
 * @brief 开关面板页：3 个 LED 指示灯 + 14 位拨码开关，EC1 导航。
 *
 * 仿 ui_system_setup 的焦点模型。EC1 旋转移动光标（0..13 循环），
 * EC1 点击翻转当前开关。EC2 点击返回 Scope（导航自动处理）。
 */
#ifndef UI_SWITCH_PANEL_H
#define UI_SWITCH_PANEL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 进入本页时由导航调用：重置光标到 0。 */
void UI_Switch_OnScreenShown(void);

/** 当前光标索引（0..13），供绘制高亮。 */
int UI_Switch_GetFocus(void);

/** EC1 旋转：光标 0..13 循环移动。 */
void UI_Switch_Ec1Detent(int dir);

/** EC1 点击：翻转当前开关。 */
void UI_Switch_Ec1Click(void);

/** 绘制开关面板主体（LED 区 + 两组拨码开关）。由 screen_switch 调用。 */
void UI_Switch_Draw(uint32_t tick_ms);

#ifdef __cplusplus
}
#endif
#endif /* UI_SWITCH_PANEL_H */
