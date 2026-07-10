/**
 * @file ui_measure_units.h
 * @brief 测量页/游标：ADC 码差 → 电压的 inline 换算（复用 dso_ui_bridge 帧元数据）。
 */
#ifndef UI_MEASURE_UNITS_H
/* 宏定义: UI_MEASURE_UNITS_H - 头文件保护宏。 */
#define UI_MEASURE_UNITS_H

#include "dso/dso_ui_bridge.h"

/*
*********************************************************************************************************
*   函 数 名: UI_Measure_FrameAdcLsbV
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static inline float UI_Measure_FrameAdcLsbV(const DSO_UI_Frame *frame)
{
    return DSO_UI_FrameAdcLsbV(frame);
}

/*
*********************************************************************************************************
*   函 数 名: UI_Measure_CodeDeltaToVolts
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static inline float UI_Measure_CodeDeltaToVolts(float code_delta, const DSO_UI_Frame *frame)
{
    return DSO_UI_CodeDeltaToVolts(code_delta, DSO_UI_FrameAdcLsbV(frame));
}

#endif /* UI_MEASURE_UNITS_H */
