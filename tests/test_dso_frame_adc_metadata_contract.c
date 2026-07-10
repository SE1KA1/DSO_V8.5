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
        fprintf(stderr, "malloc failed for %s\n", path);
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
    char *process_h = read_text_file("User/dso/dso_process.h");
    char *engine_c = read_text_file("User/dso/dso_engine.c");
    char *spec_h = read_text_file("User/dso/dso_sample_spec.h");
    char *waveform_c = read_text_file("User/ui/UI_WaveformCtrl.c");
    char *screens_c = read_text_file("User/ui/ui_screens.c");
    int rc = 0;

    /* DSO_Frame only keeps adc_bits and adc_lsb_v; adc_max/midscale are canonical in dso_sample_spec.h. */
    rc |= require_contains("User/dso/dso_process.h", process_h, "uint8_t  adc_bits;");
    rc |= require_contains("User/dso/dso_process.h", process_h, "float    adc_lsb_v;");
    rc |= reject_contains("User/dso/dso_process.h", process_h, "uint16_t adc_max;");
    rc |= reject_contains("User/dso/dso_process.h", process_h, "uint16_t adc_midscale;");
    rc |= reject_contains("User/dso/dso_process.h", process_h, "uint8_t  sample_format;");

    /* Canonical source for adc_max and midscale is dso_sample_spec.h. */
    rc |= require_contains("User/dso/dso_sample_spec.h", spec_h, "uint16_t adc_max;");
    rc |= require_contains("User/dso/dso_sample_spec.h", spec_h, "uint16_t midscale;");

    /* dso_engine.c fill_frame_metadata only writes adc_bits and adc_lsb_v to the frame. */
    rc |= require_contains("User/dso/dso_engine.c", engine_c, "const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();");
    rc |= require_contains("User/dso/dso_engine.c", engine_c, "frame->adc_bits = spec->adc_bits;");
    rc |= require_contains("User/dso/dso_engine.c", engine_c, "frame->adc_lsb_v = spec->vref / (float)spec->adc_max;");
    rc |= reject_contains("User/dso/dso_engine.c", engine_c, "frame->adc_max = spec->adc_max;");
    rc |= reject_contains("User/dso/dso_engine.c", engine_c, "frame->adc_midscale = spec->midscale;");
    rc |= reject_contains("User/dso/dso_engine.c", engine_c, "frame->sample_format =");

    rc |= require_contains("User/ui/UI_WaveformCtrl.c", waveform_c, "#include \"ui_measure_units.h\"");
    rc |= require_contains("User/ui/UI_WaveformCtrl.c", waveform_c, "UI_Measure_CodeDeltaToVolts");
    rc |= reject_contains("User/ui/UI_WaveformCtrl.c", waveform_c, "DSO_VREF / (float)DSO_ADC_MAX");

    rc |= require_contains("User/ui/ui_screens.c", screens_c, "#include \"ui_measure_units.h\"");
    rc |= require_contains("User/ui/ui_screens.c", screens_c, "UI_Measure_CodeDeltaToVolts");
    rc |= reject_contains("User/ui/ui_screens.c", screens_c, "DSO_VREF / (float)DSO_ADC_MAX");

    free(process_h);
    free(engine_c);
    free(spec_h);
    free(waveform_c);
    free(screens_c);
    return rc;
}
