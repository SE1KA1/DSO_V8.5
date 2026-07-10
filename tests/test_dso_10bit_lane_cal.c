#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dso/dso_10bit_lane_cal.h"
#include "dso/dso_sample_spec.h"

#define TEST_COUNT 2048u
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

static void build_clean_sine(uint16_t *dst,
                             uint32_t count,
                             float dc,
                             float amp,
                             float cycles)
{
    for (uint32_t i = 0; i < count; ++i) {
        float phase = (2.0f * PI_F * cycles * (float)i) / (float)count;
        dst[i] = clamp_u16(dc + amp * sinf(phase));
    }
}

static void distort_interleaved(const uint16_t *src,
                                uint16_t *dst,
                                uint32_t count,
                                float offset,
                                float gain,
                                float skew)
{
    memcpy(dst, src, sizeof(uint16_t) * count);
    for (uint32_t i = 1u; i + 1u < count; i += 2u) {
        float slope = ((float)src[i + 1u] - (float)src[i - 1u]) * 0.5f;
        float raw = offset + (gain * (float)src[i]) + (skew * slope);
        dst[i] = clamp_u16(raw);
    }
}

static float odd_lane_mae(const uint16_t *lhs, const uint16_t *rhs, uint32_t count)
{
    float total = 0.0f;
    uint32_t used = 0u;

    for (uint32_t i = 1u; i < count; i += 2u) {
        total += fabsf((float)lhs[i] - (float)rhs[i]);
        used++;
    }

    return (used == 0u) ? 0.0f : (total / (float)used);
}

static int test_apply_coeffs_removes_odd_lane_error(void)
{
    uint16_t clean[TEST_COUNT];
    uint16_t distorted[TEST_COUNT];
    DSO_10bitLaneCalCoeffs coeffs = {
        .offset_lsb = 18.0f,
        .gain = 1.045f,
        .skew = 0.30f,
        .valid = true,
    };
    float before;
    float after;

    build_clean_sine(clean, TEST_COUNT, 512.0f, 220.0f, 77.0f);
    distort_interleaved(clean, distorted, TEST_COUNT, coeffs.offset_lsb, coeffs.gain, coeffs.skew);

    before = odd_lane_mae(clean, distorted, TEST_COUNT);
    DSO_10bitLaneCal_ApplyWithCoeffs(distorted, TEST_COUNT, &coeffs, 1023u);
    after = odd_lane_mae(clean, distorted, TEST_COUNT);

    if (!(after < (before * 0.20f))) {
        fprintf(stderr, "apply coeffs did not improve error enough: before=%.3f after=%.3f\n",
                (double)before,
                (double)after);
        return 1;
    }

    return 0;
}

static int test_solve_frame_recovers_known_lane_mismatch(void)
{
    uint16_t clean[TEST_COUNT];
    uint16_t distorted[TEST_COUNT];
    DSO_10bitLaneCalCoeffs coeffs = {0};
    const float expect_offset = 21.0f;
    const float expect_gain = 1.060f;
    const float expect_skew = -0.22f;

    build_clean_sine(clean, TEST_COUNT, 512.0f, 210.0f, 83.0f);
    distort_interleaved(clean, distorted, TEST_COUNT, expect_offset, expect_gain, expect_skew);

    if (!DSO_10bitLaneCal_SolveFrame(distorted, TEST_COUNT, &coeffs)) {
        fprintf(stderr, "solve frame returned false\n");
        return 1;
    }

    if (fabsf(coeffs.offset_lsb - expect_offset) > 5.0f) {
        fprintf(stderr, "offset mismatch: got=%.3f expect=%.3f\n",
                (double)coeffs.offset_lsb,
                (double)expect_offset);
        return 1;
    }
    if (fabsf(coeffs.gain - expect_gain) > 0.03f) {
        fprintf(stderr, "gain mismatch: got=%.5f expect=%.5f\n",
                (double)coeffs.gain,
                (double)expect_gain);
        return 1;
    }
    if (fabsf(coeffs.skew - expect_skew) > 0.08f) {
        fprintf(stderr, "skew mismatch: got=%.5f expect=%.5f\n",
                (double)coeffs.skew,
                (double)expect_skew);
        return 1;
    }

    return 0;
}

int main(void)
{
    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_10BIT);

    if (test_apply_coeffs_removes_odd_lane_error() != 0) {
        return 1;
    }
    if (test_solve_frame_recovers_known_lane_mismatch() != 0) {
        return 1;
    }

    printf("all 10bit lane cal tests passed\n");
    return 0;
}
