/**
 * @file dso_acq10_tim_trigger.c
 * @brief 10-bit TIM trigger 配置实现。
 */

#include "dso_acq10_tim_trigger.h"

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_GetTimTriggerMode
*   功能说明: 查询 10-bit 采集后端使用的 TIM1 触发方式。
*   形    参: 无。
*   返 回 值: 当前固定返回 PWM1 触发模式。
*********************************************************************************************************
*/
DSO_Acq10TimTriggerMode DSO_Acq10_GetTimTriggerMode(void)
{
    return DSO_ACQ10_TIM_TRIGGER_MODE_PWM1;
}
