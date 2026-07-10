/**
 * @file dso_lane_merge.h
 * @brief 多 lane common-delta 对齐合并原语。
 */

#ifndef DSO_LANE_MERGE_H
/* 宏定义: DSO_LANE_MERGE_H - 头文件保护宏。 */
#define DSO_LANE_MERGE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t lane0_delta;
    uint32_t lane1_delta;
    uint32_t lane2_delta;
    uint32_t common_delta;
    bool phase_sync_error;
} DSO_LaneDeltaInfo;

typedef struct {
    uint32_t start0;
    uint32_t start1;
    uint32_t start2;
    uint32_t lane_count;
} DSO_LaneMergeWindow;

/*
*********************************************************************************************************
*   函 数 名: DSO_LaneMerge_CommonDelta3
*   功能说明: 根据多 lane DMA 位置计算共同窗口并执行交织样本合并。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_LaneDeltaInfo DSO_LaneMerge_CommonDelta3(uint32_t prev_pos,
                                             uint32_t pos0,
                                             uint32_t pos1,
                                             uint32_t pos2,
                                             uint32_t ring_size);
/*
*********************************************************************************************************
*   函 数 名: DSO_LaneMerge_CommonDelta2
*   功能说明: 根据多 lane DMA 位置计算共同窗口并执行交织样本合并。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_LaneDeltaInfo DSO_LaneMerge_CommonDelta2(uint32_t prev_pos,
                                             uint32_t pos0,
                                             uint32_t pos1,
                                             uint32_t ring_size);

/*
*********************************************************************************************************
*   函 数 名: DSO_LaneMerge_ResolveRecentWindow2
*   功能说明: 根据多 lane DMA 位置计算共同窗口并执行交织样本合并。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_LaneMergeWindow DSO_LaneMerge_ResolveRecentWindow2(uint32_t end0,
                                                       uint32_t end1,
                                                       uint32_t max_lane_count,
                                                       uint32_t ring_size);
/*
*********************************************************************************************************
*   函 数 名: DSO_LaneMerge_ResolveRecentWindow3
*   功能说明: 根据多 lane DMA 位置计算共同窗口并执行交织样本合并。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_LaneMergeWindow DSO_LaneMerge_ResolveRecentWindow3(uint32_t end0,
                                                       uint32_t end1,
                                                       uint32_t end2,
                                                       uint32_t max_lane_count,
                                                       uint32_t ring_size);
/*
*********************************************************************************************************
*   函 数 名: DSO_LaneMerge_ResolveNewWindow2
*   功能说明: 根据多 lane DMA 位置计算共同窗口并执行交织样本合并。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_LaneMergeWindow DSO_LaneMerge_ResolveNewWindow2(uint32_t prev_common_end,
                                                    uint32_t common_end,
                                                    uint32_t end0,
                                                    uint32_t end1,
                                                    uint32_t max_lane_count,
                                                    uint32_t ring_size);
/*
*********************************************************************************************************
*   函 数 名: DSO_LaneMerge_ResolveNewWindow3
*   功能说明: 根据多 lane DMA 位置计算共同窗口并执行交织样本合并。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_LaneMergeWindow DSO_LaneMerge_ResolveNewWindow3(uint32_t prev_common_end,
                                                    uint32_t common_end,
                                                    uint32_t end0,
                                                    uint32_t end1,
                                                    uint32_t end2,
                                                    uint32_t max_lane_count,
                                                    uint32_t ring_size);
/*
*********************************************************************************************************
*   函 数 名: DSO_LaneMerge_ResolveCommonRecentWindow2
*   功能说明: 根据多 lane DMA 位置计算共同窗口并执行交织样本合并。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_LaneMergeWindow DSO_LaneMerge_ResolveCommonRecentWindow2(uint32_t common_end,
                                                             uint32_t max_lane_count,
                                                             uint32_t ring_size);
/*
*********************************************************************************************************
*   函 数 名: DSO_LaneMerge_ResolveCommonRecentWindow3
*   功能说明: 根据多 lane DMA 位置计算共同窗口并执行交织样本合并。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_LaneMergeWindow DSO_LaneMerge_ResolveCommonRecentWindow3(uint32_t common_end,
                                                             uint32_t max_lane_count,
                                                             uint32_t ring_size);
/*
*********************************************************************************************************
*   函 数 名: DSO_LaneMerge_ResolveCommonNewWindow2
*   功能说明: 根据多 lane DMA 位置计算共同窗口并执行交织样本合并。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_LaneMergeWindow DSO_LaneMerge_ResolveCommonNewWindow2(uint32_t prev_common_end,
                                                          uint32_t common_end,
                                                          uint32_t max_lane_count,
                                                          uint32_t ring_size);
/*
*********************************************************************************************************
*   函 数 名: DSO_LaneMerge_ResolveCommonNewWindow3
*   功能说明: 根据多 lane DMA 位置计算共同窗口并执行交织样本合并。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
DSO_LaneMergeWindow DSO_LaneMerge_ResolveCommonNewWindow3(uint32_t prev_common_end,
                                                          uint32_t common_end,
                                                          uint32_t max_lane_count,
                                                          uint32_t ring_size);

/*
*********************************************************************************************************
*   函 数 名: DSO_LaneMerge_CopyAligned2
*   功能说明: 根据多 lane DMA 位置计算共同窗口并执行交织样本合并。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_LaneMerge_CopyAligned2(uint32_t start0,
                                    uint32_t start1,
                                    uint32_t lane_count,
                                    uint32_t ring_size,
                                    const uint16_t *lane0,
                                    const uint16_t *lane1,
                                    uint16_t *out_samples,
                                    uint32_t max_samples);

/*
*********************************************************************************************************
*   函 数 名: DSO_LaneMerge_CopyAligned3
*   功能说明: 根据多 lane DMA 位置计算共同窗口并执行交织样本合并。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_LaneMerge_CopyAligned3(uint32_t start0,
                                    uint32_t start1,
                                    uint32_t start2,
                                    uint32_t lane_count,
                                    uint32_t ring_size,
                                    const uint16_t *lane0,
                                    const uint16_t *lane1,
                                    const uint16_t *lane2,
                                    uint16_t *out_samples,
                                    uint32_t max_samples);

/*
*********************************************************************************************************
*   函 数 名: DSO_LaneMerge_CopyInterleaved3
*   功能说明: 根据多 lane DMA 位置计算共同窗口并执行交织样本合并。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_LaneMerge_CopyInterleaved3(uint32_t start_index,
                                        uint32_t lane_count,
                                        uint32_t ring_size,
                                        const uint16_t *lane0,
                                        const uint16_t *lane1,
                                        const uint16_t *lane2,
                                        uint16_t *out_samples,
                                        uint32_t max_samples);

/*
*********************************************************************************************************
*   函 数 名: DSO_LaneMerge_CopyInterleaved2
*   功能说明: 根据多 lane DMA 位置计算共同窗口并执行交织样本合并。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t DSO_LaneMerge_CopyInterleaved2(uint32_t start_index,
                                        uint32_t lane_count,
                                        uint32_t ring_size,
                                        const uint16_t *lane0,
                                        const uint16_t *lane1,
                                        uint16_t *out_samples,
                                        uint32_t max_samples);

#endif /* DSO_LANE_MERGE_H */
