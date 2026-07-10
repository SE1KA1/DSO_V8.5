/**
 * @file ui_screens.h
 * @brief 非示波器子页面的屏幕 ID 与绘制/生命周期钩子。
 *
 * UI_SCR_SCOPE 的波形区由 MainTask 专用路径绘制，本模块不处理示波器帧渲染。
 * 其它页通过 UI_Screen_Draw 在 MainTask 的非 scope 分支按需重绘。
 */
#ifndef UI_SCREENS_H
/* 宏定义: UI_SCREENS_H - 头文件保护宏。 */
#define UI_SCREENS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_SCR_WELCOME = 0,
    UI_SCR_MAIN_MENU,
    UI_SCR_SCOPE,
    UI_SCR_CHANNEL,
    UI_SCR_TRIGGER,
    UI_SCR_MEASURE,
    UI_SCR_SYSTEM,
    UI_SCR_COUNT
} UI_ScreenId_t;

/**
 * 绘制整屏界面。tick_ms 为当前子界面内已停留毫秒数（动画用）。
 * UI_SCR_SCOPE：实际波形由 MainTask 内 MainTask_Scope_* 绘制，此处勿依赖。
 */
void UI_Screen_Draw(UI_ScreenId_t id, uint32_t tick_ms);

/** 进入通道页时由导航调用（重置 EC 焦点等） */
void UI_Channel_OnScreenShown(void);
/** 通道页 EC1：切换焦点 0..3（CH1/CH2 的 ON/OFF 与耦合），dir 为 +1/-1 */
void UI_Channel_Ec1Detent(int dir);
/** 通道页 EC2：焦点 0/1 切换 ON/OFF，焦点 2/3 循环 DC/AC/GND */
void UI_Channel_Ec2Detent(int dir);

/*
 * 无对应 BSP/DSO 数据时的弱钩子（默认在 ui_screens.c）。
 * 在任意其它 .c 中提供同名非 weak 函数即可接入真实实现。
 */
const char *UI_Port_TrigSourceLabel(void);
const char *UI_Port_TrigCouplingLabel(void);
void UI_Port_FormatRiseTimeStr(unsigned ch_idx, char *buf, unsigned buf_size);

#ifdef __cplusplus
}
#endif

#endif /* UI_SCREENS_H */
