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
    char *text = read_text_file("User/dso/dso_acq_10bit.c", &size);

    (void)size;

    if (text == NULL) {
        fprintf(stderr, "failed to read User/dso/dso_acq_10bit.c\n");
        return 1;
    }

    if (assert_contains("User/dso/dso_acq_10bit.c", text, "case 75000000u:") != 0) {
        free(text);
        return 1;
    }
    if (assert_contains("User/dso/dso_acq_10bit.c", text, "case 150000000u:") != 0) {
        free(text);
        return 1;
    }
    if (assert_contains("User/dso/dso_acq_10bit.c", text, "periph_clk.PLL2.PLL2N = 150;") != 0) {
        free(text);
        return 1;
    }
    if (assert_contains("User/dso/dso_acq_10bit.c", text, "periph_clk.PLL2.PLL2P = 10;") != 0) {
        free(text);
        return 1;
    }
    if (assert_contains("User/dso/dso_acq_10bit.c", text, "periph_clk.PLL2.PLL2P = 5;") != 0) {
        free(text);
        return 1;
    }
    if (assert_not_contains("User/dso/dso_acq_10bit.c", text, "if (adc_input_clk_hz >= 100000000u)") != 0) {
        free(text);
        return 1;
    }
    if (assert_not_contains("User/dso/dso_acq_10bit.c", text, "periph_clk.PLL2.PLL2N = 100;") != 0) {
        free(text);
        return 1;
    }
    if (assert_not_contains("User/dso/dso_acq_10bit.c", text, "periph_clk.PLL2.PLL2P = 6;") != 0) {
        free(text);
        return 1;
    }

    free(text);
    puts("all acq10 clock contract tests passed");
    return 0;
}
