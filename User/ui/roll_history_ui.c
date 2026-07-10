/**
 * @file roll_history_ui.c
 * @brief Roll 时基切换时的 UI 预填：从 SDRAM ring 拉样本经 RollLineBuilder 生成整屏 y_coords。
 */
#include "roll_history_ui.h"

#include "dso/roll_history_mem.h"
#include "dso/roll_history_ring.h"
#include "dso/dso_acq.h"
#include "dso/dso_config.h"
#include "dso/dso_roll_line_builder.h"
#include "dso/dso_timebase.h"
#include "dso/dso_vertical.h"

#include <stdint.h>
#include <string.h>

static uint8_t  s_tb_snap_valid;
static uint32_t s_prev_tb_sec_div_ns;
static uint8_t  s_prev_tb_mode;

static bool roll_prefill_stream_channel(float time_per_pixel_ns,
                                        uint32_t sample_rate_hz,
                                        uint32_t sec_div_ns,
                                        uint32_t (*copy_span)(uint64_t, uint32_t, uint16_t *),
                                        uint64_t (*get_seq)(void),
                                        uint16_t *pull,
                                        uint16_t *line,
                                        uint32_t pull_max,
                                        uint32_t cap)
{
    uint32_t ring_depth;
    uint32_t need_eff;
    uint64_t seq;
    uint64_t cursor;
    DSO_RollLineBuilderState st;
    bool any_samples = false;

    if ((pull == NULL) || (line == NULL) || (pull_max == 0u) || (cap == 0u) ||
        (copy_span == NULL) || (get_seq == NULL) || (sample_rate_hz == 0u)) {
        return false;
    }

    seq = get_seq();
    if (seq == 0u) {
        return false;
    }

    ring_depth = (seq > (uint64_t)cap) ? cap : (uint32_t)seq;
    need_eff = DSO_RollLineBuilder_CappedNeedSamples(time_per_pixel_ns,
                                                     sample_rate_hz,
                                                     sec_div_ns,
                                                     DSO_DISPLAY_WIDTH,
                                                     ring_depth,
                                                     pull_max);
    if (need_eff == 0u) {
        return false;
    }
    cursor = seq - (uint64_t)need_eff;

    DSO_RollLineBuilder_Begin(&st, sample_rate_hz, sec_div_ns, DSO_DISPLAY_WIDTH);
    while ((cursor < seq) && (st.wix < DSO_DISPLAY_WIDTH)) {
        uint32_t space = (uint32_t)(seq - cursor);
        uint32_t chunk = (space > pull_max) ? pull_max : space;
        uint32_t n;

        n = copy_span(cursor, chunk, pull);
        if (n == 0u) {
            break;
        }
        any_samples = true;
        DSO_RollLineBuilder_Feed(&st, pull, n, line);
        cursor += (uint64_t)n;
    }
    if (!any_samples) {
        return false;
    }
    DSO_RollLineBuilder_FinishPad(&st, line);
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: RollHistory_Init
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void RollHistory_Init(void)
{
    RollHistoryMem_Init();
    RollHistoryRing_Init();
    s_tb_snap_valid = 0u;
    s_prev_tb_sec_div_ns = 0u;
    s_prev_tb_mode = 0u;
}

/*
*********************************************************************************************************
*   函 数 名: RollHistory_ServiceFeedOnce
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void RollHistory_ServiceFeedOnce(void)
{
    /* DMA -> ring feed runs in DSO_Engine_Task (dso_engine.c). */
}

/*
*********************************************************************************************************
*   函 数 名: RollHistory_ApplyRollCoherentPrefill
*   功能说明: 维护 Roll 历史缓冲、追加样本、读取窗口或查询写入进度。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void RollHistory_ApplyRollCoherentPrefill(DSO_UI_Frame *frame)
{
    uint8_t roll;
    uint8_t changed;
    uint32_t sr2;
    uint32_t pull_max;
    uint16_t *pull1;
    uint16_t *pull2;
    uint16_t *line1;
    uint16_t *line2;

    if (frame == NULL) {
        return;
    }

    pull_max = RollHistoryMem_PullMax();
    pull1 = RollHistoryMem_PullCh1();
    pull2 = RollHistoryMem_PullCh2();
    line1 = RollHistoryMem_RollLineCh1();
    line2 = RollHistoryMem_RollLineCh2();
    if ((pull_max == 0u) || (pull1 == NULL) || (line1 == NULL)) {
        return;
    }
    if ((frame->ch2_present != 0u) && ((pull2 == NULL) || (line2 == NULL))) {
        return;
    }

    roll = (frame->tb_mode == DSO_UI_TB_MODE_ROLL) ? 1u : 0u;

    if (s_tb_snap_valid == 0u) {
        s_prev_tb_sec_div_ns = frame->tb_sec_div_ns;
        s_prev_tb_mode = frame->tb_mode;
        s_tb_snap_valid = 1u;
        return;
    }

    changed = 0u;
    if (roll != 0u) {
        if ((frame->tb_sec_div_ns != s_prev_tb_sec_div_ns) || (frame->tb_mode != s_prev_tb_mode)) {
            changed = 1u;
        }
    }

    s_prev_tb_sec_div_ns = frame->tb_sec_div_ns;
    s_prev_tb_mode = frame->tb_mode;

    if ((roll == 0u) || (changed == 0u)) {
        return;
    }

    if (frame->roll_resync_in_progress) {
        return;
    }
    /* Engine gentle/resync path owns ring→pull→line; UI prefill must not run in parallel
     * (shared pull1/pull2 in SDRAM). Slow Roll (500 ms/div, 1 s/div) needs ~1e5 samples
     * per channel and resync spans many engine frames — concurrent prefill here caused
     * MemManage / corrupted return addresses in emWin. */
    if (frame->roll_ring_coherent_engine) {
        return;
    }

    if (frame->sample_rate == 0u) {
        return;
    }

    {
        uint32_t cap = RollHistoryMem_RingCap();

        if (roll_prefill_stream_channel(frame->tb_time_per_pixel_ns,
                                        frame->sample_rate,
                                        frame->tb_sec_div_ns,
                                        RollHistoryRing_CopySpanCh1,
                                        RollHistoryRing_GetTotalWrittenCh1,
                                        pull1,
                                        line1,
                                        pull_max,
                                        cap)) {
            DSO_Vertical_ApplyACDisplayFilterForChannel(DSO_CHANNEL_1,
                                                        line1,
                                                        DSO_DISPLAY_WIDTH);
            DSO_Vertical_MapYForChannel(DSO_CHANNEL_1,
                                        line1,
                                        DSO_DISPLAY_WIDTH,
                                        frame->y_coords);
        }
    }

    if (frame->ch2_present != 0) {
        sr2 = (frame->ch2_sample_rate != 0u) ? frame->ch2_sample_rate : frame->sample_rate;
        if (sr2 > 0u) {
            uint32_t cap = RollHistoryMem_RingCap();

            if (roll_prefill_stream_channel(frame->tb_time_per_pixel_ns,
                                            sr2,
                                            frame->tb_sec_div_ns,
                                            RollHistoryRing_CopySpanCh2,
                                            RollHistoryRing_GetTotalWrittenCh2,
                                            pull2,
                                            line2,
                                            pull_max,
                                            cap)) {
                DSO_Vertical_ApplyACDisplayFilterForChannel(DSO_CHANNEL_2,
                                                            line2,
                                                            DSO_DISPLAY_WIDTH);
                DSO_Vertical_MapYForChannel(DSO_CHANNEL_2,
                                            line2,
                                            DSO_DISPLAY_WIDTH,
                                            frame->y_coords_ch2);
            }
        }
    }
}
