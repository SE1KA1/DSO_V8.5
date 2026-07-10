# DSO 10-Bit Mode Replacement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the current 12-bit mode with a dedicated 10-bit mode, make 10-bit the default boot mode, and keep the existing 8-bit dual/tri modes untouched.

**Architecture:** Build the new mode in dedicated 10-bit files and keep all 8-bit-specific architecture files frozen. Shared files may receive additive 10-bit integration only in dispatch, sample-format selection, engine policy, timebase policy, UI selection, and debug metadata. Do not add fallback scaffolding or keep old/new logic paths alive in parallel.

**Tech Stack:** STM32H743 HAL ADC/TIM/DMA, Keil `project.uvprojx` target `Flash`, host-side C regression tests built with `cl /nologo /I. /IUser ... /Fe:tests\\*_new.exe`.

---

## File Map

**Frozen 8-bit architecture files:**

- `User/dso/dso_acq_8bit.c`
- `User/dso/dso_acq8_fault_policy.c`
- `User/dso/dso_acq8_fault_policy.h`
- `User/dso/dso_acq8_refresh_policy.c`
- `User/dso/dso_acq8_refresh_policy.h`
- `User/dso/dso_hs8_lane_cal.c`
- `User/dso/dso_hs8_lane_cal.h`
- `User/dso/dso_hs8_render.c`
- `User/dso/dso_hs8_render.h`

**Create for 10-bit only:**

- `User/dso/dso_acq10_stream.h`
  Pure C helper APIs for CH1 dual-lane merge and ring-copy semantics. Host-testable without HAL.
- `User/dso/dso_acq10_stream.c`
  Implementation of ordered lane interleave and `recent/new` sample copy helpers.
- `User/dso/dso_acq_10bit.h`
  Public backend interface used by `dso_acq.c`.
- `User/dso/dso_acq_10bit.c`
  HAL-backed 10-bit acquisition backend: `TIM1 CC1/CC2/CC3`, `ADC1/ADC2` on `PA6 CH3`, `ADC3` on `PC2 CH0`, DMA setup, runtime info, start/stop, debug snapshot, and buffer handoff.
- `User/dso/dso_10bit_lane_cal.h`
  Public 10-bit calibration API replacing old `dso_interleave_cal`.
- `User/dso/dso_10bit_lane_cal.c`
  CH1 dual-lane offset/gain/skew solve, apply, runtime state, and EEPROM persistence.
- `User/dso/dso_hs10_render.h`
  Public enhanced repetitive reconstruction API for 10-bit.
- `User/dso/dso_hs10_render.c`
  10-bit repetitive accumulation and render path.
- `tests/test_dso_acq10_stream.c`
  Host tests for lane-merge ordering and ring-copy behavior.
- `tests/test_dso_10bit_lane_cal.c`
  Host tests for solve/apply/state transitions of 10-bit calibration.
- `tests/test_dso_hs10_render.c`
  Host tests for 10-bit enhanced reconstruction readiness/render behavior.
- `tests/test_dso_timebase_10bit.c`
  Host tests for 10-bit table selection and HS10 render gating.
- `tests/test_ui_system_setup_10bit.c`
  Host tests for `10-bit -> 8-bit (2L) -> 8-bit (3L)` selector order and label text.
- `tests/test_dso_acq10_api_contract.c`
  Header/API contract test for the new 10-bit backend symbols.

**Modify shared integration files only:**

- `User/dso/dso_sample_spec.h`
- `User/dso/dso_sample_spec.c`
- `User/dso/dso_acquire_control.c`
- `User/dso/dso_acq.h`
- `User/dso/dso_acq.c`
- `User/dso/dso_timebase.h`
- `User/dso/dso_timebase.c`
- `User/dso/dso_render_policy.h`
- `User/dso/dso_render_policy.c`
- `User/dso/dso_engine.c`
- `User/dso/dso_process.h`
- `User/dso/dso_serial_debug.c`
- `User/dso/dso_serial_debug_format.c`
- `User/ui/ui_system_setup.h`
- `User/ui/ui_system_setup.c`
- `User/ui/UI_WaveformCtrl.h`
- `User/ui/UI_WaveformCtrl.c`
- `User/ui/ui_screens.c`
- `User/ui/MainTask.c`
- `tests/test_dso_sample_spec.c`
- `tests/test_dso_acquire_control.c`
- `tests/test_dso_hf_pipeline.c`
- `tests/test_dso_process_hs8_freq_policy.c`
- `Project/MDK-ARM(uV5)/project.uvprojx`

**Delete after references are removed:**

- `User/dso/dso_interleave_cal.h`
- `User/dso/dso_interleave_cal.c`

**Execution note:** this checkout has no `.git` metadata. The commit steps below are written for execution in a git-backed worktree; if the execution workspace still lacks `.git`, record the listed commit message in the task log and continue.

### Task 1: Lock the public 10-bit mode contract and boot defaults

**Files:**
- Modify: `tests/test_dso_sample_spec.c`
- Modify: `tests/test_dso_acquire_control.c`
- Create: `tests/test_ui_system_setup_10bit.c`
- Modify: `User/dso/dso_sample_spec.h`
- Modify: `User/dso/dso_sample_spec.c`
- Modify: `User/dso/dso_acquire_control.c`
- Modify: `User/ui/ui_system_setup.h`
- Modify: `User/ui/ui_system_setup.c`

- [ ] **Step 1: Write the failing sample-spec, acquire-mode, and UI selector tests**

Update `tests/test_dso_sample_spec.c` so the first format is `10-bit` instead of `12-bit`, and add checks like:

```c
static int test_10bit_spec_matches_mode_contract(void)
{
    const DSO_SampleSpec *spec = DSO_SampleSpec_Get(DSO_SAMPLE_FORMAT_10BIT);

    if (spec == NULL) {
        fprintf(stderr, "10-bit spec was NULL\n");
        return 1;
    }
    if (spec->adc_bits != 10u || spec->adc_max != 1023u || spec->midscale != 512u) {
        fprintf(stderr,
                "unexpected 10-bit spec: bits=%u max=%u mid=%u\n",
                (unsigned)spec->adc_bits,
                (unsigned)spec->adc_max,
                (unsigned)spec->midscale);
        return 1;
    }
    if (spec->lane_count != 2u || spec->ch2_mode != DSO_CH2_MODE_REALTIME || !spec->interleaved) {
        fprintf(stderr,
                "unexpected 10-bit acquisition flags: lanes=%u ch2_mode=%u interleaved=%u\n",
                (unsigned)spec->lane_count,
                (unsigned)spec->ch2_mode,
                spec->interleaved ? 1u : 0u);
        return 1;
    }
    return 0;
}
```

Update `tests/test_dso_acquire_control.c` so 10-bit keeps `AVERAGE`, allows `HIRES`, and cycles `SAMPLE -> PEAK -> HIRES -> AVERAGE -> SAMPLE`:

```c
static int test_10bit_cycle_sequence_includes_hires(void)
{
    DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_10BIT);

    if (DSO_AcquireCtrl_GetNextAcquireMode(DSO_ACQUIRE_MODE_SAMPLE) != DSO_ACQUIRE_MODE_PEAK_DETECT) {
        fprintf(stderr, "10-bit cycle sample->peak mismatch\n");
        return 1;
    }
    if (DSO_AcquireCtrl_GetNextAcquireMode(DSO_ACQUIRE_MODE_PEAK_DETECT) != DSO_ACQUIRE_MODE_HIRES) {
        fprintf(stderr, "10-bit cycle peak->hires mismatch\n");
        return 1;
    }
    if (DSO_AcquireCtrl_GetNextAcquireMode(DSO_ACQUIRE_MODE_HIRES) != DSO_ACQUIRE_MODE_AVERAGE) {
        fprintf(stderr, "10-bit cycle hires->average mismatch\n");
        return 1;
    }
    if (DSO_AcquireCtrl_GetNextAcquireMode(DSO_ACQUIRE_MODE_AVERAGE) != DSO_ACQUIRE_MODE_SAMPLE) {
        fprintf(stderr, "10-bit cycle average->sample mismatch\n");
        return 1;
    }
    return 0;
}
```

Create `tests/test_ui_system_setup_10bit.c` with local stubs for `DSO_Acq_*` and `UI_Nav_MarkDirty()` and assert:

```c
static int test_forward_cycle_is_10bit_dual_tri(void)
{
    Test_SetPending(DSO_SAMPLE_FORMAT_10BIT, DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED);
    UI_System_OnScreenShown();
    UI_System_Ec2Detent(+1);
    if (g_set_format != DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED || g_set_topology != DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED) {
        fprintf(stderr, "10-bit -> 8-bit(2L) forward transition mismatch\n");
        return 1;
    }
    return 0;
}
```

- [ ] **Step 2: Build the focused host tests and verify they fail before code changes**

Run: `cl /nologo /I. /IUser tests\\test_dso_sample_spec.c User\\dso\\dso_sample_spec.c /Fe:tests\\test_dso_sample_spec_new.exe`

Expected: build or runtime failure because `DSO_SAMPLE_FORMAT_10BIT` and 10-bit values do not exist yet.

Run: `cl /nologo /I. /IUser tests\\test_dso_acquire_control.c User\\dso\\dso_acquire_control.c User\\dso\\dso_sample_spec.c /Fe:tests\\test_dso_acquire_control_new.exe`

Expected: runtime failure because 10-bit currently follows the old 12-bit no-`HIRES` cycle.

Run: `cl /nologo /I. /IUser tests\\test_ui_system_setup_10bit.c User\\ui\\ui_system_setup.c User\\dso\\dso_sample_spec.c /Fe:tests\\test_ui_system_setup_10bit_new.exe`

Expected: build or runtime failure because the selector still advertises `12-bit` first.

- [ ] **Step 3: Implement the minimal public-mode changes**

Change `User/dso/dso_sample_spec.h` to define the new enum and default:

```c
typedef enum {
    DSO_SAMPLE_FORMAT_10BIT = 0,
    DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED = 1,
} DSO_SampleFormat;

#define DSO_SAMPLE_FORMAT_DEFAULT DSO_SAMPLE_FORMAT_10BIT
```

Change the first entry in `User/dso/dso_sample_spec.c` to:

```c
{
    .adc_bits = 10u,
    .adc_max = 1023u,
    .midscale = 512u,
    .vref = 3.3f,
    .lane_count = 2u,
    .ch2_mode = DSO_CH2_MODE_REALTIME,
    .interleaved = true,
    .start_immediately = true,
},
```

Change `User/dso/dso_acquire_control.c` so both 10-bit and 8-bit allow `HIRES`:

```c
static bool format_supports_hires(void)
{
    DSO_SampleFormat fmt = DSO_SampleSpec_GetActiveFormat();
    return (fmt == DSO_SAMPLE_FORMAT_10BIT) ||
           (fmt == DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
}
```

Then use `format_supports_hires()` in `clamp_acquire_mode()` and `DSO_AcquireCtrl_GetNextAcquireMode()`.

Change `User/ui/ui_system_setup.c` label and cycle logic to:

```c
if (cur_fmt == DSO_SAMPLE_FORMAT_10BIT) {
    next_fmt  = DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED;
    next_topo = DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED;
} else if (cur_topo == DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED) {
    next_fmt  = DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED;
    next_topo = DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED;
} else {
    next_fmt  = DSO_SAMPLE_FORMAT_10BIT;
    next_topo = DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED;
}
```

and:

```c
if (pend == DSO_SAMPLE_FORMAT_10BIT) {
    base = "10-bit";
} else if (pto == DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED) {
    base = "8-bit (2L)";
} else {
    base = "8-bit (3L)";
}
```

- [ ] **Step 4: Rebuild and run the focused contract tests**

Run: `cl /nologo /I. /IUser tests\\test_dso_sample_spec.c User\\dso\\dso_sample_spec.c /Fe:tests\\test_dso_sample_spec_new.exe`

Then run: `.\tests\test_dso_sample_spec_new.exe`

Expected: `all sample spec tests passed`

Run: `cl /nologo /I. /IUser tests\\test_dso_acquire_control.c User\\dso\\dso_acquire_control.c User\\dso\\dso_sample_spec.c /Fe:tests\\test_dso_acquire_control_new.exe`

Then run: `.\tests\test_dso_acquire_control_new.exe`

Expected: `all acquire control tests passed`

Run: `cl /nologo /I. /IUser tests\\test_ui_system_setup_10bit.c User\\ui\\ui_system_setup.c User\\dso\\dso_sample_spec.c /Fe:tests\\test_ui_system_setup_10bit_new.exe`

Then run: `.\tests\test_ui_system_setup_10bit_new.exe`

Expected: `all ui system setup 10bit tests passed`

- [ ] **Step 5: Commit**

```bash
git add tests/test_dso_sample_spec.c tests/test_dso_acquire_control.c tests/test_ui_system_setup_10bit.c User/dso/dso_sample_spec.h User/dso/dso_sample_spec.c User/dso/dso_acquire_control.c User/ui/ui_system_setup.h User/ui/ui_system_setup.c
git commit -m "dso: switch public mode contract to 10-bit"
```

### Task 2: Add a pure 10-bit stream helper module for CH1 merge and ring-copy semantics

**Files:**
- Create: `tests/test_dso_acq10_stream.c`
- Create: `User/dso/dso_acq10_stream.h`
- Create: `User/dso/dso_acq10_stream.c`

- [ ] **Step 1: Write failing host tests for ordered interleave and wraparound copy**

Create `tests/test_dso_acq10_stream.c` with these required cases:

```c
static int test_interleave_dual_lanes_preserves_lane_order(void)
{
    static const uint16_t lane0[] = {100u, 200u, 300u, 400u};
    static const uint16_t lane1[] = {110u, 210u, 310u, 410u};
    uint16_t merged[8] = {0};

    DSO_Acq10Stream_InterleaveDual(lane0, lane1, 4u, merged);

    if (merged[0] != 100u || merged[1] != 110u ||
        merged[2] != 200u || merged[3] != 210u ||
        merged[4] != 300u || merged[5] != 310u ||
        merged[6] != 400u || merged[7] != 410u) {
        fprintf(stderr, "interleave order mismatch\n");
        return 1;
    }
    return 0;
}
```

and:

```c
static int test_copy_new_merged_wraps_cleanly(void)
{
    static const uint16_t lane0[8] = {10u, 20u, 30u, 40u, 50u, 60u, 70u, 80u};
    static const uint16_t lane1[8] = {11u, 21u, 31u, 41u, 51u, 61u, 71u, 81u};
    uint16_t out[8] = {0};
    uint32_t n = DSO_Acq10Stream_CopyNewMerged(6u, 2u, 8u, lane0, lane1, out, 8u);

    if (n != 8u || out[0] != 70u || out[1] != 71u || out[6] != 20u || out[7] != 21u) {
        fprintf(stderr, "wrapped merged copy mismatch n=%u\n", (unsigned)n);
        return 1;
    }
    return 0;
}
```

- [ ] **Step 2: Build the new stream-helper test and verify it fails**

Run: `cl /nologo /I. /IUser tests\\test_dso_acq10_stream.c User\\dso\\dso_acq10_stream.c /Fe:tests\\test_dso_acq10_stream_new.exe`

Expected: build failure because the new helper module does not exist yet.

- [ ] **Step 3: Implement the minimal helper API**

Create `User/dso/dso_acq10_stream.h`:

```c
#ifndef DSO_ACQ10_STREAM_H
#define DSO_ACQ10_STREAM_H

#include <stdint.h>

void DSO_Acq10Stream_InterleaveDual(const uint16_t *lane0,
                                    const uint16_t *lane1,
                                    uint32_t pair_count,
                                    uint16_t *out_samples);
uint32_t DSO_Acq10Stream_CopyRecentMerged(uint32_t dma_pos,
                                          uint32_t ring_count,
                                          const uint16_t *lane0,
                                          const uint16_t *lane1,
                                          uint16_t *out_samples,
                                          uint32_t max_samples);
uint32_t DSO_Acq10Stream_CopyNewMerged(uint32_t prev_dma_pos,
                                       uint32_t dma_pos,
                                       uint32_t ring_count,
                                       const uint16_t *lane0,
                                       const uint16_t *lane1,
                                       uint16_t *out_samples,
                                       uint32_t max_samples);

#endif /* DSO_ACQ10_STREAM_H */
```

Implement `User/dso/dso_acq10_stream.c` with a single ordered contract:

```c
void DSO_Acq10Stream_InterleaveDual(const uint16_t *lane0,
                                    const uint16_t *lane1,
                                    uint32_t pair_count,
                                    uint16_t *out_samples)
{
    for (uint32_t i = 0u; i < pair_count; ++i) {
        out_samples[i * 2u] = lane0[i];
        out_samples[i * 2u + 1u] = lane1[i];
    }
}
```

Use the same ordered pair logic in both `CopyRecentMerged()` and `CopyNewMerged()`.

- [ ] **Step 4: Rebuild and run the stream-helper test**

Run: `cl /nologo /I. /IUser tests\\test_dso_acq10_stream.c User\\dso\\dso_acq10_stream.c /Fe:tests\\test_dso_acq10_stream_new.exe`

Then run: `.\tests\test_dso_acq10_stream_new.exe`

Expected: `all acq10 stream tests passed`

- [ ] **Step 5: Commit**

```bash
git add tests/test_dso_acq10_stream.c User/dso/dso_acq10_stream.h User/dso/dso_acq10_stream.c
git commit -m "dso: add 10-bit stream helpers"
```

### Task 3: Create the 10-bit lane-calibration module and replace the HF calibration contract

**Files:**
- Create: `tests/test_dso_10bit_lane_cal.c`
- Modify: `tests/test_dso_hf_pipeline.c`
- Create: `User/dso/dso_10bit_lane_cal.h`
- Create: `User/dso/dso_10bit_lane_cal.c`

- [ ] **Step 1: Port the old 12-bit calibration tests to the new 10-bit API**

Create `tests/test_dso_10bit_lane_cal.c` with the same algorithm-level expectations as the old interleave-cal tests:

```c
static int test_apply_coeffs_removes_lane1_error(void)
{
    uint16_t clean[TEST_COUNT];
    uint16_t distorted[TEST_COUNT];
    DSO_10bitLaneCalCoeffs coeffs = {
        .offset_lsb = 18.0f,
        .gain = 1.045f,
        .skew = 0.30f,
        .valid = true,
    };

    build_clean_sine(clean, TEST_COUNT, 512.0f, 220.0f, 77.0f);
    distort_interleaved(clean, distorted, TEST_COUNT, coeffs.offset_lsb, coeffs.gain, coeffs.skew, 1023.0f);

    DSO_10bitLaneCal_ApplyWithCoeffs(distorted, TEST_COUNT, &coeffs, 1023u);
    return assert_error_improved(clean, distorted, TEST_COUNT);
}
```

Modify `tests/test_dso_hf_pipeline.c` to include `dso/dso_10bit_lane_cal.h` instead of `dso/dso_interleave_cal.h`, and set:

```c
DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_10BIT);
```

- [ ] **Step 2: Build the 10-bit calibration tests and verify they fail**

Run: `cl /nologo /I. /IUser tests\\test_dso_10bit_lane_cal.c User\\dso\\dso_10bit_lane_cal.c /Fe:tests\\test_dso_10bit_lane_cal_new.exe`

Expected: build failure because the new module does not exist yet.

Run: `cl /nologo /I. /IUser tests\\test_dso_hf_pipeline.c User\\dso\\dso_10bit_lane_cal.c User\\dso\\dso_render_policy.c /Fe:tests\\test_dso_hf_pipeline_new.exe`

Expected: build or runtime failure because `test_dso_hf_pipeline.c` still targets the old calibration API.

- [ ] **Step 3: Implement the 10-bit calibration API with persistent state**

Create `User/dso/dso_10bit_lane_cal.h`:

```c
typedef struct {
    float offset_lsb;
    float gain;
    float skew;
    bool valid;
} DSO_10bitLaneCalCoeffs;

typedef struct {
    DSO_10bitLaneCalCoeffs coeffs;
    uint8_t state;
    uint8_t error;
    bool valid;
    uint8_t progress_pct;
    uint8_t good_frames;
    uint8_t target_frames;
    float last_freq_hz;
} DSO_10bitLaneCalInfo;

void DSO_10bitLaneCal_Init(void);
void DSO_10bitLaneCal_RequestRun(void);
void DSO_10bitLaneCal_Cancel(void);
void DSO_10bitLaneCal_GetInfo(DSO_10bitLaneCalInfo *out);
bool DSO_10bitLaneCal_IsValid(void);
bool DSO_10bitLaneCal_ShouldApply(void);
void DSO_10bitLaneCal_ApplyInPlace(uint16_t *samples, uint32_t count, uint16_t adc_max);
void DSO_10bitLaneCal_ApplyWithCoeffs(uint16_t *samples,
                                      uint32_t count,
                                      const DSO_10bitLaneCalCoeffs *coeffs,
                                      uint16_t adc_max);
bool DSO_10bitLaneCal_SolveFrame(const uint16_t *samples,
                                 uint32_t count,
                                 DSO_10bitLaneCalCoeffs *out);
void DSO_10bitLaneCal_ServiceFrame(const uint16_t *samples,
                                   uint32_t count,
                                   const DSO_ProcessAnalysis *analysis);
```

Implement `User/dso/dso_10bit_lane_cal.c` by porting the old `dso_interleave_cal.c` math and storage shape, but change:

- format gating from `DSO_SAMPLE_FORMAT_12BIT_STABLE` to `DSO_SAMPLE_FORMAT_10BIT`
- saturation limits from `4095`-scale assumptions to `adc_max`
- public symbol names from `DSO_InterleaveCal_*` to `DSO_10bitLaneCal_*`

- [ ] **Step 4: Rebuild and run both calibration-focused tests**

Run: `cl /nologo /I. /IUser tests\\test_dso_10bit_lane_cal.c User\\dso\\dso_10bit_lane_cal.c /Fe:tests\\test_dso_10bit_lane_cal_new.exe`

Then run: `.\tests\test_dso_10bit_lane_cal_new.exe`

Expected: `all 10bit lane cal tests passed`

Run: `cl /nologo /I. /IUser tests\\test_dso_hf_pipeline.c User\\dso\\dso_10bit_lane_cal.c User\\dso\\dso_render_policy.c /Fe:tests\\test_dso_hf_pipeline_new.exe`

Then run: `.\tests\test_dso_hf_pipeline_new.exe`

Expected: `all hf pipeline tests passed`

- [ ] **Step 5: Commit**

```bash
git add tests/test_dso_10bit_lane_cal.c tests/test_dso_hf_pipeline.c User/dso/dso_10bit_lane_cal.h User/dso/dso_10bit_lane_cal.c
git commit -m "dso: add 10-bit lane calibration"
```

### Task 4: Add HS10 enhanced reconstruction and a dedicated 10-bit timebase path

**Files:**
- Create: `tests/test_dso_hs10_render.c`
- Create: `tests/test_dso_timebase_10bit.c`
- Create: `User/dso/dso_hs10_render.h`
- Create: `User/dso/dso_hs10_render.c`
- Modify: `User/dso/dso_render_policy.h`
- Modify: `User/dso/dso_render_policy.c`
- Modify: `User/dso/dso_timebase.h`
- Modify: `User/dso/dso_timebase.c`

- [ ] **Step 1: Write failing tests for HS10 readiness and 10-bit table selection**

Create `tests/test_dso_hs10_render.c` with cases like:

```c
static int test_hs10_render_builds_for_repetitive_mid_density_input(void)
{
    DSO_HS10RenderState state = {0};
    uint16_t samples[2048];
    uint16_t display[DSO_DISPLAY_WIDTH];

    build_repetitive_sine(samples, 2048u, 512u, 180u, 73.0f);
    DSO_HS10Render_Accumulate(&state, samples, 2048u, 2500000u, 73000.0f);

    if (!DSO_HS10Render_IsReady(&state)) {
        fprintf(stderr, "hs10 render did not become ready\n");
        return 1;
    }
    if (!DSO_HS10Render_Render(&state, 500000.0f, display, DSO_DISPLAY_WIDTH)) {
        fprintf(stderr, "hs10 render did not produce display samples\n");
        return 1;
    }
    return 0;
}
```

Create `tests/test_dso_timebase_10bit.c` using `tests/dso_acq_timebase_stubs.c` and assert:

```c
DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_10BIT);
DSO_Timebase_Init();
DSO_Timebase_SetIndex(12u);
(void)DSO_Timebase_ApplyPending();
```

and require `meta.recon_mode == DSO_RECON_MODE_HS10_ENHANCED` under repetitive, mid-density 10-bit conditions.

- [ ] **Step 2: Build the new render/timebase tests and verify they fail**

Run: `cl /nologo /I. /IUser tests\\test_dso_hs10_render.c User\\dso\\dso_hs10_render.c /Fe:tests\\test_dso_hs10_render_new.exe`

Expected: build failure because `dso_hs10_render` does not exist yet.

Run: `cl /nologo /I. /IUser tests\\dso_acq_timebase_stubs.c tests\\test_dso_timebase_10bit.c User\\dso\\dso_timebase.c User\\dso\\dso_sample_spec.c User\\dso\\dso_hs10_render.c User\\dso\\dso_hs8_render.c User\\dso\\dso_render_policy.c /Fe:tests\\test_dso_timebase_10bit_new.exe`

Expected: build or runtime failure because the 10-bit timebase path does not exist yet.

- [ ] **Step 3: Implement the new render policy and timebase path**

Add a new recon enum in `User/dso/dso_render_policy.h`:

```c
typedef enum {
    DSO_RECON_MODE_RAW_LINEAR = 0,
    DSO_RECON_MODE_RAW_LIMITED = 1,
    DSO_RECON_MODE_ETS = 2,
    DSO_RECON_MODE_HS8_ENHANCED = 3,
    DSO_RECON_MODE_HS10_ENHANCED = 4,
} DSO_ReconMode;
```

Create `User/dso/dso_hs10_render.h` with the same role split as `dso_hs8_render.h`, but dedicated names:

```c
typedef struct {
    float bin_sum[DSO_HS10_RENDER_PHASE_BINS];
    uint16_t bin_count[DSO_HS10_RENDER_PHASE_BINS];
    float locked_freq_hz;
    bool ready;
} DSO_HS10RenderState;
```

In `User/dso/dso_timebase.c`:

- add `static const DSO_TimebaseEntry s_table_10bit[...]`
- add `DSO_HS10RenderState hs10[2];` to `DSO_TimebaseState`
- change `active_table()` so non-8-bit uses `s_table_10bit`
- add a 10-bit render branch that uses `DSO_HS10Render_*` without changing any HS8 branch logic

Use this table selector:

```c
static const DSO_TimebaseEntry *active_table(void)
{
    return (DSO_SampleSpec_GetActiveFormat() == DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED)
         ? active_8bit_table()
         : s_table_10bit;
}
```

- [ ] **Step 4: Rebuild and run the new render/timebase tests, then rerun the existing 8-bit timebase regression**

Run: `cl /nologo /I. /IUser tests\\test_dso_hs10_render.c User\\dso\\dso_hs10_render.c /Fe:tests\\test_dso_hs10_render_new.exe`

Then run: `.\tests\test_dso_hs10_render_new.exe`

Expected: `all hs10 render tests passed`

Run: `cl /nologo /I. /IUser tests\\dso_acq_timebase_stubs.c tests\\test_dso_timebase_10bit.c User\\dso\\dso_timebase.c User\\dso\\dso_sample_spec.c User\\dso\\dso_hs10_render.c User\\dso\\dso_hs8_render.c User\\dso\\dso_render_policy.c /Fe:tests\\test_dso_timebase_10bit_new.exe`

Then run: `.\tests\test_dso_timebase_10bit_new.exe`

Expected: `all timebase 10bit tests passed`

Run: `.\tests\test_dso_timebase_8bit.exe`

Expected: existing 8-bit timebase regression still passes unchanged.

- [ ] **Step 5: Commit**

```bash
git add tests/test_dso_hs10_render.c tests/test_dso_timebase_10bit.c User/dso/dso_hs10_render.h User/dso/dso_hs10_render.c User/dso/dso_render_policy.h User/dso/dso_render_policy.c User/dso/dso_timebase.h User/dso/dso_timebase.c
git commit -m "dso: add 10-bit render and timebase path"
```

### Task 5: Implement the HAL-backed 10-bit backend and wire the acquisition facade

**Files:**
- Create: `tests/test_dso_acq10_api_contract.c`
- Create: `User/dso/dso_acq_10bit.h`
- Create: `User/dso/dso_acq_10bit.c`
- Modify: `User/dso/dso_acq.h`
- Modify: `User/dso/dso_acq.c`

- [ ] **Step 1: Write the failing API contract test for the new backend**

Create `tests/test_dso_acq10_api_contract.c`:

```c
#include "dso/dso_acq_10bit.h"

int main(void)
{
    DSO_AcqDebugSnapshot snap = {0};
    DSO_Acq10_Init();
    DSO_Acq10_GetDebugSnapshot(&snap);
    return 0;
}
```

- [ ] **Step 2: Build the API contract test and verify it fails**

Run: `cl /nologo /I. /IUser tests\\test_dso_acq10_api_contract.c /Fe:tests\\test_dso_acq10_api_contract_new.exe`

Expected: build failure because `dso_acq_10bit.h` does not exist yet.

- [ ] **Step 3: Implement the 10-bit backend and acquisition dispatch**

Create `User/dso/dso_acq_10bit.h` with a backend API parallel to what `dso_acq.c` currently needs:

```c
void DSO_Acq10_Init(void);
void DSO_Acq10_Deinit(void);
void DSO_Acq10_Service(void);
void DSO_Acq10_Start(void);
void DSO_Acq10_Stop(void);
bool DSO_Acq10_IsRunning(void);
uint32_t DSO_Acq10_GetPos(void);
uint32_t DSO_Acq10_GetCh2Pos(void);
const volatile uint32_t *DSO_Acq10_GetBuf(void);
const volatile uint16_t *DSO_Acq10_GetCh2Buf(void);
uint32_t DSO_Acq10_CopyRecentSamples(uint32_t dma_pos,
                                     int16_t cal_offset,
                                     uint16_t *out_samples,
                                     uint32_t max_samples);
uint32_t DSO_Acq10_CopyNewSamples(uint32_t prev_dma_pos,
                                  uint32_t dma_pos,
                                  int16_t cal_offset,
                                  uint16_t *out_samples,
                                  uint32_t max_samples);
uint32_t DSO_Acq10_CopyRecentCh2Samples(uint32_t dma_pos,
                                        uint16_t *out_samples,
                                        uint32_t max_samples);
uint32_t DSO_Acq10_CopyNewCh2Samples(uint32_t prev_dma_pos,
                                     uint32_t dma_pos,
                                     uint16_t *out_samples,
                                     uint32_t max_samples);
uint32_t DSO_Acq10_GetSampleRate(void);
uint32_t DSO_Acq10_GetCh2SampleRate(void);
uint32_t DSO_Acq10_GetTimClkHz(void);
uint32_t DSO_Acq10_GetTimArr(void);
void DSO_Acq10_SetTimArr(uint32_t new_arr);
bool DSO_Acq10_GetDebugSnapshot(DSO_AcqDebugSnapshot *out);
```

In `User/dso/dso_acq_10bit.c`, implement:

- `TIM1 CC1 -> ADC1 PA6 channel 3`
- `TIM1 CC2 -> ADC2 PA6 channel 3`
- `TIM1 CC3 -> ADC3 PC2 channel 0`
- lane DMA rings for `ADC1` and `ADC2`
- single DMA ring for `ADC3`
- CH1 merged copy helpers through `dso_acq10_stream.c`

In `User/dso/dso_acq.c`, replace every old 12-bit branch with a 10-bit branch. The core dispatch shape should become:

```c
if (s_sample_format == DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED) {
    DSO_Acq8_Init();
    (void)DSO_Acq8_RequestTopology((DSO_Acq8Topology)s_pending_hs8_topology);
} else {
    DSO_Acq10_Init();
}
```

and:

```c
if (s_sample_format == DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED) {
    return DSO_Acq8_GetPos();
}
return DSO_Acq10_GetPos();
```

Repeat that replacement for `Start/Stop/Service/GetBuf/GetCh2Buf/GetSampleRate/GetTimArr/SetTimArr/CopyRecent*/CopyNew*`.

- [ ] **Step 4: Rebuild the API contract test and do a Keil `Flash` build**

Run: `cl /nologo /I. /IUser tests\\test_dso_acq10_api_contract.c /Fe:tests\\test_dso_acq10_api_contract_new.exe`

Expected: build succeeds.

Build firmware in Keil using `Project/MDK-ARM(uV5)/project.uvprojx`, target `Flash`.

Expected: Keil build succeeds and `Project/output(mdk).hex` is regenerated intentionally.

- [ ] **Step 5: Commit**

```bash
git add tests/test_dso_acq10_api_contract.c User/dso/dso_acq_10bit.h User/dso/dso_acq_10bit.c User/dso/dso_acq.h User/dso/dso_acq.c
git commit -m "dso: add 10-bit acquisition backend"
```

### Task 6: Integrate 10-bit calibration/render policy into the engine and remove the old 12-bit calibration path

**Files:**
- Modify: `User/dso/dso_engine.c`
- Modify: `User/dso/dso_process.h`
- Modify: `User/ui/UI_WaveformCtrl.h`
- Modify: `User/ui/UI_WaveformCtrl.c`
- Modify: `User/ui/ui_screens.c`
- Modify: `User/ui/MainTask.c`
- Delete: `User/dso/dso_interleave_cal.h`
- Delete: `User/dso/dso_interleave_cal.c`

- [ ] **Step 1: Replace the old calibration include and route all calibration calls through the new 10-bit module**

In `User/dso/dso_engine.c`, replace:

```c
#include "dso_interleave_cal.h"
```

with:

```c
#include "dso_10bit_lane_cal.h"
```

Add explicit format helpers:

```c
static bool engine_is_10bit_active(void)
{
    return DSO_SampleSpec_GetActiveFormat() == DSO_SAMPLE_FORMAT_10BIT;
}

static bool engine_supports_raw_preprocess(void)
{
    return engine_is_10bit_active() || engine_is_hs8_active();
}
```

Use `engine_is_10bit_active()` in:

- `engine_is_ch1_cal_ready()`
- `engine_apply_ch1_cal_if_ready()`
- `engine_analyze_and_calibrate_ch1()`
- `engine_prepare_ch1_render_source()`
- `fill_frame_metadata()`
- CH1 measurement-source selection near `DSO_Process_MeasureSamples()` vs `DSO_Process_MeasureDisplay()`

The key behavioral change is:

```c
if (engine_is_10bit_active() || engine_is_hs8_active()) {
    DSO_Process_MeasureSamples(serial_source_samples, serial_source_count, &frame);
} else {
    DSO_Process_MeasureDisplay(display_samples, meas_hi_ch1, DSO_DISPLAY_WIDTH, &frame);
}
```

- [ ] **Step 2: Replace the UI-facing calibration controls and display reads**

In `User/ui/UI_WaveformCtrl.c`, replace:

```c
void UI_Waveform_RequestInterleaveCalibration(void)
{
    DSO_InterleaveCal_RequestRun();
}
```

with:

```c
void UI_Waveform_RequestInterleaveCalibration(void)
{
    DSO_10bitLaneCal_RequestRun();
}
```

and similarly replace the cancel path.

In `User/ui/ui_screens.c` and `User/ui/MainTask.c`, remove the `dso_interleave_cal.h` include and rely only on `DSO_Frame` metadata fields already populated by the engine. Do not rename `frame->ch1_interleave_cal_valid` in this phase; keep the field for UI compatibility.

- [ ] **Step 3: Delete the old 12-bit calibration module once no references remain**

Delete:

```text
User/dso/dso_interleave_cal.h
User/dso/dso_interleave_cal.c
```

Before deleting, verify:

```powershell
rg -n "dso_interleave_cal|DSO_InterleaveCal" User tests
```

Expected before delete: only old references in files you are actively replacing.

Expected after delete and final integration edits: no matches.

- [ ] **Step 4: Rebuild the updated HF pipeline test, rerun the existing serial/debug regression, and do a Keil `Flash` build**

Run: `cl /nologo /I. /IUser tests\\test_dso_hf_pipeline.c User\\dso\\dso_10bit_lane_cal.c User\\dso\\dso_render_policy.c /Fe:tests\\test_dso_hf_pipeline_new.exe`

Then run: `.\tests\test_dso_hf_pipeline_new.exe`

Expected: `all hf pipeline tests passed`

Run: `.\tests\test_dso_serial_debug.exe`

Expected: existing serial debug regression still passes.

Build firmware in Keil using `Project/MDK-ARM(uV5)/project.uvprojx`, target `Flash`.

Expected: build succeeds with the old `dso_interleave_cal` files removed.

- [ ] **Step 5: Commit**

```bash
git add User/dso/dso_engine.c User/dso/dso_process.h User/ui/UI_WaveformCtrl.h User/ui/UI_WaveformCtrl.c User/ui/ui_screens.c User/ui/MainTask.c
git rm User/dso/dso_interleave_cal.h User/dso/dso_interleave_cal.c
git commit -m "dso: switch engine and ui to 10-bit calibration"
```

### Task 7: Update debug output, register new files in Keil, and run full regression

**Files:**
- Modify: `User/dso/dso_serial_debug.c`
- Modify: `User/dso/dso_serial_debug_format.c`
- Modify: `tests/test_dso_process_hs8_freq_policy.c`
- Modify: `Project/MDK-ARM(uV5)/project.uvprojx`

- [ ] **Step 1: Update 10-bit-facing debug strings and non-HS8 policy tests**

In `tests/test_dso_process_hs8_freq_policy.c`, replace the old non-HS8 assertion:

```c
DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_10BIT);
```

and keep the expectation that non-HS8 streams bypass the dual-pair-lock rejection.

In `User/dso/dso_serial_debug.c` and `User/dso/dso_serial_debug_format.c`, change any remaining 12-bit-facing labels or assumptions so the base mode reports `10-bit` and the debug snapshot uses the 10-bit backend sample rate / timer / CH2 rate.

- [ ] **Step 2: Register the new 10-bit sources in Keil and remove the old 12-bit calibration source**

In `Project/MDK-ARM(uV5)/project.uvprojx`, insert file entries next to the existing DSO source block:

```xml
<File>
  <FileName>dso_acq10_stream.c</FileName>
  <FileType>1</FileType>
  <FilePath>..\..\User\dso\dso_acq10_stream.c</FilePath>
</File>
<File>
  <FileName>dso_acq_10bit.c</FileName>
  <FileType>1</FileType>
  <FilePath>..\..\User\dso\dso_acq_10bit.c</FilePath>
</File>
<File>
  <FileName>dso_10bit_lane_cal.c</FileName>
  <FileType>1</FileType>
  <FilePath>..\..\User\dso\dso_10bit_lane_cal.c</FilePath>
</File>
<File>
  <FileName>dso_hs10_render.c</FileName>
  <FileType>1</FileType>
  <FilePath>..\..\User\dso\dso_hs10_render.c</FilePath>
</File>
```

and remove the old block:

```xml
<File>
  <FileName>dso_interleave_cal.c</FileName>
  <FileType>1</FileType>
  <FilePath>..\..\User\dso\dso_interleave_cal.c</FilePath>
</File>
```

- [ ] **Step 3: Build the updated non-HS8 policy test and run the full host regression set touched by this migration**

Run: `cl /nologo /I. /IUser tests\\dso_acq_timebase_stubs.c tests\\test_dso_process_hs8_freq_policy.c User\\dso\\dso_process.c User\\dso\\dso_sample_spec.c /Fe:tests\\test_dso_process_hs8_freq_policy_new.exe`

Then run: `.\tests\test_dso_process_hs8_freq_policy_new.exe`

Expected: `all hs8 dual frequency policy tests passed`

Run the focused changed/new regressions:

```powershell
.\tests\test_dso_sample_spec_new.exe
.\tests\test_dso_acquire_control_new.exe
.\tests\test_ui_system_setup_10bit_new.exe
.\tests\test_dso_acq10_stream_new.exe
.\tests\test_dso_10bit_lane_cal_new.exe
.\tests\test_dso_hs10_render_new.exe
.\tests\test_dso_timebase_10bit_new.exe
.\tests\test_dso_hf_pipeline_new.exe
.\tests\test_dso_process_hs8_freq_policy_new.exe
.\tests\test_dso_serial_debug.exe
.\tests\test_dso_timebase_8bit.exe
.\tests\test_dso_acq8_topology_contract.exe
.\tests\test_dso_acq8_tim_trigger.exe
.\tests\test_dso_acq8_sample_unpack.exe
.\tests\test_dso_acq8_dual_dma_contract.exe
```

Expected: every command exits `0`.

- [ ] **Step 4: Build Keil `Flash` and refresh the checked-in host executables that changed**

Build firmware in Keil using `Project/MDK-ARM(uV5)/project.uvprojx`, target `Flash`.

Expected: successful build and intentional `Project/output(mdk).hex` refresh if the post-build step runs.

Refresh these host binaries from the `_new.exe` builds by replacing the checked-in executables when they are the canonical runners:

```text
tests/test_dso_sample_spec.exe
tests/test_dso_acquire_control.exe
tests/test_dso_hf_pipeline.exe
tests/test_dso_process_hs8_freq_policy.exe
```

Check in these newly introduced host binaries using the `_new.exe` naming already present elsewhere in `tests/`:

```text
tests/test_ui_system_setup_10bit_new.exe
tests/test_dso_acq10_stream_new.exe
tests/test_dso_10bit_lane_cal_new.exe
tests/test_dso_hs10_render_new.exe
tests/test_dso_timebase_10bit_new.exe
tests/test_dso_acq10_api_contract_new.exe
```

- [ ] **Step 5: Commit**

```bash
git add User/dso/dso_serial_debug.c User/dso/dso_serial_debug_format.c tests/test_dso_process_hs8_freq_policy.c Project/MDK-ARM\(uV5\)/project.uvprojx tests/test_dso_sample_spec.exe tests/test_dso_acquire_control.exe tests/test_dso_hf_pipeline.exe tests/test_dso_process_hs8_freq_policy.exe
git commit -m "dso: register 10-bit mode and refresh regression artifacts"
```
