/**
 * @file dso_serial_debug_math.h
 * @brief 串口调试数学辅助 inline/声明。
 */

#ifndef DSO_SERIAL_DEBUG_MATH_H
/* 宏定义: DSO_SERIAL_DEBUG_MATH_H - 头文件保护宏。 */
#define DSO_SERIAL_DEBUG_MATH_H

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_NormalizeToCenteredPeriod
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static inline float DSO_SerialDebug_NormalizeToCenteredPeriod(float value,
                                                              float period)
{
    float half_period;

    if (period <= 0.0f) {
        return value;
    }

    half_period = 0.5f * period;
    while (value < -half_period) {
        value += period;
    }
    while (value >= half_period) {
        value -= period;
    }

    return value;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_UnwrapToAnchor
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static inline float DSO_SerialDebug_UnwrapToAnchor(float value,
                                                   float anchor,
                                                   float period)
{
    float delta;
    float half_period;

    if (period <= 0.0f) {
        return value;
    }

    delta = value - anchor;
    half_period = 0.5f * period;
    while (delta > half_period) {
        value -= period;
        delta -= period;
    }
    while (delta < -half_period) {
        value += period;
        delta += period;
    }

    return value;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_SerialDebug_PeriodicMidpoint
*   功能说明: 生成 DSO 串口诊断快照、格式化文本或发送调试信息。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static inline float DSO_SerialDebug_PeriodicMidpoint(float a,
                                                     float b,
                                                     float period)
{
    if (period <= 0.0f) {
        return 0.5f * (a + b);
    }

    b = DSO_SerialDebug_UnwrapToAnchor(b, a, period);
    return DSO_SerialDebug_NormalizeToCenteredPeriod(0.5f * (a + b), period);
}

#endif /* DSO_SERIAL_DEBUG_MATH_H */
