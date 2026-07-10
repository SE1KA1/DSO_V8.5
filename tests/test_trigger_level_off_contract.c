#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_substring(const char *haystack, const char *needle, const char *label)
{
    if (strstr(haystack, needle) == NULL) {
        fprintf(stderr, "missing %s: %s\n", label, needle);
        exit(1);
    }
}

static void load_file(const char *path, char *buf, size_t size)
{
    FILE *fp = fopen(path, "rb");
    size_t n;

    if (fp == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        exit(1);
    }

    n = fread(buf, 1u, size - 1u, fp);
    fclose(fp);
    buf[n] = '\0';
}

int main(void)
{
    char process_h[32768];
    char process_c[65536];
    char engine_c[262144];
    char main_c[262144];
    char waveform_c[65536];

    load_file("User/dso/dso_process.h", process_h, sizeof(process_h));
    load_file("User/dso/dso_process.c", process_c, sizeof(process_c));
    load_file("User/dso/dso_engine.c", engine_c, sizeof(engine_c));
    load_file("User/ui/MainTask.c", main_c, sizeof(main_c));
    load_file("User/ui/UI_WaveformCtrl.c", waveform_c, sizeof(waveform_c));

    require_substring(process_h, "bool level_enabled;", "trigger config level enable flag");
    require_substring(process_h, "trig_level_enabled;", "frame level enable flag");
    require_substring(process_c, "trigger->level_enabled", "process checks level enable flag");
    require_substring(process_c, "trig_level = avg;", "process falls back to adaptive level");
    require_substring(engine_c,
                      "trig_cfg.level_enabled = acq_info.trigger_level_enabled;",
                      "engine threads enable flag into trigger config");
    require_substring(engine_c,
                      "frame->trig_level_enabled = acq_info.trigger_level_enabled;",
                      "engine publishes frame level enable flag");
    require_substring(main_c,
                      "if (s_frame.trig_level_enabled == 0) {",
                      "trigger line hidden when level off");
    require_substring(waveform_c,
                      "DSO_AcquireCtrl_SetTriggerLevelEnabled(true);",
                      "waveform nudge re-enables manual level");
    return 0;
}
