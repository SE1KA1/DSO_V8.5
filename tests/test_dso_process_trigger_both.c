#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "dso/dso_process.h"
#include "dso/dso_sample_spec.h"
#include "dso_acq_timebase_stubs.h"

static void failf(const char *label, const char *detail, float actual, float expected)
{
    fprintf(stderr, "%s: %s actual=%.3f expected=%.3f\n",
            label,
            detail,
            (double)actual,
            (double)expected);
    exit(1);
}

static void expect_true(const char *label, int condition)
{
    if (!condition) {
        fprintf(stderr, "%s failed\n", label);
        exit(1);
    }
}

static void expect_false(const char *label, int condition)
{
    if (condition) {
        fprintf(stderr, "%s unexpectedly true\n", label);
        exit(1);
    }
}

static void expect_int(const char *label, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "%s: actual=%d expected=%d\n", label, actual, expected);
        exit(1);
    }
}

static void expect_float_close(const char *label, float actual, float expected)
{
    if (fabsf(actual - expected) > 0.001f) {
        failf(label, "float mismatch", actual, expected);
    }
}

static void analyze(const uint16_t *samples,
                    uint32_t count,
                    DSO_TriggerSlope slope,
                    DSO_ProcessAnalysis *out)
{
    DSO_TriggerConfig trigger = {
        .level_volts = 1.3f,
        .level_enabled = true,
        .slope = slope,
    };

    DSO_Process_AnalyzeSamplesEx(samples, count, 1000000u, &trigger, out);
}

static void test_rising_only_uses_rising_for_both(void)
{
    static const uint16_t samples[] = {
        40u, 35u, 30u, 25u, 20u, 15u, 10u, 20u, 30u, 40u,
        60u, 80u, 100u, 130u, 160u, 180u, 190u, 200u, 210u
    };
    DSO_ProcessAnalysis rising = {0};
    DSO_ProcessAnalysis falling = {0};
    DSO_ProcessAnalysis both = {0};

    analyze(samples, (uint32_t)(sizeof(samples) / sizeof(samples[0])), DSO_TRIGGER_SLOPE_RISING, &rising);
    analyze(samples, (uint32_t)(sizeof(samples) / sizeof(samples[0])), DSO_TRIGGER_SLOPE_FALLING, &falling);
    analyze(samples, (uint32_t)(sizeof(samples) / sizeof(samples[0])), DSO_TRIGGER_SLOPE_BOTH, &both);

    expect_true("rising-only rising triggered", rising.triggered);
    expect_false("rising-only falling triggered", falling.triggered);
    expect_true("rising-only both triggered", both.triggered);
    expect_int("rising-only both index", both.trigger_index, rising.trigger_index);
    expect_float_close("rising-only both pos", both.trigger_pos_samples, rising.trigger_pos_samples);
}

static void test_falling_only_uses_falling_for_both(void)
{
    static const uint16_t samples[] = {
        220u, 210u, 200u, 190u, 180u, 170u, 160u, 150u, 140u,
        130u, 120u, 110u, 100u, 80u, 60u, 40u, 20u, 10u
    };
    DSO_ProcessAnalysis rising = {0};
    DSO_ProcessAnalysis falling = {0};
    DSO_ProcessAnalysis both = {0};

    analyze(samples, (uint32_t)(sizeof(samples) / sizeof(samples[0])), DSO_TRIGGER_SLOPE_RISING, &rising);
    analyze(samples, (uint32_t)(sizeof(samples) / sizeof(samples[0])), DSO_TRIGGER_SLOPE_FALLING, &falling);
    analyze(samples, (uint32_t)(sizeof(samples) / sizeof(samples[0])), DSO_TRIGGER_SLOPE_BOTH, &both);

    expect_false("falling-only rising triggered", rising.triggered);
    expect_true("falling-only falling triggered", falling.triggered);
    expect_true("falling-only both triggered", both.triggered);
    expect_int("falling-only both index", both.trigger_index, falling.trigger_index);
    expect_float_close("falling-only both pos", both.trigger_pos_samples, falling.trigger_pos_samples);
}

static void test_both_picks_earliest_valid_edge(void)
{
    static const uint16_t samples[] = {
        220u, 210u, 200u, 190u, 180u, 170u, 160u, 150u, 140u,
        130u, 120u, 110u, 100u, 80u, 60u, 40u, 20u, 10u,
        20u, 30u, 40u, 60u, 80u, 100u, 130u, 160u
    };
    DSO_ProcessAnalysis rising = {0};
    DSO_ProcessAnalysis falling = {0};
    DSO_ProcessAnalysis both = {0};
    float earliest_pos;
    int earliest_idx;

    analyze(samples, (uint32_t)(sizeof(samples) / sizeof(samples[0])), DSO_TRIGGER_SLOPE_RISING, &rising);
    analyze(samples, (uint32_t)(sizeof(samples) / sizeof(samples[0])), DSO_TRIGGER_SLOPE_FALLING, &falling);
    analyze(samples, (uint32_t)(sizeof(samples) / sizeof(samples[0])), DSO_TRIGGER_SLOPE_BOTH, &both);

    expect_true("both-case rising triggered", rising.triggered);
    expect_true("both-case falling triggered", falling.triggered);
    expect_true("both-case both triggered", both.triggered);

    earliest_pos = (rising.trigger_pos_samples < falling.trigger_pos_samples)
                 ? rising.trigger_pos_samples
                 : falling.trigger_pos_samples;
    earliest_idx = (rising.trigger_pos_samples < falling.trigger_pos_samples)
                 ? rising.trigger_index
                 : falling.trigger_index;

    expect_int("both-case earliest index", both.trigger_index, earliest_idx);
    expect_float_close("both-case earliest pos", both.trigger_pos_samples, earliest_pos);
}

static void test_both_reports_no_hit_when_no_edge_hits(void)
{
    static const uint16_t samples[] = {
        100u, 100u, 100u, 100u, 100u, 100u, 100u, 100u,
        100u, 100u, 100u, 100u, 100u, 100u, 100u, 100u
    };
    DSO_ProcessAnalysis both = {0};

    analyze(samples, (uint32_t)(sizeof(samples) / sizeof(samples[0])), DSO_TRIGGER_SLOPE_BOTH, &both);

    expect_false("no-hit both triggered", both.triggered);
    expect_int("no-hit both index", both.trigger_index, -1);
    expect_float_close("no-hit both pos", both.trigger_pos_samples, -1.0f);
}

int main(void)
{
    Test_AcqStub_Reset();
    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);

    test_rising_only_uses_rising_for_both();
    test_falling_only_uses_falling_for_both();
    test_both_picks_earliest_valid_edge();
    test_both_reports_no_hit_when_no_edge_hits();
    return 0;
}
