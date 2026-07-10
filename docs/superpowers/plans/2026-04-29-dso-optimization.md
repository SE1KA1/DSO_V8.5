# DSO Optimization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Improve numeric trust, SDRAM ownership, 10-bit readiness diagnostics, and architecture documentation while preserving the frozen 8-bit path and the existing external frequency meter behavior.

**Architecture:** Add frame-local ADC scale metadata and make UI amplitude formatting consume that metadata. Move SDRAM record and roll-history ownership into one checked layout contract. Tighten 10-bit readiness with an explicit single path that reports not-ready state instead of falling back. Keep PB15/TIM12 frequency measurement unchanged.

**Tech Stack:** STM32H743 HAL, FreeRTOS, emWin UI, Keil MDK uVision project, host-side C regression tests built with `/mnt/e/mingw64/bin/gcc.exe`.

---

## Scope And Guardrails

- Do not modify `User/dso/dso_acq_8bit.c`, `User/dso/dso_hs8_lane_cal.c`, or 8-bit enhanced-render behavior except for documentation or tests that describe existing behavior.
- Do not change PB15/TIM12 external frequency meter math, hardware setup, engine snapshot fields, or UI frequency formatter.
- Do not add fallback paths. Invalid state must be represented as explicit metadata, diagnostics, or a failed readiness condition.
- Do not redesign the UI layout. This plan only changes the units used for amplitude values already displayed.
- Keep changes in `User/`, `tests/`, and `docs/architecture/`; vendor directories remain untouched.

## File Map

**Phase 1: amplitude metadata and UI conversion**

- Modify: `User/dso/dso_process.h`
- Modify: `User/dso/dso_engine.c`
- Create: `User/ui/ui_measure_units.h`
- Modify: `User/ui/UI_WaveformCtrl.c`
- Modify: `User/ui/ui_screens.c`
- Create: `tests/test_dso_ui_amplitude_units.c`
- Create: `tests/test_dso_frame_adc_metadata_contract.c`

**Phase 2: SDRAM layout contract**

- Modify: `User/bsp/inc/bsp_sdram_layout.h`
- Modify: `User/dso/roll_history_mem.c`
- Modify: `User/dso/dso_record.c`
- Create: `tests/test_dso_sdram_layout.c`
- Modify: `docs/architecture/06-memory-bsp-build-test.md`

**Phase 3: 10-bit readiness**

- Modify: `User/dso/dso_acq_10bit.c`
- Modify: `tests/test_dso_acq_ready_contract.c`
- Create: `tests/test_dso_acq10_ready_gate.c`

**Phase 4: architecture cleanup**

- Modify: `User/dso/dso_config.h`
- Modify: `User/dso/dso_acq_model.h`
- Modify: `docs/architecture/03-acquisition-and-sample-modes.md`
- Modify: `docs/architecture/06-memory-bsp-build-test.md`
- Modify: `docs/architecture/07-maintenance-boundaries-and-risks.md`
- Modify: `Doc/波形绘制方案说明.md`
- Create: `tests/test_dso_architecture_cleanup_contract.c`

## Build Variables

Use this compiler in command examples:

```bash
CC=/mnt/e/mingw64/bin/gcc.exe
```

If that compiler path is unavailable in the execution environment, use the existing checked-in `.exe` runners for unchanged tests and use the local MinGW or MSVC compiler that has previously produced the repository's host-test executables.

### Task 1: Add Frame-Local ADC Scale Metadata

**Files:**
- Modify: `User/dso/dso_process.h`
- Modify: `User/dso/dso_engine.c`
- Create: `User/ui/ui_measure_units.h`
- Modify: `User/ui/UI_WaveformCtrl.c`
- Modify: `User/ui/ui_screens.c`
- Create: `tests/test_dso_ui_amplitude_units.c`
- Create: `tests/test_dso_frame_adc_metadata_contract.c`

- [ ] **Step 1: Write the pure amplitude conversion test**

Create `tests/test_dso_ui_amplitude_units.c`:

```c
#include <math.h>
#include <stdio.h>

#include "dso/dso_process.h"
#include "ui/ui_measure_units.h"

static int expect_near(const char *name, float actual, float expected)
{
    float diff = fabsf(actual - expected);

    if (diff > 0.0005f) {
        fprintf(stderr, "%s: expected %.6f, got %.6f\n", name, (double)expected, (double)actual);
        return 1;
    }
    return 0;
}

static int test_8bit_delta_uses_255_full_scale(void)
{
    DSO_Frame frame = {0};

    frame.adc_bits = 8u;
    frame.adc_max = 255u;
    frame.adc_midscale = 128u;
    frame.adc_lsb_v = 3.3f / 255.0f;

    return expect_near("8-bit 128-code delta",
                       UI_Measure_CodeDeltaToVolts(128.0f, &frame),
                       128.0f * 3.3f / 255.0f);
}

static int test_10bit_delta_uses_1023_full_scale(void)
{
    DSO_Frame frame = {0};

    frame.adc_bits = 10u;
    frame.adc_max = 1023u;
    frame.adc_midscale = 512u;
    frame.adc_lsb_v = 3.3f / 1023.0f;

    return expect_near("10-bit 512-code delta",
                       UI_Measure_CodeDeltaToVolts(512.0f, &frame),
                       512.0f * 3.3f / 1023.0f);
}

static int test_missing_frame_metadata_is_explicit_zero(void)
{
    DSO_Frame frame = {0};

    return expect_near("missing metadata",
                       UI_Measure_CodeDeltaToVolts(100.0f, &frame),
                       0.0f);
}

int main(void)
{
    int rc = 0;

    rc |= test_8bit_delta_uses_255_full_scale();
    rc |= test_10bit_delta_uses_1023_full_scale();
    rc |= test_missing_frame_metadata_is_explicit_zero();

    if (rc == 0) {
        puts("all UI amplitude unit tests passed");
    }
    return rc;
}
```

- [ ] **Step 2: Write the frame metadata text contract test**

Create `tests/test_dso_frame_adc_metadata_contract.c`:

```c
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
    fseek(fp, 0, SEEK_END);
    length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    text = (char *)malloc((size_t)length + 1u);
    if (text == NULL) {
        fprintf(stderr, "malloc failed for %s\n", path);
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
        fprintf(stderr, "%s missing: %s\n", path, needle);
        return 1;
    }
    return 0;
}

static int reject_contains(const char *path, const char *text, const char *needle)
{
    if (strstr(text, needle) != NULL) {
        fprintf(stderr, "%s forbidden text still present: %s\n", path, needle);
        return 1;
    }
    return 0;
}

int main(void)
{
    char *process_h = read_text_file("User/dso/dso_process.h");
    char *engine_c = read_text_file("User/dso/dso_engine.c");
    char *waveform_c = read_text_file("User/ui/UI_WaveformCtrl.c");
    char *screens_c = read_text_file("User/ui/ui_screens.c");
    int rc = 0;

    rc |= require_contains("User/dso/dso_process.h", process_h, "uint8_t  sample_format;");
    rc |= require_contains("User/dso/dso_process.h", process_h, "uint8_t  adc_bits;");
    rc |= require_contains("User/dso/dso_process.h", process_h, "uint16_t adc_max;");
    rc |= require_contains("User/dso/dso_process.h", process_h, "uint16_t adc_midscale;");
    rc |= require_contains("User/dso/dso_process.h", process_h, "float    adc_lsb_v;");

    rc |= require_contains("User/dso/dso_engine.c", engine_c, "const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();");
    rc |= require_contains("User/dso/dso_engine.c", engine_c, "frame->sample_format = (uint8_t)DSO_SampleSpec_GetActiveFormat();");
    rc |= require_contains("User/dso/dso_engine.c", engine_c, "frame->adc_bits = spec->adc_bits;");
    rc |= require_contains("User/dso/dso_engine.c", engine_c, "frame->adc_max = spec->adc_max;");
    rc |= require_contains("User/dso/dso_engine.c", engine_c, "frame->adc_midscale = spec->midscale;");
    rc |= require_contains("User/dso/dso_engine.c", engine_c, "frame->adc_lsb_v = spec->vref / (float)spec->adc_max;");

    rc |= require_contains("User/ui/UI_WaveformCtrl.c", waveform_c, "#include \"ui_measure_units.h\"");
    rc |= require_contains("User/ui/UI_WaveformCtrl.c", waveform_c, "UI_Measure_CodeDeltaToVolts");
    rc |= reject_contains("User/ui/UI_WaveformCtrl.c", waveform_c, "DSO_VREF / (float)DSO_ADC_MAX");

    rc |= require_contains("User/ui/ui_screens.c", screens_c, "#include \"ui_measure_units.h\"");
    rc |= require_contains("User/ui/ui_screens.c", screens_c, "UI_Measure_CodeDeltaToVolts");
    rc |= reject_contains("User/ui/ui_screens.c", screens_c, "DSO_VREF / (float)DSO_ADC_MAX");

    free(process_h);
    free(engine_c);
    free(waveform_c);
    free(screens_c);
    return rc;
}
```

- [ ] **Step 3: Run the new tests and verify the red state**

Run:

```bash
$CC -I. -IUser tests/test_dso_ui_amplitude_units.c -lm -o tests/test_dso_ui_amplitude_units.exe
```

Expected: compile fails because `User/ui/ui_measure_units.h` does not exist yet.

Run:

```bash
$CC -I. -IUser tests/test_dso_frame_adc_metadata_contract.c -o tests/test_dso_frame_adc_metadata_contract.exe
./tests/test_dso_frame_adc_metadata_contract.exe
```

Expected: runtime failure reporting missing frame metadata fields.

- [ ] **Step 4: Add ADC metadata to `DSO_Frame`**

In `User/dso/dso_process.h`, add the metadata immediately before `sample_rate`:

```c
    uint8_t  sample_format;
    uint8_t  adc_bits;
    uint16_t adc_max;
    uint16_t adc_midscale;
    float    adc_lsb_v;
    uint32_t sample_rate;
```

- [ ] **Step 5: Fill ADC metadata in the engine**

In `User/dso/dso_engine.c`, add the active sample spec at the top of `fill_frame_metadata()` after the local variable declarations:

```c
    const DSO_SampleSpec *spec = DSO_SampleSpec_GetActive();
```

Then assign the frame fields before the timebase metadata:

```c
    frame->sample_format = (uint8_t)DSO_SampleSpec_GetActiveFormat();
    frame->adc_bits = spec->adc_bits;
    frame->adc_max = spec->adc_max;
    frame->adc_midscale = spec->midscale;
    frame->adc_lsb_v = spec->vref / (float)spec->adc_max;
```

- [ ] **Step 6: Add the UI measurement unit helper**

Create `User/ui/ui_measure_units.h`:

```c
#ifndef UI_MEASURE_UNITS_H
#define UI_MEASURE_UNITS_H

#include "dso/dso_process.h"

static inline float UI_Measure_FrameAdcLsbV(const DSO_Frame *frame)
{
    if ((frame == NULL) || (frame->adc_lsb_v <= 0.0f)) {
        return 0.0f;
    }
    return frame->adc_lsb_v;
}

static inline float UI_Measure_CodeDeltaToVolts(float code_delta, const DSO_Frame *frame)
{
    return code_delta * UI_Measure_FrameAdcLsbV(frame);
}

#endif /* UI_MEASURE_UNITS_H */
```

- [ ] **Step 7: Update scope header amplitude conversion**

In `User/ui/UI_WaveformCtrl.c`, include the helper:

```c
#include "ui_measure_units.h"
```

Replace the body of `UI_Waveform_GetMeasurements()` with frame-local conversion:

```c
void UI_Waveform_GetMeasurements(UI_Waveform_Meas_t *m)
{
    if (m == NULL) {
        return;
    }

    m->freq_hz   = s_display_frame.freq_hz;
    m->freq_valid = s_display_frame.freq_valid;
    m->vpp_v     = UI_Measure_CodeDeltaToVolts((float)s_display_frame.vpp, &s_display_frame);
    m->vrms_v    = UI_Measure_CodeDeltaToVolts(s_display_frame.vrms, &s_display_frame);
    m->duty      = s_display_frame.duty_cycle;
    m->triggered = s_display_frame.triggered;
}
```

Replace the CH2 conversion lines in `UI_Waveform_GetMeasurementsCh2()`:

```c
    m->vpp_v      = UI_Measure_CodeDeltaToVolts((float)s_display_frame.ch2_vpp, &s_display_frame);
    m->vrms_v     = UI_Measure_CodeDeltaToVolts(s_display_frame.ch2_vrms, &s_display_frame);
```

- [ ] **Step 8: Update measurement screen amplitude conversion**

In `User/ui/ui_screens.c`, include the helper:

```c
#include "ui_measure_units.h"
```

Remove the local `float adc_to_v;` and its assignment in `screen_measure()`.

Replace CH1 voltage formatting:

```c
    fmt_volts_mag(UI_Measure_CodeDeltaToVolts((float)fr.vpp, &fr), v1[2], (unsigned)sizeof(v1[2]));
    fmt_volts_signed(UI_Measure_CodeDeltaToVolts((float)fr.vmax, &fr), v1[3], (unsigned)sizeof(v1[3]));
    fmt_volts_signed(UI_Measure_CodeDeltaToVolts((float)fr.vmin, &fr), v1[4], (unsigned)sizeof(v1[4]));
```

Replace CH2 voltage formatting:

```c
        fmt_volts_mag(UI_Measure_CodeDeltaToVolts((float)fr.ch2_vpp, &fr), v2[2], (unsigned)sizeof(v2[2]));
        fmt_volts_signed(UI_Measure_CodeDeltaToVolts((float)fr.ch2_vmax, &fr), v2[3], (unsigned)sizeof(v2[3]));
        fmt_volts_signed(UI_Measure_CodeDeltaToVolts((float)fr.ch2_vmin, &fr), v2[4], (unsigned)sizeof(v2[4]));
```

- [ ] **Step 9: Run focused amplitude tests and existing frequency UI test**

Run:

```bash
$CC -I. -IUser tests/test_dso_ui_amplitude_units.c -lm -o tests/test_dso_ui_amplitude_units.exe
./tests/test_dso_ui_amplitude_units.exe
$CC -I. -IUser tests/test_dso_frame_adc_metadata_contract.c -o tests/test_dso_frame_adc_metadata_contract.exe
./tests/test_dso_frame_adc_metadata_contract.exe
./tests/test_dso_freq_ui_format.exe
./tests/test_dso_freq_meter_engine_contract.exe
```

Expected: all commands pass. The frequency tests passing confirms this task did not alter the external frequency formatter or engine handoff.

- [ ] **Step 10: Commit Task 1**

Run:

```bash
git add User/dso/dso_process.h User/dso/dso_engine.c User/ui/ui_measure_units.h User/ui/UI_WaveformCtrl.c User/ui/ui_screens.c tests/test_dso_ui_amplitude_units.c tests/test_dso_ui_amplitude_units.exe tests/test_dso_frame_adc_metadata_contract.c tests/test_dso_frame_adc_metadata_contract.exe
git commit -m "ui: use frame adc scale for amplitude measurements"
```

### Task 2: Make SDRAM Ownership Explicit

**Files:**
- Modify: `User/bsp/inc/bsp_sdram_layout.h`
- Modify: `User/dso/roll_history_mem.c`
- Modify: `User/dso/dso_record.c`
- Create: `tests/test_dso_sdram_layout.c`
- Modify: `docs/architecture/06-memory-bsp-build-test.md`

- [ ] **Step 1: Write the SDRAM non-overlap test**

Create `tests/test_dso_sdram_layout.c`:

```c
#include <stdint.h>
#include <stdio.h>

#include "bsp_sdram_layout.h"
#include "bsp_vram_layout.h"

static int require_eq_u32(const char *name, uint32_t actual, uint32_t expected)
{
    if (actual != expected) {
        fprintf(stderr, "%s: expected 0x%08lX, got 0x%08lX\n",
                name,
                (unsigned long)expected,
                (unsigned long)actual);
        return 1;
    }
    return 0;
}

static int require_order(const char *name, uint32_t low_end, uint32_t high_start)
{
    if (low_end > high_start) {
        fprintf(stderr, "%s overlap: low_end=0x%08lX high_start=0x%08lX\n",
                name,
                (unsigned long)low_end,
                (unsigned long)high_start);
        return 1;
    }
    return 0;
}

int main(void)
{
    int rc = 0;

    rc |= require_eq_u32("sdram end",
                         SDRAM_TOTAL_END,
                         (uint32_t)EXT_SDRAM_ADDR + (uint32_t)EXT_SDRAM_SIZE);
    rc |= require_eq_u32("gui heap start", SDRAM_GUI_HEAP_BUF, SDRAM_APP_BUF);
    rc |= require_eq_u32("record start", SDRAM_RECORD_BUF, SDRAM_GUI_HEAP_END);
    rc |= require_eq_u32("record end", SDRAM_RECORD_END, SDRAM_ROLL_HISTORY_BUF);
    rc |= require_eq_u32("roll end", SDRAM_ROLL_HISTORY_END, SDRAM_TOTAL_END);

    rc |= require_order("logical vram before app", VRAM_LOGICAL_ADDR + VRAM_LOGICAL_SUBFRAME_BYTES, SDRAM_APP_BUF);
    rc |= require_order("phys0 before phys1", VRAM_PHYSICAL_0_ADDR + VRAM_PHYS_FRAME_BYTES, VRAM_PHYSICAL_1_ADDR);
    rc |= require_order("phys1 before app", VRAM_PHYSICAL_1_ADDR + VRAM_PHYS_FRAME_BYTES, SDRAM_APP_BUF);
    rc |= require_order("gui heap before record", SDRAM_GUI_HEAP_END, SDRAM_RECORD_BUF);
    rc |= require_order("record before roll history", SDRAM_RECORD_END, SDRAM_ROLL_HISTORY_BUF);
    rc |= require_order("roll history within sdram", SDRAM_ROLL_HISTORY_END, SDRAM_TOTAL_END);

    if (SDRAM_RECORD_SIZE == 0u) {
        fprintf(stderr, "record size must be nonzero\n");
        rc = 1;
    }
    if (SDRAM_ROLL_HISTORY_SIZE != (3u * 1024u * 1024u)) {
        fprintf(stderr, "unexpected roll history size: %lu\n", (unsigned long)SDRAM_ROLL_HISTORY_SIZE);
        rc = 1;
    }

    if (rc == 0) {
        puts("all SDRAM layout tests passed");
    }
    return rc;
}
```

- [ ] **Step 2: Run the SDRAM layout test and verify the red state**

Run:

```bash
$CC -I. -IUser/bsp/inc tests/test_dso_sdram_layout.c -o tests/test_dso_sdram_layout.exe
./tests/test_dso_sdram_layout.exe
```

Expected: compile failure because `SDRAM_TOTAL_END`, `SDRAM_GUI_HEAP_BUF`, `SDRAM_GUI_HEAP_END`, `SDRAM_RECORD_END`, `SDRAM_ROLL_HISTORY_BUF`, `SDRAM_ROLL_HISTORY_SIZE`, and `SDRAM_ROLL_HISTORY_END` are not defined yet.

- [ ] **Step 3: Replace the SDRAM layout macros with explicit regions**

Replace `User/bsp/inc/bsp_sdram_layout.h` contents after includes with:

```c
#define SDRAM_TOTAL_BEGIN        ((uint32_t)EXT_SDRAM_ADDR)
#define SDRAM_TOTAL_SIZE         ((uint32_t)EXT_SDRAM_SIZE)
#define SDRAM_TOTAL_END          (SDRAM_TOTAL_BEGIN + SDRAM_TOTAL_SIZE)

#define SDRAM_GUI_HEAP_BUF       ((uint32_t)SDRAM_APP_BUF)
#define SDRAM_GUI_HEAP_RESERVE   (6u * 1024u * 1024u)
#define SDRAM_GUI_HEAP_END       (SDRAM_GUI_HEAP_BUF + SDRAM_GUI_HEAP_RESERVE)

#define SDRAM_ROLL_HISTORY_SIZE  (3u * 1024u * 1024u)
#define SDRAM_ROLL_HISTORY_END   SDRAM_TOTAL_END
#define SDRAM_ROLL_HISTORY_BUF   (SDRAM_ROLL_HISTORY_END - SDRAM_ROLL_HISTORY_SIZE)

#define SDRAM_RECORD_BUF         SDRAM_GUI_HEAP_END
#define SDRAM_RECORD_END         SDRAM_ROLL_HISTORY_BUF
#define SDRAM_RECORD_SIZE        (SDRAM_RECORD_END - SDRAM_RECORD_BUF)

#define BSP_SDRAM_STATIC_ASSERT(name, expr) typedef char bsp_sdram_static_assert_##name[(expr) ? 1 : -1]

BSP_SDRAM_STATIC_ASSERT(gui_heap_after_app_buf, SDRAM_GUI_HEAP_BUF == (uint32_t)SDRAM_APP_BUF);
BSP_SDRAM_STATIC_ASSERT(record_after_gui_heap, SDRAM_RECORD_BUF == SDRAM_GUI_HEAP_END);
BSP_SDRAM_STATIC_ASSERT(record_before_roll_history, SDRAM_RECORD_END <= SDRAM_ROLL_HISTORY_BUF);
BSP_SDRAM_STATIC_ASSERT(roll_history_inside_sdram, SDRAM_ROLL_HISTORY_END == SDRAM_TOTAL_END);
BSP_SDRAM_STATIC_ASSERT(record_region_nonzero, SDRAM_RECORD_END > SDRAM_RECORD_BUF);
```

Keep the include guard and `#include "bsp_fmc_sdram.h"`.

- [ ] **Step 4: Make RollHistory consume the shared layout**

In `User/dso/roll_history_mem.c`, replace:

```c
#include "bsp_fmc_sdram.h"
```

with:

```c
#include "bsp_sdram_layout.h"
```

Replace:

```c
#define ROLL_HIST_BLOCK_BYTES  (3u * 1024u * 1024u)
```

with:

```c
#define ROLL_HIST_BLOCK_BYTES  SDRAM_ROLL_HISTORY_SIZE
```

In `RollHistoryMem_Init()`, replace:

```c
    end = (uint32_t)EXT_SDRAM_ADDR + (uint32_t)EXT_SDRAM_SIZE;
```

with:

```c
    end = SDRAM_ROLL_HISTORY_END;
```

Replace:

```c
    base = end - ROLL_HIST_BLOCK_BYTES;
```

with:

```c
    base = SDRAM_ROLL_HISTORY_BUF;
```

- [ ] **Step 5: Keep record storage bounded by `SDRAM_RECORD_SIZE`**

In `User/dso/dso_record.c`, keep:

```c
#define DSO_RECORD_TOTAL_WORDS        (SDRAM_RECORD_SIZE / sizeof(uint16_t))
```

Do not add any runtime fallback or alternate record buffer. The reduced `SDRAM_RECORD_SIZE` is the single record capacity on STM32H743.

- [ ] **Step 6: Update the architecture memory document**

In `docs/architecture/06-memory-bsp-build-test.md`, replace the paragraph that says deep record can overlap RollHistory with a statement that the layout now reserves the final 3 MB for RollHistory:

```markdown
RollHistory owns the final 3 MB of SDRAM via `SDRAM_ROLL_HISTORY_BUF..SDRAM_ROLL_HISTORY_END`.
`SDRAM_RECORD_END` is defined as `SDRAM_ROLL_HISTORY_BUF`, so deep record no longer claims the RollHistory block.
The host test `tests/test_dso_sdram_layout.c` verifies that VRAM, GUI heap, record, and RollHistory are ordered and non-overlapping.
```

- [ ] **Step 7: Run SDRAM and record tests**

Run:

```bash
$CC -I. -IUser/bsp/inc tests/test_dso_sdram_layout.c -o tests/test_dso_sdram_layout.exe
./tests/test_dso_sdram_layout.exe
./tests/test_dso_engine_record_anchor_policy.exe
```

Expected: both tests pass.

- [ ] **Step 8: Commit Task 2**

Run:

```bash
git add User/bsp/inc/bsp_sdram_layout.h User/dso/roll_history_mem.c User/dso/dso_record.c docs/architecture/06-memory-bsp-build-test.md tests/test_dso_sdram_layout.c tests/test_dso_sdram_layout.exe
git commit -m "bsp: reserve roll history outside deep record"
```

### Task 3: Tighten 10-Bit Data Readiness

**Files:**
- Modify: `User/dso/dso_acq_10bit.c`
- Modify: `tests/test_dso_acq_ready_contract.c`
- Create: `tests/test_dso_acq10_ready_gate.c`

- [ ] **Step 1: Update the existing readiness contract test**

In `tests/test_dso_acq_ready_contract.c`, replace the old 10-bit expectations:

```c
    rc |= require_contains("User/dso/dso_acq_10bit.c", buf, "out->data_ready = s_acq10.running;");
    rc |= require_contains("User/dso/dso_acq_10bit.c", buf, "return s_acq10.running;");
```

with:

```c
    rc |= require_contains("User/dso/dso_acq_10bit.c", buf, "static bool acq10_all_ch1_lanes_have_dma_progress(void)");
    rc |= require_contains("User/dso/dso_acq_10bit.c", buf, "static void acq10_mark_data_not_ready(void)");
    rc |= require_contains("User/dso/dso_acq_10bit.c", buf, "static void acq10_update_data_ready_from_snapshot(void)");
    rc |= require_contains("User/dso/dso_acq_10bit.c", buf, "out->data_ready = s_acq10.data_ready;");
    rc |= require_contains("User/dso/dso_acq_10bit.c", buf, "return s_acq10.data_ready;");
```

- [ ] **Step 2: Add a 10-bit ready-gate text contract**

Create `tests/test_dso_acq10_ready_gate.c`:

```c
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
    fseek(fp, 0, SEEK_END);
    length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    text = (char *)malloc((size_t)length + 1u);
    if (text == NULL) {
        fprintf(stderr, "malloc failed\n");
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
        fprintf(stderr, "%s missing: %s\n", path, needle);
        return 1;
    }
    return 0;
}

static int reject_contains(const char *path, const char *text, const char *needle)
{
    if (strstr(text, needle) != NULL) {
        fprintf(stderr, "%s forbidden text still present: %s\n", path, needle);
        return 1;
    }
    return 0;
}

int main(void)
{
    const char *path = "User/dso/dso_acq_10bit.c";
    char *text = read_text_file(path);
    int rc = 0;

    rc |= require_contains(path, text, "bool data_ready;");
    rc |= require_contains(path, text, "uint8_t data_ready_lane_count;");
    rc |= require_contains(path, text, "s_acq10.data_ready = false;");
    rc |= require_contains(path, text, "s_acq10.data_ready = true;");
    rc |= require_contains(path, text, "s_acq10.data_ready_lane_count = 2u;");
    rc |= require_contains(path, text, "s_acq10.snapshot_common_delta == 0u");
    rc |= require_contains(path, text, "s_acq10.snapshot_common_pos == 0u");
    rc |= require_contains(path, text, "(void)DSO_Acq10_GetPos();");
    rc |= reject_contains(path, text, "out->data_ready = s_acq10.running;");
    rc |= reject_contains(path, text, "return s_acq10.running;");

    free(text);
    return rc;
}
```

- [ ] **Step 3: Run readiness tests and verify the red state**

Run:

```bash
$CC -I. -IUser tests/test_dso_acq_ready_contract.c -o tests/test_dso_acq_ready_contract.exe
./tests/test_dso_acq_ready_contract.exe
$CC -I. -IUser tests/test_dso_acq10_ready_gate.c -o tests/test_dso_acq10_ready_gate.exe
./tests/test_dso_acq10_ready_gate.exe
```

Expected: both tests fail because 10-bit readiness still maps directly to `running`.

- [ ] **Step 4: Add readiness state to `DSO_Acq10State`**

In `User/dso/dso_acq_10bit.c`, add these fields after `snapshot_phase_sync_error`:

```c
    bool data_ready;
    uint8_t data_ready_lane_count;
```

- [ ] **Step 5: Reset readiness with tracking state**

In `reset_tracking()`, add:

```c
    s_acq10.data_ready = false;
    s_acq10.data_ready_lane_count = 2u;
```

Add helper functions near `reset_tracking()`:

```c
static void acq10_mark_data_not_ready(void)
{
    s_acq10.data_ready = false;
    s_acq10.data_ready_lane_count = 2u;
}

static bool acq10_all_ch1_lanes_have_dma_progress(void)
{
    if (!s_acq10.running || (s_acq10.snapshot_common_delta == 0u)) {
        return false;
    }
    if (s_acq10.snapshot_common_pos == 0u) {
        return false;
    }
    return true;
}

static void acq10_update_data_ready_from_snapshot(void)
{
    if (acq10_all_ch1_lanes_have_dma_progress()) {
        s_acq10.data_ready = true;
    }
    s_acq10.data_ready_lane_count = 2u;
}
```

- [ ] **Step 6: Update readiness after each common-position snapshot**

At the end of `snapshot_common_position()`, after the phase-sync error counter update, add:

```c
    acq10_update_data_ready_from_snapshot();
```

- [ ] **Step 7: Clear readiness on stop**

In `DSO_Acq10_Stop()`, after `s_acq10.running = false;`, add:

```c
    acq10_mark_data_not_ready();
```

- [ ] **Step 8: Report readiness through debug snapshot and public API**

In `DSO_Acq10_GetDebugSnapshot()`, replace:

```c
    out->data_ready = s_acq10.running;
    out->data_ready_lane_count = 2u;
```

with:

```c
    out->data_ready = s_acq10.data_ready;
    out->data_ready_lane_count = s_acq10.data_ready_lane_count;
```

Replace `DSO_Acq10_IsDataReady()` with:

```c
bool DSO_Acq10_IsDataReady(void)
{
    if (!s_acq10.running) {
        return false;
    }
    (void)DSO_Acq10_GetPos();
    return s_acq10.data_ready;
}
```

- [ ] **Step 9: Run readiness tests**

Run:

```bash
$CC -I. -IUser tests/test_dso_acq_ready_contract.c -o tests/test_dso_acq_ready_contract.exe
./tests/test_dso_acq_ready_contract.exe
$CC -I. -IUser tests/test_dso_acq10_ready_gate.c -o tests/test_dso_acq10_ready_gate.exe
./tests/test_dso_acq10_ready_gate.exe
./tests/test_dso_engine_8bit_ready_contract.exe
./tests/test_dso_acq8_ready_gate.exe
```

Expected: all commands pass. The 8-bit tests passing confirms this task did not change the frozen 8-bit readiness behavior.

- [ ] **Step 10: Commit Task 3**

Run:

```bash
git add User/dso/dso_acq_10bit.c tests/test_dso_acq_ready_contract.c tests/test_dso_acq_ready_contract.exe tests/test_dso_acq10_ready_gate.c tests/test_dso_acq10_ready_gate.exe
git commit -m "dso: gate 10-bit readiness on lane progress"
```

### Task 4: Clean Architecture Wording And Lock Contracts

**Files:**
- Modify: `User/dso/dso_config.h`
- Modify: `User/dso/dso_acq_model.h`
- Modify: `docs/architecture/03-acquisition-and-sample-modes.md`
- Modify: `docs/architecture/06-memory-bsp-build-test.md`
- Modify: `docs/architecture/07-maintenance-boundaries-and-risks.md`
- Modify: `Doc/波形绘制方案说明.md`
- Create: `tests/test_dso_architecture_cleanup_contract.c`

- [ ] **Step 1: Write the architecture cleanup contract**

Create `tests/test_dso_architecture_cleanup_contract.c`:

```c
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
    fseek(fp, 0, SEEK_END);
    length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    text = (char *)malloc((size_t)length + 1u);
    if (text == NULL) {
        fprintf(stderr, "malloc failed\n");
        fclose(fp);
        exit(1);
    }
    if (fread(text, 1u, (size_t)length, fp) != (size_t)length) {
        fprintf(stderr, "read failed for %s\n", path);
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
        fprintf(stderr, "%s missing: %s\n", path, needle);
        return 1;
    }
    return 0;
}

static int reject_contains(const char *path, const char *text, const char *needle)
{
    if (strstr(text, needle) != NULL) {
        fprintf(stderr, "%s forbidden text still present: %s\n", path, needle);
        return 1;
    }
    return 0;
}

int main(void)
{
    char *config_h = read_text_file("User/dso/dso_config.h");
    char *acq_model_h = read_text_file("User/dso/dso_acq_model.h");
    char *acq_doc = read_text_file("docs/architecture/03-acquisition-and-sample-modes.md");
    char *memory_doc = read_text_file("docs/architecture/06-memory-bsp-build-test.md");
    char *risk_doc = read_text_file("docs/architecture/07-maintenance-boundaries-and-risks.md");
    char *plot_doc = read_text_file("Doc/波形绘制方案说明.md");
    int rc = 0;

    rc |= require_contains("User/dso/dso_config.h", config_h, "DSO_ADC_MAX is a legacy compatibility constant");
    rc |= require_contains("User/dso/dso_acq_model.h", acq_model_h, "tim8_* names are legacy field names; current acquisition backends use TIM1");

    rc |= require_contains("docs/architecture/03-acquisition-and-sample-modes.md", acq_doc, "默认格式为 `8-bit (3L)`");
    rc |= require_contains("docs/architecture/03-acquisition-and-sample-modes.md", acq_doc, "UI 只在 `10-bit` 与 `8-bit (3L)` 之间切换");
    rc |= reject_contains("docs/architecture/03-acquisition-and-sample-modes.md", acq_doc, "8-bit (2L)");

    rc |= require_contains("docs/architecture/06-memory-bsp-build-test.md", memory_doc, "SDRAM_RECORD_END");
    rc |= require_contains("docs/architecture/06-memory-bsp-build-test.md", memory_doc, "SDRAM_ROLL_HISTORY_BUF");
    rc |= reject_contains("docs/architecture/06-memory-bsp-build-test.md", memory_doc, "存在地址预算重叠风险");

    rc |= require_contains("docs/architecture/07-maintenance-boundaries-and-risks.md", risk_doc, "幅度换算使用 `DSO_Frame.adc_lsb_v`");
    rc |= require_contains("Doc/波形绘制方案说明.md", plot_doc, "Historical note");

    free(config_h);
    free(acq_model_h);
    free(acq_doc);
    free(memory_doc);
    free(risk_doc);
    free(plot_doc);
    return rc;
}
```

- [ ] **Step 2: Run the cleanup contract and verify the red state**

Run:

```bash
$CC -I. -IUser tests/test_dso_architecture_cleanup_contract.c -o tests/test_dso_architecture_cleanup_contract.exe
./tests/test_dso_architecture_cleanup_contract.exe
```

Expected: runtime failure because the new comments and documentation wording are not present yet.

- [ ] **Step 3: Clarify the legacy ADC max constant**

In `User/dso/dso_config.h`, add this comment immediately above `#define DSO_ADC_MAX 4095u`:

```c
/*
 * DSO_ADC_MAX is a legacy compatibility constant for old code paths and tests.
 * Active acquisition formats must use DSO_SampleSpec.adc_max or DSO_Frame.adc_lsb_v.
 */
```

Do not remove the macro in this task because unrelated legacy code may still include it.

- [ ] **Step 4: Clarify legacy TIM field names**

In `User/dso/dso_acq_model.h`, add this comment above `tim8_clk_hz` in `DSO_AcqRuntimeInfo`:

```c
/* tim8_* names are legacy field names; current acquisition backends use TIM1. */
```

Do not rename the struct fields in this task because many tests and debug formatters depend on those names.

- [ ] **Step 5: Update acquisition architecture wording**

In `docs/architecture/03-acquisition-and-sample-modes.md`, ensure the current-mode summary contains these exact sentences:

```markdown
默认格式为 `8-bit (3L)` 高速三路交织。
UI 只在 `10-bit` 与 `8-bit (3L)` 之间切换。
```

Remove active-architecture references to `8-bit (2L)` from this document. If historical mention is necessary, move it to a clearly labeled historical paragraph outside the current product path and avoid the exact string `8-bit (2L)` so the contract remains strict.

- [ ] **Step 6: Update memory and risk documents**

In `docs/architecture/06-memory-bsp-build-test.md`, document the new region order using the exact macro names:

```markdown
`SDRAM_RECORD_BUF..SDRAM_RECORD_END` is the deep-record region.
`SDRAM_ROLL_HISTORY_BUF..SDRAM_ROLL_HISTORY_END` is the RollHistory region.
`SDRAM_RECORD_END == SDRAM_ROLL_HISTORY_BUF`, so the two regions are adjacent and non-overlapping.
```

Remove the old sentence containing `存在地址预算重叠风险`.

In `docs/architecture/07-maintenance-boundaries-and-risks.md`, add:

```markdown
幅度换算使用 `DSO_Frame.adc_lsb_v`，不要用 legacy `DSO_ADC_MAX` 推断当前采样格式的满量程。
```

- [ ] **Step 7: Mark old PlotBuf document as historical**

At the top of `Doc/波形绘制方案说明.md`, add:

```markdown
> Historical note: 本文记录早期 PlotBuf/旋转优化阶段，部分尺寸和性能数据已经不再代表当前实现；当前实现以 `User/ui/dma2d_wave.h` 和 `docs/architecture/05-ui-display-and-input.md` 为准。
```

- [ ] **Step 8: Run cleanup and existing legacy tests**

Run:

```bash
$CC -I. -IUser tests/test_dso_architecture_cleanup_contract.c -o tests/test_dso_architecture_cleanup_contract.exe
./tests/test_dso_architecture_cleanup_contract.exe
./tests/test_dso_acq8_legacy_cleanup.exe
./tests/test_dso_sample_spec.exe
```

Expected: all tests pass.

- [ ] **Step 9: Commit Task 4**

Run:

```bash
git add User/dso/dso_config.h User/dso/dso_acq_model.h docs/architecture/03-acquisition-and-sample-modes.md docs/architecture/06-memory-bsp-build-test.md docs/architecture/07-maintenance-boundaries-and-risks.md Doc/波形绘制方案说明.md tests/test_dso_architecture_cleanup_contract.c tests/test_dso_architecture_cleanup_contract.exe
git commit -m "docs: clarify active dso architecture boundaries"
```

### Task 5: Final Verification

**Files:**
- No new source files beyond prior tasks.
- Verify checked-in `.exe` runners created or updated in prior tasks.

- [ ] **Step 1: Run focused new tests**

Run:

```bash
./tests/test_dso_ui_amplitude_units.exe
./tests/test_dso_frame_adc_metadata_contract.exe
./tests/test_dso_sdram_layout.exe
./tests/test_dso_acq_ready_contract.exe
./tests/test_dso_acq10_ready_gate.exe
./tests/test_dso_architecture_cleanup_contract.exe
```

Expected: all tests pass.

- [ ] **Step 2: Run existing smoke tests**

Run:

```bash
./tests/test_dso_serial_debug.exe
./tests/test_dso_hf_pipeline.exe
```

Expected: both tests pass.

- [ ] **Step 3: Run safety-adjacent existing tests**

Run:

```bash
./tests/test_dso_freq_ui_format.exe
./tests/test_dso_freq_meter_engine_contract.exe
./tests/test_dso_sample_spec.exe
./tests/test_dso_vertical.exe
./tests/test_dso_engine_record_anchor_policy.exe
./tests/test_dso_engine_8bit_ready_contract.exe
./tests/test_dso_acq8_ready_gate.exe
```

Expected: all tests pass.

- [ ] **Step 4: Check git diff and ensure no forbidden subsystem drift**

Run:

```bash
git diff --stat HEAD~4..HEAD
git diff -- User/dso/dso_acq_8bit.c User/dso/dso_hs8_lane_cal.c User/dso/dso_hs8_render.c
```

Expected: the first command summarizes only planned files. The second command produces no diff.

- [ ] **Step 5: Rebuild firmware project touched by these changes**

Open `Project/MDK-ARM(uV5)/project.uvprojx` in Keil and build target `Flash`.

Expected: build succeeds. Generated hex diffs are expected only if Keil rebuilds `Project/output(mdk).hex`; inspect and keep the generated output only when the firmware rebuild is intentional.

- [ ] **Step 6: Commit final verification note if generated artifacts changed**

If Keil updates generated firmware artifacts that should be kept, run:

```bash
git add Project/output\(mdk\).hex
git commit -m "build: refresh mdk firmware output"
```

If no generated artifacts changed, do not create a verification-only commit.

## Self-Review Checklist

- Phase 1 covers amplitude conversion and leaves external frequency display unchanged.
- Phase 2 creates a single SDRAM layout contract and removes record/RollHistory overlap.
- Phase 3 gates 10-bit readiness on lane progress without touching 8-bit code.
- Phase 4 updates code comments and docs without changing active acquisition behavior.
- All new tests have explicit compile and run commands.
- No task adds fallback-driven compatibility behavior.
