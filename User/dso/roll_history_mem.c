/**
 * @file roll_history_mem.c
 * @brief RollHistory 内存初始化。
 */

#include "roll_history_mem.h"

#include "bsp_sdram_layout.h"

#include <stddef.h>
#include <string.h>

/*
 * 32 MB SDRAM 末尾保留一块，不占用 AXI SRAM(.RAM_D1 仅 512 KB 已满)。
 * 与 LTDC 子帧、GUI_ALLOC 池（从 SDRAM_APP_BUF 起）不重叠：本块在物理末端之前。
 */
/*
 * Slow Roll (e.g. 500 ms/div, 1 s/div) needs ~1e2–2e5 samples for one offline decimate pass
 * (see DSO_RollLineBuilder_NeededSampleCount). 48K was too small and caused mid-screen gaps.
 */
#define ROLL_HIST_BLOCK_BYTES  SDRAM_ROLL_HISTORY_SIZE

/*
 * Ring depth for gentle Roll resync: engine uses rd = min(seq_end, ring_cap) in
 * DSO_RollLineBuilder_CappedNeedSamples. If serial shows [DSO] roll ring: with
 * nc1/nc2 pinned to rd while rd == ring_cap and seq_end is large, history is
 * ring-limited — raise ROLL_RING_CAP_SAMPLES (and usually ROLL_PULL_MAX_SAMPLES
 * to match) if SDRAM budget allows.
 */
#define ROLL_RING_CAP_SAMPLES  (256u * 1024u)
/* 宏定义: ROLL_PULL_MAX_SAMPLES - DSO 采集、处理、渲染或调试相关配置。 */
#define ROLL_PULL_MAX_SAMPLES  (256u * 1024u)

/* 宏定义: SCRATCH_CH1_WORDS - DSO 采集、处理、渲染或调试相关配置。 */
#define SCRATCH_CH1_WORDS  (DSO_DMA_BUF_SIZE * 2u)
/* 宏定义: SCRATCH_CH2_WORDS - DSO 采集、处理、渲染或调试相关配置。 */
#define SCRATCH_CH2_WORDS  (DSO_DMA_BUF_SIZE)

static uint8_t  s_mem_ready;

static uint16_t *s_ring_ch1;
static uint16_t *s_ring_ch2;
static uint16_t *s_pull_ch1;
static uint16_t *s_pull_ch2;
static uint16_t *s_scratch_ch1;
static uint16_t *s_scratch_ch2;
static uint16_t *s_roll_line_ch1;
static uint16_t *s_roll_line_ch2;

/*
*********************************************************************************************************
*   函 数 名: RollHistoryMem_Init
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void RollHistoryMem_Init(void)
{
    uint32_t end;
    uint32_t base;
    uint8_t *p;
    size_t off;
    size_t need;

    if (s_mem_ready != 0u) {
        return;
    }

    end = SDRAM_ROLL_HISTORY_END;
    if (ROLL_HIST_BLOCK_BYTES > (uint32_t)EXT_SDRAM_SIZE) {
        return;
    }

    need = (size_t)ROLL_RING_CAP_SAMPLES * sizeof(uint16_t) * 2u;
    need += (size_t)ROLL_PULL_MAX_SAMPLES * sizeof(uint16_t) * 2u;
    need += (size_t)SCRATCH_CH1_WORDS * sizeof(uint16_t);
    need += (size_t)SCRATCH_CH2_WORDS * sizeof(uint16_t);
    need += (size_t)DSO_DISPLAY_WIDTH * sizeof(uint16_t) * 2u;

    if (need > (size_t)ROLL_HIST_BLOCK_BYTES) {
        return;
    }

    base = SDRAM_ROLL_HISTORY_BUF;
    p = (uint8_t *)base;
    off = 0u;

    s_ring_ch1 = (uint16_t *)(p + off);
    off += (size_t)ROLL_RING_CAP_SAMPLES * sizeof(uint16_t);

    s_ring_ch2 = (uint16_t *)(p + off);
    off += (size_t)ROLL_RING_CAP_SAMPLES * sizeof(uint16_t);

    s_pull_ch1 = (uint16_t *)(p + off);
    off += (size_t)ROLL_PULL_MAX_SAMPLES * sizeof(uint16_t);

    s_pull_ch2 = (uint16_t *)(p + off);
    off += (size_t)ROLL_PULL_MAX_SAMPLES * sizeof(uint16_t);

    s_scratch_ch1 = (uint16_t *)(p + off);
    off += (size_t)SCRATCH_CH1_WORDS * sizeof(uint16_t);

    s_scratch_ch2 = (uint16_t *)(p + off);
    off += (size_t)SCRATCH_CH2_WORDS * sizeof(uint16_t);

    s_roll_line_ch1 = (uint16_t *)(p + off);
    off += (size_t)DSO_DISPLAY_WIDTH * sizeof(uint16_t);

    s_roll_line_ch2 = (uint16_t *)(p + off);
    off += (size_t)DSO_DISPLAY_WIDTH * sizeof(uint16_t);

    (void)memset((void *)base, 0, (size_t)ROLL_HIST_BLOCK_BYTES);
    s_mem_ready = 1u;
}

/*
*********************************************************************************************************
*   函 数 名: RollHistoryMem_RingCh1
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint16_t *RollHistoryMem_RingCh1(void)
{
    return s_ring_ch1;
}

/*
*********************************************************************************************************
*   函 数 名: RollHistoryMem_RingCh2
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint16_t *RollHistoryMem_RingCh2(void)
{
    return s_ring_ch2;
}

/*
*********************************************************************************************************
*   函 数 名: RollHistoryMem_RingCap
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t RollHistoryMem_RingCap(void)
{
    return ROLL_RING_CAP_SAMPLES;
}

/*
*********************************************************************************************************
*   函 数 名: RollHistoryMem_PullCh1
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint16_t *RollHistoryMem_PullCh1(void)
{
    return s_pull_ch1;
}

/*
*********************************************************************************************************
*   函 数 名: RollHistoryMem_PullCh2
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint16_t *RollHistoryMem_PullCh2(void)
{
    return s_pull_ch2;
}

/*
*********************************************************************************************************
*   函 数 名: RollHistoryMem_PullMax
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t RollHistoryMem_PullMax(void)
{
    return ROLL_PULL_MAX_SAMPLES;
}

/*
*********************************************************************************************************
*   函 数 名: RollHistoryMem_ScratchCh1
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint16_t *RollHistoryMem_ScratchCh1(void)
{
    return s_scratch_ch1;
}

/*
*********************************************************************************************************
*   函 数 名: RollHistoryMem_ScratchCh2
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint16_t *RollHistoryMem_ScratchCh2(void)
{
    return s_scratch_ch2;
}

/*
*********************************************************************************************************
*   函 数 名: RollHistoryMem_RollLineCh1
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint16_t *RollHistoryMem_RollLineCh1(void)
{
    return s_roll_line_ch1;
}

/*
*********************************************************************************************************
*   函 数 名: RollHistoryMem_RollLineCh2
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint16_t *RollHistoryMem_RollLineCh2(void)
{
    return s_roll_line_ch2;
}
