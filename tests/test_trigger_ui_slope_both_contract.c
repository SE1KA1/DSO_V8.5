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
    FILE *fp = fopen("User/ui/ui_trigger_ui.c", "rb");
    char buf[65536];
    size_t n;

    if (fp == NULL) {
        fprintf(stderr, "failed to open ui_trigger_ui.c\n");
        return 1;
    }

    n = fread(buf, 1u, sizeof(buf) - 1u, fp);
    fclose(fp);
    buf[n] = '\0';

    require_substring(buf,
                      "DSO_AcquireCtrl_SetTriggerSlope(DSO_TRIGGER_SLOPE_BOTH);",
                      "ui maps both into acquire control");
    require_substring(buf,
                      "UI_TrigUi_SetSlopeFromDso(UI_TRIG_SLOPE_BOTH, 0);",
                      "ui syncs both from backend");
    return 0;
}
