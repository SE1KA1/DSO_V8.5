/**
 * @file ui_channel_ctrl.h
 * @brief 逻辑通道 CH1/CH2 显示开关（与 Solo、MainTask 波形可见性组合使用）。
 */
#ifndef UI_CHANNEL_CTRL_H
/* 宏定义: UI_CHANNEL_CTRL_H - 头文件保护宏。 */
#define UI_CHANNEL_CTRL_H

#include "dso/dso_vertical.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 逻辑通道开关（CH1/CH2 ON/OFF）。
 * 默认由 ui_channel_ctrl.c 内存储实现；在其它 .c 中可提供强符号覆盖 Get/Set，
 * 或通过 UI_ChannelCtrl_OnEnableChanged 接到 DSO / solo / BSP。
 */
bool UI_ChannelCtrl_GetEnabled(DSO_ChannelId ch);
void UI_ChannelCtrl_SetEnabled(DSO_ChannelId ch, bool on);

/**
 * 开关变化通知：weak 默认为空。
 * 提供强符号实现以同步采集、波形 solo、继电器等。
 */
void UI_ChannelCtrl_OnEnableChanged(DSO_ChannelId ch, bool on);

#ifdef __cplusplus
}
#endif

#endif /* UI_CHANNEL_CTRL_H */
