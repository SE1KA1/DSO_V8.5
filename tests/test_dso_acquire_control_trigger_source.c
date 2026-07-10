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
    expect_int("default trigger source", (int)info.trigger_source, (int)DSO_TRIGGER_SOURCE_CH1);

    DSO_AcquireCtrl_SetTriggerSource(DSO_TRIGGER_SOURCE_CH2);
    expect_int("apply pending source", DSO_AcquireCtrl_ApplyPending(), 1);
    DSO_AcquireCtrl_GetInfo(&info);
    expect_int("pending->active source", (int)info.trigger_source, (int)DSO_TRIGGER_SOURCE_CH2);

    DSO_AcquireCtrl_SetTriggerSource((DSO_TriggerSource)99);
    expect_int("apply invalid source", DSO_AcquireCtrl_ApplyPending(), 1);
    DSO_AcquireCtrl_GetInfo(&info);
    expect_int("invalid clamps to CH1", (int)info.trigger_source, (int)DSO_TRIGGER_SOURCE_CH1);
    return 0;
}
