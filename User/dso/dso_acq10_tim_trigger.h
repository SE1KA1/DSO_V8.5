/**
 * @file dso_acq10_tim_trigger.h
 * @brief 10-bit TIM1 触发模式选择（PWM1 等）。
 */

#ifndef DSO_ACQ10_TIM_TRIGGER_H
/* 宏定义: DSO_ACQ10_TIM_TRIGGER_H - 头文件保护宏。 */
#define DSO_ACQ10_TIM_TRIGGER_H

typedef enum {
    /* 使用定时器 timing event 触发 ADC。 */
    DSO_ACQ10_TIM_TRIGGER_MODE_TIMING = 0,
    /* 使用 TIM PWM1 边沿触发 ADC，当前 10-bit 主路径使用该模式。 */
    DSO_ACQ10_TIM_TRIGGER_MODE_PWM1 = 1
} DSO_Acq10TimTriggerMode;

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10_GetTimTriggerMode
*   功能说明: 返回 10-bit 后端当前采用的 TIM1 触发模式。
*   形    参: 无。
*   返 回 值: DSO_Acq10TimTriggerMode 枚举。
*********************************************************************************************************
*/
DSO_Acq10TimTriggerMode DSO_Acq10_GetTimTriggerMode(void);

#endif /* DSO_ACQ10_TIM_TRIGGER_H */
