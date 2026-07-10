/**
 * @file dso_acq10_stream.h
 * @brief 10-bit 双 lane 合并与 ring-copy 纯 C helper（可 host 测试）。
 */

#ifndef DSO_ACQ10_STREAM_H
/* 宏定义: DSO_ACQ10_STREAM_H - 头文件保护宏。 */
#define DSO_ACQ10_STREAM_H

#include <stdint.h>

/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10Stream_InterleaveDual
*   功能说明: 处理 10-bit 采集后端的双 lane 样本交织、recent/new 窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_Acq10Stream_InterleaveDual(const uint16_t *lane0,
                                    const uint16_t *lane1,
                                    uint32_t pair_count,
                                    uint16_t *out_samples);
/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10Stream_CopyRecentMerged
*   功能说明: 处理 10-bit 采集后端的双 lane 样本交织、recent/new 窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq10Stream_CopyRecentMerged(uint32_t dma_pos,
                                          uint32_t ring_count,
                                          const uint16_t *lane0,
                                          const uint16_t *lane1,
                                          uint16_t *out_samples,
                                          uint32_t max_samples);
/*
*********************************************************************************************************
*   函 数 名: DSO_Acq10Stream_CopyNewMerged
*   功能说明: 处理 10-bit 采集后端的双 lane 样本交织、recent/new 窗口拷贝。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_Acq10Stream_CopyNewMerged(uint32_t prev_dma_pos,
                                       uint32_t dma_pos,
                                       uint32_t ring_count,
                                       const uint16_t *lane0,
                                       const uint16_t *lane1,
                                       uint16_t *out_samples,
                                       uint32_t max_samples);

#endif /* DSO_ACQ10_STREAM_H */
