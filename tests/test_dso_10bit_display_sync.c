#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "dso/dso_10bit_display_sync.h"

static int test_maps_ch1_trigger_to_ch2_rate_ratio_with_fraction(void)
{
    DSO_ProcessAnalysis ch1 = {0};
    DSO_ProcessAnalysis ch2 = {0};

    ch1.triggered = true;
    ch1.trigger_index = 101;
    ch1.trigger_pos_samples = 101.75f;

    if (!DSO_10bitDisplaySync_AlignCh2TriggerToCh1(&ch1,
                                                   10000000u,
                                                   5000000u,
                                                   512u,
                                                   &ch2)) {
        fprintf(stderr, "align helper returned false\n");
        return 1;
    }

    if (!ch2.triggered) {
        fprintf(stderr, "ch2 trigger flag was not asserted\n");
        return 1;
    }
    if (ch2.trigger_index != 50) {
        fprintf(stderr, "unexpected ch2 trigger index: %d\n", (int)ch2.trigger_index);
        return 1;
    }
    if (fabsf(ch2.trigger_pos_samples - 50.875f) > 0.001f) {
        fprintf(stderr, "unexpected ch2 trigger position: %.6f\n", ch2.trigger_pos_samples);
        return 1;
    }

    return 0;
}

static int test_clamps_ch2_trigger_to_last_sample(void)
{
    DSO_ProcessAnalysis ch1 = {0};
    DSO_ProcessAnalysis ch2 = {0};

    ch1.triggered = true;
    ch1.trigger_index = 4095;
    ch1.trigger_pos_samples = 4095.50f;

    if (!DSO_10bitDisplaySync_AlignCh2TriggerToCh1(&ch1,
                                                   10000000u,
                                                   5000000u,
                                                   128u,
                                                   &ch2)) {
        fprintf(stderr, "align helper returned false for clamp case\n");
        return 1;
    }

    if (ch2.trigger_index != 127) {
        fprintf(stderr, "ch2 trigger index was not clamped: %d\n", (int)ch2.trigger_index);
        return 1;
    }
    if (fabsf(ch2.trigger_pos_samples - 127.0f) > 0.001f) {
        fprintf(stderr, "ch2 trigger position was not clamped: %.6f\n", ch2.trigger_pos_samples);
        return 1;
    }

    return 0;
}

static int test_hs10_is_allowed_only_for_ch1(void)
{
    if (!DSO_10bitDisplaySync_AllowHs10ForChannel(0u)) {
        fprintf(stderr, "HS10 must stay enabled for CH1\n");
        return 1;
    }
    if (DSO_10bitDisplaySync_AllowHs10ForChannel(1u)) {
        fprintf(stderr, "HS10 must stay disabled for CH2\n");
        return 1;
    }

    return 0;
}

int main(void)
{
    if (test_maps_ch1_trigger_to_ch2_rate_ratio_with_fraction() != 0) {
        return 1;
    }
    if (test_clamps_ch2_trigger_to_last_sample() != 0) {
        return 1;
    }
    if (test_hs10_is_allowed_only_for_ch1() != 0) {
        return 1;
    }

    puts("all 10bit display sync tests passed");
    return 0;
}
