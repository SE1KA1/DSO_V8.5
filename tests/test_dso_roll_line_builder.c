#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dso/dso_config.h"
#include "dso/dso_roll_line_builder.h"
#include "dso/dso_roll_physics.h"

static int test_needed_sample_count_clamps(void)
{
    uint32_t n;

    n = DSO_RollLineBuilder_NeededSampleCount(0.0f, 1000000u, DSO_DISPLAY_WIDTH, 10000u);
    if (n != 0u) {
        fprintf(stderr, "needed count: expected 0 for invalid tpp, got %u\n", n);
        return 1;
    }

    n = DSO_RollLineBuilder_NeededSampleCount(1000.0f, 0u, DSO_DISPLAY_WIDTH, 10000u);
    if (n != 0u) {
        fprintf(stderr, "needed count: expected 0 for zero rate, got %u\n", n);
        return 1;
    }

    n = DSO_RollLineBuilder_NeededSampleCount(10.0f, 1000000u, DSO_DISPLAY_WIDTH, 500u);
    if (n != 500u) {
        fprintf(stderr, "needed count: expected clamp to pull_max 500, got %u\n", n);
        return 1;
    }

    return 0;
}

static int test_needed_full_not_clamped_to_pull(void)
{
    uint32_t full;
    uint32_t clamped;

    /* ~100 samples/pixel at 1 Msps -> need ~61k > pull_max 500 */
    full = DSO_RollLineBuilder_NeededSampleCountFull(1.0e8f, 1000000u, DSO_DISPLAY_WIDTH);
    clamped = DSO_RollLineBuilder_NeededSampleCount(1.0e8f, 1000000u, DSO_DISPLAY_WIDTH, 500u);
    if (full <= clamped) {
        fprintf(stderr, "needed full: expected full > clamped(500), full=%u clamped=%u\n",
                full, clamped);
        return 1;
    }
    if (clamped != 500u) {
        fprintf(stderr, "clamped: expected 500, got %u\n", clamped);
        return 1;
    }
    return 0;
}

static int test_incremental_feed_matches_build(void)
{
    enum { W = 32u, N = 8000u };
    uint16_t samples[N];
    uint16_t out_mono[32];
    uint16_t out_inc[32];
    DSO_RollLineBuilderState st;
    uint32_t i;
    uint32_t step = 700u;

    for (i = 0u; i < N; i++) {
        samples[i] = (uint16_t)(i % 251u);
    }

    (void)memset(out_mono, 0, sizeof(out_mono));
    DSO_RollLineBuilder_Build(samples, N, 200000u, 500000u, out_mono, W);

    (void)memset(out_inc, 0, sizeof(out_inc));
    DSO_RollLineBuilder_Begin(&st, 200000u, 500000u, W);
    for (i = 0u; i < N; i += step) {
        uint32_t rem = N - i;
        uint32_t c = (rem > step) ? step : rem;

        DSO_RollLineBuilder_Feed(&st, samples + i, c, out_inc);
    }
    DSO_RollLineBuilder_FinishPad(&st, out_inc);

    for (i = 0u; i < W; i++) {
        if (out_mono[i] != out_inc[i]) {
            fprintf(stderr, "incremental mismatch at %u: mono=%u inc=%u\n",
                    i,
                    (unsigned)out_mono[i],
                    (unsigned)out_inc[i]);
            return 1;
        }
    }
    return 0;
}

static int test_build_fills_width_and_boxcar(void)
{
    enum { W = 16u };
    uint16_t samples[256];
    uint16_t out[W];
    uint32_t i;

    for (i = 0u; i < 256u; i++) {
        samples[i] = (uint16_t)(i & 0xFFu);
    }

    (void)memset(out, 0, sizeof(out));
    /* 1e-3 s/div * 10 div = 10 ms window; W columns -> easy ratio */
    DSO_RollLineBuilder_Build(samples,
                              256u,
                              256000u,
                              1000000u,
                              out,
                              W);

    for (i = 0u; i < W; i++) {
        if (out[i] == 0u) {
            fprintf(stderr, "build: out[%u] unexpectedly zero\n", i);
            return 1;
        }
    }
    return 0;
}

static int test_remainder_export_nonzero(void)
{
    DSO_RollLineBuilderState st;
    uint16_t out[8];
    float rs;
    float rc;

    (void)memset(out, 0, sizeof(out));
    DSO_RollLineBuilder_Begin(&st, 1000u, 1000000u, 8u);
    /* spp = 1000 * (1e-3*10/8) = 1.25 — feed 3 samples -> partial bin remains */
    {
        uint16_t s3[3] = { 100u, 200u, 300u };
        DSO_RollLineBuilder_Feed(&st, s3, 3u, out);
    }
    DSO_RollLineBuilder_ExportRemainder(&st, &rs, &rc);
    if (rc <= 0.0f) {
        fprintf(stderr, "remainder: expected partial acc_count > 0\n");
        return 1;
    }
    return 0;
}

static int test_slow_roll_cap_reduces_need(void)
{
    float tpp = DSO_RollPhysics_TimePerPixelNs(1000000000u, DSO_DISPLAY_WIDTH);
    uint32_t full;
    uint32_t capped;

    full = DSO_RollLineBuilder_NeededSampleCountFull(tpp, 1000000u, DSO_DISPLAY_WIDTH);
    capped = DSO_RollLineBuilder_CappedNeedSamples(tpp,
                                                   1000000u,
                                                   1000000000u,
                                                   DSO_DISPLAY_WIDTH,
                                                   full + 1000u,
                                                   full + 1000u);
    if (full == 0u || capped >= full) {
        fprintf(stderr, "slow cap: expected capped < full, full=%u capped=%u\n", full, capped);
        return 1;
    }
    return 0;
}

static int test_physics_matches_builder_spp(void)
{
    uint32_t sec = 1000000u;
    uint32_t sr  = 256000u;
    uint32_t w   = 16u;
    DSO_RollLineBuilderState st;
    float spp_ph;

    spp_ph = DSO_RollPhysics_SamplesPerPixel(sec, sr, w);
    DSO_RollLineBuilder_Begin(&st, sr, sec, w);
    if (fabsf(spp_ph - st.samples_per_pixel) > 1e-4f) {
        fprintf(stderr, "spp mismatch: physics=%f builder=%f\n", spp_ph, st.samples_per_pixel);
        return 1;
    }
    return 0;
}

int main(void)
{
    if (test_needed_sample_count_clamps() != 0) {
        return 1;
    }
    if (test_needed_full_not_clamped_to_pull() != 0) {
        return 1;
    }
    if (test_incremental_feed_matches_build() != 0) {
        return 1;
    }
    if (test_build_fills_width_and_boxcar() != 0) {
        return 1;
    }
    if (test_remainder_export_nonzero() != 0) {
        return 1;
    }
    if (test_slow_roll_cap_reduces_need() != 0) {
        return 1;
    }
    if (test_physics_matches_builder_spp() != 0) {
        return 1;
    }
    printf("all dso_roll_line_builder tests passed\n");
    return 0;
}
