#include <stdio.h>
#include <stdlib.h>

#include "dso/dso_acquire_control.h"
#include "dso/dso_sample_spec.h"

static void expect_int(const char *label, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "%s: actual=%d expected=%d\n", label, actual, expected);
        exit(1);
    }
}

static void expect_float_close(const char *label, float actual, float expected)
{
    float diff = actual - expected;

    if (diff < 0.0f) {
        diff = -diff;
    }
    if (diff > 0.001f) {
        fprintf(stderr, "%s: actual=%.3f expected=%.3f\n", label, (double)actual, (double)expected);
        exit(1);
    }
}

int main(void)
{
    DSO_AcquireCtrlInfo info = {0};

    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
    DSO_AcquireCtrl_Init();

    DSO_AcquireCtrl_GetInfo(&info);
    expect_int("default level enabled", info.trigger_level_enabled ? 1 : 0, 1);

    DSO_AcquireCtrl_SetTriggerLevelVolts(1.25f);
    expect_int("apply explicit level", DSO_AcquireCtrl_ApplyPending(), 1);
    DSO_AcquireCtrl_GetInfo(&info);
    expect_int("explicit level keeps enabled", info.trigger_level_enabled ? 1 : 0, 1);
    expect_float_close("explicit level stored", info.trigger_level_v, 1.25f);

    DSO_AcquireCtrl_SetTriggerLevelEnabled(false);
    expect_int("apply level off", DSO_AcquireCtrl_ApplyPending(), 1);
    DSO_AcquireCtrl_GetInfo(&info);
    expect_int("level disabled", info.trigger_level_enabled ? 1 : 0, 0);
    expect_float_close("stored level preserved when off", info.trigger_level_v, 1.25f);

    DSO_AcquireCtrl_SetTriggerLevelEnabled(true);
    expect_int("apply level on", DSO_AcquireCtrl_ApplyPending(), 1);
    DSO_AcquireCtrl_GetInfo(&info);
    expect_int("level re-enabled", info.trigger_level_enabled ? 1 : 0, 1);
    expect_float_close("stored level restored", info.trigger_level_v, 1.25f);
    return 0;
}
