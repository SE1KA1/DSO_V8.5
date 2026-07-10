/**
 * @file dso_lane_merge.c
 * @brief lane merge 算法实现。
 */

#include "dso_lane_merge.h"

#include <stddef.h>

/*
*********************************************************************************************************
*   函 数 名: ring_delta
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t ring_delta(uint32_t prev_pos, uint32_t pos, uint32_t ring_size)
{
    if (ring_size == 0u) {
        return 0u;
    }

    return (pos >= prev_pos)
         ? (pos - prev_pos)
         : ((ring_size - prev_pos) + pos);
}

/*
*********************************************************************************************************
*   函 数 名: ring_start
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t ring_start(uint32_t end_pos, uint32_t count, uint32_t ring_size)
{
    if ((ring_size == 0u) || (count == 0u)) {
        return 0u;
    }

    return (end_pos >= count)
         ? (end_pos - count)
         : (ring_size - (count - end_pos));
}

/*
*********************************************************************************************************
*   函 数 名: resolve_recent_window
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static DSO_LaneMergeWindow resolve_recent_window(uint32_t end0,
                                                 uint32_t end1,
                                                 uint32_t end2,
                                                 uint32_t max_lane_count,
                                                 uint32_t ring_size,
                                                 bool use_lane2)
{
    DSO_LaneMergeWindow out = {0};

    if ((ring_size == 0u) || (max_lane_count == 0u)) {
        return out;
    }

    out.lane_count = (max_lane_count > ring_size) ? ring_size : max_lane_count;
    out.start0 = ring_start(end0, out.lane_count, ring_size);
    out.start1 = ring_start(end1, out.lane_count, ring_size);
    out.start2 = use_lane2 ? ring_start(end2, out.lane_count, ring_size) : 0u;
    return out;
}

/*
*********************************************************************************************************
*   函 数 名: resolve_new_window
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static DSO_LaneMergeWindow resolve_new_window(uint32_t prev_common_end,
                                              uint32_t common_end,
                                              uint32_t end0,
                                              uint32_t end1,
                                              uint32_t end2,
                                              uint32_t max_lane_count,
                                              uint32_t ring_size,
                                              bool use_lane2)
{
    DSO_LaneMergeWindow out = {0};

    if ((ring_size == 0u) || (max_lane_count == 0u)) {
        return out;
    }

    out.lane_count = ring_delta(prev_common_end, common_end, ring_size);
    if (out.lane_count > max_lane_count) {
        out.lane_count = max_lane_count;
    }
    if (out.lane_count == 0u) {
        return out;
    }

    out.start0 = ring_start(end0, out.lane_count, ring_size);
    out.start1 = ring_start(end1, out.lane_count, ring_size);
    out.start2 = use_lane2 ? ring_start(end2, out.lane_count, ring_size) : 0u;
    return out;
}

/*
*********************************************************************************************************
*   函 数 名: resolve_common_recent_window
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static DSO_LaneMergeWindow resolve_common_recent_window(uint32_t common_end,
                                                        uint32_t max_lane_count,
                                                        uint32_t ring_size,
                                                        bool use_lane2)
{
    DSO_LaneMergeWindow out = {0};

    if ((ring_size == 0u) || (max_lane_count == 0u)) {
        return out;
    }

    out.lane_count = (max_lane_count > ring_size) ? ring_size : max_lane_count;
    out.start0 = ring_start(common_end, out.lane_count, ring_size);
    out.start1 = out.start0;
    out.start2 = use_lane2 ? out.start0 : 0u;
    return out;
}

/*
*********************************************************************************************************
*   函 数 名: resolve_common_new_window
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static DSO_LaneMergeWindow resolve_common_new_window(uint32_t prev_common_end,
                                                     uint32_t common_end,
                                                     uint32_t max_lane_count,
                                                     uint32_t ring_size,
                                                     bool use_lane2)
{
    DSO_LaneMergeWindow out = {0};

    if ((ring_size == 0u) || (max_lane_count == 0u)) {
        return out;
    }

    out.lane_count = ring_delta(prev_common_end, common_end, ring_size);
    if (out.lane_count > max_lane_count) {
        out.lane_count = max_lane_count;
    }
    if (out.lane_count == 0u) {
        return out;
    }

    out.start0 = ring_start(common_end, out.lane_count, ring_size);
    out.start1 = out.start0;
    out.start2 = use_lane2 ? out.start0 : 0u;
    return out;
}

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
                                             uint32_t ring_size)
{
    DSO_LaneDeltaInfo out = {0};

    out.lane0_delta = ring_delta(prev_pos, pos0, ring_size);
    out.lane1_delta = ring_delta(prev_pos, pos1, ring_size);
    out.lane2_delta = ring_delta(prev_pos, pos2, ring_size);
    out.common_delta = out.lane0_delta;

    if (out.lane1_delta < out.common_delta) {
        out.common_delta = out.lane1_delta;
    }
    if (out.lane2_delta < out.common_delta) {
        out.common_delta = out.lane2_delta;
    }

    out.phase_sync_error = (out.lane0_delta != out.lane1_delta) ||
                           (out.lane0_delta != out.lane2_delta);
    return out;
}

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
                                             uint32_t ring_size)
{
    DSO_LaneDeltaInfo out = {0};

    out.lane0_delta = ring_delta(prev_pos, pos0, ring_size);
    out.lane1_delta = ring_delta(prev_pos, pos1, ring_size);
    out.common_delta = out.lane0_delta;

    if (out.lane1_delta < out.common_delta) {
        out.common_delta = out.lane1_delta;
    }

    out.phase_sync_error = (out.lane0_delta != out.lane1_delta);
    return out;
}

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
                                                       uint32_t ring_size)
{
    return resolve_recent_window(end0, end1, 0u, max_lane_count, ring_size, false);
}

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
                                                       uint32_t ring_size)
{
    return resolve_recent_window(end0, end1, end2, max_lane_count, ring_size, true);
}

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
                                                    uint32_t ring_size)
{
    return resolve_new_window(prev_common_end,
                              common_end,
                              end0,
                              end1,
                              0u,
                              max_lane_count,
                              ring_size,
                              false);
}

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
                                                    uint32_t ring_size)
{
    return resolve_new_window(prev_common_end,
                              common_end,
                              end0,
                              end1,
                              end2,
                              max_lane_count,
                              ring_size,
                              true);
}

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
                                                             uint32_t ring_size)
{
    return resolve_common_recent_window(common_end, max_lane_count, ring_size, false);
}

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
                                                             uint32_t ring_size)
{
    return resolve_common_recent_window(common_end, max_lane_count, ring_size, true);
}

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
                                                          uint32_t ring_size)
{
    return resolve_common_new_window(prev_common_end, common_end, max_lane_count, ring_size, false);
}

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
                                                          uint32_t ring_size)
{
    return resolve_common_new_window(prev_common_end, common_end, max_lane_count, ring_size, true);
}

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
                                        uint32_t max_samples)
{
    return DSO_LaneMerge_CopyAligned2(start_index,
                                      start_index,
                                      lane_count,
                                      ring_size,
                                      lane0,
                                      lane1,
                                      out_samples,
                                      max_samples);
}

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
                                    uint32_t max_samples)
{
    uint32_t copied = 0u;

    if ((ring_size == 0u) || (lane0 == NULL) || (lane1 == NULL) ||
        (out_samples == NULL)) {
        return 0u;
    }

    for (uint32_t i = 0; i < lane_count; ++i) {
        uint32_t idx0 = (start0 + i) % ring_size;
        uint32_t idx1 = (start1 + i) % ring_size;

        if ((copied + 2u) > max_samples) {
            break;
        }

        out_samples[copied++] = lane0[idx0];
        out_samples[copied++] = lane1[idx1];
    }

    return copied;
}

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
                                        uint32_t max_samples)
{
    return DSO_LaneMerge_CopyAligned3(start_index,
                                      start_index,
                                      start_index,
                                      lane_count,
                                      ring_size,
                                      lane0,
                                      lane1,
                                      lane2,
                                      out_samples,
                                      max_samples);
}

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
                                    uint32_t max_samples)
{
    uint32_t copied = 0u;

    if ((ring_size == 0u) || (lane0 == NULL) || (lane1 == NULL) ||
        (lane2 == NULL) || (out_samples == NULL)) {
        return 0u;
    }

    for (uint32_t i = 0; i < lane_count; ++i) {
        uint32_t idx0 = (start0 + i) % ring_size;
        uint32_t idx1 = (start1 + i) % ring_size;
        uint32_t idx2 = (start2 + i) % ring_size;

        if ((copied + 3u) > max_samples) {
            break;
        }

        out_samples[copied++] = lane0[idx0];
        out_samples[copied++] = lane1[idx1];
        out_samples[copied++] = lane2[idx2];
    }

    return copied;
}
