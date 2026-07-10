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
    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(fp);
        exit(1);
    }
    length = ftell(fp);
    if (length < 0) {
        fprintf(stderr, "failed to tell %s\n", path);
        fclose(fp);
        exit(1);
    }
    rewind(fp);

    text = (char *)malloc((size_t)length + 1u);
    if (text == NULL) {
        fprintf(stderr, "failed to allocate %ld bytes for %s\n", length + 1, path);
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
        fprintf(stderr, "%s: missing \"%s\"\n", path, needle);
        return 0;
    }
    return 1;
}

static int require_order(const char *path,
                         const char *text,
                         const char *first,
                         const char *second)
{
    const char *first_pos = strstr(text, first);
    const char *second_pos = strstr(text, second);

    if (first_pos == NULL) {
        fprintf(stderr, "%s: missing ordered token \"%s\"\n", path, first);
        return 0;
    }
    if (second_pos == NULL) {
        fprintf(stderr, "%s: missing ordered token \"%s\"\n", path, second);
        return 0;
    }
    if (first_pos >= second_pos) {
        fprintf(stderr, "%s: \"%s\" must appear before \"%s\"\n",
                path, first, second);
        return 0;
    }
    return 1;
}

static int test_render_latch_cleans_physical_frame_before_commit(void)
{
    const char *path = "emWin/Config/LCDConf_Lin_Template.c";
    char *text = read_text_file(path);
    int ok = 1;

    ok &= require_contains(path, text, "static void LCDConf_RenderLatch_CleanPhysicalFrame");
    ok &= require_contains(path, text, "SCB_CleanDCache_by_Addr((uint32_t *)phys_addr");
    ok &= require_contains(path, text, "VRAM_PHYS_FRAME_BYTES");
    ok &= require_contains(path, text, "__DMB();");
    ok &= require_contains(path, text, "LCDConf_RenderLatch_CleanPhysicalFrame(phys_addr);");
    ok &= require_order(path, text,
                        "LCDConf_RenderLatch_CleanPhysicalFrame(phys_addr);",
                        "s_rl_prep_saved_cy = DWT->CYCCNT - t_prep_begin;");
    ok &= require_order(path, text,
                        "LCDConf_RenderLatch_CleanPhysicalFrame(phys_addr);",
                        "void LCDConf_RenderLatch_Commit(void)");

    free(text);
    return ok ? 0 : 1;
}

static int test_current_display_ab_switches_match_freeze_diagnostic(void)
{
    int ok = 1;
    char *main_text = read_text_file("User/main.c");
    char *stress_text = read_text_file("User/bsp/y_t_stress_ab.h");
    char *ui_text = read_text_file("User/ui/ui_app_config.h");

    ok &= require_contains("User/main.c", main_text,
                           "#define LCD_DIAGNOSTIC_MODE     0");
    ok &= require_contains("User/bsp/y_t_stress_ab.h", stress_text,
                           "#define Y_T_AB_DISABLE_DCACHE  0");
    ok &= require_contains("User/bsp/y_t_stress_ab.h", stress_text,
                           "#define Y_T_AB_DISABLE_DMA2D_FILL_COPY  0");
    ok &= require_contains("User/bsp/y_t_stress_ab.h", stress_text,
                           "#define Y_T_AB_WRAP_SCOPE_RENDER_WITH_GUI_LOCK  0");
    ok &= require_contains("User/bsp/y_t_stress_ab.h", stress_text,
                           "#define Y_T_AB_CLEAN_DCACHE_BEFORE_SCOPE_FRAME  0");
    ok &= require_contains("User/bsp/y_t_stress_ab.h", stress_text,
                           "#define Y_T_AB_CLEANINV_LOGICAL_VRAM_BEFORE_LATCH  0");
    ok &= require_contains("User/bsp/y_t_stress_ab.h", stress_text,
                           "#define Y_T_AB_NONCACHEABLE_DISPLAY_VRAM  0");
    ok &= require_contains("User/bsp/y_t_stress_ab.h", stress_text,
                           "#define Y_T_AB_FORCE_FULL_ROTATE_EVERY_FRAME  0");
    ok &= require_contains("User/bsp/y_t_stress_ab.h", stress_text,
                           "#define Y_T_AB_USE_LTDC_VBR_RELOAD  0");
    ok &= require_contains("User/bsp/y_t_stress_ab.h", stress_text,
                           "#define Y_T_AB_MIRROR_PHYSICAL_FRAMEBUFFERS  0");
    ok &= require_contains("User/bsp/y_t_stress_ab.h", stress_text,
                           "#define Y_T_AB_FREEZE_SCOPE_AFTER_FIRST_FRAME  0");
    ok &= require_contains("User/bsp/y_t_stress_ab.h", stress_text,
                           "#define Y_T_AB_FREEZE_MONITOR_PHYSICAL_FRAME  0");
    ok &= require_contains("User/bsp/y_t_stress_ab.h", stress_text,
                           "#define Y_T_AB_DISABLE_WAVEGEN_TASK  0");
    ok &= require_contains("User/bsp/y_t_stress_ab.h", stress_text,
                           "#define Y_T_AB_DISABLE_MAINTASK_PERIODIC_LOG  0");
    ok &= require_contains("User/bsp/y_t_stress_ab.h", stress_text,
                           "#define Y_T_AB_DISABLE_DSO_ENGINE_TASK  0");
    ok &= require_contains("User/bsp/y_t_stress_ab.h", stress_text,
                           "#define Y_T_AB_DISABLE_ACQ_START  0");
    ok &= require_contains("User/bsp/y_t_stress_ab.h", stress_text,
                           "#define Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE  0");
    ok &= require_contains("User/bsp/y_t_stress_ab.h", stress_text,
                           "#define Y_T_AB_DSO_ENGINE_ACQ_SERVICE_ONLY  0");
    ok &= require_contains("User/bsp/y_t_stress_ab.h", stress_text,
                           "#define Y_T_AB_DISABLE_ACQ_DMA_IRQ_AFTER_START  0");
    ok &= require_contains("User/bsp/y_t_stress_ab.h", stress_text,
                           "#define Y_T_AB_DISABLE_ACQ_DMA_HT_TC_IRQ_SOURCE  1");
    ok &= require_contains("User/ui/MainTask.c", read_text_file("User/ui/MainTask.c"),
                           "Y_T_AB_DISABLE_MAINTASK_PERIODIC_LOG == 0");
    ok &= require_contains("User/main.c", main_text,
                           "#include \"y_t_stress_ab.h\"");
    ok &= require_contains("User/main.c", main_text,
                           "#if DSO_WAVEGEN_ENABLE && (Y_T_AB_DISABLE_WAVEGEN_TASK == 0)");
    ok &= require_contains("User/main.c", main_text,
                           "#if (USE_DSO_ENGINE != 0) && (Y_T_AB_DISABLE_DSO_ENGINE_TASK == 0)");
    ok &= require_contains("User/dso/dso_acq.c", read_text_file("User/dso/dso_acq.c"),
                           "#if (Y_T_AB_DISABLE_ACQ_START == 0)");
    ok &= require_contains("User/dso/dso_acq_8bit.c", read_text_file("User/dso/dso_acq_8bit.c"),
                           "#if (Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE == 0)");
    ok &= require_contains("User/dso/dso_acq_10bit.c", read_text_file("User/dso/dso_acq_10bit.c"),
                           "#if (Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE == 0)");
    ok &= require_contains("User/dso/dso_engine.c", read_text_file("User/dso/dso_engine.c"),
                           "#if (Y_T_AB_DSO_ENGINE_ACQ_SERVICE_ONLY != 0)");
    ok &= require_contains("User/dso/dso_acq_8bit.c", read_text_file("User/dso/dso_acq_8bit.c"),
                           "#if (Y_T_AB_DISABLE_ACQ_DMA_IRQ_AFTER_START != 0)");
    ok &= require_contains("User/dso/dso_acq_10bit.c", read_text_file("User/dso/dso_acq_10bit.c"),
                           "#if (Y_T_AB_DISABLE_ACQ_DMA_IRQ_AFTER_START != 0)");
    ok &= require_contains("User/dso/dso_acq_8bit.c", read_text_file("User/dso/dso_acq_8bit.c"),
                           "#if (Y_T_AB_DISABLE_ACQ_DMA_HT_TC_IRQ_SOURCE != 0)");
    ok &= require_contains("User/dso/dso_acq_10bit.c", read_text_file("User/dso/dso_acq_10bit.c"),
                           "#if (Y_T_AB_DISABLE_ACQ_DMA_HT_TC_IRQ_SOURCE != 0)");
    ok &= require_contains("User/dso/dso_acq_8bit.c", read_text_file("User/dso/dso_acq_8bit.c"),
                           "__HAL_DMA_DISABLE_IT(&s_acq8.hdma_adc1, DMA_IT_HT | DMA_IT_TC);");
    ok &= require_contains("User/dso/dso_acq_10bit.c", read_text_file("User/dso/dso_acq_10bit.c"),
                           "__HAL_DMA_DISABLE_IT(&s_acq10.hdma_adc1, DMA_IT_HT | DMA_IT_TC);");
    ok &= require_contains("User/ui/ui_app_config.h", ui_text,
                           "#define MAINTASK_PLOTBUF_ROTATE_BODY_EVERY_FRAME  0");
    ok &= require_contains("User/ui/ui_app_config.h", ui_text,
                           "#define MAINTASK_AB_DISABLE_PLOTBUF_WAVEFORM  0");
    ok &= require_contains("User/ui/ui_app_config.h", ui_text,
                           "#define LCDCONF_RENDER_LATCH_YIELD_BEFORE_COMMIT  1");

    free(main_text);
    free(stress_text);
    free(ui_text);
    return ok ? 0 : 1;
}

static int test_scope_freeze_skips_render_after_first_scope_frame(void)
{
    const char *path = "User/ui/MainTask.c";
    char *text = read_text_file(path);
    int ok = 1;

    ok &= require_contains(path, text, "Y_T_AB_FREEZE_SCOPE_AFTER_FIRST_FRAME");
    ok &= require_contains(path, text, "static uint8_t s_scope_freeze_frame_done");
    ok &= require_contains(path, text, "MainTask_Scope_RenderFrame_FrozenIdle");
    ok &= require_contains(path, text, "s_scope_freeze_frame_done = 1U;");
    ok &= require_contains(path, text, "MainTask_ScopeFreeze_MonitorPhysicalFrame");
    ok &= require_contains(path, text, "LTDC_Layer1->CFBAR");
    ok &= require_contains(path, text, "SCB_InvalidateDCache_by_Addr((uint32_t *)phys_addr");
    ok &= require_contains(path, text, "VRAM_PHYS_FRAME_BYTES");
    ok &= require_contains(path, text, "[LCD-FREEZE]");
    ok &= require_order(path, text,
                        "if (s_scope_freeze_frame_done != 0U)",
                        "MainTask_Scope_RenderFrame();");
    ok &= require_order(path, text,
                        "if (s_scope_freeze_frame_done != 0U)",
                        "MainTask_Scope_RenderFrame_FrozenIdle();");

    free(text);
    return ok ? 0 : 1;
}

int main(void)
{
    if (test_render_latch_cleans_physical_frame_before_commit() != 0) {
        return 1;
    }
    if (test_current_display_ab_switches_match_freeze_diagnostic() != 0) {
        return 1;
    }
    if (test_scope_freeze_skips_render_after_first_scope_frame() != 0) {
        return 1;
    }

    printf("all render latch cache contract tests passed\n");
    return 0;
}
