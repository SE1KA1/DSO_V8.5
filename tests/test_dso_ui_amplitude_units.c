#include <math.h>
#include <stdio.h>

#include "ui/ui_measure_units.h"

static int expect_near(const char *name, float actual, float expected)
{
    float diff = fabsf(actual - expected);

    if (diff > 0.0005f) {
        fprintf(stderr, "%s: expected %.6f, got %.6f\n",
                name,
                (double)expected,
                (double)actual);
        return 1;
    }
    return 0;
}

static int test_8bit_delta_uses_255_full_scale(void)
{
    DSO_UI_Frame frame = {0};

    frame.adc_bits = 8u;
    frame.adc_lsb_v = 3.3f / 255.0f;

    return expect_near("8-bit 128-code delta",
                       UI_Measure_CodeDeltaToVolts(128.0f, &frame),
                       128.0f * 3.3f / 255.0f);
}

static int test_10bit_delta_uses_1023_full_scale(void)
{
    DSO_UI_Frame frame = {0};

    frame.adc_bits = 10u;
    frame.adc_lsb_v = 3.3f / 1023.0f;

    return expect_near("10-bit 512-code delta",
                       UI_Measure_CodeDeltaToVolts(512.0f, &frame),
                       512.0f * 3.3f / 1023.0f);
}

static int test_missing_frame_metadata_is_explicit_zero(void)
{
    DSO_UI_Frame frame = {0};

    return expect_near("missing metadata",
                       UI_Measure_CodeDeltaToVolts(100.0f, &frame),
                       0.0f);
}

int main(void)
{
    int rc = 0;

    rc |= test_8bit_delta_uses_255_full_scale();
    rc |= test_10bit_delta_uses_1023_full_scale();
    rc |= test_missing_frame_metadata_is_explicit_zero();

    if (rc == 0) {
        puts("all UI amplitude unit tests passed");
    }
    return rc;
}
