#include <stdio.h>

#include "dso/dso_acq_model.h"

static int test_shared_clock_targets_are_75mhz(void)
{
    if (DSO_ADC_CORE_TARGET_HZ != 75000000u ||
        DSO_ADC_INPUT_CLK_DEFAULT_HZ != 75000000u ||
        DSO_ADC_INPUT_CLK_REVV_HZ != 150000000u) {
        fprintf(stderr,
                "unexpected shared clock targets: core=%lu default=%lu revv=%lu\n",
                (unsigned long)DSO_ADC_CORE_TARGET_HZ,
                (unsigned long)DSO_ADC_INPUT_CLK_DEFAULT_HZ,
                (unsigned long)DSO_ADC_INPUT_CLK_REVV_HZ);
        return 1;
    }

    return 0;
}

static int test_revision_target_input_mapping(void)
{
    if (DSO_AcqModel_TargetInputClockHz(DSO_ACQ_REVISION_UNKNOWN) != 75000000u) {
        fprintf(stderr, "unknown revision target input clock mismatch\n");
        return 1;
    }
    if (DSO_AcqModel_TargetInputClockHz(DSO_ACQ_REVISION_Y) != 75000000u) {
        fprintf(stderr, "Rev.Y target input clock mismatch\n");
        return 1;
    }
    if (DSO_AcqModel_TargetInputClockHz(DSO_ACQ_REVISION_V) != 150000000u) {
        fprintf(stderr, "Rev.V target input clock mismatch\n");
        return 1;
    }

    return 0;
}

static int test_revy_runtime_build_keeps_75mhz_core(void)
{
    DSO_AcqRuntimeInfo rt = DSO_AcqModel_Build(DSO_ACQ_REVID_Y,
                                               DSO_DUAL_SWEEP_PROFILE_STABLE93,
                                               DSO_AcqModel_TargetInputClockHz(DSO_ACQ_REVISION_Y),
                                               200000000u,
                                               DSO_ADC_SAMPLE_CYCLES_X10,
                                               DSO_ADC_SAR_CYCLES_X10_10BIT,
                                               DSO_ADC_DELAY_CYCLES,
                                               false);

    if (rt.adc_input_clk_hz != 75000000u || rt.adc_core_divider != 1u || rt.adc_core_clk_hz != 75000000u) {
        fprintf(stderr,
                "Rev.Y runtime mismatch: in=%lu div=%lu core=%lu\n",
                (unsigned long)rt.adc_input_clk_hz,
                (unsigned long)rt.adc_core_divider,
                (unsigned long)rt.adc_core_clk_hz);
        return 1;
    }

    return 0;
}

static int test_revv_runtime_build_keeps_75mhz_core(void)
{
    DSO_AcqRuntimeInfo rt = DSO_AcqModel_Build(DSO_ACQ_REVID_V,
                                               DSO_DUAL_SWEEP_PROFILE_STABLE93,
                                               DSO_AcqModel_TargetInputClockHz(DSO_ACQ_REVISION_V),
                                               200000000u,
                                               DSO_ADC_SAMPLE_CYCLES_X10,
                                               DSO_ADC_SAR_CYCLES_X10_10BIT,
                                               DSO_ADC_DELAY_CYCLES,
                                               false);

    if (rt.adc_input_clk_hz != 150000000u || rt.adc_core_divider != 2u || rt.adc_core_clk_hz != 75000000u) {
        fprintf(stderr,
                "Rev.V runtime mismatch: in=%lu div=%lu core=%lu\n",
                (unsigned long)rt.adc_input_clk_hz,
                (unsigned long)rt.adc_core_divider,
                (unsigned long)rt.adc_core_clk_hz);
        return 1;
    }

    return 0;
}

static int test_single_adc_trigger_acceptance_rate_is_quantized(void)
{
    uint32_t trigger_div = 0u;
    uint32_t accepted_rate;

    accepted_rate = DSO_AcqModel_AcceptedTriggerRateHz(8333333u,
                                                       DSO_ACQ8_SINGLE_SAFE_SAMPLE_RATE_HZ,
                                                       &trigger_div);
    if ((accepted_rate != 2777777u) || (trigger_div != 3u)) {
        fprintf(stderr,
                "fast single ADC acceptance mismatch: rate=%lu div=%lu\n",
                (unsigned long)accepted_rate,
                (unsigned long)trigger_div);
        return 1;
    }

    accepted_rate = DSO_AcqModel_AcceptedTriggerRateHz(3333333u,
                                                       DSO_ACQ8_SINGLE_SAFE_SAMPLE_RATE_HZ,
                                                       &trigger_div);
    if ((accepted_rate != 3333333u) || (trigger_div != 1u)) {
        fprintf(stderr,
                "safe single ADC acceptance mismatch: rate=%lu div=%lu\n",
                (unsigned long)accepted_rate,
                (unsigned long)trigger_div);
        return 1;
    }

    return 0;
}

static int test_tri_adc_trigger_acceptance_uses_uniform_33msps_target(void)
{
    uint32_t trigger_div = 0u;
    uint32_t accepted_rate;
    uint32_t uniform_trigger_rate;

    if ((DSO_ACQ8_TRI_INTERLEAVED_TIM_PERIOD_CLKS != 18u) ||
        (DSO_ACQ8_TRI_INTERLEAVED_TIM_ARR != 17u) ||
        (DSO_ACQ8_TRI_INTERLEAVED_SAMPLE_RATE_HZ != 33333333u)) {
        fprintf(stderr,
                "tri ADC high-rate contract mismatch: period=%lu arr=%lu fs=%lu\n",
                (unsigned long)DSO_ACQ8_TRI_INTERLEAVED_TIM_PERIOD_CLKS,
                (unsigned long)DSO_ACQ8_TRI_INTERLEAVED_TIM_ARR,
                (unsigned long)DSO_ACQ8_TRI_INTERLEAVED_SAMPLE_RATE_HZ);
        return 1;
    }

    uniform_trigger_rate = DSO_TIM8_CLK_HZ / DSO_ACQ8_TRI_INTERLEAVED_TIM_PERIOD_CLKS;
    accepted_rate = DSO_AcqModel_AcceptedTriggerRateHz(uniform_trigger_rate,
                                                       DSO_ACQ8_TRI_INTERLEAVED_LANE_RATE_HZ,
                                                       &trigger_div);
    if ((accepted_rate != DSO_ACQ8_TRI_INTERLEAVED_LANE_RATE_HZ) || (trigger_div != 1u)) {
        fprintf(stderr,
                "tri ADC uniform trigger acceptance mismatch: rate=%lu div=%lu\n",
                (unsigned long)accepted_rate,
                (unsigned long)trigger_div);
        return 1;
    }

    accepted_rate = DSO_AcqModel_AcceptedTriggerRateHz(DSO_TIM8_CLK_HZ / 16u,
                                                       DSO_ACQ8_TRI_INTERLEAVED_LANE_RATE_HZ,
                                                       &trigger_div);
    if ((accepted_rate == (DSO_TIM8_CLK_HZ / 16u)) || (trigger_div != 2u)) {
        fprintf(stderr,
                "tri ADC zero-margin 37.5MSPS point was not rejected: rate=%lu div=%lu\n",
                (unsigned long)accepted_rate,
                (unsigned long)trigger_div);
        return 1;
    }

    return 0;
}

static int test_independent_lane_required_period(void)
{
    DSO_AcqRuntimeInfo rt;
    uint32_t required;

    required = DSO_AcqModel_RequiredPeriodClocks(200000000u,
                                                 75000000u,
                                                 DSO_ADC_SAMPLE_CYCLES_X10,
                                                 DSO_ADC_SAR_CYCLES_X10_10BIT,
                                                 DSO_ADC_DELAY_CYCLES);
    if (required != 22u) {
        fprintf(stderr,
                "10-bit independent lane period mismatch: required=%lu\n",
                (unsigned long)required);
        return 1;
    }

    rt = DSO_AcqModel_Build(DSO_ACQ_REVID_Y,
                            DSO_DUAL_SWEEP_PROFILE_STABLE93,
                            DSO_AcqModel_TargetInputClockHz(DSO_ACQ_REVISION_Y),
                            200000000u,
                            DSO_ADC_SAMPLE_CYCLES_X10,
                            DSO_ADC_SAR_CYCLES_X10_10BIT,
                            DSO_ADC_DELAY_CYCLES,
                            false);

    if (!rt.profile_valid) {
        fprintf(stderr, "10-bit stable profile should be valid\n");
        return 1;
    }
    if ((rt.lane_rate_hz != 4651162u) ||
        (rt.sample_rate_hz != 9302325u) ||
        (rt.trigger_rate_hz != 4651162u)) {
        fprintf(stderr,
                "10-bit stable rate mismatch: lane=%lu sample=%lu trigger=%lu\n",
                (unsigned long)rt.lane_rate_hz,
                (unsigned long)rt.sample_rate_hz,
                (unsigned long)rt.trigger_rate_hz);
        return 1;
    }

    return 0;
}

int main(void)
{
    if (test_shared_clock_targets_are_75mhz() != 0) {
        return 1;
    }
    if (test_revision_target_input_mapping() != 0) {
        return 1;
    }
    if (test_revy_runtime_build_keeps_75mhz_core() != 0) {
        return 1;
    }
    if (test_revv_runtime_build_keeps_75mhz_core() != 0) {
        return 1;
    }
    if (test_single_adc_trigger_acceptance_rate_is_quantized() != 0) {
        return 1;
    }
    if (test_tri_adc_trigger_acceptance_uses_uniform_33msps_target() != 0) {
        return 1;
    }
    if (test_independent_lane_required_period() != 0) {
        return 1;
    }

    puts("all acq model 75mhz tests passed");
    return 0;
}
