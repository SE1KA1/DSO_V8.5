#include <stdio.h>
#include <string.h>

static int read_file(const char *path, char *buf, size_t cap)
{
    FILE *fp = fopen(path, "rb");
    size_t n;

    if (fp == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        return 1;
    }
    n = fread(buf, 1u, cap - 1u, fp);
    fclose(fp);
    buf[n] = '\0';
    return 0;
}

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
    char buf[65536];

    if (read_file("User/dso/dso_acq.h", buf, sizeof(buf)) != 0) {
        return 1;
    }
    rc |= require_contains("User/dso/dso_acq.h", buf, "bool DSO_Acq_IsDataReady(void);");
    rc |= require_contains("User/dso/dso_acq.h", buf, "bool data_ready;");
    rc |= require_contains("User/dso/dso_acq.h", buf, "uint8_t data_ready_lane_count;");

    if (read_file("User/dso/dso_acq.c", buf, sizeof(buf)) != 0) {
        return 1;
    }
    rc |= require_contains("User/dso/dso_acq.c", buf, "return DSO_Acq8_IsDataReady();");
    rc |= require_contains("User/dso/dso_acq.c", buf, "return DSO_Acq10_IsDataReady();");

    if (read_file("User/dso/dso_acq_8bit.h", buf, sizeof(buf)) != 0) {
        return 1;
    }
    rc |= require_contains("User/dso/dso_acq_8bit.h", buf, "bool DSO_Acq8_IsDataReady(void);");

    if (read_file("User/dso/dso_acq_10bit.h", buf, sizeof(buf)) != 0) {
        return 1;
    }
    rc |= require_contains("User/dso/dso_acq_10bit.h", buf, "bool DSO_Acq10_IsDataReady(void);");

    if (read_file("User/dso/dso_acq_10bit.c", buf, sizeof(buf)) != 0) {
        return 1;
    }
    rc |= require_contains("User/dso/dso_acq_10bit.c", buf, "static bool acq10_all_ch1_lanes_have_dma_progress(void)");
    rc |= require_contains("User/dso/dso_acq_10bit.c", buf, "static void acq10_mark_data_not_ready(void)");
    rc |= require_contains("User/dso/dso_acq_10bit.c", buf, "static void acq10_update_data_ready_from_snapshot(void)");
    rc |= require_contains("User/dso/dso_acq_10bit.c", buf, "out->data_ready = s_acq10.data_ready;");
    rc |= require_contains("User/dso/dso_acq_10bit.c", buf, "return s_acq10.data_ready;");

    if (rc == 0) {
        puts("all acquisition readiness facade contract tests passed");
    }
    return rc;
}
