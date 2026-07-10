#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_substring(const char *haystack, const char *needle, const char *label)
{
    if (strstr(haystack, needle) == NULL) {
        fprintf(stderr, "missing %s: %s\n", label, needle);
        exit(1);
    }
}

int main(void)
{
    FILE *fp = fopen("User/dso/dso_engine.c", "rb");
    char buf[262144];
    size_t n;

    if (fp == NULL) {
        fprintf(stderr, "failed to open dso_engine.c\n");
        return 1;
    }

    n = fread(buf, 1u, sizeof(buf) - 1u, fp);
    fclose(fp);
    buf[n] = '\0';

    require_substring(buf,
                      "prev->trigger_source != cur->trigger_source",
                      "record reset on source change");
    require_substring(buf,
                      "frame.triggered = active_analysis->triggered;",
                      "frame uses active source");
    require_substring(buf,
                      "DSO_AcquireCtrl_OnFrameResult(active_analysis->triggered);",
                      "single uses active source");
    require_substring(buf,
                      "DSO_AcquireCtrl_NormalizeTriggerSource(DSO_TRIGGER_SOURCE_CH1)",
                      "engine normalizes invalid ch2 source");
    require_substring(buf,
                      "ctrl_changed = true;",
                      "normalization marks control changed");
    return 0;
}
