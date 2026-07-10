#include <stdio.h>
#include <stdlib.h>

#include "dso/dso_acquire_control.h"

static void expect_int(const char *label, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "%s: actual=%d expected=%d\n", label, actual, expected);
        exit(1);
    }
}

int main(void)
{
    DSO_AcquireCtrlInfo info = {0};

    DSO_AcquireCtrl_Init();
    DSO_AcquireCtrl_SetTriggerSource(DSO_TRIGGER_SOURCE_CH2);
    expect_int("apply source ch2", DSO_AcquireCtrl_ApplyPending(), 1);
    DSO_AcquireCtrl_GetInfo(&info);
    expect_int("active source before normalize", (int)info.trigger_source, (int)DSO_TRIGGER_SOURCE_CH2);

    expect_int("normalize changed", DSO_AcquireCtrl_NormalizeTriggerSource(DSO_TRIGGER_SOURCE_CH1), 1);
    DSO_AcquireCtrl_GetInfo(&info);
    expect_int("active source after normalize", (int)info.trigger_source, (int)DSO_TRIGGER_SOURCE_CH1);

    expect_int("normalize unchanged", DSO_AcquireCtrl_NormalizeTriggerSource(DSO_TRIGGER_SOURCE_CH1), 0);

    expect_int("normalize invalid", DSO_AcquireCtrl_NormalizeTriggerSource((DSO_TriggerSource)99), 0);
    DSO_AcquireCtrl_GetInfo(&info);
    expect_int("invalid stays clamped to CH1", (int)info.trigger_source, (int)DSO_TRIGGER_SOURCE_CH1);

    DSO_AcquireCtrl_SetTriggerMode(DSO_TRIGGER_MODE_SINGLE);
    expect_int("apply single mode", DSO_AcquireCtrl_ApplyPending(), 1);
    DSO_AcquireCtrl_RearmSingle();
    expect_int("apply single rearm", DSO_AcquireCtrl_ApplyPending(), 1);
    DSO_AcquireCtrl_OnFrameResult(true);
    DSO_AcquireCtrl_GetInfo(&info);
    expect_int("captured before normalize", info.single_captured ? 1 : 0, 1);

    expect_int("normalize ch2 for single", DSO_AcquireCtrl_NormalizeTriggerSource(DSO_TRIGGER_SOURCE_CH2), 1);
    DSO_AcquireCtrl_GetInfo(&info);
    expect_int("single captured cleared", info.single_captured ? 1 : 0, 0);
    expect_int("single armed restored", info.single_armed ? 1 : 0, 1);
    expect_int("single source now ch2", (int)info.trigger_source, (int)DSO_TRIGGER_SOURCE_CH2);
    return 0;
}
