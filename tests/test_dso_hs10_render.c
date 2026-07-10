#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dso/dso_hs10_render.h"

#define TEST_SAMPLE_COUNT 2048u
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

static int test_hs10_render_build_gate_accepts_mid_density_repetitive_input(void)
{
    if (!DSO_HS10Render_ShouldBuild(true, true, true, false, 0.82f, 5.5f)) {
        fprintf(stderr, "hs10 build gate rejected valid repetitive mid-density input\n");
        return 1;
    }

    return 0;
}

static int test_hs10_render_accumulates_and_renders(void)
{
    DSO_HS10RenderState state;
    uint16_t samples[TEST_SAMPLE_COUNT];
    uint16_t display[128];

    memset(&state, 0, sizeof(state));
    memset(display, 0, sizeof(display));
    build_repetitive_sine(samples, TEST_SAMPLE_COUNT, 512.0f, 220.0f, 73.0f);

    DSO_HS10Render_Accumulate(&state,
                              samples,
                              TEST_SAMPLE_COUNT,
                              2500000u,
                              73000.0f,
                              (float)(TEST_SAMPLE_COUNT / 2u));

    if (!DSO_HS10Render_IsReady(&state)) {
        fprintf(stderr, "hs10 render did not become ready\n");
        return 1;
    }

    if (!DSO_HS10Render_Render(&state, 500000.0f, 1023u, 512u, display, 128u)) {
        fprintf(stderr, "hs10 render did not produce display samples\n");
        return 1;
    }

    return 0;
}

int main(void)
{
    if (test_hs10_render_build_gate_accepts_mid_density_repetitive_input() != 0) {
        return 1;
    }
    if (test_hs10_render_accumulates_and_renders() != 0) {
        return 1;
    }

    printf("all hs10 render tests passed\n");
    return 0;
}
