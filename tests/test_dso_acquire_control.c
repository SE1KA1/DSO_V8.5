#include <stdio.h>

#include "dso/dso_acquire_control.h"
#include "dso/dso_sample_spec.h"

static int test_10bit_keeps_average_mode_enabled(void)
{
    DSO_AcquireCtrlInfo info;

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_10BIT);
    DSO_AcquireCtrl_Init();
    DSO_AcquireCtrl_SetAcquireMode(DSO_ACQUIRE_MODE_AVERAGE);
    (void)DSO_AcquireCtrl_ApplyPending();
    DSO_AcquireCtrl_GetInfo(&info);

    if (info.acquire_mode != DSO_ACQUIRE_MODE_AVERAGE) {
        fprintf(stderr,
                "10-bit average mode was unexpectedly clamped to %u\n",
                (unsigned)info.acquire_mode);
        return 1;
    }

    return 0;
}

static int test_8bit_keeps_average_mode_enabled(void)
{
    DSO_AcquireCtrlInfo info;

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
    DSO_AcquireCtrl_Init();
    DSO_AcquireCtrl_SetAcquireMode(DSO_ACQUIRE_MODE_AVERAGE);
    (void)DSO_AcquireCtrl_ApplyPending();
    DSO_AcquireCtrl_GetInfo(&info);

    if (info.acquire_mode != DSO_ACQUIRE_MODE_AVERAGE) {
        fprintf(stderr,
                "8-bit average mode was unexpectedly clamped: mode=%u\n",
                (unsigned)info.acquire_mode);
        return 1;
    }

    return 0;
}

static int test_8bit_still_allows_peak_detect(void)
{
    DSO_AcquireCtrlInfo info;

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
    DSO_AcquireCtrl_Init();
    DSO_AcquireCtrl_SetAcquireMode(DSO_ACQUIRE_MODE_PEAK_DETECT);
    (void)DSO_AcquireCtrl_ApplyPending();
    DSO_AcquireCtrl_GetInfo(&info);

    if (info.acquire_mode != DSO_ACQUIRE_MODE_PEAK_DETECT) {
        fprintf(stderr,
                "8-bit peak detect was unexpectedly clamped: mode=%u\n",
                (unsigned)info.acquire_mode);
        return 1;
    }

    return 0;
}

static int test_8bit_allows_hires_mode(void)
{
    DSO_AcquireCtrlInfo info;

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
    DSO_AcquireCtrl_Init();
    DSO_AcquireCtrl_SetAcquireMode(DSO_ACQUIRE_MODE_HIRES);
    (void)DSO_AcquireCtrl_ApplyPending();
    DSO_AcquireCtrl_GetInfo(&info);

    if (info.acquire_mode != DSO_ACQUIRE_MODE_HIRES) {
        fprintf(stderr,
                "8-bit hires mode was unexpectedly clamped: mode=%u\n",
                (unsigned)info.acquire_mode);
        return 1;
    }

    return 0;
}

static int test_average_accepts_raw_record_lengths(void)
{
    static const uint16_t frame0[4] = {0u, 100u, 0u, 100u};
    static const uint16_t frame1[4] = {100u, 0u, 100u, 0u};
    const uint16_t *avg0;
    const uint16_t *avg1;

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
    DSO_AcquireCtrl_Init();
    DSO_AcquireCtrl_SetAcquireMode(DSO_ACQUIRE_MODE_AVERAGE);
    (void)DSO_AcquireCtrl_ApplyPending();

    avg0 = DSO_AcquireCtrl_ApplyAverageForChannel(0u, frame0, 4u);

    if (avg0 == NULL) {
        fprintf(stderr, "average returned null buffer\n");
        return 1;
    }

    if ((avg0[0] != 0u) || (avg0[1] != 100u) || (avg0[2] != 0u) || (avg0[3] != 100u)) {
        fprintf(stderr,
                "first average frame did not preserve raw samples: %u,%u,%u,%u\n",
                (unsigned)avg0[0],
                (unsigned)avg0[1],
                (unsigned)avg0[2],
                (unsigned)avg0[3]);
        return 1;
    }

    avg1 = DSO_AcquireCtrl_ApplyAverageForChannel(0u, frame1, 4u);
    if (avg1 == NULL) {
        fprintf(stderr, "second average returned null buffer\n");
        return 1;
    }

    if ((avg1[0] != 50u) || (avg1[1] != 50u) || (avg1[2] != 50u) || (avg1[3] != 50u)) {
        fprintf(stderr,
                "second average frame did not converge on midpoint: %u,%u,%u,%u\n",
                (unsigned)avg1[0],
                (unsigned)avg1[1],
                (unsigned)avg1[2],
                (unsigned)avg1[3]);
        return 1;
    }

    return 0;
}

static int test_8bit_cycle_sequence_includes_hires(void)
{
    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);

    if (DSO_AcquireCtrl_GetNextAcquireMode(DSO_ACQUIRE_MODE_SAMPLE) !=
        DSO_ACQUIRE_MODE_PEAK_DETECT) {
        fprintf(stderr, "8-bit cycle sample->peak mismatch\n");
        return 1;
    }
    if (DSO_AcquireCtrl_GetNextAcquireMode(DSO_ACQUIRE_MODE_PEAK_DETECT) !=
        DSO_ACQUIRE_MODE_HIRES) {
        fprintf(stderr, "8-bit cycle peak->hires mismatch\n");
        return 1;
    }
    if (DSO_AcquireCtrl_GetNextAcquireMode(DSO_ACQUIRE_MODE_HIRES) !=
        DSO_ACQUIRE_MODE_AVERAGE) {
        fprintf(stderr, "8-bit cycle hires->average mismatch\n");
        return 1;
    }
    if (DSO_AcquireCtrl_GetNextAcquireMode(DSO_ACQUIRE_MODE_AVERAGE) !=
        DSO_ACQUIRE_MODE_SAMPLE) {
        fprintf(stderr, "8-bit cycle average->sample mismatch\n");
        return 1;
    }

    return 0;
}

static int test_10bit_cycle_sequence_includes_hires(void)
{
    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_10BIT);

    if (DSO_AcquireCtrl_GetNextAcquireMode(DSO_ACQUIRE_MODE_SAMPLE) !=
        DSO_ACQUIRE_MODE_PEAK_DETECT) {
        fprintf(stderr, "10-bit cycle sample->peak mismatch\n");
        return 1;
    }
    if (DSO_AcquireCtrl_GetNextAcquireMode(DSO_ACQUIRE_MODE_PEAK_DETECT) !=
        DSO_ACQUIRE_MODE_HIRES) {
        fprintf(stderr, "10-bit cycle peak->hires mismatch\n");
        return 1;
    }
    if (DSO_AcquireCtrl_GetNextAcquireMode(DSO_ACQUIRE_MODE_HIRES) !=
        DSO_ACQUIRE_MODE_AVERAGE) {
        fprintf(stderr, "10-bit cycle hires->average mismatch\n");
        return 1;
    }
    if (DSO_AcquireCtrl_GetNextAcquireMode(DSO_ACQUIRE_MODE_AVERAGE) !=
        DSO_ACQUIRE_MODE_SAMPLE) {
        fprintf(stderr, "10-bit cycle average->sample mismatch\n");
        return 1;
    }

    return 0;
}

static int test_run_requested_applies_without_control_reset(void)
{
    DSO_AcquireCtrlInfo info;
    bool changed;

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_10BIT);
    DSO_AcquireCtrl_Init();
    DSO_AcquireCtrl_GetInfo(&info);
    if (!info.run_requested || !DSO_AcquireCtrl_IsRunRequested()) {
        fprintf(stderr, "run_requested did not default to RUN\n");
        return 1;
    }

    DSO_AcquireCtrl_SetTriggerMode(DSO_TRIGGER_MODE_SINGLE);
    changed = DSO_AcquireCtrl_ApplyPending();
    DSO_AcquireCtrl_GetInfo(&info);
    if (!changed || !info.single_armed || info.single_captured) {
        fprintf(stderr, "single trigger setup did not arm as expected\n");
        return 1;
    }

    DSO_AcquireCtrl_SetRunRequested(false);
    changed = DSO_AcquireCtrl_ApplyPending();
    DSO_AcquireCtrl_GetInfo(&info);
    if (changed) {
        fprintf(stderr, "run_requested STOP was reported as control reset\n");
        return 1;
    }
    if (info.run_requested || DSO_AcquireCtrl_IsRunRequested()) {
        fprintf(stderr, "run_requested STOP did not apply\n");
        return 1;
    }
    if (!info.single_armed || info.single_captured) {
        fprintf(stderr, "run_requested STOP disturbed single trigger state\n");
        return 1;
    }

    DSO_AcquireCtrl_SetRunRequested(true);
    changed = DSO_AcquireCtrl_ApplyPending();
    DSO_AcquireCtrl_GetInfo(&info);
    if (changed) {
        fprintf(stderr, "run_requested RUN was reported as control reset\n");
        return 1;
    }
    if (!info.run_requested || !DSO_AcquireCtrl_IsRunRequested()) {
        fprintf(stderr, "run_requested RUN did not apply\n");
        return 1;
    }
    if (!info.single_armed || info.single_captured) {
        fprintf(stderr, "run_requested RUN disturbed single trigger state\n");
        return 1;
    }

    return 0;
}

int main(void)
{
    if (test_10bit_keeps_average_mode_enabled() != 0) {
        return 1;
    }
    if (test_8bit_keeps_average_mode_enabled() != 0) {
        return 1;
    }
    if (test_8bit_still_allows_peak_detect() != 0) {
        return 1;
    }
    if (test_8bit_allows_hires_mode() != 0) {
        return 1;
    }
    if (test_average_accepts_raw_record_lengths() != 0) {
        return 1;
    }
    if (test_8bit_cycle_sequence_includes_hires() != 0) {
        return 1;
    }
    if (test_10bit_cycle_sequence_includes_hires() != 0) {
        return 1;
    }
    if (test_run_requested_applies_without_control_reset() != 0) {
        return 1;
    }

    printf("all acquire control tests passed\n");
    return 0;
}
