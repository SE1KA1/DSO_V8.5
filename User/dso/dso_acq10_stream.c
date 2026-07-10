/**
 * @file dso_acq10_stream.c
 * @brief 10-bit stream 合并实现。
 */

#include "dso_acq10_stream.h"

/*
*********************************************************************************************************
*   函 数 名: min_u32
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t min_u32(uint32_t a, uint32_t b)
{
    return (a < b) ? a : b;
}

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
                                    uint16_t *out_samples)
{
    uint32_t i;

    if ((lane0 == 0) || (lane1 == 0) || (out_samples == 0)) {
        return;
    }

    for (i = 0u; i < pair_count; ++i) {
        out_samples[i * 2u] = lane0[i];
        out_samples[i * 2u + 1u] = lane1[i];
    }
}

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
                                          uint32_t max_samples)
{
    uint32_t pair_count;
    uint32_t start;
    uint32_t i;

    if ((ring_count == 0u) || (lane0 == 0) || (lane1 == 0) || (out_samples == 0)) {
        return 0u;
    }

    pair_count = min_u32(ring_count, max_samples / 2u);
    start = (dma_pos + ring_count - pair_count) % ring_count;

    for (i = 0u; i < pair_count; ++i) {
        uint32_t idx = (start + i) % ring_count;
        out_samples[i * 2u] = lane0[idx];
        out_samples[i * 2u + 1u] = lane1[idx];
    }

    return pair_count * 2u;
}

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
                                       uint32_t max_samples)
{
    uint32_t delta;
    uint32_t pair_count;
    uint32_t start;
    uint32_t i;

    if ((ring_count == 0u) || (lane0 == 0) || (lane1 == 0) || (out_samples == 0)) {
        return 0u;
    }

    delta = (dma_pos >= prev_dma_pos)
          ? (dma_pos - prev_dma_pos)
          : (ring_count - prev_dma_pos + dma_pos);
    pair_count = min_u32(delta, max_samples / 2u);
    start = (dma_pos + ring_count - pair_count) % ring_count;

    for (i = 0u; i < pair_count; ++i) {
        uint32_t idx = (start + i) % ring_count;
        out_samples[i * 2u] = lane0[idx];
        out_samples[i * 2u + 1u] = lane1[idx];
    }

    return pair_count * 2u;
}
