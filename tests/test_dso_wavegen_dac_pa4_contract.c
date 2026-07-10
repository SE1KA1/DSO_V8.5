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
    const char *path = "User/dso/dso_wavegen_dac_pa4.c";
    char *text = read_text_file(path);
    int ok = 1;

    ok &= require_contains(path, text, "DMA1_Stream5");
    ok &= require_contains(path, text, "DAC_TRIGGER_T5_TRGO");
    ok &= require_contains(path, text, "__HAL_RCC_TIM5_CLK_ENABLE();");
    ok &= require_contains(path, text, "TIM5->CR2 = TIM_CR2_MMS_1;");
    ok &= require_contains(path, text, "DMA_PRIORITY_VERY_HIGH");
    ok &= require_contains(path, text, "SCB_CleanDCache_by_Addr");
    ok &= require_contains(path, text, "TIM5->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;");
    ok &= require_not_contains(path, text, "DAC_TRIGGER_T6_TRGO");
    ok &= require_not_contains(path, text, "DAC_TRIGGER_T8_TRGO");

    free(text);
    return ok ? 0 : 1;
}
