#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_text_file(const char *path)
{
    FILE *fp;
    long size;
    char *buf;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        exit(1);
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(fp);
        exit(1);
    }
    size = ftell(fp);
    if (size < 0) {
        fprintf(stderr, "failed to tell %s\n", path);
        fclose(fp);
        exit(1);
    }
    rewind(fp);

    buf = (char *)malloc((size_t)size + 1u);
    if (buf == NULL) {
        fprintf(stderr, "failed to allocate %ld bytes\n", size + 1);
        fclose(fp);
        exit(1);
    }
    if (fread(buf, 1u, (size_t)size, fp) != (size_t)size) {
        fprintf(stderr, "failed to read %s\n", path);
        free(buf);
        fclose(fp);
        exit(1);
    }
    buf[size] = '\0';
    fclose(fp);
    return buf;
}

static int require_contains(const char *path, const char *text,
                            const char *needle, const char *label)
{
    if (strstr(text, needle) == NULL) {
        fprintf(stderr, "%s: missing %s: %s\n", path, label, needle);
        return 0;
    }
    return 1;
}

static int forbid_contains(const char *path, const char *text,
                           const char *needle, const char *label)
{
    if (strstr(text, needle) != NULL) {
        fprintf(stderr, "%s: forbidden %s: %s\n", path, label, needle);
        return 0;
    }
    return 1;
}

int main(void)
{
    const char *path = "User/dso/dso_engine.c";
    char *text = read_text_file(path);
    int ok = 1;

    ok &= require_contains(path,
                           text,
                           "bool record_anchor_on_trigger = false;",
                           "explicit record anchor policy state");
    ok &= require_contains(path,
                           text,
                           "record_anchor_on_trigger = active_analysis->triggered ||\n"
                           "                                       (!DSO_Acq_IsRunning() && record_info.trigger_valid);",
                           "stale trigger anchors only while physically stopped");
    ok &= require_contains(path,
                           text,
                           "DSO_Record_CopyViewWindow(window_samples,\n"
                           "                                                                 record_anchor_on_trigger,",
                           "CH1 record anchor argument");
    ok &= require_contains(path,
                           text,
                           "DSO_Record_CopyViewWindowCh2(ch2_window_samples,\n"
                           "                                                                        record_anchor_on_trigger,",
                           "CH2 record anchor argument");
    ok &= require_contains(path,
                           text,
                           "Keep the live trace visible from the recent DMA window instead of publishing nothing.",
                           "fallback to recent samples when record view is empty while running");
    ok &= forbid_contains(path,
                          text,
                          "record_info.trigger_valid || analysis.triggered",
                          "stale trigger anchor while running");

    free(text);
    return ok ? 0 : 1;
}
