/**
 * @file roll_history_ring.c
 * @brief Roll ring push/copy/seq 实现。
 */

#include "roll_history_ring.h"

#include "roll_history_mem.h"

#include <string.h>

static uint64_t s_seq_ch1;
static uint64_t s_seq_ch2;

/*
*********************************************************************************************************
*   函 数 名: RollHistoryRing_GetTotalWrittenCh1
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint64_t RollHistoryRing_GetTotalWrittenCh1(void)
{
    return s_seq_ch1;
}

/*
*********************************************************************************************************
*   函 数 名: RollHistoryRing_GetTotalWrittenCh2
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint64_t RollHistoryRing_GetTotalWrittenCh2(void)
{
    return s_seq_ch2;
}

/*
*********************************************************************************************************
*   函 数 名: ring_copy_span
*   功能说明: 在 Roll 历史 ring 中完成样本追加、窗口复制、清空或写入进度查询。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t ring_copy_span(const uint16_t *ring,
                               uint64_t seq,
                               uint32_t cap,
                               uint64_t global_start,
                               uint32_t count,
                               uint16_t *dst)
{
    uint64_t oldest;
    uint64_t end_exclusive;
    uint64_t max_count;
    uint32_t i;

    if ((ring == NULL) || (cap == 0u) || (dst == NULL) || (count == 0u) || (seq == 0u)) {
        return 0u;
    }

    oldest = (seq > (uint64_t)cap) ? (seq - (uint64_t)cap) : 0u;
    end_exclusive = seq;

    if (global_start < oldest) {
        uint64_t skip = oldest - global_start;

        if (skip >= (uint64_t)count) {
            return 0u;
        }
        global_start = oldest;
        count -= (uint32_t)skip;
    }

    if (global_start >= end_exclusive) {
        return 0u;
    }

    max_count = end_exclusive - global_start;
    if ((uint64_t)count > max_count) {
        count = (uint32_t)max_count;
    }

    if (count == 0u) {
        return 0u;
    }

    for (i = 0u; i < count; i++) {
        uint64_t g = global_start + (uint64_t)i;

        dst[i] = ring[g % (uint64_t)cap];
    }

    return count;
}

/*
*********************************************************************************************************
*   函 数 名: RollHistoryRing_CopySpanCh1
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t RollHistoryRing_CopySpanCh1(uint64_t global_start, uint32_t count, uint16_t *dst)
{
    return ring_copy_span(RollHistoryMem_RingCh1(),
                          s_seq_ch1,
                          RollHistoryMem_RingCap(),
                          global_start,
                          count,
                          dst);
}

/*
*********************************************************************************************************
*   函 数 名: RollHistoryRing_CopySpanCh2
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t RollHistoryRing_CopySpanCh2(uint64_t global_start, uint32_t count, uint16_t *dst)
{
    return ring_copy_span(RollHistoryMem_RingCh2(),
                          s_seq_ch2,
                          RollHistoryMem_RingCap(),
                          global_start,
                          count,
                          dst);
}

/*
*********************************************************************************************************
*   函 数 名: RollHistoryRing_Init
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void RollHistoryRing_Init(void)
{
    RollHistoryRing_Clear();
}

/*
*********************************************************************************************************
*   函 数 名: RollHistoryRing_Clear
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void RollHistoryRing_Clear(void)
{
    uint16_t *r1 = RollHistoryMem_RingCh1();
    uint16_t *r2 = RollHistoryMem_RingCh2();
    uint32_t cap = RollHistoryMem_RingCap();

    s_seq_ch1 = 0u;
    s_seq_ch2 = 0u;

    if ((r1 != NULL) && (r2 != NULL) && (cap > 0u)) {
        (void)memset(r1, 0, (size_t)cap * sizeof(uint16_t));
        (void)memset(r2, 0, (size_t)cap * sizeof(uint16_t));
    }
}

/*
*********************************************************************************************************
*   函 数 名: append_ring
*   功能说明: 在 Roll 历史 ring 中完成样本追加、窗口复制、清空或写入进度查询。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void append_ring(uint16_t *ring,
                        uint64_t *seq,
                        uint32_t cap,
                        const uint16_t *samples,
                        uint32_t count)
{
    uint32_t i;

    if ((ring == NULL) || (cap == 0u) || (samples == NULL) || (count == 0u)) {
        return;
    }

    for (i = 0u; i < count; i++) {
        uint64_t s = *seq;

        ring[s % (uint64_t)cap] = samples[i];
        *seq = s + 1u;
    }
}

/*
*********************************************************************************************************
*   函 数 名: RollHistoryRing_AppendCh1
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void RollHistoryRing_AppendCh1(const uint16_t *samples, uint32_t count)
{
    append_ring(RollHistoryMem_RingCh1(), &s_seq_ch1, RollHistoryMem_RingCap(), samples, count);
}

/*
*********************************************************************************************************
*   函 数 名: RollHistoryRing_AppendCh2
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void RollHistoryRing_AppendCh2(const uint16_t *samples, uint32_t count)
{
    append_ring(RollHistoryMem_RingCh2(), &s_seq_ch2, RollHistoryMem_RingCap(), samples, count);
}

/*
*********************************************************************************************************
*   函 数 名: copy_latest
*   功能说明: 在 Roll 历史 ring 中完成样本追加、窗口复制、清空或写入进度查询。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t copy_latest(const uint16_t *ring,
                            uint64_t seq,
                            uint32_t cap,
                            uint32_t max_out,
                            uint16_t *dst)
{
    uint64_t want;
    uint64_t start;

    if ((ring == NULL) || (cap == 0u) || (dst == NULL) || (max_out == 0u) || (seq == 0u)) {
        return 0u;
    }

    want = (uint64_t)max_out;
    if (want > seq) {
        want = seq;
    }

    start = seq - want;
    return ring_copy_span(ring, seq, cap, start, (uint32_t)want, dst);
}

/*
*********************************************************************************************************
*   函 数 名: RollHistoryRing_CopyLatestCh1
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t RollHistoryRing_CopyLatestCh1(uint32_t max_out, uint16_t *dst)
{
    return copy_latest(RollHistoryMem_RingCh1(), s_seq_ch1, RollHistoryMem_RingCap(), max_out, dst);
}

/*
*********************************************************************************************************
*   函 数 名: RollHistoryRing_CopyLatestCh2
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t RollHistoryRing_CopyLatestCh2(uint32_t max_out, uint16_t *dst)
{
    return copy_latest(RollHistoryMem_RingCh2(), s_seq_ch2, RollHistoryMem_RingCap(), max_out, dst);
}
