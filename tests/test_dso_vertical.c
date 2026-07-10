#include <stdint.h>
#include <stdio.h>

#include "dso/dso_sample_spec.h"
#include "dso/dso_vertical.h"

static int test_default_vertical_scale_is_500mv_per_div_for_both_channels(void)
{
    DSO_VerticalInfo ch1 = {0};
    DSO_VerticalInfo ch2 = {0};

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
    DSO_Vertical_Init();
    DSO_Vertical_GetInfoForChannel(DSO_CHANNEL_1, &ch1);
    DSO_Vertical_GetInfoForChannel(DSO_CHANNEL_2, &ch2);

    if ((ch1.vdiv_index != 5u) || (ch1.mv_per_div != 500u)) {
        fprintf(stderr,
                "CH1 default vdiv: expected index 5 / 500mV, got index %u / %lumV\n",
                (unsigned)ch1.vdiv_index,
                (unsigned long)ch1.mv_per_div);
        return 1;
    }

    if ((ch2.vdiv_index != 5u) || (ch2.mv_per_div != 500u)) {
        fprintf(stderr,
                "CH2 default vdiv: expected index 5 / 500mV, got index %u / %lumV\n",
                (unsigned)ch2.vdiv_index,
                (unsigned long)ch2.mv_per_div);
        return 1;
    }

    return 0;
}

static int test_default_vertical_scale_keeps_full_8bit_adc_range_inside_plot(void)
{
    int16_t y_min;
    int16_t y_mid;
    int16_t y_max;
    int16_t center;
    int16_t tolerance;

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
    DSO_Vertical_Init();
    DSO_Vertical_SetOffsetForChannel(DSO_CHANNEL_1, 0.0f);

    y_min = DSO_Vertical_MapSampleToPlotYForChannel(DSO_CHANNEL_1, 0u);
    y_mid = DSO_Vertical_MapSampleToPlotYForChannel(DSO_CHANNEL_1, 128u);
    y_max = DSO_Vertical_MapSampleToPlotYForChannel(DSO_CHANNEL_1, 255u);

    if ((y_min <= 0) || (y_max <= 0)) {
        fprintf(stderr,
                "default 500mV/div should not rail at top: y_min=%d y_max=%d\n",
                (int)y_min,
                (int)y_max);
        return 1;
    }

    if ((y_min >= (int16_t)(DSO_DISPLAY_HEIGHT - 1u)) ||
        (y_max >= (int16_t)(DSO_DISPLAY_HEIGHT - 1u))) {
        fprintf(stderr,
                "default 500mV/div should not rail at bottom: y_min=%d y_max=%d\n",
                (int)y_min,
                (int)y_max);
        return 1;
    }

    center = (int16_t)((DSO_DISPLAY_HEIGHT - 1u) / 2u);
    tolerance = (int16_t)((DSO_DISPLAY_HEIGHT + 15u) / 16u);
    if ((y_mid < (center - tolerance)) || (y_mid > (center + tolerance))) {
        fprintf(stderr,
                "midscale should remain near plot center, got y_mid=%d center=%d tol=%d\n",
                (int)y_mid,
                (int)center,
                (int)tolerance);
        return 1;
    }

    return 0;
}

int main(void)
{
    if (test_default_vertical_scale_is_500mv_per_div_for_both_channels() != 0) {
        return 1;
    }
    if (test_default_vertical_scale_keeps_full_8bit_adc_range_inside_plot() != 0) {
        return 1;
    }

    printf("all vertical tests passed\n");
    return 0;
}
