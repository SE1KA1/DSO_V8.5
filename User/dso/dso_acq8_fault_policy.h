/**
 * @file dso_acq8_fault_policy.h
 * @brief 8-bit 采集故障策略（auto fallback 等，默认关闭）。
 */

#ifndef DSO_ACQ8_FAULT_POLICY_H
/* 宏定义: DSO_ACQ8_FAULT_POLICY_H - 头文件保护宏。 */
#define DSO_ACQ8_FAULT_POLICY_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t phase_sync_error_limit;
    bool auto_fallback_to_12bit;
} DSO_Acq8FaultPolicy;

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetFaultPolicy
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_Acq8FaultPolicy DSO_Acq8_GetFaultPolicy(void);

#endif /* DSO_ACQ8_FAULT_POLICY_H */
