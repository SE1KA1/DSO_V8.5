/**
 * @file dso_acq8_refresh_policy.h
 * @brief 8-bit preview 刷新策略。
 */

#ifndef DSO_ACQ8_REFRESH_POLICY_H
/* 宏定义: DSO_ACQ8_REFRESH_POLICY_H - 头文件保护宏。 */
#define DSO_ACQ8_REFRESH_POLICY_H

#include <stdbool.h>

typedef struct {
    bool allow_runtime_refresh;
    bool pause_timer;
    bool stop_adc1_dma;
    bool stop_adc2_dma;
    bool stop_adc3_dma;
    bool restart_adc1_dma;
    bool restart_adc2_dma;
    bool restart_adc3_dma;
} DSO_Acq8PreviewRefreshPolicy;

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq8_GetPreviewRefreshPolicy
*   功能说明: 处理 8-bit 高速采集后端的 lane 拓扑、DMA 状态、样本解包或窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_Acq8PreviewRefreshPolicy DSO_Acq8_GetPreviewRefreshPolicy(void);

#endif /* DSO_ACQ8_REFRESH_POLICY_H */
