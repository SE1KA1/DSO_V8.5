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
    char *engine = read_file("User/dso/dso_engine.c");
    char *main_task = read_file("User/ui/MainTask.c");
    char *uvproj = read_file("Project/MDK-ARM(uV5)/project.uvprojx");

    require_text(engine, "DSO_FreqMeter_Init();");
    require_text(engine, "DSO_FreqMeter_Start();");
    require_text(engine, "frame.ext_freq_valid");
    require_text(engine, "frame.ext_freq_millihz");
    require_text(engine, "analysis.freq_hz");
    reject_text(engine, "frame.freq_hz = meter.freq_hz;");

    reject_text(main_task, "DSO_FreqMeter_Init();");
    reject_text(main_task, "DSO_FreqMeter_Start();");
    require_text(main_task, "#include \"ui_freq_format.h\"");
    require_text(main_task, "UI_FreqFormat_ExtMilliHz");
    require_text(main_task, "UI_FreqFormat_ExtMilliHzCompact");
    require_text(main_task, "UI_FreqFormat_FoldMilliHz");
    require_text(main_task, "hdr_meas_disp_mode_cal_ext");
    require_text(main_task, "\"   EXT:\"");
    reject_text(main_task, "if (m.freq_valid && m.freq_hz > 0.0f)");
    reject_text(main_task, "static void FormatExtFreq");
    reject_text(main_task, "s_frame.ext_freq_millihz & 0xFFFFu");

    require_text(uvproj, "ui_freq_format.c");
    require_text(uvproj, "..\\..\\User\\ui\\ui_freq_format.c");

    free(engine);
    free(main_task);
    free(uvproj);
    return 0;
}
