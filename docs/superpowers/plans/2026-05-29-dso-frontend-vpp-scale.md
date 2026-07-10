# DSO Frontend Vpp Scale Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add fixed per-channel front-end scaling so displayed Vpp/Vrms values match external input-terminal measurements.

**Architecture:** Keep `dso_process.c` measuring ADC code deltas exactly as it does today. Add a focused `dso_frontend_cal` module that converts ADC-code deltas to calibrated input volts, then route UI measurement helpers through it.

**Tech Stack:** STM32 firmware C, FreeRTOS/emWin app code, host-side C regression tests.

---

## File Structure

- Create `User/dso/dso_frontend_cal.h`: public conversion API and default fixed scale macros.
- Create `User/dso/dso_frontend_cal.c`: channel clamp and conversion implementation.
- Modify `User/ui/UI_WaveformCtrl.c`: use calibrated conversion for CH1/CH2 Vpp and Vrms.
- Modify `User/ui/ui_measure_units.h`: expose calibrated measurement helper for measurement pages.
- Modify `User/ui/ui_screens.c`: use calibrated helper for measurement page Vpp.
- Modify `tests/test_dso_ui_amplitude_units.c`: cover raw conversion and calibrated conversion.

## Task 1: Add Failing Host Test

**Files:**
- Modify: `tests/test_dso_ui_amplitude_units.c`

- [ ] **Step 1: Include the new calibration header**

Add after existing includes:

```c
#include "dso/dso_frontend_cal.h"
```

- [ ] **Step 2: Add tests for calibrated input voltage conversion**

Add before `main()`:

```c
static int test_ch1_input_delta_applies_frontend_scale(void)
{
    DSO_UI_Frame frame = {0};

    frame.adc_bits = 10u;
    frame.adc_lsb_v = 3.3f / 1023.0f;

    return expect_near("CH1 calibrated input delta",
                       UI_Measure_CodeDeltaToInputVolts(DSO_CHANNEL_1, 512.0f, &frame),
                       512.0f * 3.3f / 1023.0f * 0.5f);
}

static int test_ch2_input_delta_applies_frontend_scale(void)
{
    DSO_UI_Frame frame = {0};

    frame.adc_bits = 8u;
    frame.adc_lsb_v = 3.3f / 255.0f;

    return expect_near("CH2 calibrated input delta",
                       UI_Measure_CodeDeltaToInputVolts(DSO_CHANNEL_2, 128.0f, &frame),
                       128.0f * 3.3f / 255.0f * 0.5f);
}

static int test_missing_metadata_input_delta_is_zero(void)
{
    DSO_UI_Frame frame = {0};

    return expect_near("missing metadata calibrated input delta",
                       UI_Measure_CodeDeltaToInputVolts(DSO_CHANNEL_1, 100.0f, &frame),
                       0.0f);
}
```

- [ ] **Step 3: Call the new tests from `main()`**

Add after the existing three `rc |= ...` lines:

```c
    rc |= test_ch1_input_delta_applies_frontend_scale();
    rc |= test_ch2_input_delta_applies_frontend_scale();
    rc |= test_missing_metadata_input_delta_is_zero();
```

- [ ] **Step 4: Run test and confirm failure**

Run:

```bash
gcc -std=c99 -Wall -Wextra -IUser -I. tests/test_dso_ui_amplitude_units.c User/dso/dso_ui_bridge.c -lm -o /tmp/test_dso_ui_amplitude_units
/tmp/test_dso_ui_amplitude_units
```

Expected: compile fails because `dso/dso_frontend_cal.h` or `UI_Measure_CodeDeltaToInputVolts` does not exist.

## Task 2: Add Frontend Calibration Module

**Files:**
- Create: `User/dso/dso_frontend_cal.h`
- Create: `User/dso/dso_frontend_cal.c`

- [ ] **Step 1: Create header**

```c
/**
 * @file dso_frontend_cal.h
 * @brief Fixed front-end voltage scale for input-terminal measurement display.
 */
#ifndef DSO_FRONTEND_CAL_H
#define DSO_FRONTEND_CAL_H

#include "dso_vertical.h"

#ifndef DSO_FRONTEND_CAL_CH1_SCALE
#define DSO_FRONTEND_CAL_CH1_SCALE 0.5f
#endif

#ifndef DSO_FRONTEND_CAL_CH2_SCALE
#define DSO_FRONTEND_CAL_CH2_SCALE 0.5f
#endif

float DSO_FrontendCal_GetScaleForChannel(DSO_ChannelId channel);
float DSO_FrontendCal_CodeDeltaToInputVolts(DSO_ChannelId channel,
                                            float code_delta,
                                            float adc_lsb_v);

#endif /* DSO_FRONTEND_CAL_H */
```

- [ ] **Step 2: Create implementation**

```c
/**
 * @file dso_frontend_cal.c
 * @brief Fixed front-end voltage scale for input-terminal measurement display.
 */
#include "dso_frontend_cal.h"

float DSO_FrontendCal_GetScaleForChannel(DSO_ChannelId channel)
{
    if (channel == DSO_CHANNEL_2) {
        return DSO_FRONTEND_CAL_CH2_SCALE;
    }
    return DSO_FRONTEND_CAL_CH1_SCALE;
}

float DSO_FrontendCal_CodeDeltaToInputVolts(DSO_ChannelId channel,
                                            float code_delta,
                                            float adc_lsb_v)
{
    if ((code_delta <= 0.0f) || (adc_lsb_v <= 0.0f)) {
        return 0.0f;
    }
    return code_delta * adc_lsb_v * DSO_FrontendCal_GetScaleForChannel(channel);
}
```

## Task 3: Route UI Measurement Helpers Through Calibration

**Files:**
- Modify: `User/ui/ui_measure_units.h`
- Modify: `User/ui/UI_WaveformCtrl.c`
- Modify: `User/ui/ui_screens.c`

- [ ] **Step 1: Extend measurement units helper**

In `User/ui/ui_measure_units.h`, include the calibration header:

```c
#include "dso/dso_frontend_cal.h"
```

Add this inline helper after `UI_Measure_CodeDeltaToVolts`:

```c
static inline float UI_Measure_CodeDeltaToInputVolts(DSO_ChannelId channel,
                                                     float code_delta,
                                                     const DSO_UI_Frame *frame)
{
    return DSO_FrontendCal_CodeDeltaToInputVolts(channel,
                                                 code_delta,
                                                 DSO_UI_FrameAdcLsbV(frame));
}
```

- [ ] **Step 2: Update `UI_WaveformCtrl.c` includes**

Add:

```c
#include "ui_measure_units.h"
```

- [ ] **Step 3: Update CH1 Vpp/Vrms conversion**

Replace:

```c
    m->vpp_v     = DSO_UI_CodeDeltaToVolts((float)s_display_frame.vpp,
                                             DSO_UI_FrameAdcLsbV(&s_display_frame));
    m->vrms_v    = DSO_UI_CodeDeltaToVolts(s_display_frame.vrms,
                                             DSO_UI_FrameAdcLsbV(&s_display_frame));
```

with:

```c
    m->vpp_v     = UI_Measure_CodeDeltaToInputVolts(DSO_CHANNEL_1,
                                                     (float)s_display_frame.vpp,
                                                     &s_display_frame);
    m->vrms_v    = UI_Measure_CodeDeltaToInputVolts(DSO_CHANNEL_1,
                                                     s_display_frame.vrms,
                                                     &s_display_frame);
```

- [ ] **Step 4: Update CH2 Vpp/Vrms conversion**

Replace:

```c
    m->vpp_v      = DSO_UI_CodeDeltaToVolts((float)s_display_frame.ch2_vpp,
                                              DSO_UI_FrameAdcLsbV(&s_display_frame));
    m->vrms_v     = DSO_UI_CodeDeltaToVolts(s_display_frame.ch2_vrms,
                                              DSO_UI_FrameAdcLsbV(&s_display_frame));
```

with:

```c
    m->vpp_v      = UI_Measure_CodeDeltaToInputVolts(DSO_CHANNEL_2,
                                                      (float)s_display_frame.ch2_vpp,
                                                      &s_display_frame);
    m->vrms_v     = UI_Measure_CodeDeltaToInputVolts(DSO_CHANNEL_2,
                                                      s_display_frame.ch2_vrms,
                                                      &s_display_frame);
```

- [ ] **Step 5: Update measurement page direct Vpp formatting**

In `User/ui/ui_screens.c`, replace direct Vpp conversions:

```c
fmt_volts_mag(DSO_UI_CodeDeltaToVolts((float)fr.vpp, DSO_UI_FrameAdcLsbV(&fr)), v1[2], (unsigned)sizeof(v1[2]));
fmt_volts_mag(DSO_UI_CodeDeltaToVolts((float)fr.ch2_vpp, DSO_UI_FrameAdcLsbV(&fr)), v2[2], (unsigned)sizeof(v2[2]));
```

with:

```c
fmt_volts_mag(UI_Measure_CodeDeltaToInputVolts(DSO_CHANNEL_1, (float)fr.vpp, &fr), v1[2], (unsigned)sizeof(v1[2]));
fmt_volts_mag(UI_Measure_CodeDeltaToInputVolts(DSO_CHANNEL_2, (float)fr.ch2_vpp, &fr), v2[2], (unsigned)sizeof(v2[2]));
```

## Task 4: Build and Verify Host Tests

**Files:**
- Test: `tests/test_dso_ui_amplitude_units.c`
- Test existing runners where available.

- [ ] **Step 1: Build updated amplitude test**

Run:

```bash
gcc -std=c99 -Wall -Wextra -IUser -I. tests/test_dso_ui_amplitude_units.c User/dso/dso_ui_bridge.c User/dso/dso_frontend_cal.c -lm -o tests/test_dso_ui_amplitude_units_new.exe
```

Expected: no compiler errors.

- [ ] **Step 2: Run updated amplitude test**

Run:

```bash
./tests/test_dso_ui_amplitude_units_new.exe
```

Expected:

```text
all UI amplitude unit tests passed
```

- [ ] **Step 3: Run existing named regression tests**

Run:

```bash
./tests/test_dso_serial_debug.exe
./tests/test_dso_hf_pipeline.exe
```

Expected:

```text
all serial debug tests passed
all hf pipeline tests passed
```

- [ ] **Step 4: Check firmware project integration note**

Open `Project/MDK-ARM(uV5)/project.uvprojx` and `Project/EWARMv8/project.ewp` only if this branch needs IDE project synchronization. If source files must be registered manually, add `User/dso/dso_frontend_cal.c` to both projects in the same group as other `User/dso` files.
