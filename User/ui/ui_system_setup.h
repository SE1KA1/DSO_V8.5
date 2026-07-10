/**
 * @file ui_system_setup.h
 * @brief 系统设置页：ADC 8-bit/10-bit 格式切换与维护项焦点。
 *
 * EC 路由见 ui_nav.c；格式变更经 DSO_Acq_RequestSampleFormat 写入 pending，由引擎 Apply。
 */
#ifndef UI_SYSTEM_SETUP_H
/* 宏定义: UI_SYSTEM_SETUP_H - 头文件保护宏。 */
#define UI_SYSTEM_SETUP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 进入 System 页时调用（复位焦点行） */
void UI_System_OnScreenShown(void);

/** 当前焦点行：0 = ADC 分辨率（下拉），1 = 维护项 */
uint8_t UI_System_GetFocusRow(void);

/** 旋钮步进：与 Channel / Trigger 页相同由 ui_nav 去抖后调用 */
void UI_System_Ec1Detent(int dir);
void UI_System_Ec2Detent(int dir);

/** 当前 DSO 采集选项文案：10-bit / 8-bit 三路(3L)（供绘制） */
const char *UI_System_AdcFormatLabel(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_SYSTEM_SETUP_H */
