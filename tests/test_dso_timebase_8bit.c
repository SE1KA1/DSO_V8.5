#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "dso_acq_timebase_stubs.h"
#include "dso/dso_sample_spec.h"
#include "dso/dso_timebase.h"

#define TEST_SAMPLE_COUNT (DSO_DISPLAY_WIDTH * 4u)

static int test_8bit_single_lane_high_confidence_stays_on_raw_path(void)
{
    uint16_t samples[TEST_SAMPLE_COUNT];
    uint16_t display[DSO_DISPLAY_WIDTH];
    DSO_ProcessAnalysis analysis = {0};
    DSO_RenderMeta meta = {0};

    for (uint32_t i = 0; i < TEST_SAMPLE_COUNT; ++i) {
        samples[i] = (uint16_t)((i * 13u) & 0xFFu);
    }

    Test_AcqStub_Reset();
    Test_AcqStub_SetLaneCount(1u);

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
    DSO_Timebase_Init();
    DSO_Timebase_SetIndex(12u);
    (void)DSO_Timebase_ApplyPending();

    analysis.freq_valid = true;
    analysis.freq_hz = 380000.0f;
    analysis.trigger_index = (int32_t)(TEST_SAMPLE_COUNT / 2u);
    analysis.trigger_pos_samples = (float)analysis.trigger_index + 0.5f;
    analysis.triggered = true;
    analysis.samples_per_cycle = 4.39f;
    analysis.repetitive_confidence = 0.92f;
    analysis.clipped = false;

    DSO_Timebase_RenderToBuffer(samples,
                                TEST_SAMPLE_COUNT,
                                1666666u,
                                &analysis,
                                50u,
                                DSO_ACQUIRE_MODE_SAMPLE,
                                true,
                                display,
                                NULL,
                                &meta);

    if ((meta.recon_mode != DSO_RECON_MODE_RAW_LIMITED) ||
        meta.allow_ets_requested ||
        meta.allow_ets_effective ||
        meta.hs8_candidate ||
        meta.hs8_ready ||
        meta.ets_candidate ||
        meta.ets_active ||
        (meta.effective_sample_rate_hz != 1666666.0f)) {
        fprintf(stderr,
                "8-bit single-lane high-confidence path did not stay raw-limited: mode=%u req=%u eff=%u hs8=%u/%u ets=%u/%u rate=%.2f\n",
                (unsigned)meta.recon_mode,
                meta.allow_ets_requested ? 1u : 0u,
                meta.allow_ets_effective ? 1u : 0u,
                meta.hs8_candidate ? 1u : 0u,
                meta.hs8_ready ? 1u : 0u,
                meta.ets_candidate ? 1u : 0u,
                meta.ets_active ? 1u : 0u,
                (double)meta.effective_sample_rate_hz);
        return 1;
    }

    return 0;
}

static int test_8bit_single_lane_low_confidence_stays_on_raw_path(void)
{
    uint16_t samples[TEST_SAMPLE_COUNT];
    uint16_t display[DSO_DISPLAY_WIDTH];
    DSO_ProcessAnalysis analysis = {0};
    DSO_RenderMeta meta = {0};

    for (uint32_t i = 0; i < TEST_SAMPLE_COUNT; ++i) {
        samples[i] = (uint16_t)((i * 7u) & 0xFFu);
    }

    Test_AcqStub_Reset();
    Test_AcqStub_SetLaneCount(1u);

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
    DSO_Timebase_Init();
    DSO_Timebase_SetIndex(12u);
    (void)DSO_Timebase_ApplyPending();

    analysis.freq_valid = true;
    analysis.freq_hz = 380000.0f;
    analysis.trigger_index = (int32_t)(TEST_SAMPLE_COUNT / 2u);
    analysis.trigger_pos_samples = (float)analysis.trigger_index + 0.25f;
    analysis.triggered = true;
    analysis.samples_per_cycle = 4.39f;
    analysis.repetitive_confidence = 0.20f;
    analysis.clipped = false;

    DSO_Timebase_RenderToBuffer(samples,
                                TEST_SAMPLE_COUNT,
                                1666666u,
                                &analysis,
                                50u,
                                DSO_ACQUIRE_MODE_SAMPLE,
                                true,
                                display,
                                NULL,
                                &meta);

    if ((meta.recon_mode == DSO_RECON_MODE_HS8_ENHANCED) ||
        meta.hs8_candidate ||
        meta.hs8_ready ||
        meta.ets_candidate ||
        meta.ets_active) {
        fprintf(stderr,
                "8-bit low-confidence path unexpectedly entered enhanced render: mode=%u hs8=%u/%u ets=%u/%u\n",
                (unsigned)meta.recon_mode,
                meta.hs8_candidate ? 1u : 0u,
                meta.hs8_ready ? 1u : 0u,
                meta.ets_candidate ? 1u : 0u,
                meta.ets_active ? 1u : 0u);
        return 1;
    }

    return 0;
}

static int test_8bit_timebase_reports_actual_single_lane_rate(void)
{
    DSO_TimebaseInfo info = {0};

    Test_AcqStub_Reset();
    Test_AcqStub_SetLaneCount(1u);
    Test_AcqStub_SetTimClkHz(200000000u);

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
    DSO_Timebase_Init();
    DSO_Timebase_SetIndex(0u);
    (void)DSO_Timebase_ApplyPending();
    DSO_Timebase_GetInfo(&info);

    if ((info.hw_sample_rate != 8333333u) ||
        (DSO_Timebase_GetSampleRateHz() != 8333333u)) {
        fprintf(stderr,
                "8-bit timebase did not report actual single-lane rate: info=%lu get=%lu\n",
                (unsigned long)info.hw_sample_rate,
                (unsigned long)DSO_Timebase_GetSampleRateHz());
        return 1;
    }

    return 0;
}

static int test_8bit_timebase_uses_tri_lane_count_for_multilane_gating(void)
{
    uint16_t samples[TEST_SAMPLE_COUNT];
    uint16_t display[DSO_DISPLAY_WIDTH];
    DSO_ProcessAnalysis analysis = {0};
    DSO_RenderMeta meta = {0};

    for (uint32_t i = 0; i < TEST_SAMPLE_COUNT; ++i) {
        samples[i] = (uint16_t)((i * 9u) & 0xFFu);
    }

    Test_AcqStub_Reset();
    Test_AcqStub_SetLaneCount(3u);

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
    DSO_Timebase_Init();
    DSO_Timebase_SetIndex(0u);
    (void)DSO_Timebase_ApplyPending();

    analysis.freq_valid = true;
    analysis.freq_hz = 400000.0f;
    analysis.trigger_index = (int32_t)(TEST_SAMPLE_COUNT / 2u);
    analysis.trigger_pos_samples = (float)analysis.trigger_index;
    analysis.triggered = true;
    analysis.samples_per_cycle = 4.00f;
    analysis.repetitive_confidence = 0.90f;
    analysis.clipped = false;

    DSO_Timebase_RenderToBuffer(samples,
                                TEST_SAMPLE_COUNT,
                                1666666u,
                                &analysis,
                                50u,
                                DSO_ACQUIRE_MODE_SAMPLE,
                                true,
                                display,
                                NULL,
                                &meta);

    if (!meta.allow_ets_requested || !meta.allow_ets_effective) {
        fprintf(stderr,
                "8-bit multilane gating ignored runtime lane count: req=%u eff=%u\n",
                meta.allow_ets_requested ? 1u : 0u,
                meta.allow_ets_effective ? 1u : 0u);
        return 1;
    }

    return 0;
}

static int test_8bit_tri_lane_cal_ready_gate_blocks_enhanced_recon(void)
{
    uint16_t samples[TEST_SAMPLE_COUNT];
    uint16_t display[DSO_DISPLAY_WIDTH];
    DSO_ProcessAnalysis analysis = {0};
    DSO_RenderMeta meta = {0};

    for (uint32_t i = 0; i < TEST_SAMPLE_COUNT; ++i) {
        samples[i] = (uint16_t)((i * 11u) & 0xFFu);
    }

    Test_AcqStub_Reset();
    Test_AcqStub_SetLaneCount(3u);

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
    DSO_Timebase_Init();
    DSO_Timebase_SetIndex(0u);
    (void)DSO_Timebase_ApplyPending();

    analysis.freq_valid = true;
    analysis.freq_hz = 400000.0f;
    analysis.trigger_index = (int32_t)(TEST_SAMPLE_COUNT / 2u);
    analysis.trigger_pos_samples = (float)analysis.trigger_index;
    analysis.triggered = true;
    analysis.samples_per_cycle = 4.00f;
    analysis.repetitive_confidence = 0.90f;
    analysis.clipped = false;

    DSO_Timebase_SetHs8CalReadyForChannel(0u, false);
    DSO_Timebase_RenderToBuffer(samples,
                                TEST_SAMPLE_COUNT,
                                1666666u,
                                &analysis,
                                50u,
                                DSO_ACQUIRE_MODE_SAMPLE,
                                true,
                                display,
                                NULL,
                                &meta);

    if (!meta.allow_ets_requested ||
        meta.allow_ets_effective ||
        meta.hs8_candidate ||
        meta.ets_candidate ||
        meta.hs8_ready ||
        meta.ets_active) {
        fprintf(stderr,
                "8-bit tri-lane cal gate did not block enhanced recon: req=%u eff=%u hs8=%u/%u ets=%u/%u\n",
                meta.allow_ets_requested ? 1u : 0u,
                meta.allow_ets_effective ? 1u : 0u,
                meta.hs8_candidate ? 1u : 0u,
                meta.hs8_ready ? 1u : 0u,
                meta.ets_candidate ? 1u : 0u,
                meta.ets_active ? 1u : 0u);
        return 1;
    }

    DSO_Timebase_SetHs8CalReadyForChannel(0u, true);
    DSO_Timebase_RenderToBuffer(samples,
                                TEST_SAMPLE_COUNT,
                                1666666u,
                                &analysis,
                                50u,
                                DSO_ACQUIRE_MODE_SAMPLE,
                                true,
                                display,
                                NULL,
                                &meta);

    if (!meta.allow_ets_requested ||
        !meta.allow_ets_effective ||
        !meta.hs8_candidate ||
        !meta.ets_candidate) {
        fprintf(stderr,
                "8-bit tri-lane cal gate did not re-enable enhanced candidates: req=%u eff=%u hs8=%u ets=%u\n",
                meta.allow_ets_requested ? 1u : 0u,
                meta.allow_ets_effective ? 1u : 0u,
                meta.hs8_candidate ? 1u : 0u,
                meta.ets_candidate ? 1u : 0u);
        return 1;
    }

    return 0;
}

static int test_8bit_dual_lane_midrate_entries_use_detuned_arr(void)
{
    DSO_TimebaseInfo info = {0};

    Test_AcqStub_Reset();
    Test_AcqStub_SetLaneCount(2u);
    Test_AcqStub_SetTimClkHz(200000000u);

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
    DSO_Timebase_Init();

    DSO_Timebase_SetIndex(11u);
    (void)DSO_Timebase_ApplyPending();
    DSO_Timebase_GetInfo(&info);
    if ((DSO_Timebase_GetTimArr() != 55u) ||
        (info.hw_sample_rate != 7142856u) ||
        (DSO_Timebase_GetSampleRateHz() != 7142856u)) {
        fprintf(stderr,
                "8-bit dual-lane index 11 not detuned as expected: arr=%lu info=%lu get=%lu\n",
                (unsigned long)DSO_Timebase_GetTimArr(),
                (unsigned long)info.hw_sample_rate,
                (unsigned long)DSO_Timebase_GetSampleRateHz());
        return 1;
    }

    DSO_Timebase_SetIndex(12u);
    (void)DSO_Timebase_ApplyPending();
    DSO_Timebase_GetInfo(&info);
    if ((DSO_Timebase_GetTimArr() != 111u) ||
        (info.hw_sample_rate != 3571428u) ||
        (DSO_Timebase_GetSampleRateHz() != 3571428u)) {
        fprintf(stderr,
                "8-bit dual-lane index 12 not detuned as expected: arr=%lu info=%lu get=%lu\n",
                (unsigned long)DSO_Timebase_GetTimArr(),
                (unsigned long)info.hw_sample_rate,
                (unsigned long)DSO_Timebase_GetSampleRateHz());
        return 1;
    }

    return 0;
}

static int test_8bit_tri_lane_midrate_entries_use_tri_arr_ladder(void)
{
    DSO_TimebaseInfo info = {0};

    Test_AcqStub_Reset();
    Test_AcqStub_SetLaneCount(3u);
    Test_AcqStub_SetTimClkHz(200000000u);

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
    DSO_Timebase_Init();

    DSO_Timebase_SetIndex(11u);
    (void)DSO_Timebase_ApplyPending();
    DSO_Timebase_GetInfo(&info);
    if ((DSO_Timebase_GetTimArr() != 71u) ||
        (info.hw_sample_rate != 8333331u) ||
        (DSO_Timebase_GetSampleRateHz() != 8333331u)) {
        fprintf(stderr,
                "8-bit tri-lane index 11 not using tri ARR ladder: arr=%lu info=%lu get=%lu\n",
                (unsigned long)DSO_Timebase_GetTimArr(),
                (unsigned long)info.hw_sample_rate,
                (unsigned long)DSO_Timebase_GetSampleRateHz());
        return 1;
    }

    DSO_Timebase_SetIndex(12u);
    (void)DSO_Timebase_ApplyPending();
    DSO_Timebase_GetInfo(&info);
    if ((DSO_Timebase_GetTimArr() != 143u) ||
        (info.hw_sample_rate != 4166664u) ||
        (DSO_Timebase_GetSampleRateHz() != 4166664u)) {
        fprintf(stderr,
                "8-bit tri-lane index 12 not using tri ARR ladder: arr=%lu info=%lu get=%lu\n",
                (unsigned long)DSO_Timebase_GetTimArr(),
                (unsigned long)info.hw_sample_rate,
                (unsigned long)DSO_Timebase_GetSampleRateHz());
        return 1;
    }

    return 0;
}

static int test_8bit_peak_detect_uses_minmax_envelope(void)
{
    uint16_t samples[TEST_SAMPLE_COUNT];
    uint16_t display[DSO_DISPLAY_WIDTH];
    uint16_t display_hi[DSO_DISPLAY_WIDTH];
    DSO_ProcessAnalysis analysis = {0};
    DSO_RenderMeta meta = {0};

    for (uint32_t bucket = 0u; bucket < DSO_DISPLAY_WIDTH; ++bucket) {
        uint32_t base = bucket * 4u;
        samples[base + 0u] = 16u;
        samples[base + 1u] = 220u;
        samples[base + 2u] = 48u;
        samples[base + 3u] = 192u;
    }

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
    DSO_Timebase_Init();
    DSO_Timebase_SetIndex(12u);
    (void)DSO_Timebase_ApplyPending();

    analysis.trigger_index = (int32_t)(TEST_SAMPLE_COUNT / 2u);
    analysis.trigger_pos_samples = (float)analysis.trigger_index;
    analysis.triggered = true;

    DSO_Timebase_RenderToBuffer(samples,
                                TEST_SAMPLE_COUNT,
                                1666666u,
                                &analysis,
                                50u,
                                DSO_ACQUIRE_MODE_PEAK_DETECT,
                                false,
                                display,
                                display_hi,
                                &meta);

    if ((display[0] > 32u) || (display[2] > 64u)) {
        fprintf(stderr,
                "peak detect lost low envelope: d0=%u d2=%u\n",
                (unsigned)display[0],
                (unsigned)display[2]);
        return 1;
    }
    if ((display_hi[1] < 180u) || (display_hi[3] < 180u)) {
        fprintf(stderr,
                "peak detect lost high envelope: dh1=%u dh3=%u\n",
                (unsigned)display_hi[1],
                (unsigned)display_hi[3]);
        return 1;
    }

    (void)meta;
    return 0;
}

static int test_8bit_hires_boxcar_reduces_rate_and_noise(void)
{
    uint16_t samples[TEST_SAMPLE_COUNT];
    uint16_t out[TEST_SAMPLE_COUNT];
    uint32_t out_rate_hz = 0u;
    uint32_t out_count;

    for (uint32_t bucket = 0u; bucket < DSO_DISPLAY_WIDTH; ++bucket) {
        uint32_t base = bucket * 4u;
        samples[base + 0u] = 100u;
        samples[base + 1u] = 140u;
        samples[base + 2u] = 100u;
        samples[base + 3u] = 140u;
    }

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
    DSO_Timebase_Init();

    out_count = DSO_Timebase_HiResBoxcar(samples,
                                         TEST_SAMPLE_COUNT,
                                         4000u,
                                         out,
                                         TEST_SAMPLE_COUNT,
                                         &out_rate_hz);

    if (out_count != DSO_DISPLAY_WIDTH) {
        fprintf(stderr,
                "hires output count mismatch: got=%lu want=%u\n",
                (unsigned long)out_count,
                (unsigned)DSO_DISPLAY_WIDTH);
        return 1;
    }
    if (out_rate_hz != 1000u) {
        fprintf(stderr,
                "hires effective rate mismatch: got=%lu want=1000\n",
                (unsigned long)out_rate_hz);
        return 1;
    }
    if ((out[0] != 120u) || (out[1] != 120u) || (out[2] != 120u)) {
        fprintf(stderr,
                "hires boxcar did not average samples: %u,%u,%u\n",
                (unsigned)out[0],
                (unsigned)out[1],
                (unsigned)out[2]);
        return 1;
    }

    return 0;
}

int main(void)
{
    if (test_8bit_peak_detect_uses_minmax_envelope() != 0) {
        return 1;
    }
    if (test_8bit_hires_boxcar_reduces_rate_and_noise() != 0) {
        return 1;
    }
    if (test_8bit_timebase_uses_tri_lane_count_for_multilane_gating() != 0) {
        return 1;
    }
    if (test_8bit_tri_lane_cal_ready_gate_blocks_enhanced_recon() != 0) {
        return 1;
    }
    if (test_8bit_tri_lane_midrate_entries_use_tri_arr_ladder() != 0) {
        return 1;
    }

    printf("all 8-bit timebase tests passed\n");
    return 0;
}
