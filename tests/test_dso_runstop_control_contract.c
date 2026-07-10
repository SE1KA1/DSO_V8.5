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

static int require_not_contains(const char *path, const char *text,
                                const char *needle, const char *label)
{
    if (strstr(text, needle) != NULL) {
        fprintf(stderr, "%s: forbidden %s: %s\n", path, label, needle);
        return 0;
    }
    return 1;
}

int main(void)
{
    char *ui = read_text_file("User/ui/UI_WaveformCtrl.c");
    char *trigger_ui = read_text_file("User/ui/ui_trigger_ui.c");
    char *ctrl_h = read_text_file("User/dso/dso_acquire_control.h");
    char *ctrl_c = read_text_file("User/dso/dso_acquire_control.c");
    char *engine = read_text_file("User/dso/dso_engine.c");
    int ok = 1;

    ok &= require_contains("User/ui/UI_WaveformCtrl.c",
                           ui,
                           "return DSO_AcquireCtrl_IsRunRequested();",
                           "UI status follows logical run request");
    ok &= require_contains("User/ui/UI_WaveformCtrl.c",
                           ui,
                           "if (DSO_AcquireCtrl_IsRunRequested()) {",
                           "UI toggle reads logical run request");
    ok &= require_contains("User/ui/UI_WaveformCtrl.c",
                           ui,
                           "DSO_AcquireCtrl_SetRunRequested(false);",
                           "UI STOP request routed through acquire control");
    ok &= require_contains("User/ui/UI_WaveformCtrl.c",
                           ui,
                           "DSO_AcquireCtrl_SetRunRequested(true);",
                           "UI RUN request routed through acquire control");
    ok &= require_not_contains("User/ui/UI_WaveformCtrl.c",
                               ui,
                               "DSO_Acq_Stop();",
                               "UI direct acquisition stop");
    ok &= require_not_contains("User/ui/UI_WaveformCtrl.c",
                               ui,
                               "DSO_Acq_Start();",
                               "UI direct acquisition start");

    ok &= require_contains("User/ui/ui_trigger_ui.c",
                           trigger_ui,
                           "DSO_AcquireCtrl_SetRunRequested(true);",
                           "trigger SINGLE rearm requests logical RUN");
    ok &= require_not_contains("User/ui/ui_trigger_ui.c",
                               trigger_ui,
                               "DSO_Acq_Start();",
                               "trigger UI direct acquisition start");

    ok &= require_contains("User/dso/dso_acquire_control.h",
                           ctrl_h,
                           "bool run_requested;",
                           "run request state field");
    ok &= require_contains("User/dso/dso_acquire_control.h",
                           ctrl_h,
                           "void DSO_AcquireCtrl_SetRunRequested(bool running);",
                           "run request setter declaration");
    ok &= require_contains("User/dso/dso_acquire_control.h",
                           ctrl_h,
                           "bool DSO_AcquireCtrl_IsRunRequested(void);",
                           "run request getter declaration");
    ok &= require_contains("User/dso/dso_acquire_control.c",
                           ctrl_c,
                           "bool pending_run_change;",
                           "separate run request pending bit");
    ok &= require_contains("User/dso/dso_acquire_control.c",
                           ctrl_c,
                           "info.run_requested = true;",
                           "default run requested");
    ok &= require_contains("User/dso/dso_engine.c",
                           engine,
                           "run_requested = DSO_AcquireCtrl_IsRunRequested();",
                           "engine consumes run request");

    free(ui);
    free(trigger_ui);
    free(ctrl_h);
    free(ctrl_c);
    free(engine);
    return ok ? 0 : 1;
}
