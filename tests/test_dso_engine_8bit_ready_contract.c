#include <stdio.h>
#include <string.h>

static int require_contains(const char *path, const char *text, const char *needle)
{
    if (strstr(text, needle) == NULL) {
        fprintf(stderr, "%s missing: %s\n", path, needle);
        return 1;
    }
    return 0;
}

static int require_not_contains(const char *path, const char *text, const char *needle)
{
    if (strstr(text, needle) != NULL) {
        fprintf(stderr, "%s still contains obsolete text: %s\n", path, needle);
        return 1;
    }
    return 0;
}

static int require_order(const char *path,
                         const char *text,
                         const char *first,
                         const char *second)
{
    const char *p_first = strstr(text, first);
    const char *p_second = strstr(text, second);

    if ((p_first == NULL) || (p_second == NULL)) {
        fprintf(stderr, "%s cannot check order: %s before %s\n", path, first, second);
        return 1;
    }
    if (p_first >= p_second) {
        fprintf(stderr, "%s order violation: %s must appear before %s\n", path, first, second);
        return 1;
    }
    return 0;
}

static char *read_text_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    long length;
    char *text;

    if (fp == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    text = (char *)malloc((size_t)length + 1u);
    if (text == NULL) {
        fprintf(stderr, "malloc failed for %s\n", path);
        fclose(fp);
        return NULL;
    }
    if (fread(text, 1u, (size_t)length, fp) != (size_t)length) {
        fprintf(stderr, "failed to read %s\n", path);
        free(text);
        fclose(fp);
        return NULL;
    }
    text[length] = '\0';
    fclose(fp);
    return text;
}

int main(void)
{
    int rc = 0;
    const char *engine_path = "User/dso/dso_engine.c";
    const char *roll_path = "User/dso/dso_engine_roll.c";
    char *engine_text = read_text_file(engine_path);
    char *roll_text = read_text_file(roll_path);

    if (engine_text == NULL) {
        return 1;
    }
    if (roll_text == NULL) {
        free(engine_text);
        return 1;
    }

    rc |= require_contains(engine_path, engine_text, "bool sample_restart_hold_pending;");
    rc |= require_contains(engine_path, engine_text, "static void engine_log_acq_hold");
    rc |= require_contains(engine_path, engine_text, "reason=%s");
    rc |= require_contains(engine_path, engine_text, "static void engine_hold_until_acq_ready");
    rc |= require_contains(engine_path, engine_text, "sample_restart_hold_pending = true;");
    rc |= require_contains(engine_path, engine_text, "if (sample_restart_hold_pending)");
    rc |= require_contains(engine_path, engine_text, "!DSO_Acq_IsDataReady()");
    rc |= require_contains(engine_path, engine_text, "\"sample_restart\"");
    /* RollHistoryRing_Clear moved to dso_engine_roll.c (in DSO_EngineRoll_RingFeedReset) */
    rc |= require_contains(roll_path, roll_text, "RollHistoryRing_Clear();");
    rc |= require_contains(engine_path, engine_text, "DSO_Timebase_ResetRollColumnState();");
    rc |= require_contains(engine_path, engine_text, "*roll_prev_pos = pos;");
    rc |= require_contains(engine_path, engine_text, "*ingest_prev_pos = pos;");
    rc |= require_order(engine_path,
                        engine_text,
                        "if (sample_restart_hold_pending)",
                        "DSO_EngineRoll_RingFeed(pos,");
    rc |= require_not_contains(engine_path, engine_text, "sample_restart_roll_guard_pending");
    rc |= require_not_contains(engine_path, engine_text, "DSO_ENGINE_ROLL_RESTART_MIN_VALID_PX");
    rc |= require_not_contains(engine_path, engine_text, "DSO_Timebase_GetRollColumnEnvelopeActive");
    rc |= require_not_contains(engine_path, engine_text, "bool hs8_restart_hold_pending;");
    rc |= require_not_contains(engine_path, engine_text, "\"hs8_restart\"");
    rc |= require_not_contains(engine_path, engine_text, "((frame_id - last_emit_frame) < 8u)");

    free(engine_text);
    free(roll_text);

    if (rc == 0) {
        puts("all engine sample-restart readiness contract tests passed");
    }
    return rc;
}
