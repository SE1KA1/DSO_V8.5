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

    require_substring(buf, "(void)snprintf(b, n, \"Edge\");", "edge-only formatter");
    require_absent(buf, "static void cycle_type_user(void)", "type cycler function");
    require_absent(buf, "case 1:", "type row ec2 branch");
    require_absent(buf, "static const char *t[] = { \"Edge\", \"Pulse\", \"Video\" };", "legacy type options");
    require_absent(buf, "UI_TRIG_TY_EDGE", "legacy type state");
    return 0;
}
