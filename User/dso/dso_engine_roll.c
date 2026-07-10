/**
 * @file dso_engine_roll.c
 * @brief Roll 引擎逻辑实现。
 */

/*
 * dso_engine_roll.c — Roll subsystem extracted from dso_engine.c
 *
 * Pure mechanical extraction — zero algorithm changes.
 */

#include "dso_engine_roll.h"
#include "dso_acq.h"
#include "dso_config.h"
#include "dso_roll_line_builder.h"
#include "dso_timebase.h"
#include "roll_history_mem.h"
#include "roll_history_ring.h"
#include "debug_uart_lock.h"

#include <stdio.h>
#include <string.h>

/* ── Forward declaration of s_roll_tb_hold_until_ms (defined in dso_engine.c) ── */

extern uint32_t s_roll_tb_hold_until_ms;

/* ── Logging macro (duplicated from dso_engine.c) ── */

#if DSO_ROLL_SERIAL_DIAG_ENABLE || DSO_RUNTIME_LOG_ENABLE
/* 宏定义: DSO_ROLL_LOG - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ROLL_LOG(...) \
    do { \
        DebugUartLock(); \
        printf(__VA_ARGS__); \
        DebugUartUnlock(); \
    } while (0)
#else
/* 宏定义: DSO_ROLL_LOG - DSO 采集、处理、渲染或调试相关配置。 */
#define DSO_ROLL_LOG(...) do { } while (0)
#endif

/* ── Persistent state ── */

static uint8_t s_engine_ring_feed_inited;

typedef struct {
    uint8_t  active;
    uint8_t  ch1_done;
    uint8_t  ch2_done;
    uint8_t  ch2_skip;
    uint64_t cursor1;
    uint64_t end1;
    uint64_t cursor2;
    uint64_t end2;
    uint32_t pull_max;
    uint32_t ring_cap;
    DSO_RollLineBuilderState b1;
    DSO_RollLineBuilderState b2;
    DSO_TimebaseInfo tb_info;
} EngineRollResyncState;

static EngineRollResyncState s_rr;

/* ── Internal helpers ── */

static void engine_roll_resync_abort(void)
{
    (void)memset(&s_rr, 0, sizeof(s_rr));
}

/*
*********************************************************************************************************
*   函 数 名: engine_roll_resync_finish_ch1
*   功能说明: 在 Roll 显示链路中完成滚动列构建、物理参数计算、历史缓冲或重同步处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_roll_resync_finish_ch1(void)
{
    uint16_t *line1 = RollHistoryMem_RollLineCh1();
    float rs;
    float rc;

    if (line1 == NULL) {
        s_rr.ch1_done = 1u;
        return;
    }

    DSO_RollLineBuilder_FinishPad(&s_rr.b1, line1);
    DSO_RollLineBuilder_ExportRemainder(&s_rr.b1, &rs, &rc);
    DSO_Timebase_SeedRollDecimatedLineEx(0u, line1, DSO_DISPLAY_WIDTH, 0, rs, rc);
    s_rr.ch1_done = 1u;
}

/*
*********************************************************************************************************
*   函 数 名: engine_roll_resync_finish_ch2
*   功能说明: 在 Roll 显示链路中完成滚动列构建、物理参数计算、历史缓冲或重同步处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_roll_resync_finish_ch2(void)
{
    uint16_t *line2 = RollHistoryMem_RollLineCh2();
    float rs;
    float rc;

    if (line2 == NULL) {
        s_rr.ch2_done = 1u;
        return;
    }

    DSO_RollLineBuilder_FinishPad(&s_rr.b2, line2);
    DSO_RollLineBuilder_ExportRemainder(&s_rr.b2, &rs, &rc);
    DSO_Timebase_SeedRollDecimatedLineEx(1u, line2, DSO_DISPLAY_WIDTH, 0, rs, rc);
    s_rr.ch2_done = 1u;
}

/*
*********************************************************************************************************
*   函 数 名: engine_roll_resync_feed_ch1
*   功能说明: 在 Roll 显示链路中完成滚动列构建、物理参数计算、历史缓冲或重同步处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t engine_roll_resync_feed_ch1(uint16_t *pull1, uint32_t budget)
{
    uint32_t used = 0u;

    while ((used < budget) && (s_rr.cursor1 < s_rr.end1) && (s_rr.b1.wix < DSO_DISPLAY_WIDTH)) {
        uint32_t space = (uint32_t)(s_rr.end1 - s_rr.cursor1);
        uint32_t chunk = space;
        uint32_t n;
        uint16_t *line1;

        if (chunk > s_rr.pull_max) {
            chunk = s_rr.pull_max;
        }
        if (chunk > (budget - used)) {
            chunk = budget - used;
        }
        if (chunk == 0u) {
            break;
        }
        n = RollHistoryRing_CopySpanCh1(s_rr.cursor1, chunk, pull1);
        if (n == 0u) {
            break;
        }
        line1 = RollHistoryMem_RollLineCh1();
        if (line1 == NULL) {
            break;
        }
        DSO_RollLineBuilder_Feed(&s_rr.b1, pull1, n, line1);
        s_rr.cursor1 += (uint64_t)n;
        used += n;
    }
    return used;
}

/*
*********************************************************************************************************
*   函 数 名: engine_roll_resync_feed_ch2
*   功能说明: 在 Roll 显示链路中完成滚动列构建、物理参数计算、历史缓冲或重同步处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint32_t engine_roll_resync_feed_ch2(uint16_t *pull2, uint32_t budget)
{
    uint32_t used = 0u;

    while ((used < budget) && (s_rr.cursor2 < s_rr.end2) && (s_rr.b2.wix < DSO_DISPLAY_WIDTH)) {
        uint32_t space = (uint32_t)(s_rr.end2 - s_rr.cursor2);
        uint32_t chunk = space;
        uint32_t n;
        uint16_t *line2;

        if (chunk > s_rr.pull_max) {
            chunk = s_rr.pull_max;
        }
        if (chunk > (budget - used)) {
            chunk = budget - used;
        }
        if (chunk == 0u) {
            break;
        }
        n = RollHistoryRing_CopySpanCh2(s_rr.cursor2, chunk, pull2);
        if (n == 0u) {
            break;
        }
        line2 = RollHistoryMem_RollLineCh2();
        if (line2 == NULL) {
            break;
        }
        DSO_RollLineBuilder_Feed(&s_rr.b2, pull2, n, line2);
        s_rr.cursor2 += (uint64_t)n;
        used += n;
    }
    return used;
}

/*
 * Drain helpers: if RollHistoryRing_CopySpan* returns 0 without advancing cursors,
 * the naive while loops in resync_begin would spin forever (watchdog / "卡死").
 */
static void engine_roll_resync_drain_ch1(uint16_t *pull1)
{
    uint32_t iter;

    for (iter = 0u;
         (iter < (DSO_DISPLAY_WIDTH * 32u)) &&
         (s_rr.cursor1 < s_rr.end1) && (s_rr.b1.wix < DSO_DISPLAY_WIDTH);
         iter++) {
        uint64_t before = s_rr.cursor1;

        (void)engine_roll_resync_feed_ch1(pull1, s_rr.pull_max);
        if (s_rr.cursor1 == before) {
            break;
        }
    }
}

/*
*********************************************************************************************************
*   函 数 名: engine_roll_resync_drain_ch2
*   功能说明: 在 Roll 显示链路中完成滚动列构建、物理参数计算、历史缓冲或重同步处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void engine_roll_resync_drain_ch2(uint16_t *pull2)
{
    uint32_t iter;

    for (iter = 0u;
         (iter < (DSO_DISPLAY_WIDTH * 32u)) &&
         (s_rr.cursor2 < s_rr.end2) && (s_rr.b2.wix < DSO_DISPLAY_WIDTH);
         iter++) {
        uint64_t before = s_rr.cursor2;

        (void)engine_roll_resync_feed_ch2(pull2, s_rr.pull_max);
        if (s_rr.cursor2 == before) {
            break;
        }
    }
}

/*
 * Seed roll display line state from CH1 ring only (matches legacy 8-bit tri path when CH2
 * is unavailable). seq1_total is RollHistoryRing_GetTotalWrittenCh1().
 */
static bool engine_roll_resync_seed_ch1_only(uint16_t *pull1,
                                            uint32_t sample_rate_hz,
                                            uint64_t seq1_total)
{
    uint32_t need_cap1;
    uint32_t ne1;
    uint32_t rd1;

    if ((pull1 == NULL) || (sample_rate_hz == 0u) || (seq1_total == 0u)) {
        return false;
    }

    rd1 = (seq1_total > (uint64_t)s_rr.ring_cap) ? s_rr.ring_cap : (uint32_t)seq1_total;
    need_cap1 = DSO_RollLineBuilder_CappedNeedSamples(s_rr.tb_info.time_per_pixel_ns,
                                                       sample_rate_hz,
                                                       s_rr.tb_info.sec_div_ns,
                                                       DSO_DISPLAY_WIDTH,
                                                       rd1,
                                                       s_rr.pull_max);
    if (need_cap1 == 0u) {
        return false;
    }

    /* Unseeded channel would keep old roll_buf/accumulators while RollRender uses the new
     * sec/div; clear CH2 so it refills coherently from live samples. */
    DSO_Timebase_ResetRollColumnStateForChannel(1u);

    ne1 = need_cap1;
    if (ne1 < 1u) {
        ne1 = 1u;
    }
    if ((uint64_t)ne1 > seq1_total) {
        ne1 = (uint32_t)seq1_total;
    }

    s_rr.end1 = seq1_total;
    s_rr.cursor1 = seq1_total - (uint64_t)ne1;

    DSO_RollLineBuilder_Begin(&s_rr.b1, sample_rate_hz, s_rr.tb_info.sec_div_ns, DSO_DISPLAY_WIDTH);
    engine_roll_resync_drain_ch1(pull1);
    engine_roll_resync_finish_ch1();

    s_rr.ch2_skip = 1u;
    s_rr.ch2_done = 1u;
    s_rr.active = 0u;
    return true;
}

/*
 * Seed roll display line from CH2 ring only when CH1 CappedNeedSamples is zero but CH2 can seed.
 * seq2_total is RollHistoryRing_GetTotalWrittenCh2().
 */
static bool engine_roll_resync_seed_ch2_only(uint16_t *pull2,
                                              uint32_t ch2_sample_rate_hz,
                                              uint64_t seq2_total)
{
    uint32_t need_cap2;
    uint32_t ne2;
    uint32_t rd2;

    if ((pull2 == NULL) || (RollHistoryMem_RollLineCh2() == NULL) ||
        (ch2_sample_rate_hz == 0u) || (seq2_total == 0u)) {
        return false;
    }

    rd2 = (seq2_total > (uint64_t)s_rr.ring_cap) ? s_rr.ring_cap : (uint32_t)seq2_total;
    need_cap2 = DSO_RollLineBuilder_CappedNeedSamples(s_rr.tb_info.time_per_pixel_ns,
                                                       ch2_sample_rate_hz,
                                                       s_rr.tb_info.sec_div_ns,
                                                       DSO_DISPLAY_WIDTH,
                                                       rd2,
                                                       s_rr.pull_max);
    if (need_cap2 == 0u) {
        return false;
    }

    DSO_Timebase_ResetRollColumnStateForChannel(0u);

    ne2 = need_cap2;
    if (ne2 < 1u) {
        ne2 = 1u;
    }
    if ((uint64_t)ne2 > seq2_total) {
        ne2 = (uint32_t)seq2_total;
    }

    s_rr.end2 = seq2_total;
    s_rr.cursor2 = seq2_total - (uint64_t)ne2;

    DSO_RollLineBuilder_Begin(&s_rr.b2, ch2_sample_rate_hz, s_rr.tb_info.sec_div_ns, DSO_DISPLAY_WIDTH);
    engine_roll_resync_drain_ch2(pull2);
    engine_roll_resync_finish_ch2();

    s_rr.ch1_done = 1u;
    s_rr.ch2_done = 1u;
    s_rr.ch2_skip = 0u;
    s_rr.active = 0u;
    return true;
}

/*
 * Per-channel ring seed: each stream uses its own seq end (seq1 vs seq2) and precomputed
 * need_cap. If need_cap==0 for a channel, reset that channel only so RollRender refills with
 * the new timebase (no stale decimation). Avoids ch2_only wiping a good CH1 seed first.
 */
static bool engine_roll_resync_seed_ring_per_channel(uint16_t *pull1,
                                                      uint16_t *pull2,
                                                      uint32_t sample_rate_hz,
                                                      uint32_t ch2_sample_rate_hz,
                                                      uint64_t seq1_total,
                                                      uint64_t seq2_total,
                                                      uint32_t need_cap1,
                                                      uint32_t need_cap2)
{
    uint32_t ne1;
    uint32_t ne2;

    if ((pull1 == NULL) || (pull2 == NULL) || (RollHistoryMem_RollLineCh2() == NULL) ||
        (sample_rate_hz == 0u) || (ch2_sample_rate_hz == 0u) || (seq1_total == 0u) ||
        (seq2_total == 0u)) {
        return false;
    }

    s_rr.ch1_done = 0u;
    s_rr.ch2_done = 0u;
    s_rr.ch2_skip = 0u;

    if (need_cap1 > 0u) {
        ne1 = need_cap1;
        if (ne1 < 1u) {
            ne1 = 1u;
        }
        if ((uint64_t)ne1 > seq1_total) {
            ne1 = (uint32_t)seq1_total;
        }
        s_rr.end1    = seq1_total;
        s_rr.cursor1 = seq1_total - (uint64_t)ne1;
        DSO_RollLineBuilder_Begin(&s_rr.b1, sample_rate_hz, s_rr.tb_info.sec_div_ns, DSO_DISPLAY_WIDTH);
        engine_roll_resync_drain_ch1(pull1);
        engine_roll_resync_finish_ch1();
    } else {
        DSO_Timebase_ResetRollColumnStateForChannel(0u);
    }

    if (need_cap2 > 0u) {
        ne2 = need_cap2;
        if (ne2 < 1u) {
            ne2 = 1u;
        }
        if ((uint64_t)ne2 > seq2_total) {
            ne2 = (uint32_t)seq2_total;
        }
        s_rr.end2    = seq2_total;
        s_rr.cursor2 = seq2_total - (uint64_t)ne2;
        DSO_RollLineBuilder_Begin(&s_rr.b2, ch2_sample_rate_hz, s_rr.tb_info.sec_div_ns, DSO_DISPLAY_WIDTH);
        engine_roll_resync_drain_ch2(pull2);
        engine_roll_resync_finish_ch2();
    } else {
        DSO_Timebase_ResetRollColumnStateForChannel(1u);
    }

    s_rr.ch1_done = 1u;
    s_rr.ch2_done = 1u;
    s_rr.active = 0u;
    return true;
}

/* ── Public API ── */

void DSO_EngineRoll_Init(void)
{
    /* No additional init needed; RollHistory init is done in main.c. */
}

/*
*********************************************************************************************************
*   函 数 名: DSO_EngineRoll_RingFeedReset
*   功能说明: 处理引擎侧 Roll 模式 ring feed、重同步和显示行重建。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_EngineRoll_RingFeedReset(void)
{
    s_engine_ring_feed_inited = 0u;
    RollHistoryRing_Clear();
    engine_roll_resync_abort();
}

/*
*********************************************************************************************************
*   函 数 名: DSO_EngineRoll_RingFeed
*   功能说明: 处理引擎侧 Roll 模式 ring feed、重同步和显示行重建。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_EngineRoll_RingFeed(uint32_t pos,
                              uint32_t pos_ch2,
                              const volatile uint32_t *buf,
                              const volatile uint16_t *buf_ch2,
                              int16_t cal,
                              bool ch2_available)
{
    uint16_t *sc1;
    uint16_t *sc2;
    static uint32_t feed_pos;
    static uint32_t feed_pos_ch2;
    uint32_t n;

    if (!DSO_Acq_IsRunning()) {
        DSO_EngineRoll_RingFeedReset();
        return;
    }

    sc1 = RollHistoryMem_ScratchCh1();
    sc2 = RollHistoryMem_ScratchCh2();
    if ((sc1 == NULL) || (sc2 == NULL)) {
        return;
    }

    if (s_engine_ring_feed_inited == 0u) {
        feed_pos = pos;
        feed_pos_ch2 = pos_ch2;
        s_engine_ring_feed_inited = 1u;
        return;
    }

    n = DSO_Acq_CopyNewSamples(feed_pos, pos, buf, cal, sc1, DSO_DMA_BUF_SIZE * 2u);
    feed_pos = pos;
    if (n > 0u) {
        RollHistoryRing_AppendCh1(sc1, n);
    }

    if (ch2_available && (buf_ch2 != NULL)) {
        uint32_t p2 = pos_ch2;

        n = DSO_Acq_CopyNewCh2Samples(feed_pos_ch2, p2, buf_ch2, sc2, DSO_DMA_BUF_SIZE);
        feed_pos_ch2 = p2;
        if (n > 0u) {
            RollHistoryRing_AppendCh2(sc2, n);
        }
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_EngineRoll_TbHoldActive
*   功能说明: 处理引擎侧 Roll 模式 ring feed、重同步和显示行重建。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_EngineRoll_TbHoldActive(uint32_t now_ms)
{
    return (s_roll_tb_hold_until_ms != 0u) && (now_ms < s_roll_tb_hold_until_ms);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_EngineRoll_ResyncInProgress
*   功能说明: 处理引擎侧 Roll 模式 ring feed、重同步和显示行重建。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_EngineRoll_ResyncInProgress(void)
{
    return (s_rr.active != 0u);
}

/*
*********************************************************************************************************
*   函 数 名: DSO_EngineRoll_SetTbHold
*   功能说明: 处理引擎侧 Roll 模式 ring feed、重同步和显示行重建。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_EngineRoll_SetTbHold(uint32_t until_ms)
{
    s_roll_tb_hold_until_ms = until_ms;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_EngineRoll_ClearTbHold
*   功能说明: 处理引擎侧 Roll 模式 ring feed、重同步和显示行重建。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_EngineRoll_ClearTbHold(void)
{
    s_roll_tb_hold_until_ms = 0u;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_EngineRoll_UpdateTbHoldIfResyncing
*   功能说明: 处理引擎侧 Roll 模式 ring feed、重同步和显示行重建。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_EngineRoll_UpdateTbHoldIfResyncing(uint32_t now_ms)
{
    if ((s_rr.active != 0u) && (DSO_Timebase_GetMode() == TB_MODE_ROLL)) {
        s_roll_tb_hold_until_ms = now_ms + DSO_ROLL_TB_HOLD_MS;
    }
    if ((s_roll_tb_hold_until_ms != 0u) && (now_ms >= s_roll_tb_hold_until_ms) && (s_rr.active == 0u)) {
        s_roll_tb_hold_until_ms = 0u;
    }
}

/*
*********************************************************************************************************
*   函 数 名: DSO_EngineRoll_ResyncService
*   功能说明: 处理引擎侧 Roll 模式 ring feed、重同步和显示行重建。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_EngineRoll_ResyncService(void)
{
    uint16_t *pull1;
    uint16_t *pull2;
    bool just_finished;
    uint32_t cap1;
    uint32_t cap2;

    just_finished = false;
    if (s_rr.active == 0u) {
        return false;
    }
    if (DSO_Timebase_GetMode() != TB_MODE_ROLL) {
        engine_roll_resync_abort();
        return false;
    }

    pull1 = RollHistoryMem_PullCh1();
    pull2 = RollHistoryMem_PullCh2();
    if ((pull1 == NULL) || (s_rr.pull_max == 0u)) {
        engine_roll_resync_abort();
        return false;
    }

    cap1 = DSO_ENGINE_ROLL_RESYNC_MAX_SAMPLES_PER_FRAME / 2u;
    cap2 = DSO_ENGINE_ROLL_RESYNC_MAX_SAMPLES_PER_FRAME - cap1;

    if (s_rr.ch1_done == 0u) {
        (void)engine_roll_resync_feed_ch1(pull1, cap1);
    }
    if ((s_rr.ch2_done == 0u) && (s_rr.ch2_skip == 0u) && (pull2 != NULL)) {
        (void)engine_roll_resync_feed_ch2(pull2, cap2);
    }

    if ((s_rr.ch1_done == 0u) &&
        ((s_rr.cursor1 >= s_rr.end1) || (s_rr.b1.wix >= DSO_DISPLAY_WIDTH))) {
        engine_roll_resync_finish_ch1();
    }

    if ((s_rr.ch2_done == 0u) && (s_rr.ch2_skip == 0u) &&
        ((s_rr.cursor2 >= s_rr.end2) || (s_rr.b2.wix >= DSO_DISPLAY_WIDTH))) {
        engine_roll_resync_finish_ch2();
    }

    if ((s_rr.ch1_done != 0u) && (s_rr.ch2_done != 0u)) {
        s_rr.active = 0u;
        just_finished = true;
    }
    return just_finished;
}

/*
*********************************************************************************************************
*   函 数 名: DSO_EngineRoll_ResyncBegin
*   功能说明: 处理引擎侧 Roll 模式 ring feed、重同步和显示行重建。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool DSO_EngineRoll_ResyncBegin(uint32_t sample_rate_hz,
                                 uint32_t ch2_sample_rate_hz,
                                 bool ch2_available,
                                 const DSO_TimebaseInfo *tb_info)
{
    uint32_t need_cap1;
    uint32_t need_cap2;
    uint32_t rd1;
    uint32_t rd2;
    uint64_t seq1;
    uint64_t seq2;
    uint16_t *pull1;
    uint16_t *pull2;

    engine_roll_resync_abort();

    if ((tb_info == NULL) || (sample_rate_hz == 0u)) {
        return false;
    }

    s_rr.pull_max = RollHistoryMem_PullMax();
    s_rr.ring_cap = RollHistoryMem_RingCap();
    pull1 = RollHistoryMem_PullCh1();
    if ((s_rr.pull_max == 0u) || (s_rr.ring_cap == 0u) || (pull1 == NULL) || (RollHistoryMem_RollLineCh1() == NULL)) {
        return false;
    }

    seq1 = RollHistoryRing_GetTotalWrittenCh1();
    if (seq1 == 0u) {
        return false;
    }

    (void)memcpy(&s_rr.tb_info, tb_info, sizeof(s_rr.tb_info));

    s_rr.ch1_done = 0u;
    s_rr.ch2_done = 0u;
    s_rr.ch2_skip = 0u;

    if (!ch2_available) {
        return engine_roll_resync_seed_ch1_only(pull1, sample_rate_hz, seq1);
    }

    pull2 = RollHistoryMem_PullCh2();
    if (pull2 == NULL) {
        return engine_roll_resync_seed_ch1_only(pull1, sample_rate_hz, seq1);
    }

    seq2 = RollHistoryRing_GetTotalWrittenCh2();
    if (seq2 == 0u) {
        return engine_roll_resync_seed_ch1_only(pull1, sample_rate_hz, seq1);
    }

    {
        uint32_t sr2 = (ch2_sample_rate_hz != 0u) ? ch2_sample_rate_hz : sample_rate_hz;

        rd1 = (seq1 > (uint64_t)s_rr.ring_cap) ? s_rr.ring_cap : (uint32_t)seq1;
        rd2 = (seq2 > (uint64_t)s_rr.ring_cap) ? s_rr.ring_cap : (uint32_t)seq2;

        need_cap1 = DSO_RollLineBuilder_CappedNeedSamples(tb_info->time_per_pixel_ns,
                                                           sample_rate_hz,
                                                           tb_info->sec_div_ns,
                                                           DSO_DISPLAY_WIDTH,
                                                           rd1,
                                                           s_rr.pull_max);
        need_cap2 = DSO_RollLineBuilder_CappedNeedSamples(tb_info->time_per_pixel_ns,
                                                           sr2,
                                                           tb_info->sec_div_ns,
                                                           DSO_DISPLAY_WIDTH,
                                                           rd2,
                                                           s_rr.pull_max);
#if DSO_ROLL_SERIAL_DIAG_ENABLE || DSO_RUNTIME_LOG_ENABLE
        DSO_ROLL_LOG("[DSO] roll ring: seq1=%llu seq2=%llu rd1=%u rd2=%u nc1=%u nc2=%u ring_cap=%u pull_max=%u perch\r\n",
                     (unsigned long long)seq1,
                     (unsigned long long)seq2,
                     (unsigned)rd1,
                     (unsigned)rd2,
                     (unsigned)need_cap1,
                     (unsigned)need_cap2,
                     (unsigned)s_rr.ring_cap,
                     (unsigned)s_rr.pull_max);
#endif
        if ((need_cap1 == 0u) && (need_cap2 == 0u)) {
            return false;
        }
        if ((need_cap1 == 0u) || (need_cap2 == 0u)) {
            DSO_ROLL_LOG("[DSO] gentle roll: per-channel ring seed (one nc=0 -> reset that channel)\r\n");
        }
        return engine_roll_resync_seed_ring_per_channel(pull1,
                                                         pull2,
                                                         sample_rate_hz,
                                                         sr2,
                                                         seq1,
                                                         seq2,
                                                         need_cap1,
                                                         need_cap2);
    }
}
