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
        fprintf(stderr, "failed to open %s\n", path);
        exit(1);
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(fp);
        exit(1);
    }
    size = ftell(fp);
    if (size < 0) {
        fprintf(stderr, "failed to tell %s\n", path);
        fclose(fp);
        exit(1);
    }
    rewind(fp);

    buf = (char *)malloc((size_t)size + 1u);
    if (buf == NULL) {
        fprintf(stderr, "failed to allocate %ld bytes\n", size + 1);
        fclose(fp);
        exit(1);
    }
    if (fread(buf, 1u, (size_t)size, fp) != (size_t)size) {
        fprintf(stderr, "failed to read %s\n", path);
        free(buf);
        fclose(fp);
        exit(1);
    }
    buf[size] = '\0';
    fclose(fp);
    return buf;
}

static int require_contains(const char *path, const char *text,
                            const char *needle, const char *label)
{
    if (strstr(text, needle) == NULL) {
        fprintf(stderr, "%s: missing %s: %s\n", path, label, needle);
        return 0;
    }
    return 1;
}

int main(void)
{
    const char *path = "User/dso/dso_engine.c";
    char *text = read_text_file(path);
    int ok = 1;

    ok &= require_contains(path,
                           text,
                           "bool prev_run_requested;",
                           "logical RUN edge state");
    ok &= require_contains(path,
                           text,
                           "prev_run_requested = DSO_AcquireCtrl_IsRunRequested();",
                           "initial logical RUN edge state");
    ok &= require_contains(path,
                           text,
                           "if (run_requested && !prev_run_requested) {",
                           "logical RUN edge detection");
    ok &= require_contains(path,
                           text,
                           "if (!run_requested) {\n"
                           "            prev_run_requested = run_requested;\n"
                           "            vTaskDelay(pdMS_TO_TICKS(DSO_ENGINE_SERVICE_INTERVAL_MS));\n"
                           "            continue;\n"
                           "        }",
                           "logical STOP freezes frame publishing");
    ok &= require_contains(path,
                           text,
                           "if (DSO_Record_IsFrozen()) {\n"
                           "                DSO_Record_Reset((DSO_RecordDepth)acq_info.record_depth_index,\n"
                           "                                 acq_info.pretrigger_pct,\n"
                           "                                 sample_rate);\n"
                           "            } else {\n"
                           "                DSO_Record_ClearTrigger();\n"
                           "            }",
                           "record lifecycle on logical RUN edge");
    ok &= require_contains(path,
                           text,
                           "ingest_prev_pos = pos;\n"
                           "            ingest_prev_pos_ch2 = pos_ch2;\n"
                           "            roll_prev_pos = pos;\n"
                           "            roll_prev_pos_ch2 = pos_ch2;",
                           "engine cursor resync on logical RUN edge");
    ok &= require_contains(path,
                           text,
                           "if (run_requested && !DSO_Acq_IsRunning()) {",
                           "engine-owned acquisition start");
    ok &= require_contains(path,
                           text,
                           "DSO_AcquireCtrl_SetRunRequested(false);",
                           "SINGLE frozen state clears logical RUN request");

    free(text);
    return ok ? 0 : 1;
}
