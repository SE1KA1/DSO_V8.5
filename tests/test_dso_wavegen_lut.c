#include <stdint.h>
#include <stdio.h>

#include "dso/dso_wavegen_lut.h"

static int test_square_2_point_50_percent(void)
{
    uint16_t lut[2] = {0};
    uint16_t count = 0u;
    DSO_WavegenLutConfig cfg = {
        .wave = DSO_WAVEGEN_WAVE_KIND_SQUARE,
        .lut_count = 2u,
        .offset = 2048u,
        .amplitude = 1200u,
        .duty_permille = 500u,
    };

    if (DSO_WavegenLut_Build(&cfg, lut, 2u, &count) != DSO_WAVEGEN_OK) {
        fprintf(stderr, "square build failed\n");
        return 1;
    }
    if ((count != 2u) || (lut[0] != 3248u) || (lut[1] != 848u)) {
        fprintf(stderr, "bad square lut: count=%u %u %u\n", count, lut[0], lut[1]);
        return 1;
    }
    return 0;
}

static int test_triangle_20_point_shape(void)
{
    uint16_t lut[20] = {0};
    uint16_t count = 0u;
    DSO_WavegenLutConfig cfg = {
        .wave = DSO_WAVEGEN_WAVE_KIND_TRIANGLE,
        .lut_count = 20u,
        .offset = 2048u,
        .amplitude = 1200u,
        .duty_permille = 500u,
    };

    if (DSO_WavegenLut_Build(&cfg, lut, 20u, &count) != DSO_WAVEGEN_OK) {
        fprintf(stderr, "triangle build failed\n");
        return 1;
    }
    if ((count != 20u) || (lut[0] != 848u) || (lut[9] != 3248u) ||
        (lut[10] != 3248u) || (lut[19] != 848u)) {
        fprintf(stderr,
                "bad triangle key points: count=%u p0=%u p9=%u p10=%u p19=%u\n",
                count,
                lut[0],
                lut[9],
                lut[10],
                lut[19]);
        return 1;
    }
    return 0;
}

static int test_sine_32_point_range(void)
{
    uint16_t lut[32] = {0};
    uint16_t count = 0u;
    uint16_t min_v = 4095u;
    uint16_t max_v = 0u;
    uint16_t i;
    DSO_WavegenLutConfig cfg = {
        .wave = DSO_WAVEGEN_WAVE_KIND_SINE,
        .lut_count = 32u,
        .offset = 2048u,
        .amplitude = 1200u,
        .duty_permille = 500u,
    };

    if (DSO_WavegenLut_Build(&cfg, lut, 32u, &count) != DSO_WAVEGEN_OK) {
        fprintf(stderr, "sine build failed\n");
        return 1;
    }
    for (i = 0u; i < count; ++i) {
        if (lut[i] < min_v) {
            min_v = lut[i];
        }
        if (lut[i] > max_v) {
            max_v = lut[i];
        }
    }
    if ((count != 32u) || (lut[0] != 2048u) || (min_v < 848u) || (max_v > 3248u)) {
        fprintf(stderr,
                "bad sine range: count=%u first=%u min=%u max=%u\n",
                count,
                lut[0],
                min_v,
                max_v);
        return 1;
    }
    return 0;
}

int main(void)
{
    if (test_square_2_point_50_percent() != 0) {
        return 1;
    }
    if (test_triangle_20_point_shape() != 0) {
        return 1;
    }
    if (test_sine_32_point_range() != 0) {
        return 1;
    }
    printf("all wavegen lut tests passed\n");
    return 0;
}
