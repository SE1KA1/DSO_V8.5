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

static int require_ordered(const char *path,
                           const char *text,
                           const char *first,
                           const char *second)
{
    const char *a = strstr(text, first);
    const char *b = strstr(text, second);

    if (a == NULL) {
        fprintf(stderr, "%s: missing first marker \"%s\"\n", path, first);
        return 0;
    }
    if (b == NULL) {
        fprintf(stderr, "%s: missing second marker \"%s\"\n", path, second);
        return 0;
    }
    if (a > b) {
        fprintf(stderr, "%s: marker \"%s\" must appear before \"%s\"\n",
                path, first, second);
        return 0;
    }
    return 1;
}

int main(void)
{
    const char *acq10_path = "User/dso/dso_acq_10bit.c";
    const char *model_path = "User/dso/dso_acq_model.h";
    char *acq10 = read_text_file(acq10_path);
    char *model = read_text_file(model_path);
    int ok = 1;

    ok &= require_contains(acq10_path, acq10, "return period_clocks / 4u;");
    ok &= require_contains(acq10_path, acq10, "return (period_clocks * 3u) / 4u;");
    ok &= require_contains(acq10_path, acq10, "return period_clocks / 2u;");
    ok &= require_not_contains(acq10_path, acq10, "return period_clocks / 6u;");
    ok &= require_contains(acq10_path, acq10, "static void start_tim1_pwm_channels_synchronized(void)");
    ok &= require_contains(acq10_path, acq10, "TIM_CCxChannelCmd(tim, TIM_CHANNEL_1, TIM_CCx_ENABLE);");
    ok &= require_contains(acq10_path, acq10, "TIM_CCxChannelCmd(tim, TIM_CHANNEL_2, TIM_CCx_ENABLE);");
    ok &= require_contains(acq10_path, acq10, "TIM_CCxChannelCmd(tim, TIM_CHANNEL_3, TIM_CCx_ENABLE);");
    ok &= require_contains(acq10_path, acq10, "__HAL_TIM_MOE_ENABLE(&s_acq10.htim1);");
    ok &= require_contains(acq10_path, acq10, "__HAL_TIM_ENABLE(&s_acq10.htim1);");
    ok &= require_ordered(acq10_path,
                          acq10,
                          "TIM_CCxChannelCmd(tim, TIM_CHANNEL_3, TIM_CCx_ENABLE);",
                          "__HAL_TIM_ENABLE(&s_acq10.htim1);");
    ok &= require_contains(acq10_path, acq10, "start_tim1_pwm_channels_synchronized();");
    ok &= require_not_contains(acq10_path, acq10, "HAL_TIM_PWM_Start(&s_acq10.htim1, TIM_CHANNEL_1)");
    ok &= require_not_contains(acq10_path, acq10, "10-bit reports the active lane0 DMA position");
    ok &= require_not_contains(acq10_path, acq10, "s_acq10.common_pos = pos0;");

    ok &= require_contains(acq10_path, acq10, "DSO_LaneMerge_CommonDelta2(s_acq10.common_pos");
    ok &= require_contains(acq10_path, acq10, "s_acq10.common_pos + delta.common_delta");
    ok &= require_contains(acq10_path, acq10, "s_acq10.snapshot_common_delta = delta.common_delta;");
    ok &= require_contains(acq10_path, acq10, "s_acq10.snapshot_phase_sync_error = delta.phase_sync_error;");

    ok &= require_contains(acq10_path, acq10, "static void invalidate_dma_u16_window");
    ok &= require_contains(acq10_path, acq10, "static void invalidate_dma_u16_recent");
    ok &= require_contains(acq10_path, acq10, "static void prepare_all_dma_rx_buffers");
    ok &= require_contains(acq10_path, acq10, "invalidate_dma_u16_window(s_dma_lane0");
    ok &= require_contains(acq10_path, acq10, "invalidate_dma_u16_window(s_dma_lane1");
    ok &= require_contains(acq10_path, acq10, "invalidate_dma_u16_window(s_dma_ch2");
    ok &= require_ordered(acq10_path,
                          acq10,
                          "prepare_all_dma_rx_buffers();",
                          "HAL_ADC_Start_DMA(&s_acq10.hadc1");
    ok &= require_contains(acq10_path, acq10, "static void reset_dma_irq(IRQn_Type irqn)");
    ok &= require_contains(acq10_path, acq10, "HAL_NVIC_DisableIRQ(irqn);");
    ok &= require_contains(acq10_path, acq10, "HAL_NVIC_ClearPendingIRQ(irqn);");
    ok &= require_contains(acq10_path, acq10, "reset_dma_irq(DMA1_Stream1_IRQn);");
    ok &= require_contains(acq10_path, acq10, "reset_dma_irq(DMA1_Stream2_IRQn);");
    ok &= require_contains(acq10_path, acq10, "reset_dma_irq(DMA2_Stream1_IRQn);");

    ok &= require_contains(model_path, model, "independent ADC lane");
    ok &= require_not_contains(model_path, model, "packed DMA word");
    ok &= require_not_contains(model_path, model, "fallback-invalid");

    free(acq10);
    free(model);
    return ok ? 0 : 1;
}
