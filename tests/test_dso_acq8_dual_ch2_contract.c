#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_text_file(const char *path, long *out_size)
{
    FILE *fp;
    long size;
    char *buf;

    if ((path == NULL) || (out_size == NULL)) {
        return NULL;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }

    size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    buf = (char *)malloc((size_t)size + 1u);
    if (buf == NULL) {
        fclose(fp);
        return NULL;
    }

    if (fread(buf, 1u, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        return NULL;
    }

    buf[size] = '\0';
    fclose(fp);
    *out_size = size;
    return buf;
}

static int assert_contains(const char *path, const char *text, const char *needle)
{
    if ((path == NULL) || (text == NULL) || (needle == NULL)) {
        return 1;
    }

    if (strstr(text, needle) == NULL) {
        fprintf(stderr, "expected %s to contain: %s\n", path, needle);
        return 1;
    }

    return 0;
}

static int assert_not_contains(const char *path, const char *text, const char *needle)
{
    if ((path == NULL) || (text == NULL) || (needle == NULL)) {
        return 1;
    }

    if (strstr(text, needle) != NULL) {
        fprintf(stderr, "expected %s to not contain: %s\n", path, needle);
        return 1;
    }

    return 0;
}

int main(void)
{
    long size = 0;
    char *text = read_text_file("User/dso/dso_acq_8bit.c", &size);

    (void)size;

    if (text == NULL) {
        fprintf(stderr, "failed to read User/dso/dso_acq_8bit.c\n");
        return 1;
    }

    if (assert_contains("User/dso/dso_acq_8bit.c",
                        text,
                        "config_adc_channel(&s_acq8.hadc3, ADC_CHANNEL_1)") != 0) {
        free(text);
        return 1;
    }

    if (assert_not_contains("User/dso/dso_acq_8bit.c",
                            text,
                            "config_adc_channel(&s_acq8.hadc3, ADC_CHANNEL_0)") != 0) {
        free(text);
        return 1;
    }

    if (assert_not_contains("User/dso/dso_acq_8bit.c",
                            text,
                            "configure_dual_interleaved_backend") != 0) {
        free(text);
        return 1;
    }

    if (assert_not_contains("User/dso/dso_acq_8bit.c",
                            text,
                            "topology_has_realtime_ch2") != 0) {
        free(text);
        return 1;
    }

    if (assert_not_contains("User/dso/dso_acq_8bit.c",
                            text,
                            "DSO_LaneMerge_CommonDelta2") != 0) {
        free(text);
        return 1;
    }

    if (assert_contains("User/dso/dso_acq_8bit.c",
                        text,
                        "bool DSO_Acq8_IsCh2Available(void)\n{\n    return false;\n}") != 0) {
        free(text);
        return 1;
    }

    if (assert_contains("User/dso/dso_acq_8bit.c",
                        text,
                        "uint32_t DSO_Acq8_GetCh2SampleRate(void)\n{\n    return 0u;\n}") != 0) {
        free(text);
        return 1;
    }

    free(text);
    puts("all acq8 tri-only CH2-disabled contract tests passed");
    return 0;
}
