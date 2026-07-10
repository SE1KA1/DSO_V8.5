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

static void load_file(const char *path, char *buf, size_t size)
{
    FILE *fp = fopen(path, "rb");
    size_t n;

    if (fp == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        exit(1);
    }
    n = fread(buf, 1u, size - 1u, fp);
    fclose(fp);
    buf[n] = '\0';
}

int main(void)
{
    char process_buf[32768];
    char engine_buf[262144];

    load_file("User/dso/dso_process.h", process_buf, sizeof(process_buf));
    load_file("User/dso/dso_engine.c", engine_buf, sizeof(engine_buf));

    require_substring(process_buf, "uint8_t  trig_source;", "frame trigger source field");
    require_substring(engine_buf, "frame->trig_source = (uint8_t)acq_info.trigger_source;",
                      "frame metadata source assignment");
    return 0;
}
