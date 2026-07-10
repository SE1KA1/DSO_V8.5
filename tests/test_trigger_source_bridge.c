#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "ui/ui_trigger_ui.h"
#include "dso/dso_acquire_control.h"

static DSO_TriggerSource g_last_source = DSO_TRIGGER_SOURCE_CH1;
static UI_TrigUiSource_t g_last_ui = UI_TRIG_SRC_CH1;
static bool g_ch2_available = false;

bool DSO_Acq_IsCh2Available(void)
{
    return g_ch2_available;
}

void DSO_AcquireCtrl_SetTriggerSource(DSO_TriggerSource source)
{
    g_last_source = source;
}

void DSO_AcquireCtrl_GetInfo(DSO_AcquireCtrlInfo *out)
{
    if (out == NULL) {
        return;
    }

    *out = (DSO_AcquireCtrlInfo){0};
    out->trigger_source = g_last_source;
}

void UI_TrigUi_SetSourceFromDso(UI_TrigUiSource_t v, int request_redraw)
{
    (void)request_redraw;
    g_last_ui = v;
}

void UI_TriggerCtrl_OnSourceChanged(UI_TrigUiSource_t s);

static void expect_int(const char *label, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "%s: actual=%d expected=%d\n", label, actual, expected);
        exit(1);
    }
}

int main(void)
{
    g_ch2_available = false;
    g_last_source = DSO_TRIGGER_SOURCE_CH1;
    g_last_ui = UI_TRIG_SRC_CH1;
    UI_TriggerCtrl_OnSourceChanged(UI_TRIG_SRC_CH2);
    expect_int("fallback source", (int)g_last_source, (int)DSO_TRIGGER_SOURCE_CH1);
    expect_int("fallback ui mirror", (int)g_last_ui, (int)UI_TRIG_SRC_CH1);

    g_ch2_available = true;
    UI_TriggerCtrl_OnSourceChanged(UI_TRIG_SRC_CH2);
    expect_int("ch2 source", (int)g_last_source, (int)DSO_TRIGGER_SOURCE_CH2);
    expect_int("ch2 ui mirror", (int)g_last_ui, (int)UI_TRIG_SRC_CH2);

    UI_TriggerCtrl_OnSourceChanged(UI_TRIG_SRC_CH1);
    expect_int("ch1 source", (int)g_last_source, (int)DSO_TRIGGER_SOURCE_CH1);
    expect_int("ch1 ui mirror", (int)g_last_ui, (int)UI_TRIG_SRC_CH1);
    return 0;
}
