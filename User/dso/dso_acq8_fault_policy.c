/**
 * @file dso_acq8_fault_policy.c
 * @brief 8-bit fault 检测与恢复策略。
 */

#include "dso_acq8_fault_policy.h"

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetFaultPolicy
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_Acq8FaultPolicy DSO_Acq8_GetFaultPolicy(void)
{
    DSO_Acq8FaultPolicy policy = {
        .phase_sync_error_limit = 8u,
        .auto_fallback_to_12bit = false,
    };

    return policy;
}
