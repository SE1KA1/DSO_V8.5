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

static void normalize_newlines(char *text)
{
    char *src;
    char *dst;

    if (text == NULL) {
        return;
    }

    src = text;
    dst = text;
    while (*src != '\0') {
        if (*src != '\r') {
            *dst++ = *src;
        }
        ++src;
    }
    *dst = '\0';
}

static int assert_contains(const char *path, const char *text, const char *needle)
{
    if ((path == NULL) || (text == NULL) || (needle == NULL)) {
        return 1;
    }

    if (strstr(text, needle) == NULL) {
        fprintf(stderr, "expected policy text missing in %s: %s\n", path, needle);
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
        fprintf(stderr, "stale policy text still present in %s: %s\n", path, needle);
        return 1;
    }

    return 0;
}

int main(void)
{
    const char *path = "User/dso/dso_engine_cal.c";
    long size = 0;
    char *text = read_text_file(path, &size);
    int rc = 0;

    (void)size;

    if (text == NULL) {
        fprintf(stderr, "failed to read %s\n", path);
        return 1;
    }

    normalize_newlines(text);

    rc |= assert_not_contains(path,
                              text,
                              "if (format_changed || tb_changed) {\n"
                              "                DSO_HS8LaneCal_Reset(&s_engine_hs8_lane_cal);");
    rc |= assert_contains(path,
                          text,
                          "if (format_changed || hs8_topology_changed) {");
    rc |= assert_contains(path,
                          text,
                          "static bool engine_hs8_refine_low_freq_from_lane0");
    rc |= assert_contains(path,
                          text,
                          "DSO_ENGINE_HS8_LANE_BOOTSTRAP_MAX_FREQ_HZ");
    rc |= assert_contains(path,
                          text,
                          "#define DSO_ENGINE_HS8_CAL_MAX_FREQ_HZ");
    rc |= assert_contains(path,
                          text,
                          "#define DSO_ENGINE_HS8_LANE_BOOTSTRAP_MIN_SPC     3.0f");
    rc |= assert_contains(path,
                          text,
                          "(analysis->freq_hz > DSO_ENGINE_HS8_CAL_MAX_FREQ_HZ)");
    rc |= assert_contains(path,
                          text,
                          "DSO_ProcessAnalysis merged_analysis = {0};");
    rc |= assert_contains(path,
                          text,
                          "merged_analysis = *analysis;");
    rc |= assert_contains(path,
                          text,
                          "engine_hs8_build_cal_analysis");
    rc |= assert_contains(path,
                          text,
                          "engine_hs8_can_calibrate(lane_count, &cal_analysis)");
    rc |= assert_contains(path,
                          text,
                          "cal_analysis.freq_hz");
    rc |= assert_contains(path,
                          text,
                          "DSO_ENGINE_HS8_LANE_BOOTSTRAP_FREQ_TOL");
    rc |= assert_not_contains(path,
                              text,
                              "DSO_HS8LaneCal_IsReadyForLaneCount(&s_engine_hs8_lane_cal, (uint8_t)lane_count)) {\n"
                              "        return false;");
    rc |= assert_contains(path,
                          text,
                          "!hs8_topology_changed");
    rc |= assert_contains(path,
                          text,
                          "if (format_changed || hs8_topology_changed || ctrl_changed || (tb_changed && !gentle_tb))");
    rc |= assert_contains(path,
                          text,
                          "DSO_HS8LaneCal_Reset(&s_engine_hs8_lane_cal);");
    rc |= assert_not_contains(path,
                              text,
                              "} else {\n"
                              "                DSO_HS8LaneCal_ResetForLaneCount(&s_engine_hs8_lane_cal, lane_count);");
    rc |= assert_contains(path,
                          text,
                          "} else if ((s_engine_hs8_reentry_state != DSO_ENGINE_HS8_REENTRY_SETTLING) &&");
    rc |= assert_contains(path,
                          text,
                          "DSO_HS8LaneCal_ResetForLaneCount(&s_engine_hs8_lane_cal, lane_count);");
    rc |= assert_contains(path,
                          text,
                          "frame->ch1_interleave_cal_valid = engine_is_ch1_cal_ready();");

    free(text);

    if (rc == 0) {
        puts("all engine hs8 calibration policy tests passed");
    }

    return rc;
}
