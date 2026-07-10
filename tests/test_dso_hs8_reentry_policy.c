#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_text_file(const char *path)
{
    FILE *fp;
    long size;
    char *buf;

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
    return buf;
}

static int require_contains(const char *path, const char *text, const char *needle)
{
    if (strstr(text, needle) == NULL) {
        fprintf(stderr, "missing required HS8 reentry policy in %s: %s\n", path, needle);
        return 1;
    }
    return 0;
}

static int require_not_contains(const char *path, const char *text, const char *needle)
{
    if (strstr(text, needle) != NULL) {
        fprintf(stderr, "stale HS8 reentry policy remains in %s: %s\n", path, needle);
        return 1;
    }
    return 0;
}

int main(void)
{
    const char *engine_path = "User/dso/dso_engine_cal.c";
    const char *tb_c_path = "User/dso/dso_timebase.c";
    const char *tb_h_path = "User/dso/dso_timebase.h";
    char *engine = read_text_file(engine_path);
    char *tb_c = read_text_file(tb_c_path);
    char *tb_h = read_text_file(tb_h_path);
    int rc = 0;

    if ((engine == NULL) || (tb_c == NULL) || (tb_h == NULL)) {
        fprintf(stderr, "failed to read HS8 reentry policy sources\n");
        free(engine);
        free(tb_c);
        free(tb_h);
        return 1;
    }

    rc |= require_contains(engine_path, engine, "DSO_ENGINE_HS8_REENTRY_SETTLING");
    rc |= require_contains(engine_path, engine, "DSO_ENGINE_HS8_REENTRY_CALIBRATING");
    rc |= require_contains(engine_path, engine, "engine_hs8_reentry_begin");
    rc |= require_contains(engine_path, engine, "engine_hs8_reentry_note_ready");
    rc |= require_contains(engine_path, engine, "DSO_Timebase_ResetHs8ReconstructionForChannel(0u);");
    rc |= require_contains(engine_path, engine, "*out_analysis = analysis_for_cal;");
    rc |= require_contains(engine_path, engine, "engine_hs8_build_cal_analysis");
    rc |= require_contains(engine_path, engine, "engine_hs8_measure_lane0_bootstrap");
    rc |= require_contains(engine_path, engine, "engine_hs8_apply_lane0_bootstrap(&analysis_for_cal");
    rc |= require_contains(engine_path, engine, "analysis->clipped = lane_analysis->clipped;");
    rc |= require_contains(engine_path, engine, "frame->ch1_interleave_cal_valid = engine_is_ch1_cal_ready();");
    rc |= require_not_contains(engine_path, engine, "frame->ch1_interleave_cal_valid = false;\n        frame->cal_state = 0u;");

    rc |= require_contains(tb_h_path, tb_h, "void DSO_Timebase_ResetHs8ReconstructionForChannel(uint8_t channel);");
    rc |= require_contains(tb_c_path, tb_c, "void DSO_Timebase_ResetHs8ReconstructionForChannel(uint8_t channel)");
    rc |= require_contains(tb_c_path, tb_c, "DSO_HS8Render_Reset(&s_tb.hs8[idx]);");
    rc |= require_contains(tb_c_path, tb_c, "ets_reset_channel(idx);");

    free(engine);
    free(tb_c);
    free(tb_h);

    if (rc == 0) {
        puts("all HS8 reentry policy tests passed");
    }
    return rc;
}
