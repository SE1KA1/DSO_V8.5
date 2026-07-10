#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_text_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    long length;
    char *text;

    if (fp == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    text = (char *)malloc((size_t)length + 1u);
    if (text == NULL) {
        fprintf(stderr, "malloc failed\n");
        fclose(fp);
        exit(1);
    }
    if (fread(text, 1u, (size_t)length, fp) != (size_t)length) {
        fprintf(stderr, "failed to read %s\n", path);
        free(text);
        fclose(fp);
        exit(1);
    }
    text[length] = '\0';
    fclose(fp);
    return text;
}

static int require_contains(const char *path, const char *text, const char *needle)
{
    if (strstr(text, needle) == NULL) {
        fprintf(stderr, "%s missing: %s\n", path, needle);
        return 1;
    }
    return 0;
}

static int reject_contains(const char *path, const char *text, const char *needle)
{
    if (strstr(text, needle) != NULL) {
        fprintf(stderr, "%s forbidden text still present: %s\n", path, needle);
        return 1;
    }
    return 0;
}

int main(void)
{
    const char *path = "User/dso/dso_acq_10bit.c";
    char *text = read_text_file(path);
    int rc = 0;

    rc |= require_contains(path, text, "bool data_ready;");
    rc |= require_contains(path, text, "uint8_t data_ready_lane_count;");
    rc |= require_contains(path, text, "s_acq10.data_ready = false;");
    rc |= require_contains(path, text, "s_acq10.data_ready = true;");
    rc |= require_contains(path, text, "s_acq10.data_ready_lane_count = 2u;");
    rc |= require_contains(path, text, "s_acq10.snapshot_common_delta == 0u");
    rc |= require_contains(path, text, "s_acq10.snapshot_common_pos == 0u");
    rc |= require_contains(path, text, "(void)DSO_Acq10_GetPos();");
    rc |= reject_contains(path, text, "out->data_ready = s_acq10.running;");
    rc |= reject_contains(path,
                          text,
                          "bool DSO_Acq10_IsDataReady(void)\n"
                          "{\n"
                          "    return s_acq10.running;\n"
                          "}");

    free(text);
    return rc;
}
