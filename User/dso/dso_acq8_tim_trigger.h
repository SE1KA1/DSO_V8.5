/**
 * @file dso_acq8_tim_trigger.h
 * @brief 8-bit TIM 触发模式选择。
 */

#ifndef DSO_ACQ8_TIM_TRIGGER_H
/* 宏定义: DSO_ACQ8_TIM_TRIGGER_H - 头文件保护宏。 */
#define DSO_ACQ8_TIM_TRIGGER_H

typedef enum {
    /* 使用定时器 timing event 触发 ADC。 */
    DSO_ACQ8_TIM_TRIGGER_MODE_TIMING = 0,
    /* 使用 TIM PWM1 边沿触发 ADC，当前 8-bit 主路径使用该模式。 */
    DSO_ACQ8_TIM_TRIGGER_MODE_PWM1 = 1
} DSO_Acq8TimTriggerMode;

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetTimTriggerMode
*   功能说明: 返回 8-bit 后端当前采用的 TIM 触发模式。
*   形    参: 无。
*   返 回 值: DSO_Acq8TimTriggerMode 枚举。
*********************************************************************************************************
*/
DSO_Acq8TimTriggerMode DSO_Acq8_GetTimTriggerMode(void);

#endif /* DSO_ACQ8_TIM_TRIGGER_H */
