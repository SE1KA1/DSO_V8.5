#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "dso/dso_sample_spec.h"

static int test_10bit_spec_matches_existing_pipeline_assumptions(void)
{
    const DSO_SampleSpec *spec = DSO_SampleSpec_Get(DSO_SAMPLE_FORMAT_10BIT);

    if (spec == NULL) {
        fprintf(stderr, "10-bit spec was NULL\n");
        return 1;
    }
    if (spec->adc_bits != 10u || spec->adc_max != 1023u || spec->midscale != 512u) {
        fprintf(stderr,
                "unexpected 10-bit spec: bits=%u max=%u mid=%u\n",
                (unsigned)spec->adc_bits,
                (unsigned)spec->adc_max,
                (unsigned)spec->midscale);
        return 1;
    }
    if (spec->lane_count != 2u || spec->ch2_mode != DSO_CH2_MODE_REALTIME) {
        fprintf(stderr,
                "unexpected 10-bit acquisition flags: lanes=%u ch2_mode=%u\n",
                (unsigned)spec->lane_count,
                (unsigned)spec->ch2_mode);
        return 1;
    }

    return 0;
}

static int test_8bit_spec_matches_tri_interleaved_product_path(void)
{
    const DSO_SampleSpec *spec = DSO_SampleSpec_Get(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);

    if (spec == NULL) {
        fprintf(stderr, "8-bit spec was NULL\n");
        return 1;
    }
    if (spec->adc_bits != 8u || spec->adc_max != 255u || spec->midscale != 128u) {
        fprintf(stderr,
                "unexpected 8-bit spec: bits=%u max=%u mid=%u\n",
                (unsigned)spec->adc_bits,
                (unsigned)spec->adc_max,
                (unsigned)spec->midscale);
        return 1;
    }
    if ((spec->lane_count != 3u) ||
        (spec->ch2_mode != DSO_CH2_MODE_DISABLED) ||
        !spec->interleaved) {
        fprintf(stderr,
                "unexpected 8-bit acquisition flags: lanes=%u ch2_mode=%u interleaved=%u\n",
                (unsigned)spec->lane_count,
                (unsigned)spec->ch2_mode,
                spec->interleaved ? 1u : 0u);
        return 1;
    }

    return 0;
}

static int test_default_sample_format_is_8bit_high_speed(void)
{
    if (DSO_SAMPLE_FORMAT_DEFAULT != DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED) {
        fprintf(stderr,
                "unexpected default format enum=%u\n",
                (unsigned)DSO_SAMPLE_FORMAT_DEFAULT);
        return 1;
    }
    if (DSO_SampleSpec_GetActiveFormat() != DSO_SAMPLE_FORMAT_DEFAULT) {
        fprintf(stderr,
                "active format %u did not match default %u\n",
                (unsigned)DSO_SampleSpec_GetActiveFormat(),
                (unsigned)DSO_SAMPLE_FORMAT_DEFAULT);
        return 1;
    }

    return 0;
}

static int test_both_sample_formats_require_immediate_start(void)
{
    const DSO_SampleSpec *spec10 = DSO_SampleSpec_Get(DSO_SAMPLE_FORMAT_10BIT);
    const DSO_SampleSpec *spec8 = DSO_SampleSpec_Get(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);

    if ((spec10 == NULL) || (spec8 == NULL)) {
        fprintf(stderr, "sample spec lookup failed\n");
        return 1;
    }
    if (!spec10->start_immediately) {
        fprintf(stderr, "10-bit format should start immediately after init\n");
        return 1;
    }
    if (!spec8->start_immediately) {
        fprintf(stderr, "8-bit format should start immediately after init\n");
        return 1;
    }

    return 0;
}

static int test_clamp_uses_runtime_adc_max(void)
{
    const DSO_SampleSpec *spec10 = DSO_SampleSpec_Get(DSO_SAMPLE_FORMAT_10BIT);
    const DSO_SampleSpec *spec8 = DSO_SampleSpec_Get(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);

    if (DSO_SampleSpec_ClampFromI32(spec10, -1) != 0u) {
        fprintf(stderr, "10-bit clamp failed for negative input\n");
        return 1;
    }
    if (DSO_SampleSpec_ClampFromI32(spec10, 9000) != 1023u) {
        fprintf(stderr, "10-bit clamp failed for upper saturation\n");
        return 1;
    }
    if (DSO_SampleSpec_ClampFromI32(spec8, -9) != 0u) {
        fprintf(stderr, "8-bit clamp failed for negative input\n");
        return 1;
    }
    if (DSO_SampleSpec_ClampFromI32(spec8, 999) != 255u) {
        fprintf(stderr, "8-bit clamp failed for upper saturation\n");
        return 1;
    }

    return 0;
}

int main(void)
{
    if (test_10bit_spec_matches_existing_pipeline_assumptions() != 0) {
        return 1;
    }
    if (test_8bit_spec_matches_tri_interleaved_product_path() != 0) {
        return 1;
    }
    if (test_default_sample_format_is_8bit_high_speed() != 0) {
        return 1;
    }
    if (test_both_sample_formats_require_immediate_start() != 0) {
        return 1;
    }
    if (test_clamp_uses_runtime_adc_max() != 0) {
        return 1;
    }

    printf("all sample spec tests passed\n");
    return 0;
}
