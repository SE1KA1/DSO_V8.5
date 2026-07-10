#include <stdio.h>
#include <string.h>

static int require_contains(const char *path, const char *text, const char *needle)
{
    if (strstr(text, needle) == NULL) {
        fprintf(stderr, "%s missing: %s\n", path, needle);
        return 1;
    }
    return 0;
}

int main(void)
{
    int rc = 0;
    const char *path = "User/dso/dso_acq_8bit.c";
    FILE *fp = fopen(path, "rb");
    char text[180000];
    size_t n;

    if (fp == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        return 1;
    }
    n = fread(text, 1u, sizeof(text) - 1u, fp);
    fclose(fp);
    text[n] = '\0';

    rc |= require_contains(path, text, "static bool acq8_all_active_lanes_have_dma_progress(void)");
    rc |= require_contains(path, text, "static void acq8_mark_data_not_ready(void)");
    rc |= require_contains(path, text, "static void acq8_update_data_ready_from_snapshot(void)");
    rc |= require_contains(path, text, "s_acq8.data_ready = false;");
    rc |= require_contains(path, text, "s_acq8.data_ready = true;");
    rc |= require_contains(path, text, "out->data_ready = s_acq8.data_ready;");
    rc |= require_contains(path, text, "out->data_ready_lane_count = s_acq8.data_ready_lane_count;");
    rc |= require_contains(path, text, "bool DSO_Acq8_IsDataReady(void)");

    if (rc == 0) {
        puts("all acq8 readiness gate tests passed");
    }
    return rc;
}
