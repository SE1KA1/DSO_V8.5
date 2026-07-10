#include <stdio.h>

#include "dso/dso_wavegen_timer_math.h"

static int test_240mhz_to_15mhz(void)
{
    DSO_WavegenTimerPlan plan = {0};

    if (DSO_WavegenTimer_CalcPlan(240000000u, 15000000u, &plan) != DSO_WAVEGEN_OK) {
        fprintf(stderr, "15 MHz plan failed\n");
        return 1;
    }
    if ((plan.psc != 0u) || (plan.arr != 15u) || (plan.actual_update_hz != 15000000u)) {
        fprintf(stderr,
                "bad 15 MHz plan: psc=%lu arr=%lu actual=%lu\n",
                (unsigned long)plan.psc,
                (unsigned long)plan.arr,
                (unsigned long)plan.actual_update_hz);
        return 1;
    }
    return 0;
}

static int test_rejects_zero_update(void)
{
    DSO_WavegenTimerPlan plan = {0};

    if (DSO_WavegenTimer_CalcPlan(240000000u, 0u, &plan) != DSO_WAVEGEN_INVALID) {
        fprintf(stderr, "zero update accepted\n");
        return 1;
    }
    return 0;
}

static int test_fractional_update_reports_actual(void)
{
    DSO_WavegenTimerPlan plan = {0};

    if (DSO_WavegenTimer_CalcPlan(240000000u, 14976000u, &plan) != DSO_WAVEGEN_OK) {
        fprintf(stderr, "14.976 MHz plan failed\n");
        return 1;
    }
    if ((plan.actual_update_hz < 14100000u) || (plan.actual_update_hz > 16000000u)) {
        fprintf(stderr, "actual update outside useful range: %lu\n", (unsigned long)plan.actual_update_hz);
        return 1;
    }
    return 0;
}

int main(void)
{
    if (test_240mhz_to_15mhz() != 0) {
        return 1;
    }
    if (test_rejects_zero_update() != 0) {
        return 1;
    }
    if (test_fractional_update_reports_actual() != 0) {
        return 1;
    }
    printf("all wavegen timer math tests passed\n");
    return 0;
}
