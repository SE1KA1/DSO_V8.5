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

static void require_absent(const char *haystack, const char *needle, const char *label)
{
    if (strstr(haystack, needle) != NULL) {
        fprintf(stderr, "unexpected %s: %s\n", label, needle);
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
                      "static const char *t[] = { \"+1.25V\", \"0.00V\", \"OFF\" };",
                      "zero-level options");
    require_absent(buf, "-0.50V", "legacy negative level label");
    require_substring(buf,
                      "DSO_AcquireCtrl_SetTriggerLevelEnabled(false);",
                      "off disables manual trigger level");
    require_substring(buf,
                      "DSO_AcquireCtrl_SetTriggerLevelEnabled(true);",
                      "explicit presets enable manual trigger level");
    require_substring(buf,
                      "DSO_AcquireCtrl_SetTriggerLevelVolts(0.0f);",
                      "zero preset writes zero volts");
    return 0;
}
