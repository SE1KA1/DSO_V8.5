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
    char *header_text;
    char *hal_text;
    int rc = 0;

    (void)size;

    if (text == NULL) {
        fprintf(stderr, "failed to read User/dso/dso_acq_8bit.c\n");
        return 1;
    }

    header_text = read_text_file("User/dso/dso_acq.h", &size);
    if (header_text == NULL) {
        fprintf(stderr, "failed to read User/dso/dso_acq.h\n");
        free(text);
        return 1;
    }

    hal_text = read_text_file("User/dso/dso_acq_hal_stm32h7.c", &size);
    if (hal_text == NULL) {
        fprintf(stderr, "failed to read User/dso/dso_acq_hal_stm32h7.c\n");
        free(header_text);
        free(text);
        return 1;
    }

    rc |= assert_contains("User/dso/dso_acq_8bit.c",
                          text,
                          "static uint16_t s_dma_single0[DSO_DMA_BUF_SIZE]");
    rc |= assert_contains("User/dso/dso_acq_8bit.c",
                          text,
                          "static uint16_t s_dma_dual1[DSO_DMA_BUF_SIZE]");
    rc |= assert_contains("User/dso/dso_acq_8bit.c",
                          text,
                          "static uint16_t s_dma_lane2[DSO_DMA_BUF_SIZE]");

    rc |= assert_contains("User/dso/dso_acq_8bit.c", text, "ADC_EXTERNALTRIG_T1_CC1");
    rc |= assert_contains("User/dso/dso_acq_8bit.c", text, "ADC_EXTERNALTRIG_T1_CC2");
    rc |= assert_contains("User/dso/dso_acq_8bit.c", text, "ADC_EXTERNALTRIG_T1_CC3");
    rc |= assert_contains("User/dso/dso_acq_8bit.c", text, "multi.Mode = ADC_MODE_INDEPENDENT;");

    rc |= assert_contains("User/dso/dso_acq_8bit.c",
                          text,
                          "init_dma_handle(&s_acq8.hdma_adc1,\n                    DMA2_Stream0,\n                    DMA_REQUEST_ADC1");
    rc |= assert_contains("User/dso/dso_acq_8bit.c",
                          text,
                          "init_dma_handle(&s_acq8.hdma_adc2,\n                    DMA1_Stream0,\n                    DMA_REQUEST_ADC2");
    rc |= assert_contains("User/dso/dso_acq_8bit.c",
                          text,
                          "init_dma_handle(&s_acq8.hdma_adc3,\n                    DMA2_Stream1,\n                    DMA_REQUEST_ADC3");
    rc |= assert_contains("User/dso/dso_acq_8bit.c",
                          text,
                          "hdma->Init.Priority = DMA_PRIORITY_VERY_HIGH;");
    rc |= assert_not_contains("User/dso/dso_acq_8bit.c",
                              text,
                              "DMA1_Stream2,\n                    DMA_REQUEST_ADC2");

    rc |= assert_contains("User/dso/dso_acq_8bit.c", text, "HAL_ADC_Start_DMA(&s_acq8.hadc1");
    rc |= assert_contains("User/dso/dso_acq_8bit.c", text, "HAL_ADC_Start_DMA(&s_acq8.hadc2");
    rc |= assert_contains("User/dso/dso_acq_8bit.c", text, "HAL_ADC_Start_DMA(&s_acq8.hadc3");
    rc |= assert_contains("User/dso/dso_acq_8bit.c", text, "install_acq8_dma_callbacks(&s_acq8.hadc1);");
    rc |= assert_contains("User/dso/dso_acq_8bit.c", text, "install_acq8_dma_callbacks(&s_acq8.hadc2);");
    rc |= assert_contains("User/dso/dso_acq_8bit.c", text, "install_acq8_dma_callbacks(&s_acq8.hadc3);");
    rc |= assert_contains("User/dso/dso_acq_8bit.c",
                          text,
                          "hadc->DMA_Handle->XferHalfCpltCallback = acq8_dma_half_callback;");
    rc |= assert_contains("User/dso/dso_acq_8bit.c",
                          text,
                          "hadc->DMA_Handle->XferCpltCallback = acq8_dma_full_callback;");
    rc |= assert_contains("User/dso/dso_acq_8bit.c", text, "__HAL_ADC_CLEAR_FLAG(hadc, ADC_FLAG_OVR);");
    rc |= assert_contains("User/dso/dso_acq_8bit.c",
                          text,
                          "clear_active_adc_overrun_flags();\n    start_triphasic_timer();");

    rc |= assert_contains("User/dso/dso_acq_8bit.c",
                          text,
                          "config_adc_channel(&s_acq8.hadc3, ADC_CHANNEL_1)");
    rc |= assert_not_contains("User/dso/dso_acq_8bit.c",
                              text,
                              "config_adc_channel(&s_acq8.hadc3, ADC_CHANNEL_0)");
    rc |= assert_contains("User/dso/dso_acq_8bit.c", text, "DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED");
    rc |= assert_not_contains("User/dso/dso_acq_8bit.c", text, "configure_dual_interleaved_backend");
    rc |= assert_not_contains("User/dso/dso_acq_8bit.c", text, "ADC_DUALMODE_INTERL");
    rc |= assert_not_contains("User/dso/dso_acq_8bit.c", text, "HAL_ADCEx_MultiModeStart_DMA");
    rc |= assert_not_contains("User/dso/dso_acq_8bit.c", text, "s_dma_interl_words");

    rc |= assert_contains("User/dso/dso_acq.h", header_text, "lane_physical_window_count");
    rc |= assert_contains("User/dso/dso_acq.h", header_text, "lane0_physical_window");
    rc |= assert_contains("User/dso/dso_acq.h", header_text, "lane1_physical_window");
    rc |= assert_contains("User/dso/dso_acq.h", header_text, "lane2_physical_window");
    rc |= assert_contains("User/dso/dso_acq_8bit.c",
                          text,
                          "s_acq8.snapshot_pos0,\n                                       out->lane0_physical_window");
    rc |= assert_contains("User/dso/dso_acq_8bit.c",
                          text,
                          "s_acq8.snapshot_pos1,\n                                       out->lane1_physical_window");
    rc |= assert_contains("User/dso/dso_acq_8bit.c",
                          text,
                          "s_acq8.snapshot_pos2,\n                                       out->lane2_physical_window");

    rc |= assert_contains("User/dso/dso_acq_hal_stm32h7.c",
                          hal_text,
                          "void DMA1_Stream0_IRQHandler(void)\n{\n    DSO_Acq_DMA2_IRQHandler();\n}");
    rc |= assert_contains("User/dso/dso_acq_hal_stm32h7.c",
                          hal_text,
                          "void DMA2_Stream0_IRQHandler(void)\n{\n    DSO_Acq_DMA_IRQHandler();\n}");

    free(hal_text);
    free(header_text);
    free(text);

    if (rc != 0) {
        return 1;
    }

    puts("all acq8 tri-only dma contract tests passed");
    return 0;
}
