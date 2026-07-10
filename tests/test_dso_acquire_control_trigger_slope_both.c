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
    DSO_AcquireCtrl_GetInfo(&info);
    expect_int("default slope", (int)info.trigger_slope, (int)DSO_TRIGGER_SLOPE_RISING);

    DSO_AcquireCtrl_SetTriggerSlope(DSO_TRIGGER_SLOPE_BOTH);
    expect_int("apply both slope", DSO_AcquireCtrl_ApplyPending(), 1);
    DSO_AcquireCtrl_GetInfo(&info);
    expect_int("pending->active both", (int)info.trigger_slope, (int)DSO_TRIGGER_SLOPE_BOTH);

    DSO_AcquireCtrl_SetTriggerSlope((DSO_TriggerSlope)99);
    expect_int("apply invalid slope", DSO_AcquireCtrl_ApplyPending(), 1);
    DSO_AcquireCtrl_GetInfo(&info);
    expect_int("invalid clamps to rising", (int)info.trigger_slope, (int)DSO_TRIGGER_SLOPE_RISING);
    return 0;
}
