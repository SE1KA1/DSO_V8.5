#include <stdio.h>

#include "dso/dso_config.h"
#include "dso/dso_wavegen.h"

#ifndef DSO_WAVEGEN_PROFILE_BRINGUP_SQUARE_1K
#error "DSO_WAVEGEN_PROFILE_BRINGUP_SQUARE_1K must be defined"
#endif
#ifndef DSO_WAVEGEN_PROFILE_SQUARE_100K
#error "DSO_WAVEGEN_PROFILE_SQUARE_100K must be defined"
#endif
#ifndef DSO_WAVEGEN_PROFILE_SQUARE_250K
#error "DSO_WAVEGEN_PROFILE_SQUARE_250K must be defined"
#endif
#ifndef DSO_WAVEGEN_PROFILE_SQUARE_500K
#error "DSO_WAVEGEN_PROFILE_SQUARE_500K must be defined"
#endif
#ifndef DSO_WAVEGEN_PROFILE_SQUARE_1M
#error "DSO_WAVEGEN_PROFILE_SQUARE_1M must be defined"
#endif
#ifndef DSO_WAVEGEN_PROFILE_SQUARE_2M
#error "DSO_WAVEGEN_PROFILE_SQUARE_2M must be defined"
#endif
#ifndef DSO_WAVEGEN_PROFILE_SQUARE_3M
#error "DSO_WAVEGEN_PROFILE_SQUARE_3M must be defined"
#endif
#ifndef DSO_WAVEGEN_PROFILE_SQUARE_3500K
#error "DSO_WAVEGEN_PROFILE_SQUARE_3500K must be defined"
#endif

static int expect_profile(uint32_t profile_id,
                          DSO_WavegenWaveKind wave,
                          uint32_t wave_hz,
                          uint16_t lut_count,
                          uint32_t update_hz)
{
    DSO_WavegenProfile profile = {0};
    uint32_t calculated_update_hz = 0u;

    if (!DSO_Wavegen_GetProfile(profile_id, &profile)) {
        fprintf(stderr, "profile %lu missing\n", (unsigned long)profile_id);
        return 1;
    }
    if ((profile.wave != wave) ||
        (profile.wave_hz != wave_hz) ||
        (profile.lut_count != lut_count)) {
        fprintf(stderr,
                "bad profile %lu: wave=%d hz=%lu lut=%u\n",
                (unsigned long)profile_id,
                (int)profile.wave,
                (unsigned long)profile.wave_hz,
                profile.lut_count);
        return 1;
    }
    if (!DSO_Wavegen_CalcUpdateHz(&profile, &calculated_update_hz)) {
        fprintf(stderr, "update calc failed for profile %lu\n", (unsigned long)profile_id);
        return 1;
    }
    if (calculated_update_hz != update_hz) {
        fprintf(stderr,
                "bad update for profile %lu: got=%lu expected=%lu\n",
                (unsigned long)profile_id,
                (unsigned long)calculated_update_hz,
                (unsigned long)update_hz);
        return 1;
    }
    return 0;
}

int main(void)
{
    if (expect_profile(DSO_WAVEGEN_PROFILE_SINE_32_468K,
                       DSO_WAVEGEN_WAVE_KIND_SINE,
                       468000u,
                       32u,
                       14976000u) != 0) {
        return 1;
    }
    if (expect_profile(DSO_WAVEGEN_PROFILE_SINE_16_937K,
                       DSO_WAVEGEN_WAVE_KIND_SINE,
                       937000u,
                       16u,
                       14992000u) != 0) {
        return 1;
    }
    if (expect_profile(DSO_WAVEGEN_PROFILE_SINE_10_1500K,
                       DSO_WAVEGEN_WAVE_KIND_SINE,
                       1500000u,
                       10u,
                       15000000u) != 0) {
        return 1;
    }
    if (expect_profile(DSO_WAVEGEN_PROFILE_TRIANGLE_20_750K,
                       DSO_WAVEGEN_WAVE_KIND_TRIANGLE,
                       750000u,
                       20u,
                       15000000u) != 0) {
        return 1;
    }
    if (expect_profile(DSO_WAVEGEN_PROFILE_TRIANGLE_32_468K,
                       DSO_WAVEGEN_WAVE_KIND_TRIANGLE,
                       468000u,
                       32u,
                       14976000u) != 0) {
        return 1;
    }
    if (expect_profile(DSO_WAVEGEN_PROFILE_SQUARE_2_7500K,
                       DSO_WAVEGEN_WAVE_KIND_SQUARE,
                       7500000u,
                       2u,
                       15000000u) != 0) {
        return 1;
    }
    if (expect_profile(DSO_WAVEGEN_PROFILE_BRINGUP_SQUARE_1K,
                       DSO_WAVEGEN_WAVE_KIND_SQUARE,
                       1000u,
                       2u,
                       2000u) != 0) {
        return 1;
    }
    if (expect_profile(DSO_WAVEGEN_PROFILE_SQUARE_100K,
                       DSO_WAVEGEN_WAVE_KIND_SQUARE,
                       100000u,
                       2u,
                       200000u) != 0) {
        return 1;
    }
    if (expect_profile(DSO_WAVEGEN_PROFILE_SQUARE_250K,
                       DSO_WAVEGEN_WAVE_KIND_SQUARE,
                       250000u,
                       2u,
                       500000u) != 0) {
        return 1;
    }
    if (expect_profile(DSO_WAVEGEN_PROFILE_SQUARE_500K,
                       DSO_WAVEGEN_WAVE_KIND_SQUARE,
                       500000u,
                       2u,
                       1000000u) != 0) {
        return 1;
    }
    if (expect_profile(DSO_WAVEGEN_PROFILE_SQUARE_1M,
                       DSO_WAVEGEN_WAVE_KIND_SQUARE,
                       1000000u,
                       2u,
                       2000000u) != 0) {
        return 1;
    }
    if (expect_profile(DSO_WAVEGEN_PROFILE_SQUARE_2M,
                       DSO_WAVEGEN_WAVE_KIND_SQUARE,
                       2000000u,
                       2u,
                       4000000u) != 0) {
        return 1;
    }
    if (expect_profile(DSO_WAVEGEN_PROFILE_SQUARE_3M,
                       DSO_WAVEGEN_WAVE_KIND_SQUARE,
                       3000000u,
                       2u,
                       6000000u) != 0) {
        return 1;
    }
    if (expect_profile(DSO_WAVEGEN_PROFILE_SQUARE_3500K,
                       DSO_WAVEGEN_WAVE_KIND_SQUARE,
                       3500000u,
                       2u,
                       7000000u) != 0) {
        return 1;
    }

    printf("all wavegen profile tests passed\n");
    return 0;
}
