#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "dso_acq_timebase_stubs.h"
#include "dso/dso_sample_spec.h"
#include "dso/dso_timebase.h"

#define TEST_SAMPLE_COUNT (DSO_DISPLAY_WIDTH * 4u)
#define PI_F 3.14159265358979323846f

static uint16_t clamp_u16(float v)
{
    if (v < 0.0f) {
        return 0u;
    }
    if (v > 1023.0f) {
        return 1023u;
    }
    return (uint16_t)(v + 0.5f);
}

static void build_repetitive_sine(uint16_t *dst,
                                  uint32_t count,
                                  float dc,
                                  float amp,
                                  float cycles)
{
    uint32_t i;
    for (i = 0u; i < count; ++i) {
        float phase = (2.0f * PI_F * cycles * (float)i) / (float)count;
        dst[i] = clamp_u16(dc + amp * sinf(phase));
    }
}

static int test_10bit_mid_density_repetitive_path_enters_hs10_mode(void)
{
    uint16_t samples[TEST_SAMPLE_COUNT];
    uint16_t display[DSO_DISPLAY_WIDTH];
    DSO_ProcessAnalysis analysis = {0};
    DSO_RenderMeta meta = {0};

    build_repetitive_sine(samples, TEST_SAMPLE_COUNT, 512.0f, 220.0f, 93.44f);

    Test_AcqStub_Reset();

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_10BIT);
    DSO_Timebase_Init();
    DSO_Timebase_SetIndex(3u);
    (void)DSO_Timebase_ApplyPending();

    analysis.freq_valid = true;
    analysis.freq_hz = 73000.0f;
    analysis.trigger_index = (int32_t)(TEST_SAMPLE_COUNT / 2u);
    analysis.trigger_pos_samples = (float)analysis.trigger_index + 0.5f;
    analysis.triggered = true;
    analysis.samples_per_cycle = 5.5f;
    analysis.repetitive_confidence = 0.88f;
    analysis.clipped = false;

    DSO_Timebase_RenderToBuffer(samples,
                                TEST_SAMPLE_COUNT,
                                2500000u,
                                &analysis,
                                50u,
                                DSO_ACQUIRE_MODE_SAMPLE,
                                true,
                                display,
                                NULL,
                                &meta);

    if (meta.recon_mode != DSO_RECON_MODE_HS10_ENHANCED) {
        fprintf(stderr, "10-bit timebase did not enter HS10 mode: mode=%u\n",
                (unsigned)meta.recon_mode);
        return 1;
    }

    return 0;
}

static int test_10bit_ch2_path_does_not_enter_hs10_mode(void)
{
    uint16_t samples[TEST_SAMPLE_COUNT];
    uint16_t display[DSO_DISPLAY_WIDTH];
    DSO_ProcessAnalysis analysis = {0};
    DSO_RenderMeta meta = {0};

    build_repetitive_sine(samples, TEST_SAMPLE_COUNT, 512.0f, 220.0f, 93.44f);

    Test_AcqStub_Reset();

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_10BIT);
    DSO_Timebase_Init();
    DSO_Timebase_SetIndex(3u);
    (void)DSO_Timebase_ApplyPending();

    analysis.freq_valid = true;
    analysis.freq_hz = 73000.0f;
    analysis.trigger_index = (int32_t)(TEST_SAMPLE_COUNT / 2u);
    analysis.trigger_pos_samples = (float)analysis.trigger_index + 0.5f;
    analysis.triggered = true;
    analysis.samples_per_cycle = 5.5f;
    analysis.repetitive_confidence = 0.88f;
    analysis.clipped = false;

    DSO_Timebase_RenderToBufferForChannel(1u,
                                          samples,
                                          TEST_SAMPLE_COUNT,
                                          2500000u,
                                          &analysis,
                                          50u,
                                          DSO_ACQUIRE_MODE_SAMPLE,
                                          true,
                                          display,
                                          NULL,
                                          &meta);

    if (meta.recon_mode == DSO_RECON_MODE_HS10_ENHANCED) {
        fprintf(stderr, "10-bit CH2 path must not enter HS10 mode\n");
        return 1;
    }

    return 0;
}

int main(void)
{
    if (test_10bit_mid_density_repetitive_path_enters_hs10_mode() != 0) {
        return 1;
    }
    if (test_10bit_ch2_path_does_not_enter_hs10_mode() != 0) {
        return 1;
    }

    printf("all timebase 10bit tests passed\n");
    return 0;
}
