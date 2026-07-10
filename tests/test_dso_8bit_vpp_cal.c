#include <math.h>
#include <stdio.h>

#include "dso/dso_8bit_vpp_cal.h"

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

static int test_invalid_frequency_keeps_legacy_scale(void)
{
    return expect_near("invalid frequency",
                       DSO_8bitVppCal_ScaleForFreq(0.0f, 0.600f),
                       0.600f);
}

static int test_low_frequency_uses_unity_scale(void)
{
    int rc = 0;

    rc |= expect_near("500 kHz",
                      DSO_8bitVppCal_ScaleForFreq(500000.0f, 0.600f),
                      1.000f);
    rc |= expect_near("1 MHz",
                      DSO_8bitVppCal_ScaleForFreq(1000000.0f, 0.600f),
                      1.000f);
    return rc;
}

static int test_mid_band_interpolates_toward_high_frequency_scale(void)
{
    int rc = 0;

    rc |= expect_near("5 MHz fast",
                      DSO_8bitVppCal_ScaleForFreq(5000000.0f, 0.600f),
                      0.920f);
    rc |= expect_near("6.5 MHz fast",
                      DSO_8bitVppCal_ScaleForFreq(6500000.0f, 0.600f),
                      0.850f);
    rc |= expect_near("8 MHz fast",
                      DSO_8bitVppCal_ScaleForFreq(8000000.0f, 0.600f),
                      0.780f);
    rc |= expect_near("8.5 MHz fast",
                      DSO_8bitVppCal_ScaleForFreq(8500000.0f, 0.600f),
                      0.690f);
    return rc;
}

static int test_high_frequency_reaches_existing_scale_from_9mhz(void)
{
    int rc = 0;

    rc |= expect_near("9 MHz fast",
                      DSO_8bitVppCal_ScaleForFreq(9000000.0f, 0.600f),
                      0.600f);
    rc |= expect_near("10 MHz fast",
                      DSO_8bitVppCal_ScaleForFreq(10000000.0f, 0.600f),
                      0.600f);
    rc |= expect_near("12 MHz fast",
                      DSO_8bitVppCal_ScaleForFreq(12000000.0f, 0.600f),
                      0.600f);
    rc |= expect_near("12 MHz normal",
                      DSO_8bitVppCal_ScaleForFreq(12000000.0f, 0.560f),
                      0.560f);
    return rc;
}

int main(void)
{
    int rc = 0;

    rc |= test_invalid_frequency_keeps_legacy_scale();
    rc |= test_low_frequency_uses_unity_scale();
    rc |= test_mid_band_interpolates_toward_high_frequency_scale();
    rc |= test_high_frequency_reaches_existing_scale_from_9mhz();

    if (rc == 0) {
        puts("all 8-bit Vpp calibration tests passed");
    }
    return rc;
}
