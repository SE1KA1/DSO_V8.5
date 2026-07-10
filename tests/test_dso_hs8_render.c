#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_SAMPLE_COUNT 512u
#define TEST_DISPLAY_WIDTH 128u
#define PI_F 3.14159265358979323846f

typedef struct {
    float locked_freq_hz;
    bool ready;
    float bin_sum[128];
    uint16_t bin_count[128];
} DSO_HS8RenderState;

void DSO_HS8Render_Reset(DSO_HS8RenderState *state);
bool DSO_HS8Render_ShouldBuild(bool fast_mode,
                               bool sample_mode,
                               bool freq_valid,
                               bool clipped,
                               float repetitive_confidence,
                               float samples_per_cycle);
void DSO_HS8Render_Accumulate(DSO_HS8RenderState *state,
                              const uint16_t *samples,
                              uint32_t sample_count,
                              uint32_t sample_rate_hz,
                              float freq_hz,
                              float trig_pos_samples);
bool DSO_HS8Render_IsReady(const DSO_HS8RenderState *state);
bool DSO_HS8Render_Render(const DSO_HS8RenderState *state,
                          float sec_div_ns,
                          uint16_t adc_max,
                          uint16_t midscale,
                          uint16_t *out_display,
                          uint32_t display_width);
float DSO_HS8Render_EffectiveRateHz(const DSO_HS8RenderState *state);

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

static void build_sine(uint16_t *dst,
                       uint32_t count,
                       float dc,
                       float amp,
                       float freq_hz,
                       float sample_rate_hz,
                       float phase_offset)
{
    for (uint32_t i = 0u; i < count; ++i) {
        float phase = ((2.0f * PI_F * freq_hz * (float)i) / sample_rate_hz) + phase_offset;
        dst[i] = clamp_u8(dc + amp * sinf(phase));
    }
}

static int display_span(const uint16_t *samples, uint32_t count)
{
    uint16_t vmin = 255u;
    uint16_t vmax = 0u;

    for (uint32_t i = 0u; i < count; ++i) {
        if (samples[i] < vmin) {
            vmin = samples[i];
        }
        if (samples[i] > vmax) {
            vmax = samples[i];
        }
    }

    return (int)vmax - (int)vmin;
}

static int test_should_build_for_sparse_repetitive_fast_signal(void)
{
    if (!DSO_HS8Render_ShouldBuild(true, true, true, false, 0.93f, 3.2f)) {
        fprintf(stderr, "expected build decision for sparse repetitive signal\n");
        return 1;
    }

    if (DSO_HS8Render_ShouldBuild(true, true, true, true, 0.93f, 3.2f)) {
        fprintf(stderr, "build decision ignored clipped guard\n");
        return 1;
    }

    if (DSO_HS8Render_ShouldBuild(false, true, true, false, 0.93f, 3.2f)) {
        fprintf(stderr, "build decision ignored fast-mode guard\n");
        return 1;
    }

    return 0;
}

static int test_accumulate_reaches_ready_and_renders_nonflat_output(void)
{
    DSO_HS8RenderState state;
    uint16_t samples[TEST_SAMPLE_COUNT];
    uint16_t display[TEST_DISPLAY_WIDTH];
    const float sample_rate_hz = 25000000.0f;
    const float freq_hz = 6250000.0f;

    DSO_HS8Render_Reset(&state);
    memset(display, 0, sizeof(display));

    for (uint32_t frame = 0u; frame < 20u; ++frame) {
        float trig_pos = 33.2f + ((float)frame * 0.37f);
        float phase_offset = ((float)frame * 0.11f);

        build_sine(samples,
                   TEST_SAMPLE_COUNT,
                   128.0f,
                   70.0f,
                   freq_hz,
                   sample_rate_hz,
                   phase_offset);
        DSO_HS8Render_Accumulate(&state,
                                 samples,
                                 TEST_SAMPLE_COUNT,
                                 (uint32_t)sample_rate_hz,
                                 freq_hz,
                                 trig_pos);
    }

    if (!DSO_HS8Render_IsReady(&state)) {
        fprintf(stderr, "render state never reached ready\n");
        return 1;
    }

    if (!DSO_HS8Render_Render(&state, 100.0f, 255u, 128u, display, TEST_DISPLAY_WIDTH)) {
        fprintf(stderr, "render returned false after ready\n");
        return 1;
    }

    if (display_span(display, TEST_DISPLAY_WIDTH) < 40) {
        fprintf(stderr, "rendered waveform span too small: %d\n", display_span(display, TEST_DISPLAY_WIDTH));
        return 1;
    }

    if (DSO_HS8Render_EffectiveRateHz(&state) < (freq_hz * 64.0f)) {
        fprintf(stderr, "effective rate did not scale with reconstructed bins: %.2f\n",
                (double)DSO_HS8Render_EffectiveRateHz(&state));
        return 1;
    }

    return 0;
}

static int test_frequency_jump_clears_ready_state(void)
{
    DSO_HS8RenderState state;
    uint16_t samples[TEST_SAMPLE_COUNT];

    DSO_HS8Render_Reset(&state);

    for (uint32_t frame = 0u; frame < 18u; ++frame) {
        build_sine(samples,
                   TEST_SAMPLE_COUNT,
                   128.0f,
                   68.0f,
                   6250000.0f,
                   25000000.0f,
                   (float)frame * 0.08f);
        DSO_HS8Render_Accumulate(&state,
                                 samples,
                                 TEST_SAMPLE_COUNT,
                                 25000000u,
                                 6250000.0f,
                                 21.4f + ((float)frame * 0.21f));
    }

    if (!DSO_HS8Render_IsReady(&state)) {
        fprintf(stderr, "state did not become ready before frequency jump\n");
        return 1;
    }

    build_sine(samples,
               TEST_SAMPLE_COUNT,
               128.0f,
               68.0f,
               4200000.0f,
               25000000.0f,
               0.0f);
    DSO_HS8Render_Accumulate(&state,
                             samples,
                             TEST_SAMPLE_COUNT,
                             25000000u,
                             4200000.0f,
                             19.8f);

    if (DSO_HS8Render_IsReady(&state)) {
        fprintf(stderr, "state stayed ready after frequency jump\n");
        return 1;
    }

    return 0;
}

int main(void)
{
    int rc = 0;

    rc |= test_should_build_for_sparse_repetitive_fast_signal();
    rc |= test_accumulate_reaches_ready_and_renders_nonflat_output();
    rc |= test_frequency_jump_clears_ready_state();

    if (rc == 0) {
        puts("all hs8 render tests passed");
    }

    return rc;
}
