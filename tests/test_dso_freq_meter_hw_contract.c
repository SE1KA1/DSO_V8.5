#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    long len;
    char *buf;

    if (fp == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buf = (char *)malloc((size_t)len + 1u);
    if (buf == NULL) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    if (fread(buf, 1u, (size_t)len, fp) != (size_t)len) {
        fprintf(stderr, "failed to read %s\n", path);
        exit(1);
    }

    buf[len] = '\0';
    fclose(fp);
    return buf;
}

static void require_text(const char *buf, const char *needle)
{
    if (strstr(buf, needle) == NULL) {
        fprintf(stderr, "missing text: %s\n", needle);
        exit(1);
    }
}

static void reject_text(const char *buf, const char *needle)
{
    if (strstr(buf, needle) != NULL) {
        fprintf(stderr, "forbidden text still present: %s\n", needle);
        exit(1);
    }
}

int main(void)
{
    char *hw_h = read_file("User/dso/dso_freq_meter_hw_tim12.h");
    char *hw_c = read_file("User/dso/dso_freq_meter_hw_tim12.c");
    char *meter_c = read_file("User/dso/dso_freq_meter.c");

    require_text(hw_h, "DSO_FreqMeterHwTim12_ConfigureCaptureMode");
    require_text(hw_h, "DSO_FreqMeterHwTim12_ConfigureCounterMode");
    require_text(hw_h, "DSO_FreqMeterHwTim12_LatchAndResetCounter");
    require_text(hw_h, "DSO_FreqMeterHwTim12_TakeCaptureResult");

    require_text(hw_c, "GPIO_AF2_TIM12");
    require_text(hw_c, "TIM_TS_TI2FP2 | TIM_SLAVEMODE_EXTERNAL1");
    require_text(hw_c, "TIM_DIER_UIE | TIM_DIER_CC2IE");
    require_text(hw_c, "DSO_FREQ_METER_CAPTURE_MIN_TICKS");
    require_text(hw_c, "disable_capture_interrupt");
    require_text(hw_c, "TIM12->DIER &= ~TIM_DIER_CC2IE");
    require_text(hw_c, "TIM12->CCER &= ~TIM_CCER_CC2E");
    require_text(hw_c, "delta < DSO_FREQ_METER_CAPTURE_MIN_TICKS");
    require_text(hw_c, "void TIM8_BRK_TIM12_IRQHandler(void)");
    require_text(hw_c, "DSO_FreqMeterHwTim12_IRQHandler();");
    reject_text(hw_h, "DSO_FreqMeterHwCaptureEvent");
    reject_text(hw_h, "DSO_FreqMeterHwTim12_TakeCaptureEvent");

    reject_text(meter_c, "void TIM8_BRK_TIM12_IRQHandler(void)");
    reject_text(meter_c, "static void configure_pb15_tim12_ch2");
    reject_text(meter_c, "static void configure_tim12_external_counter");

    free(hw_h);
    free(hw_c);
    free(meter_c);
    return 0;
}
