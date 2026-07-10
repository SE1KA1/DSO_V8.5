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
    fseek(fp, 0, SEEK_END);
    length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    text = (char *)malloc((size_t)length + 1u);
    if (text == NULL) {
        fprintf(stderr, "malloc failed\n");
        fclose(fp);
        exit(1);
    }
    if (fread(text, 1u, (size_t)length, fp) != (size_t)length) {
        fprintf(stderr, "read failed for %s\n", path);
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

static int reject_contains(const char *path, const char *text, const char *needle)
{
    if (strstr(text, needle) != NULL) {
        fprintf(stderr, "%s forbidden text still present: %s\n", path, needle);
        return 1;
    }
    return 0;
}

int main(void)
{
    char *config_h = read_text_file("User/dso/dso_config.h");
    char *acq_model_h = read_text_file("User/dso/dso_acq_model.h");
    char *ui_screens_c = read_text_file("User/ui/ui_screens.c");
    char *waveform_h = read_text_file("User/ui/UI_WaveformCtrl.h");
    char *main_c = read_text_file("User/main.c");
    char *acq_doc = read_text_file("docs/architecture/03-acquisition-and-sample-modes.md");
    char *memory_doc = read_text_file("docs/architecture/06-memory-bsp-build-test.md");
    char *risk_doc = read_text_file("docs/architecture/07-maintenance-boundaries-and-risks.md");
    int rc = 0;

    rc |= require_contains("User/dso/dso_config.h",
                           config_h,
                           "DSO_ADC_MAX is a legacy compatibility constant");
    rc |= require_contains("User/dso/dso_acq_model.h",
                           acq_model_h,
                           "tim8_* names are legacy field names; current acquisition backends use TIM1");
    rc |= require_contains("User/ui/ui_screens.c",
                           ui_screens_c,
                           "ADC 8-bit/10-bit  factory  EC nav");
    rc |= require_contains("User/ui/ui_screens.c",
                           ui_screens_c,
                           "EC1: row  EC2: 10-bit / 8-bit (3L)  *=pending  |  BACK: scope");
    rc |= reject_contains("User/ui/ui_screens.c",
                          ui_screens_c,
                          "ADC 8/12");
    rc |= reject_contains("User/ui/ui_screens.c",
                          ui_screens_c,
                          "8b-2L");
    rc |= require_contains("User/ui/UI_WaveformCtrl.h",
                           waveform_h,
                           "RUN / STOP (routes to DSO_AcquireCtrl)");
    rc |= reject_contains("User/ui/UI_WaveformCtrl.h",
                          waveform_h,
                          "RUN / STOP (routes to DSO_Acq)");
    rc |= require_contains("User/main.c",
                           main_c,
                           "NUM_BUFFERS=1");
    rc |= reject_contains("User/main.c",
                          main_c,
                          "NUM_BUFFERS=2");

    rc |= require_contains("docs/architecture/03-acquisition-and-sample-modes.md",
                           acq_doc,
                           "默认格式为 `8-bit (3L)`");
    rc |= require_contains("docs/architecture/03-acquisition-and-sample-modes.md",
                           acq_doc,
                           "UI 只在 `10-bit` 与 `8-bit (3L)` 之间切换");
    rc |= reject_contains("docs/architecture/03-acquisition-and-sample-modes.md",
                          acq_doc,
                          "8-bit (2L)");

    rc |= require_contains("docs/architecture/06-memory-bsp-build-test.md",
                           memory_doc,
                           "SDRAM_RECORD_BUF..SDRAM_RECORD_END");
    rc |= require_contains("docs/architecture/06-memory-bsp-build-test.md",
                           memory_doc,
                           "SDRAM_ROLL_HISTORY_BUF..SDRAM_ROLL_HISTORY_END");
    rc |= require_contains("docs/architecture/06-memory-bsp-build-test.md",
                           memory_doc,
                           "IAR 工程当前不作为维护目标");
    rc |= reject_contains("docs/architecture/06-memory-bsp-build-test.md",
                          memory_doc,
                          "存在地址预算重叠风险");
    rc |= reject_contains("docs/architecture/06-memory-bsp-build-test.md",
                          memory_doc,
                          "如果 IAR");

    rc |= require_contains("docs/architecture/07-maintenance-boundaries-and-risks.md",
                           risk_doc,
                           "幅度换算使用 `DSO_Frame.adc_lsb_v`");
    rc |= require_contains("docs/architecture/07-maintenance-boundaries-and-risks.md",
                           risk_doc,
                           "IAR 工程当前不作为维护目标");
    rc |= reject_contains("docs/architecture/07-maintenance-boundaries-and-risks.md",
                          risk_doc,
                          "如果 IAR");
    free(config_h);
    free(acq_model_h);
    free(ui_screens_c);
    free(waveform_h);
    free(main_c);
    free(acq_doc);
    free(memory_doc);
    free(risk_doc);
    return rc;
}
