#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dso/dso_10bit_lane_cal.h"
#include "dso/dso_render_policy.h"
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

static int test_render_policy_prefers_ets_for_sparse_repetitive_fast_mode(void)
{
    DSO_RenderPolicyInput in = {
        .fast_mode = true,
        .sample_mode = true,
        .allow_ets = true,
        .freq_valid = true,
        .repetitive_confidence = 0.92f,
        .samples_per_cycle = 2.3f,
        .clipped = false,
        .ets_ready = false,
    };
    DSO_RenderPolicyDecision out = {0};

    DSO_RenderPolicy_Decide(&in, &out);

    if (!out.ets_building || out.recon_mode != DSO_RECON_MODE_RAW_LIMITED ||
        out.display_quality != DSO_DISPLAY_QUALITY_REPEAT_REQUIRED) {
        fprintf(stderr, "unexpected ETS-build decision: mode=%d quality=%d build=%d\n",
                (int)out.recon_mode,
                (int)out.display_quality,
                out.ets_building ? 1 : 0);
        return 1;
    }

    in.ets_ready = true;
    DSO_RenderPolicy_Decide(&in, &out);
    if (out.recon_mode != DSO_RECON_MODE_ETS || out.display_quality != DSO_DISPLAY_QUALITY_GOOD) {
        fprintf(stderr, "unexpected ETS-ready decision: mode=%d quality=%d\n",
                (int)out.recon_mode,
                (int)out.display_quality);
        return 1;
    }

    return 0;
}

static int test_render_policy_marks_mid_density_as_limited_real_time(void)
{
    DSO_RenderPolicyInput in = {
        .fast_mode = true,
        .sample_mode = true,
        .allow_ets = true,
        .freq_valid = true,
        .repetitive_confidence = 0.60f,
        .samples_per_cycle = 3.7f,
        .clipped = false,
        .ets_ready = false,
    };
    DSO_RenderPolicyDecision out = {0};

    DSO_RenderPolicy_Decide(&in, &out);

    if (out.recon_mode != DSO_RECON_MODE_RAW_LIMITED ||
        out.display_quality != DSO_DISPLAY_QUALITY_LIMITED ||
        out.ets_building) {
        fprintf(stderr, "unexpected limited-RT decision: mode=%d quality=%d build=%d\n",
                (int)out.recon_mode,
                (int)out.display_quality,
                out.ets_building ? 1 : 0);
        return 1;
    }

    return 0;
}

static int test_render_policy_starts_ets_build_for_borderline_sparse_fast_mode(void)
{
    DSO_RenderPolicyInput in = {
        .fast_mode = true,
        .sample_mode = true,
        .allow_ets = true,
        .freq_valid = true,
        .repetitive_confidence = 0.90f,
        .samples_per_cycle = 4.2f,
        .clipped = false,
        .ets_ready = false,
    };
    DSO_RenderPolicyDecision out = {0};

    DSO_RenderPolicy_Decide(&in, &out);

    if (!out.ets_building ||
        out.recon_mode != DSO_RECON_MODE_RAW_LIMITED ||
        out.display_quality != DSO_DISPLAY_QUALITY_REPEAT_REQUIRED) {
        fprintf(stderr, "unexpected borderline ETS-build decision: mode=%d quality=%d build=%d\n",
                (int)out.recon_mode,
                (int)out.display_quality,
                out.ets_building ? 1 : 0);
        return 1;
    }

    return 0;
}

int main(void)
{
    int rc = 0;

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_10BIT);
    rc |= test_apply_coeffs_removes_odd_lane_error();
    rc |= test_solve_frame_recovers_known_lane_mismatch();
    rc |= test_render_policy_prefers_ets_for_sparse_repetitive_fast_mode();
    rc |= test_render_policy_starts_ets_build_for_borderline_sparse_fast_mode();
    rc |= test_render_policy_marks_mid_density_as_limited_real_time();

    if (rc == 0) {
        printf("all hf pipeline tests passed\n");
    }

    return rc;
}
