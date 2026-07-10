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
    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(fp);
        exit(1);
    }
    length = ftell(fp);
    if (length < 0) {
        fprintf(stderr, "failed to tell %s\n", path);
        fclose(fp);
        exit(1);
    }
    rewind(fp);
    text = (char *)malloc((size_t)length + 1u);
    if (text == NULL) {
        fprintf(stderr, "failed to allocate %ld bytes for %s\n", length + 1, path);
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

static int require_not_contains(const char *path, const char *text, const char *needle)
{
    if (strstr(text, needle) != NULL) {
        fprintf(stderr, "%s still contains obsolete text: %s\n", path, needle);
        return 1;
    }
    return 0;
}

int main(void)
{
    int rc = 0;
    const char *path = "User/dso/dso_timebase.c";
    char *text = read_text_file(path);

    rc |= require_contains(path, text, "static const DSO_TimebaseEntry s_table_8bit_tri");
    rc |= require_contains(path, text, "return s_table_8bit_tri;");
    rc |= require_contains(path, text, "DSO_ACQ8_TRI_INTERLEAVED_TIM_ARR");
    rc |= require_contains(path, text, "DSO_ACQ8_TRI_INTERLEAVED_SAMPLE_RATE_HZ");
    rc |= require_not_contains(path, text, "s_table_8bit_dual");
    rc |= require_not_contains(path, text, "static const DSO_TimebaseEntry s_table_8bit[");

    free(text);
    if (rc == 0) {
        puts("all 8-bit tri-only timebase contract tests passed");
    }
    return rc;
}
