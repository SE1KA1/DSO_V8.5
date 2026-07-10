/**
 * @file ui_nav.h
 * @brief 页面状态机与输入事件 API（Poll 在 UI_EcPollTask，Draw 在 MainTask）。
 *
 * 当前界面：UI_Nav_GetScreen / SetScreen。
 * 重绘请求：UI_Nav_MarkDirty → MainTask 非 scope 分支 ConsumeRedraw。
 * 示波器子模式：UI_SCOPE_UI_WAVE（旋钮调参） vs UI_SCOPE_UI_MENU（侧栏五项）。
 */
#ifndef UI_NAV_H
/* 宏定义: UI_NAV_H - 头文件保护宏。 */
#define UI_NAV_H

#include "ui_screens.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_NAV_KEY_UP = 0,
    UI_NAV_KEY_DOWN,
    UI_NAV_KEY_LEFT,
    UI_NAV_KEY_RIGHT,
    UI_NAV_KEY_OK,
    UI_NAV_KEY_BACK,
    UI_NAV_KEY_MENU,
} UI_NavKey_t;

/** 由 ui_ec_poll_task 周期调用，勿与 MainTask 重复调用 */
void UI_Nav_Poll(void);

/** 由底层按键驱动调用；当前为占位，后续接菜单焦点与 UI_Nav_SetScreen */
void UI_Nav_OnKey(UI_NavKey_t key);

/** 逻辑当前界面（接键后切换；自动演示模式下可由外部同步） */
void UI_Nav_SetScreen(UI_ScreenId_t id);
UI_ScreenId_t UI_Nav_GetScreen(void);

/**
 * 主菜单卡片焦点 0..4（UP/DOWN 修改并请求重绘；其它界面可忽略）。
 */
int UI_Nav_GetMenuFocus(void);

/** 旋钮功能模式：0=档位调节（ms/div, V/div），1=位置调节（CH1/CH2 垂直位移） */
int UI_Nav_GetKnobMode(void);
const char *UI_Nav_GetKnobModeLabel(void);

/** 示波器页 UI：波形旋钮控制 vs 右侧菜单选择（EC1 长按切换/确认见 ui_nav.c） */
typedef enum {
    UI_SCOPE_UI_WAVE = 0,
    UI_SCOPE_UI_MENU
} UI_ScopeUiMode_t;

UI_ScopeUiMode_t UI_Nav_GetScopeUiMode(void);

/** 测量值/参数等变化时调用，下一帧非示波器界面会重绘（与按键效果相同） */
void UI_Nav_MarkDirty(void);

/** MainTask 用：若返回 1 表示应重绘，并清除内部请求标志 */
int UI_Nav_ConsumeRedraw(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_NAV_H */
