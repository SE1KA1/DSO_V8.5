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
    char ui_h_buf[32768];
    char ui_c_buf[65536];
    char main_buf[131072];

    load_file("User/ui/ui_trigger_ui.h", ui_h_buf, sizeof(ui_h_buf));
    load_file("User/ui/ui_trigger_ui.c", ui_c_buf, sizeof(ui_c_buf));
    load_file("User/ui/MainTask.c", main_buf, sizeof(main_buf));

    require_substring(ui_h_buf,
                      "void UI_Trigger_SyncFrameState(const DSO_Frame *frame);",
                      "frame sync declaration");
    require_substring(ui_c_buf,
                      "UI_TrigUi_SetSourceFromDso(UI_TRIG_SRC_CH2, 0);",
                      "screen entry sync for ch2");
    require_substring(ui_c_buf,
                      "void UI_Trigger_SyncFrameState(const DSO_Frame *frame)",
                      "frame sync implementation");
    require_substring(main_buf,
                      "UI_Trigger_SyncFrameState(&s_frame);",
                      "main task frame sync call");
    return 0;
}
