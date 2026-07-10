#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_text_file(const char *path, size_t *out_size)
{
    FILE *fp = fopen(path, "rb");
    char *buf;
    size_t size;

    if (fp == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        fprintf(stderr, "failed to seek %s\n", path);
        return NULL;
    }

    size = (size_t)ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        fprintf(stderr, "failed to rewind %s\n", path);
        return NULL;
    }

    buf = (char *)malloc(size + 1u);
    if (buf == NULL) {
        fclose(fp);
        fprintf(stderr, "oom reading %s\n", path);
        return NULL;
    }

    if (size > 0u && fread(buf, 1u, size, fp) != size) {
        free(buf);
        fclose(fp);
        fprintf(stderr, "failed to read %s\n", path);
        return NULL;
    }

    fclose(fp);
    buf[size] = '\0';
    if (out_size != NULL) {
        *out_size = size;
    }
    return buf;
}

static char *normalize_newlines(const char *src)
{
    size_t len = strlen(src);
    char *dst = (char *)malloc(len + 1u);
    size_t w = 0u;

    if (dst == NULL) {
        return NULL;
    }

    for (size_t i = 0u; i < len; ++i) {
        if (src[i] != '\r') {
            dst[w++] = src[i];
        }
    }

    dst[w] = '\0';
    return dst;
}

static int require_contains(const char *haystack, const char *needle, const char *label)
{
    if (strstr(haystack, needle) == NULL) {
        fprintf(stderr, "missing %s\nneedle:\n%s\n", label, needle);
        return 1;
    }
    return 0;
}

static int require_not_contains(const char *haystack, const char *needle, const char *label)
{
    if (strstr(haystack, needle) != NULL) {
        fprintf(stderr, "unexpected %s\nneedle:\n%s\n", label, needle);
        return 1;
    }
    return 0;
}

int main(void)
{
    size_t size = 0u;
    char *raw_engine = read_text_file("User/dso/dso_engine.c", &size);
    char *raw_roll = read_text_file("User/dso/dso_engine_roll.c", &size);
    char *raw_config = read_text_file("User/dso/dso_config.h", &size);
    char *engine_text;
    char *roll_text;
    char *config_text;
    int rc = 0;

    if (raw_engine == NULL) {
        return 1;
    }
    if (raw_roll == NULL) {
        free(raw_engine);
        return 1;
    }
    if (raw_config == NULL) {
        free(raw_engine);
        free(raw_roll);
        return 1;
    }

    engine_text = normalize_newlines(raw_engine);
    free(raw_engine);
    if (engine_text == NULL) {
        free(raw_roll);
        free(raw_config);
        return 1;
    }

    roll_text = normalize_newlines(raw_roll);
    free(raw_roll);
    if (roll_text == NULL) {
        free(engine_text);
        free(raw_config);
        return 1;
    }

    config_text = normalize_newlines(raw_config);
    free(raw_config);
    if (config_text == NULL) {
        free(engine_text);
        free(roll_text);
        return 1;
    }

    rc |= require_contains(engine_text,
                           "DSO_Process_AnalyzeSamplesEx(s_engine_recent_samples,\n"
                           "                                             copied,\n"
                           "                                             sample_rate,\n"
                           "                                             &trig_cfg,\n"
                           "                                             &analysis);",
                           "roll-mode raw-source analysis");
    rc |= require_contains(engine_text,
                           "!DSO_Process_IsHs8DualFreqReliable(serial_source_samples,\n"
                           "                                               serial_source_count,\n"
                           "                                               &analysis)",
                           "published dual-hs8 frequency reliability gate");
    rc |= require_contains(engine_text,
                           "if (roll_mode) {\n"
                           "        DSO_Process_MeasureDisplay(display_lo, display_hi, DSO_DISPLAY_WIDTH, out);\n"
                           "        return;\n"
                           "    }\n\n"
                           "    if (DSO_EngineCal_SupportsRawPreprocess()",
                           "roll-mode measurement uses display buffer before raw-preprocess path");
    rc |= require_contains(engine_text,
                           "if (roll_mode) {\n"
                           "        return \"roll_display\";\n"
                           "    }\n"
                           "    if (DSO_EngineCal_SupportsRawPreprocess() && (serial_count > 0u)) {\n"
                           "        return \"raw\";\n"
                           "    }",
                           "roll-mode Vpp diagnostic source names display measurement");
    rc |= require_contains(engine_text,
                           "static uint32_t engine_vpp_source_count(bool roll_mode,\n"
                           "                                        uint32_t serial_count)\n"
                           "{\n"
                           "    return roll_mode ? (uint32_t)DSO_DISPLAY_WIDTH : serial_count;\n"
                           "}",
                           "roll-mode Vpp diagnostic source count names display width");
    rc |= require_contains(engine_text,
                           "static uint16_t engine_median3_u16(uint16_t a, uint16_t b, uint16_t c)",
                           "Vpp diagnostic lane median helper");
    rc |= require_contains(engine_text,
                           "printf(\"[VM] ch=1 mmn=%u mmx=%u mpp=%u mv=%.3f/%.3f/%.3f lmn=%u lmx=%u lpp=%u lv=%.3f/%.3f/%.3f spr=%u\\r\\n\"",
                           "Vpp diagnostic merged-vs-lane-median line");
    rc |= require_contains(engine_text,
                           "float scaled_pp = pp_v * scale;",
                           "Vpp diagnostic centered amplitude scaling");
    rc |= require_contains(config_text,
                           "#define DSO_MEASURE_AMP_SCALE 0.680f",
                           "formal measurement scale is 0.680");
    rc |= require_contains(config_text,
                           "#define DSO_HF_MEASURE_AMP_SCALE 0.600f",
                           "high-frequency measurement scale is 0.600");
    rc |= require_contains(config_text,
                           "#define DSO_HF_NORMAL_MEASURE_AMP_SCALE 0.560f",
                           "normal high-frequency measurement scale is 0.560");
    rc |= require_contains(config_text,
                           "#define DSO_VPP_DIAG_AMP_SCALE DSO_MEASURE_AMP_SCALE",
                           "Vpp diagnostic scale follows formal measurement scale");
    rc |= require_contains(engine_text,
                           "#include \"dso_8bit_vpp_cal.h\"",
                           "engine includes 8-bit Vpp calibration curve");
    rc |= require_contains(engine_text,
                           "printf(\"[VS] ch=1 sc=%.3f smv=%.3f/%.3f/%.3f slv=%.3f/%.3f/%.3f\\r\\n\"",
                           "Vpp diagnostic scaled voltage line");
    rc |= require_contains(engine_text,
                           "static void engine_apply_measure_amp_scale_to_frame(DSO_Frame *frame, float scale)",
                           "formal measurement scaling helper");
    rc |= require_contains(engine_text,
                           "static float engine_hf_measure_amp_scale(void)\n"
                           "{\n"
                           "    return (DSO_Timebase_GetMode() == TB_MODE_NORMAL) ? DSO_HF_NORMAL_MEASURE_AMP_SCALE\n"
                           "                                                      : DSO_HF_MEASURE_AMP_SCALE;\n"
                           "}",
                           "separate normal high-frequency measurement scale helper");
    rc |= require_contains(engine_text,
                           "static float engine_measure_amp_scale_for_frame(const DSO_Frame *frame,\n"
                           "                                                bool hf_lane_measurement)",
                           "frame-aware 8-bit Vpp calibration selector");
    rc |= require_contains(engine_text,
                           "if (frame->ext_freq_valid && (frame->ext_freq_millihz > 0ull)) {\n"
                           "        return (float)((double)frame->ext_freq_millihz / 1000.0);\n"
                           "    }\n"
                           "    if (frame->freq_valid && (frame->freq_hz > 0.0f)) {",
                           "8-bit Vpp calibration frequency prefers external meter");
    rc |= require_contains(engine_text,
                           "return DSO_8bitVppCal_ScaleForFreq(engine_vpp_cal_freq_hz(frame),\n"
                           "                                       legacy_scale);",
                           "8-bit Vpp frequency-response calibration call");
    rc |= require_contains(engine_text,
                           "if (DSO_Timebase_GetMode() == TB_MODE_ROLL) {\n"
                           "        return false;\n"
                           "    }\n"
                           "    if (DSO_SampleSpec_GetActiveFormat() != DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED) {",
                           "high-speed lane measurement applies outside roll mode");
    rc |= require_contains(engine_text,
                           "hf_lane_measurement = engine_apply_hf_lane_measurement_to_frame(&frame,\n"
                           "                                                                            serial_source_samples,\n"
                           "                                                                            serial_source_count);\n"
                           "            measure_scale = engine_measure_amp_scale_for_frame(&frame,\n"
                           "                                                               hf_lane_measurement);\n"
                           "            engine_apply_measure_amp_scale_to_frame(&frame, measure_scale);\n"
                           "#if DSO_VPP_DIAG_ENABLE\n"
                           "            engine_log_vpp_publish_frame(&frame, measure_scale, hf_lane_measurement);\n"
                           "#endif\n"
                           "        }\n"
                           "        DSO_UI_SendFrame(&frame);",
                           "high-frequency lane measurement and scaling before UI publish");
    rc |= require_contains(engine_text,
                           "printf(\"[VH] ch=1 sc=%.3f hlv=%.3f/%.3f/%.3f\\r\\n\"",
                           "high-frequency lane scaled diagnostic line");
    rc |= require_not_contains(engine_text,
                               "\"roll_raw_new\"",
                               "obsolete roll raw-new Vpp source label");
    /* The seed helpers moved to dso_engine_roll.c */
    rc |= require_contains(roll_text,
                           "static bool engine_roll_resync_seed_ch1_only(",
                           "roll resync CH1-only seed helper");
    rc |= require_contains(roll_text,
                           "static bool engine_roll_resync_seed_ch2_only(",
                           "roll resync CH2-only seed helper");
    rc |= require_contains(roll_text,
                           "static bool engine_roll_resync_seed_ring_per_channel(",
                           "per-channel roll ring seed helper");
    rc |= require_contains(roll_text,
                           "gentle roll: per-channel ring seed (one nc=0 -> reset that channel)",
                           "per-channel roll resync need_cap zero log");
    rc |= require_contains(roll_text,
                           "return engine_roll_resync_seed_ring_per_channel(pull1,",
                           "roll resync uses explicit per-channel seed path");

    free(engine_text);
    free(roll_text);
    free(config_text);

    if (rc == 0) {
        printf("all engine roll frequency policy tests passed\n");
    }

    return rc;
}
