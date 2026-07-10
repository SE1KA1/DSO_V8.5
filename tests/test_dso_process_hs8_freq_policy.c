#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dso_acq_timebase_stubs.h"
#include "dso/dso_process.h"
#include "dso/dso_sample_spec.h"

static void build_pair_locked_samples(uint16_t *dst, uint32_t count)
{
    static const uint16_t pattern[] = {
        26u, 26u, 155u, 155u, 162u, 162u, 26u, 26u,
        239u, 239u, 27u, 27u, 154u, 154u, 160u, 160u
    };
    const uint32_t pattern_count = (uint32_t)(sizeof(pattern) / sizeof(pattern[0]));

    for (uint32_t i = 0u; i < count; ++i) {
        dst[i] = pattern[i % pattern_count];
    }
}

static void build_clean_ramp_samples(uint16_t *dst, uint32_t count)
{
    for (uint32_t i = 0u; i < count; ++i) {
        dst[i] = (uint16_t)((i * 7u) & 0xFFu);
    }
}

static int test_hs8_dual_pair_locked_freq_is_not_reliable(void)
{
    uint16_t samples[256];
    DSO_ProcessAnalysis analysis = {
        .freq_hz = 714321.88f,
        .freq_valid = true,
        .samples_per_cycle = 5.0f,
        .repetitive_confidence = 0.87f,
    };

    build_pair_locked_samples(samples, (uint32_t)(sizeof(samples) / sizeof(samples[0])));

    Test_AcqStub_Reset();
    Test_AcqStub_SetLaneCount(2u);
    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);

    if (DSO_Process_IsHs8DualFreqReliable(samples,
                                          (uint32_t)(sizeof(samples) / sizeof(samples[0])),
                                          &analysis)) {
        fprintf(stderr, "pair-locked dual-hs8 stream should be treated as unreliable\n");
        return 1;
    }

    return 0;
}

static int test_hs8_dual_low_density_stream_remains_reliable(void)
{
    uint16_t samples[256];
    DSO_ProcessAnalysis analysis = {
        .freq_hz = 35252.59f,
        .freq_valid = true,
        .samples_per_cycle = 9.99f,
        .repetitive_confidence = 0.99f,
    };

    build_clean_ramp_samples(samples, (uint32_t)(sizeof(samples) / sizeof(samples[0])));

    Test_AcqStub_Reset();
    Test_AcqStub_SetLaneCount(2u);
    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);

    if (!DSO_Process_IsHs8DualFreqReliable(samples,
                                           (uint32_t)(sizeof(samples) / sizeof(samples[0])),
                                           &analysis)) {
        fprintf(stderr, "clean lower-density dual-hs8 stream should remain reliable\n");
        return 1;
    }

    return 0;
}

static int test_non_hs8_or_single_lane_remains_reliable(void)
{
    uint16_t samples[64];
    DSO_ProcessAnalysis analysis = {
        .freq_hz = 400000.0f,
        .freq_valid = true,
        .samples_per_cycle = 4.0f,
        .repetitive_confidence = 0.80f,
    };

    memset(samples, 0, sizeof(samples));

    Test_AcqStub_Reset();
    Test_AcqStub_SetLaneCount(1u);
    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);

    if (!DSO_Process_IsHs8DualFreqReliable(samples,
                                           (uint32_t)(sizeof(samples) / sizeof(samples[0])),
                                           &analysis)) {
        fprintf(stderr, "single-lane hs8 stream should not be rejected by dual-lane policy\n");
        return 1;
    }

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_10BIT);
    Test_AcqStub_SetLaneCount(2u);

    if (!DSO_Process_IsHs8DualFreqReliable(samples,
                                           (uint32_t)(sizeof(samples) / sizeof(samples[0])),
                                           &analysis)) {
        fprintf(stderr, "10-bit stream should not be rejected by hs8 dual policy\n");
        return 1;
    }

    return 0;
}

static int test_tri_lane_stream_bypasses_dual_pair_lock_rejection(void)
{
    uint16_t samples[256];
    DSO_ProcessAnalysis analysis = {
        .freq_hz = 714321.88f,
        .freq_valid = true,
        .samples_per_cycle = 5.0f,
        .repetitive_confidence = 0.87f,
    };

    build_pair_locked_samples(samples, (uint32_t)(sizeof(samples) / sizeof(samples[0])));

    Test_AcqStub_Reset();
    Test_AcqStub_SetLaneCount(3u);
    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);

    if (!DSO_Process_IsHs8DualFreqReliable(samples,
                                           (uint32_t)(sizeof(samples) / sizeof(samples[0])),
                                           &analysis)) {
        fprintf(stderr, "tri-lane hs8 stream should bypass the dual pair-lock rejection\n");
        return 1;
    }

    return 0;
}

int main(void)
{
    if (test_hs8_dual_pair_locked_freq_is_not_reliable() != 0) {
        return 1;
    }
    if (test_hs8_dual_low_density_stream_remains_reliable() != 0) {
        return 1;
    }
    if (test_non_hs8_or_single_lane_remains_reliable() != 0) {
        return 1;
    }
    if (test_tri_lane_stream_bypasses_dual_pair_lock_rejection() != 0) {
        return 1;
    }

    printf("all hs8 dual frequency policy tests passed\n");
    return 0;
}
