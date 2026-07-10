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
        fprintf(stderr, "%s: missing \"%s\"\n", path, needle);
        return 0;
    }
    return 1;
}

static int require_not_contains(const char *path, const char *text, const char *needle)
{
    if (strstr(text, needle) != NULL) {
        fprintf(stderr, "%s: forbidden \"%s\"\n", path, needle);
        return 0;
    }
    return 1;
}

int main(void)
{
    const char *path = "User/dso/dso_acq_8bit.c";
    char *text = read_text_file(path);
    int ok = 1;

    ok &= require_contains(path, text, "return period_clocks / 6u;");
    ok &= require_contains(path, text, "return period_clocks / 2u;");
    ok &= require_contains(path, text, "return (period_clocks * 5u) / 6u;");

    ok &= require_contains(path, text, "DSO_LaneMerge_CommonDelta3");
    ok &= require_contains(path, text, "invalidate_dma_u16_window(s_dma_single0");
    ok &= require_contains(path, text, "invalidate_dma_u16_window(s_dma_dual1");
    ok &= require_contains(path, text, "invalidate_dma_u16_window(s_dma_lane2");
    ok &= require_contains(path, text, "HAL_ADC_Start_DMA(&s_acq8.hadc1, (uint32_t *)s_dma_single0");
    ok &= require_contains(path, text, "HAL_ADC_Start_DMA(&s_acq8.hadc2, (uint32_t *)s_dma_dual1");
    ok &= require_contains(path, text, "HAL_ADC_Start_DMA(&s_acq8.hadc3, (uint32_t *)s_dma_lane2");
    ok &= require_contains(path, text, "config_adc_channel(&s_acq8.hadc3, ADC_CHANNEL_1)");

    ok &= require_not_contains(path, text, "HAL_ADCEx_MultiModeStart_DMA");
    ok &= require_not_contains(path, text, "s_dma_interl_words");
    ok &= require_not_contains(path, text, "ADC_DUALMODE");
    ok &= require_not_contains(path, text, "configure_dual_interleaved_backend");
    ok &= require_not_contains(path, text, "configure_single_safe_backend");
    ok &= require_not_contains(path, text, "ADC_CHANNEL_0");

    free(text);
    return ok ? 0 : 1;
}
