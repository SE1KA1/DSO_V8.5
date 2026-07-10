#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dso_hs8_lane_cal.h"

#define TEST_COUNT 3072u
#define PI_F 3.14159265358979323846f

static uint16_t clamp_u8(float v)
{
    if (v < 0.0f) {
        return 0u;
    }
    if (v > 255.0f) {
        return 255u;
    }
    return (uint16_t)(v + 0.5f);
}

static void build_clean_sine(uint16_t *dst,
                             uint32_t count,
                             float dc,
                             float amp,
                             float cycles)
{
    for (uint32_t i = 0; i < count; ++i) {
        float phase = (2.0f * PI_F * cycles * (float)i) / (float)count;
        dst[i] = clamp_u8(dc + amp * sinf(phase));
    }
}

static void distort_three_phase(const uint16_t *src,
                                uint16_t *dst,
                                uint32_t count,
                                const DSO_HS8LaneCoeffs phase[3])
{
    memcpy(dst, src, sizeof(uint16_t) * count);
    for (uint32_t i = 1u; i + 1u < count; ++i) {
        uint32_t lane = i % 3u;
        float slope;
        float raw;

        if (lane == 0u) {
            continue;
        }

        slope = ((float)src[i + 1u] - (float)src[i - 1u]) * 0.5f;
        raw = phase[lane].offset_lsb +
              (phase[lane].gain * (float)src[i]) +
              (phase[lane].skew * slope);
        dst[i] = clamp_u8(raw);
    }
}

static float lane_mae(const uint16_t *lhs,
                      const uint16_t *rhs,
                      uint32_t count,
                      uint32_t lane)
{
    float total = 0.0f;
    uint32_t used = 0u;

    for (uint32_t i = lane; i < count; i += 3u) {
        total += fabsf((float)lhs[i] - (float)rhs[i]);
        used++;
    }

    return (used == 0u) ? 0.0f : (total / (float)used);
}

static void distort_two_phase(const uint16_t *src,
                              uint16_t *dst,
                              uint32_t count,
                              const DSO_HS8LaneCoeffs *lane1)
{
    memcpy(dst, src, sizeof(uint16_t) * count);
    for (uint32_t i = 1u; i + 1u < count; i += 2u) {
        float slope = ((float)src[i + 1u] - (float)src[i - 1u]) * 0.5f;
        float raw = lane1->offset_lsb +
                    (lane1->gain * (float)src[i]) +
                    (lane1->skew * slope);
        dst[i] = clamp_u8(raw);
    }
}

static float lane_mae_stride(const uint16_t *lhs,
                             const uint16_t *rhs,
                             uint32_t count,
                             uint32_t lane,
                             uint32_t stride)
{
    float total = 0.0f;
    uint32_t used = 0u;

    for (uint32_t i = lane; i < count; i += stride) {
        total += fabsf((float)lhs[i] - (float)rhs[i]);
        used++;
    }

    return (used == 0u) ? 0.0f : (total / (float)used);
}

static int test_solve_frame_recovers_phase1_and_phase2_distortion(void)
{
    uint16_t clean[TEST_COUNT];
    uint16_t distorted[TEST_COUNT];
    DSO_HS8LaneCoeffs solved[3] = {{0}};
    DSO_HS8LaneCoeffs expect[3] = {
        { .offset_lsb = 0.0f,  .gain = 1.0f,  .skew = 0.0f,  .valid = true },
        { .offset_lsb = 11.0f, .gain = 1.08f, .skew = 0.32f, .valid = true },
        { .offset_lsb = -8.0f, .gain = 0.94f, .skew = -0.24f, .valid = true },
    };

    build_clean_sine(clean, TEST_COUNT, 128.0f, 80.0f, 133.0f);
    distort_three_phase(clean, distorted, TEST_COUNT, expect);

    if (!DSO_HS8LaneCal_SolveFrame(distorted, TEST_COUNT, 255u, solved)) {
        fprintf(stderr, "solve frame returned false\n");
        return 1;
    }

    if (!solved[1].valid || !solved[2].valid) {
        fprintf(stderr, "phase coefficients were not marked valid\n");
        return 1;
    }

    if (fabsf(solved[1].offset_lsb - expect[1].offset_lsb) > 4.0f ||
        fabsf(solved[1].gain - expect[1].gain) > 0.06f ||
        fabsf(solved[1].skew - expect[1].skew) > 0.15f) {
        fprintf(stderr, "phase1 solve mismatch: off=%.3f gain=%.4f skew=%.4f\n",
                (double)solved[1].offset_lsb,
                (double)solved[1].gain,
                (double)solved[1].skew);
        return 1;
    }

    if (fabsf(solved[2].offset_lsb - expect[2].offset_lsb) > 4.0f ||
        fabsf(solved[2].gain - expect[2].gain) > 0.06f ||
        fabsf(solved[2].skew - expect[2].skew) > 0.15f) {
        fprintf(stderr, "phase2 solve mismatch: off=%.3f gain=%.4f skew=%.4f\n",
                (double)solved[2].offset_lsb,
                (double)solved[2].gain,
                (double)solved[2].skew);
        return 1;
    }

    return 0;
}

static int test_apply_coeffs_significantly_reduces_phase_error(void)
{
    uint16_t clean[TEST_COUNT];
    uint16_t distorted[TEST_COUNT];
    DSO_HS8LaneCoeffs solved[3] = {{0}};
    DSO_HS8LaneCoeffs expect[3] = {
        { .offset_lsb = 0.0f, .gain = 1.0f, .skew = 0.0f, .valid = true },
        { .offset_lsb = 9.0f, .gain = 1.06f, .skew = 0.26f, .valid = true },
        { .offset_lsb = -7.0f, .gain = 0.92f, .skew = -0.21f, .valid = true },
    };
    float before1;
    float before2;
    float after1;
    float after2;

    build_clean_sine(clean, TEST_COUNT, 128.0f, 76.0f, 111.0f);
    distort_three_phase(clean, distorted, TEST_COUNT, expect);

    before1 = lane_mae(clean, distorted, TEST_COUNT, 1u);
    before2 = lane_mae(clean, distorted, TEST_COUNT, 2u);

    if (!DSO_HS8LaneCal_SolveFrame(distorted, TEST_COUNT, 255u, solved)) {
        fprintf(stderr, "solve frame returned false before apply\n");
        return 1;
    }

    DSO_HS8LaneCal_ApplyWithCoeffs(solved, 255u, distorted, TEST_COUNT);
    after1 = lane_mae(clean, distorted, TEST_COUNT, 1u);
    after2 = lane_mae(clean, distorted, TEST_COUNT, 2u);

    if (!(after1 < (before1 * 0.40f)) || !(after2 < (before2 * 0.40f))) {
        fprintf(stderr, "apply did not improve enough: p1 %.3f->%.3f p2 %.3f->%.3f\n",
                (double)before1,
                (double)after1,
                (double)before2,
                (double)after2);
        return 1;
    }

    return 0;
}

static int test_service_frame_requires_repetitive_valid_signal_and_resets_on_freq_jump(void)
{
    uint16_t clean[TEST_COUNT];
    uint16_t distorted[TEST_COUNT];
    DSO_HS8LaneCoeffs phase[3] = {
        { .offset_lsb = 0.0f,  .gain = 1.0f,  .skew = 0.0f,  .valid = true },
        { .offset_lsb = 10.0f, .gain = 1.05f, .skew = 0.20f, .valid = true },
        { .offset_lsb = -6.0f, .gain = 0.95f, .skew = -0.18f, .valid = true },
    };
    DSO_HS8LaneCalState state;

    build_clean_sine(clean, TEST_COUNT, 128.0f, 70.0f, 121.0f);
    distort_three_phase(clean, distorted, TEST_COUNT, phase);
    DSO_HS8LaneCal_Reset(&state);

    if (DSO_HS8LaneCal_ServiceFrame(&state,
                                    distorted,
                                    TEST_COUNT,
                                    255u,
                                    3000000.0f,
                                    false,
                                    false,
                                    0.95f)) {
        fprintf(stderr, "service frame unexpectedly updated on invalid frequency\n");
        return 1;
    }

    for (uint32_t i = 0u; i < 4u; ++i) {
        if (!DSO_HS8LaneCal_ServiceFrame(&state,
                                         distorted,
                                         TEST_COUNT,
                                         255u,
                                         3000000.0f,
                                         true,
                                         false,
                                         0.95f)) {
            fprintf(stderr, "service frame did not accept stable repetitive frame %lu\n",
                    (unsigned long)i);
            return 1;
        }
    }

    if (!DSO_HS8LaneCal_IsReady(&state)) {
        fprintf(stderr, "state never became ready after stable frames\n");
        return 1;
    }

    (void)DSO_HS8LaneCal_ServiceFrame(&state,
                                      distorted,
                                      TEST_COUNT,
                                      255u,
                                      4200000.0f,
                                      true,
                                      false,
                                      0.95f);

    if (DSO_HS8LaneCal_IsReady(&state)) {
        fprintf(stderr, "state stayed ready after frequency jump\n");
        return 1;
    }

    return 0;
}

static int test_dual_lane_service_and_apply_reduce_lane1_error(void)
{
    uint16_t clean[TEST_COUNT];
    uint16_t distorted[TEST_COUNT];
    DSO_HS8LaneCalState state;
    DSO_HS8LaneCoeffs lane1 = {
        .offset_lsb = 7.0f,
        .gain = 1.07f,
        .skew = 0.18f,
        .valid = true,
    };
    float before;
    float after;

    build_clean_sine(clean, TEST_COUNT, 128.0f, 72.0f, 97.0f);
    distort_two_phase(clean, distorted, TEST_COUNT, &lane1);
    before = lane_mae_stride(clean, distorted, TEST_COUNT, 1u, 2u);

    DSO_HS8LaneCal_ResetForLaneCount(&state, 2u);
    for (uint32_t i = 0u; i < 4u; ++i) {
        if (!DSO_HS8LaneCal_ServiceFrameEx(&state,
                                           distorted,
                                           TEST_COUNT,
                                           255u,
                                           2500000.0f,
                                           true,
                                           false,
                                           0.96f,
                                           2u)) {
            fprintf(stderr, "dual service frame rejected stable frame %lu\n",
                    (unsigned long)i);
            return 1;
        }
    }

    if (!DSO_HS8LaneCal_IsReadyForLaneCount(&state, 2u)) {
        fprintf(stderr, "dual calibration never became ready\n");
        return 1;
    }

    DSO_HS8LaneCal_ApplyEx(&state, 255u, distorted, TEST_COUNT, 2u);
    after = lane_mae_stride(clean, distorted, TEST_COUNT, 1u, 2u);
    if (!(after < (before * 0.45f))) {
        fprintf(stderr, "dual apply did not improve enough: %.3f->%.3f\n",
                (double)before,
                (double)after);
        return 1;
    }

    return 0;
}

static int test_dual_lane_spc_gate_requires_enough_per_lane_density(void)
{
    if (DSO_HS8LaneCal_IsSampleDensityUsable(3.33f, 2u)) {
        fprintf(stderr, "dual lane SPC gate incorrectly accepted 3.33 total spc\n");
        return 1;
    }

    if (DSO_HS8LaneCal_IsSampleDensityUsable(5.00f, 2u)) {
        fprintf(stderr, "dual lane SPC gate incorrectly accepted 5.00 total spc\n");
        return 1;
    }

    if (!DSO_HS8LaneCal_IsSampleDensityUsable(8.00f, 2u)) {
        fprintf(stderr, "dual lane SPC gate rejected 8.00 total spc\n");
        return 1;
    }

    if (!DSO_HS8LaneCal_IsSampleDensityUsable(12.00f, 2u)) {
        fprintf(stderr, "dual lane SPC gate rejected 12.00 total spc\n");
        return 1;
    }

    return 0;
}

int main(void)
{
    int rc = 0;

    rc |= test_solve_frame_recovers_phase1_and_phase2_distortion();
    rc |= test_apply_coeffs_significantly_reduces_phase_error();
    rc |= test_service_frame_requires_repetitive_valid_signal_and_resets_on_freq_jump();
    rc |= test_dual_lane_service_and_apply_reduce_lane1_error();
    rc |= test_dual_lane_spc_gate_requires_enough_per_lane_density();

    if (rc == 0) {
        puts("all hs8 lane calibration tests passed");
    }

    return rc;
}
