/**
 * @file roll_history_ring.h
 * @brief RollHistory CH1/CH2 环形缓冲操作 API。
 */

#ifndef ROLL_HISTORY_RING_H
/* 宏定义: ROLL_HISTORY_RING_H - 头文件保护宏。 */
#define ROLL_HISTORY_RING_H

#include <stdint.h>

/** Monotonic sample count (next global index after last append). */
uint64_t RollHistoryRing_GetTotalWrittenCh1(void);
/*
*********************************************************************************************************
*   函 数 名: RollHistoryRing_GetTotalWrittenCh2
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint64_t RollHistoryRing_GetTotalWrittenCh2(void);

/**
 * Copy contiguous CH1 samples in time order starting at global index global_start.
 * Clamps to data currently present in the ring. Returns samples copied.
 */
uint32_t RollHistoryRing_CopySpanCh1(uint64_t global_start, uint32_t count, uint16_t *dst);

/*
*********************************************************************************************************
*   函 数 名: RollHistoryRing_CopySpanCh2
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t RollHistoryRing_CopySpanCh2(uint64_t global_start, uint32_t count, uint16_t *dst);

/*
*********************************************************************************************************
*   函 数 名: RollHistoryRing_Init
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void RollHistoryRing_Init(void);
/*
*********************************************************************************************************
*   函 数 名: RollHistoryRing_Clear
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void RollHistoryRing_Clear(void);

/*
*********************************************************************************************************
*   函 数 名: RollHistoryRing_AppendCh1
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void RollHistoryRing_AppendCh1(const uint16_t *samples, uint32_t count);
/*
*********************************************************************************************************
*   函 数 名: RollHistoryRing_AppendCh2
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void RollHistoryRing_AppendCh2(const uint16_t *samples, uint32_t count);

/** Copy up to max_out newest CH1 samples into dst (oldest first). Returns count copied. */
uint32_t RollHistoryRing_CopyLatestCh1(uint32_t max_out, uint16_t *dst);

/** Copy up to max_out newest CH2 samples into dst (oldest first). Returns count copied. */
uint32_t RollHistoryRing_CopyLatestCh2(uint32_t max_out, uint16_t *dst);

#endif /* ROLL_HISTORY_RING_H */
