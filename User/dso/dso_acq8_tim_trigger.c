/**
 * @file dso_acq8_tim_trigger.c
 * @brief 8-bit TIM trigger 实现。
 */

#include "dso_acq8_tim_trigger.h"

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetTimTriggerMode
*   功能说明: 查询 8-bit 采集后端使用的定时器触发方式。
*   形    参: 无。
*   返 回 值: 当前固定返回 PWM1 触发模式。
*********************************************************************************************************
*/
DSO_Acq8TimTriggerMode DSO_Acq8_GetTimTriggerMode(void)
{
    return DSO_ACQ8_TIM_TRIGGER_MODE_PWM1;
}
