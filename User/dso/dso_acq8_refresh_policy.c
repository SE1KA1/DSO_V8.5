/**
 * @file dso_acq8_refresh_policy.c
 * @brief 8-bit refresh 策略实现。
 */

#include "dso_acq8_refresh_policy.h"

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetPreviewRefreshPolicy
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_Acq8PreviewRefreshPolicy DSO_Acq8_GetPreviewRefreshPolicy(void)
{
    DSO_Acq8PreviewRefreshPolicy policy = {
        .allow_runtime_refresh = false,
        .pause_timer = true,
        .stop_adc1_dma = false,
        .stop_adc2_dma = false,
        .stop_adc3_dma = true,
        .restart_adc1_dma = false,
        .restart_adc2_dma = false,
        .restart_adc3_dma = true,
    };

    return policy;
}
