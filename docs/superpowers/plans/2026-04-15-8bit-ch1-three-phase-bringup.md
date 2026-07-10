# 8-Bit CH1 Three-Phase Bring-Up Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a new 8-bit CH1 three-phase interleaved acquisition path on top of the current STM32H743 codebase while preserving the working `single-safe` and current `dual-interleaved` paths during bring-up.

**Architecture:** Keep the existing 8-bit backend contract intact and add a third topology for `ADC1 + ADC2 + ADC3` independent triggered sampling. Localize the main implementation to `dso_acq_8bit.c`, expose the new lane count through the existing runtime interfaces, then only widen downstream modules where the current code is explicitly hard-coded to dual-lane assumptions. Do not remove the current dual path until the new three-phase path is electrically verified on hardware.

**Tech Stack:** C, STM32H7 HAL, FreeRTOS firmware code, host-side C tests, UART debug formatting, Keil/IAR embedded firmware projects.

---

### Task 1: Lock the scope and the hardware assumptions to the current codebase

**Files:**
- Modify: `docs/superpowers/plans/2026-04-15-8bit-ch1-three-phase-bringup.md`
- Reference only: `User/dso/dso_acq_8bit.c`
- Reference only: `User/dso/dso_config.h`
- Reference only: `User/bsp/bsp.c`
- Reference only: `User/bsp/src/bsp_cpu_adc.c`

- [ ] **Step 1: Freeze the current scope**

The intended scope is:

- keep `DSO_ACQ8_TOPOLOGY_SINGLE_SAFE`
- keep `DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED`
- add one new topology for three-phase CH1 capture
- do not remove the dual path in the same patch
- do not redesign the 12-bit pipeline
- do not touch UI behavior beyond exposing valid runtime metadata

- [ ] **Step 2: Record the current code facts that the implementation must respect**

Use these facts as the baseline:

```text
System clock tree today:
  SYSCLK = 400 MHz
  HCLK   = 200 MHz
  APB2   = 100 MHz
  TIM1 kernel clock seen by current code = 200 MHz

Current 8-bit backend today:
  topology ids: single-safe / dual-interleaved
  single-safe: TIM8 -> ADC1
  dual-interleaved: TIM1 CC1 -> ADC1, TIM1 CC2 -> ADC2
  ADC3 not part of the active 8-bit CH1 capture path

Current lane-count model today:
  8-bit lane count comes from DSO_Acq8_GetLaneCount()
  timebase / lane-cal / debug already tolerate lane_count == 3 in several places
  process frequency guard still contains a dual-specific policy
```

- [ ] **Step 3: Record the verified CH1 third-lane pin mapping**

The current code evidence can now be resolved against the STM32H743 package documentation:

```text
Evidence A:
  User/dso/dso_acq_8bit.c configures PA6 and PC3 as analog-capable pins

Evidence B:
  User/bsp/src/bsp_cpu_adc.c documents PC3 = ADC123_IN13

Evidence C:
  User/dso/dso_acq.c currently configures ADC3 regular channel as ADC_CHANNEL_0 in 12-bit mode

Official package mapping:
  PC3   = ADC12_INP13
  PC3_C = ADC3_INP1

Current board-side implication:
  User/dso/dso_acq_8bit.c closes the PC3 analog switch, so the external CH1 third lane can reach PC3_C
```

Implementation rule:

- for the new 8-bit three-phase CH1 path, `ADC3` must be configured for `ADC_CHANNEL_1`
- `ADC1` and `ADC2` stay on the PA6 / `ADC12_INP3` lane used today
- the current `ADC_CHANNEL_0` setting in `User/dso/dso_acq.c` is not a valid reference for the new CH1 third-lane path

- [ ] **Step 4: Record the clock-model mismatch that must be fixed before implementation**

The user target, the current code, and the official datasheet do not match:

```text
User target:
  ADC kernel = 54 MHz
  per-ADC rate = 9 MSPS
  total rate = 27 MSPS
  TIM1 = 108 MHz or 216 MHz

Current code:
  ADC kernel = 50 MHz
  TIM1 runtime clock = 200 MHz
  8-bit SAR model = 3.5 cycles (macro = 35 x10)

Official STM32H743xI/G datasheet guidance:
  ADC clock frequency max = 36 MHz for rev Y electrical characteristics
  8-bit fast-channel single-ADC rate = 6.0 MSPS at fADC = 36 MHz
  Reference-manual timing model = TCONV = TSMPL + TSAR, with 8-bit TSAR = 4.5 cycles and SMP min = 1.5 cycles
```

This means implementation must first choose one of two strategies:

- Strategy A: keep the current 200 MHz TIM1 and bring up three-phase at an achievable integer-tick rate first
- Strategy B: rework the clock tree so TIM1 is actually 108 MHz or 216 MHz and update the ADC PLL2, but keep the ADC clock inside a verified electrical-spec limit unless intentionally doing overclock experiments

For the first implementation pass, use Strategy A.

Treat `54 MHz / 9 MSPS per ADC / 27 MSPS total` as an experimental overclock target, not the baseline production plan, unless the exact silicon revision and lab validation prove it safe.

### Task 2: Extend the 8-bit topology contract without breaking existing dual-lane code

**Files:**
- Modify: `User/dso/dso_config.h`
- Modify: `User/dso/dso_acq_8bit.c`
- Modify if needed: `User/dso/dso_acq.h`

- [ ] **Step 1: Add a dedicated three-phase topology ID**

Add one new topology constant in `User/dso/dso_config.h`:

```c
#define DSO_ACQ8_TOPOLOGY_SINGLE_SAFE          1u
#define DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED     2u
#define DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED      3u
```

Do not change `DSO_ACQ8_TOPOLOGY_DEFAULT` yet. Keep default on the known-good dual path until hardware verification is done.

- [ ] **Step 2: Update topology helpers in `User/dso/dso_acq_8bit.c`**

Target helpers:

- `sanitize_topology()`
- `topology_is_single_safe()`
- `topology_is_interleaved()`
- `topology_lane_count()`
- `topology_uses_lane2()`
- `topology_uses_tim8()`
- `topology_profile_name()`

Required behavior:

```c
static bool topology_is_tri_interleaved(DSO_Acq8Topology topology)
{
    return topology == DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED;
}

static uint8_t topology_lane_count(DSO_Acq8Topology topology)
{
    if (topology_is_single_safe(topology)) {
        return 1u;
    }
    if (topology_is_tri_interleaved(topology)) {
        return 3u;
    }
    return 2u;
}

static bool topology_uses_lane2(DSO_Acq8Topology topology)
{
    return topology_is_tri_interleaved(topology);
}
```

- [ ] **Step 3: Keep all public API signatures stable**

Do not add new exported acquisition APIs unless blocked. Reuse:

- `DSO_Acq_GetLaneCount()`
- `DSO_Acq_GetHs8Topology()`
- `DSO_Acq_RequestHs8Topology()`
- `DSO_Acq_GetRuntimeInfo()`

This keeps higher-level modules decoupled from the specific topology implementation.

### Task 3: Fix the runtime clock model before wiring the third ADC lane

**Files:**
- Modify: `User/dso/dso_config.h`
- Modify: `User/dso/dso_acq_8bit.c`
- Modify if needed: `User/dso/dso_acq_model.h`
- Test: `tests/dso_acq_timebase_stubs.c`
- Test: `tests/test_dso_timebase_8bit.c`

- [ ] **Step 1: Stop hard-coding the 8-bit ADC kernel clock to 50 MHz**

Replace the current fixed macro with a configurable one:

```c
#ifndef DSO_ACQ8_ADC_INPUT_CLK_HZ
#define DSO_ACQ8_ADC_INPUT_CLK_HZ 54000000u
#endif
```

Then use it consistently in `refresh_runtime_info()`.

- [ ] **Step 2: Add a clear runtime comment for Strategy A bring-up**

Document that the first three-phase bring-up is not required to hit 27 MSPS immediately:

```c
/*
 * Three-phase bring-up keeps the current system clock tree first.
 * TIM1 currently runs from the 200 MHz APB2 timer kernel clock, so
 * 27 MSPS total cannot be represented exactly with an integer ARR that
 * also supports exact 120-degree spacing. We therefore validate the
 * three-lane acquisition path first at the nearest stable integer-tick
 * operating point, then optionally retune the global clock tree later.
 */
```

- [ ] **Step 3: Correct the effective sample-rate math for lane_count == 3**

Current code does:

```c
s_acq8.runtime.sample_rate_hz = 2u * s_acq8.runtime.trigger_rate_hz;
```

Replace with:

```c
uint8_t lane_count = topology_lane_count(s_acq8.active_topology);
s_acq8.runtime.lane_rate_hz = s_acq8.runtime.trigger_rate_hz;
s_acq8.runtime.sample_rate_hz =
    s_acq8.runtime.trigger_rate_hz * (uint32_t)lane_count;
```

- [ ] **Step 4: Fix the timer-kernel-clock helper if future TIMPRE changes are introduced**

Current helper:

```c
static uint32_t timer_clock_from_pclk(uint32_t pclk_hz, uint32_t apb_divider)
{
    return (apb_divider == RCC_HCLK_DIV1) ? pclk_hz : (pclk_hz * 2u);
}
```

This is only correct for the current default `TIMPRE` behavior. Before any clock-tree redesign, replace or annotate it so future 108/216 MHz work does not silently report the wrong rate.

- [ ] **Step 5: Choose the initial three-phase ARR**

With current `TIM1 = 200 MHz`, the first three-phase bring-up should use an integer period divisible by 3.

Recommended first operating points:

```text
period = 24 ticks -> f_trig = 8.333333 MHz -> total = 25.000000 MSPS
period = 21 ticks -> f_trig = 9.523809 MHz -> total = 28.571428 MSPS
```

Use `period = 24` for the first stable bring-up because it is slower and safer.

- [ ] **Step 6: Add a host test for lane-count driven sample-rate math**

Extend `tests/test_dso_timebase_8bit.c` with a case like:

```c
Test_AcqStub_Reset();
Test_AcqStub_SetLaneCount(3u);
Test_AcqStub_SetTimClkHz(200000000u);
DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
DSO_Timebase_Init();
DSO_Timebase_SetIndex(0u);
(void)DSO_Timebase_ApplyPending();

/* For the chosen tri-lane ARR entry, hw_sample_rate must match trig * 3. */
```

### Task 4: Implement the actual three-phase backend in `dso_acq_8bit.c`

**Files:**
- Modify: `User/dso/dso_acq_8bit.c`
- Reuse: `User/dso/dso_lane_merge.c`
- Reuse: `User/dso/dso_lane_merge.h`

- [ ] **Step 1: Add the third DMA ring buffer**

Add a dedicated third lane buffer next to the existing two:

```c
static uint16_t s_dma_lane0[DSO_DMA_BUF_SIZE]
    __attribute__((section(".RAM_D2"), aligned(32)));
static uint16_t s_dma_lane1[DSO_DMA_BUF_SIZE]
    __attribute__((section(".RAM_D2"), aligned(32)));
static uint16_t s_dma_lane2[DSO_DMA_BUF_SIZE]
    __attribute__((section(".RAM_D2"), aligned(32)));
```

If you keep the existing names for lane0/lane1 to minimize churn, only add the new third one and document the mapping clearly.

- [ ] **Step 2: Split the current interleaved backend into dual and tri variants**

Do not overload one function with deep branching. Create two focused helpers:

```c
static void configure_dual_interleaved_backend(void);
static void configure_tri_interleaved_backend(void);
```

Then dispatch from:

```c
static void configure_active_backend(void)
{
    if (topology_is_single_safe(s_acq8.active_topology)) {
        configure_single_safe_backend();
    } else if (topology_is_tri_interleaved(s_acq8.active_topology)) {
        configure_tri_interleaved_backend();
    } else {
        configure_dual_interleaved_backend();
    }
}
```

- [ ] **Step 3: Configure ADC3 as the third CH1 lane**

Inside `configure_tri_interleaved_backend()`:

- initialize `hadc1` with `ADC_EXTERNALTRIG_T1_CC1`
- initialize `hadc2` with `ADC_EXTERNALTRIG_T1_CC2`
- initialize `hadc3` with `ADC_EXTERNALTRIG_T1_CC3`
- force all three to 8-bit, no left shift
- keep all three in independent mode
- configure the correct regular channel for the CH1 third-lane pin
- start three independent calibrations
- attach DMA:
  - ADC1 -> `DMA1_Stream1`
  - ADC2 -> `DMA1_Stream2`
  - ADC3 -> `DMA2_Stream1`

Do not use ADC multimode anywhere in this path.

- [ ] **Step 4: Start and stop all three streams together**

Add focused streaming helpers:

```c
static void start_tri_interleaved_streaming(void)
{
    HAL_ADC_Start_DMA(&s_acq8.hadc1, (uint32_t *)s_dma_lane0, DSO_DMA_BUF_SIZE);
    HAL_ADC_Start_DMA(&s_acq8.hadc2, (uint32_t *)s_dma_lane1, DSO_DMA_BUF_SIZE);
    HAL_ADC_Start_DMA(&s_acq8.hadc3, (uint32_t *)s_dma_lane2, DSO_DMA_BUF_SIZE);
    start_triphasic_timer();
    reset_common_tracking();
}
```

Mirror this with a matching stop helper.

- [ ] **Step 5: Snapshot positions using the existing lane-merge utility**

Replace the dual-only common-position logic with a tri-lane path:

```c
static void snapshot_tri_interleaved_position(void)
{
    uint32_t pos0 = dma_pos_or_zero(&s_acq8.hdma_adc1);
    uint32_t pos1 = dma_pos_or_zero(&s_acq8.hdma_adc2);
    uint32_t pos2 = dma_pos_or_zero(&s_acq8.hdma_adc3);
    DSO_LaneDeltaInfo delta =
        DSO_LaneMerge_CommonDelta3(s_acq8.common_pos,
                                   pos0,
                                   pos1,
                                   pos2,
                                   DSO_DMA_BUF_SIZE);

    s_acq8.snapshot_pos0 = pos0;
    s_acq8.snapshot_pos1 = pos1;
    s_acq8.snapshot_pos2 = pos2;
    s_acq8.snapshot_common_delta = delta.common_delta;
    s_acq8.snapshot_phase_sync_error = delta.phase_sync_error;
    s_acq8.common_pos = ring_start(pos0, delta.lane0_delta);
}
```

Keep the actual common-position choice consistent and documented; the important point is to stop faking lane2 as inactive.

- [ ] **Step 6: Merge recent/new samples with three-lane copy primitives**

For tri-lane paths, replace the hand-written dual merge with `DSO_LaneMerge_CopyAligned3()` or `DSO_Acq8_CopyAligned3Bytes()` depending on the chosen lane storage representation.

Expected shape:

```c
uint32_t lane_count = /* common number of per-lane samples */;
return DSO_LaneMerge_CopyAligned3(start0,
                                  start1,
                                  start2,
                                  lane_count,
                                  DSO_DMA_BUF_SIZE,
                                  lane0_decoded,
                                  lane1_decoded,
                                  lane2_decoded,
                                  out_samples,
                                  max_samples);
```

Do not duplicate a new bespoke 3-lane merge loop if the helper already fits.

### Task 5: Make TIM1 phase placement explicit and robust

**Files:**
- Modify: `User/dso/dso_acq_8bit.c`
- Test: `tests/test_dso_acq8_tim_trigger.c`

- [ ] **Step 1: Keep using the existing triphasic timer start/stop hooks**

Current code already has:

- `config_tim1_phase()`
- `start_triphasic_timer()`
- `stop_triphasic_timer()`

Do not invent parallel timer helpers. Extend the existing ones.

- [ ] **Step 2: Keep the phase offsets centered in the period**

For `period = 24`, use:

```text
CCR1 = 4
CCR2 = 12
CCR3 = 20
```

For `period = 21`, use:

```text
CCR1 = 3
CCR2 = 10
CCR3 = 17
```

Avoid `CCR1 = 0` in the first implementation pass. It is phase-equivalent in discrete time, but it sits on the update boundary and is harder to reason about during bring-up.

- [ ] **Step 3: Replace the implicit helper math with documented lane-count logic**

Update `tim1_cc_pulse()` so the tri-lane case is explicit:

```c
case 3u:
    switch (which) {
    case 0u: return period_clocks / 6u;
    case 1u: return period_clocks / 2u;
    default: return (period_clocks * 5u) / 6u;
    }
```

Keep this discrete-center style and document why it is chosen.

### Task 6: Widen downstream consumers only where they are still dual-specific

**Files:**
- Modify: `User/dso/dso_timebase.c`
- Modify: `User/dso/dso_process.c`
- Modify: `User/dso/dso_serial_debug.c`
- Modify if needed: `User/dso/dso_serial_debug_format.c`

- [ ] **Step 1: Add a tri-lane 8-bit timebase table**

Current code has:

- `s_table_8bit`
- `s_table_8bit_dual`

Add:

```c
static const DSO_TimebaseEntry s_table_8bit_tri[DSO_TIMEBASE_COUNT] = {
    /* entries based on the chosen tri-lane ARR ladder */
};
```

Then switch `active_8bit_table()` to:

```c
uint8_t lane_count = DSO_Acq_GetLaneCount();
if (lane_count >= 3u) {
    return s_table_8bit_tri;
}
return (lane_count > 1u) ? s_table_8bit_dual : s_table_8bit;
```

- [ ] **Step 2: Remove dual-specific naming from the frequency-reliability guard**

Current function:

```c
bool DSO_Process_IsHs8DualFreqReliable(...)
```

Do not delete it blindly. First convert it into a lane-count aware policy:

- lane_count < 2 -> always true
- lane_count == 2 -> keep current pair-match heuristic
- lane_count >= 3 -> skip the dual pair-lock rejection for now, or implement a new tri-lane repeat-lock heuristic in a separate helper

The safe first pass is to bypass the dual-only rejection when `lane_count >= 3`.

- [ ] **Step 3: Keep serial/debug visibility but stop assuming only two active interleaved lanes**

Audit these debug concepts:

- compact phase mode activation
- interleave phase stats
- lane windows
- `HS8D` summary fields

The logging format already supports `lane2`. The main required change is to stop routing tri-lane runs through dual-only phase-stat logic.

### Task 7: Update and extend host-side tests around the new three-phase path

**Files:**
- Modify: `tests/test_dso_acq8_topology_contract.c`
- Modify: `tests/test_dso_acq8_dual_dma_contract.c`
- Modify: `tests/test_dso_acq8_legacy_cleanup.c`
- Modify: `tests/test_dso_process_hs8_freq_policy.c`
- Modify: `tests/test_dso_timebase_8bit.c`
- Reuse: `tests/test_dso_hs8_lane_cal.c`
- Reuse: `tests/test_dso_acq8_sample_unpack.c`
- Reuse: `tests/test_dso_serial_debug.c`

- [ ] **Step 1: Replace the stale “three-phase forbidden” topology contract**

Current test explicitly forbids `DSO_ACQ8_TOPOLOGY_THREE_PHASE`. Replace it with:

```c
#ifndef DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED
#error "DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED must be defined"
#endif
```

And assert topology IDs are distinct.

- [ ] **Step 2: Split the DMA contract into dual and tri assertions**

Do not destroy the existing dual contract. Add tri-specific assertions such as:

- `ADC_EXTERNALTRIG_T1_CC3`
- `DMA_REQUEST_ADC3`
- `DMA2_Stream1`
- `HAL_ADC_Start_DMA(&s_acq8.hadc3`
- `topology_uses_lane2(...) == true` for tri mode

- [ ] **Step 3: Replace stale legacy-cleanup assertions**

The current cleanup test bans `TOPOLOGY_THREE_PHASE`; that is now invalid. Update it so it bans only truly stale names and implementation leftovers, not the new supported tri topology.

- [ ] **Step 4: Keep and rely on the already-useful tri-lane math tests**

These tests are already relevant and should stay green:

- `.\tests\test_dso_hs8_lane_cal_new.exe`
- `.\tests\test_dso_acq8_sample_unpack.exe`
- `.\tests\test_dso_serial_debug.exe`

- [ ] **Step 5: Add one tri-lane policy test for the dual-specific frequency guard**

In `tests/test_dso_process_hs8_freq_policy.c`, add a case that sets:

```c
Test_AcqStub_SetLaneCount(3u);
DSO_SampleSpec_SetActive(DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED);
```

Then verify the dual pair-lock rejection does not falsely reject a tri-lane stream.

### Task 8: Board bring-up sequence after the code compiles and host tests pass

**Files:**
- No new source file required in this task
- Run-time targets: `User/dso/dso_acq_8bit.c`, UART debug output, actual board input path

- [ ] **Step 1: Bring up with the slow integer-tick tri-lane point**

Use:

```text
TIM1 period = 24
CCR1/CCR2/CCR3 = 4/12/20
ADC kernel = 54 MHz
per ADC = 8.333333 MSPS
total = 25.000000 MSPS
```

This is the recommended first electrical validation point.

- [ ] **Step 2: Validate only the hardware plumbing first**

Watch the `HS8R` line and confirm:

- `tim_arr`
- `tim_ccr1/2/3`
- `dma_ndtr0/1/2`
- `adc1/2/3_cfgr`

Expected:

- all three DMA counters move
- all three ADCs trigger from TIM1 compare events
- no immediate overrun storm

- [ ] **Step 3: Validate lane alignment second**

Watch the `HS8D` / `HS8W` lines and confirm:

- `lane0_pos`, `lane1_pos`, `lane2_pos` all advance
- `common_delta` is stable
- `phase_sync_error` stays low
- merged lane windows show the expected 0/120/240 degree staggering

- [ ] **Step 4: Validate analysis and calibration third**

Feed a repetitive sine and confirm:

- `freq_valid == 1`
- `samples_per_cycle` is plausible for the chosen total sample rate
- `HS8CAL` converges for lane1 and lane2
- `HS8` enhanced render can accumulate without lane2 constantly failing

- [ ] **Step 5: Only then retune toward the user target**

After the 25 MSPS tri-lane path is stable:

- decide whether 28.57 MSPS on current clock tree is acceptable
- otherwise create a separate clock-tree task to make TIM1 actually 108 MHz or 216 MHz
- only in that later task update the global system clock and the timer-kernel model

### Task 9: Focused verification commands for this plan

**Files:**
- Verification only

- [ ] **Step 1: Run the host tests that should already stay green**

Run:

```powershell
.\tests\test_dso_hs8_lane_cal_new.exe
.\tests\test_dso_acq8_sample_unpack.exe
.\tests\test_dso_serial_debug.exe
.\tests\test_dso_timebase_8bit.exe
```

Expected:

- all pass before any implementation
- all pass after the implementation, with updates where topology contracts changed

- [ ] **Step 2: Run the contract tests that must be edited as part of this work**

Run:

```powershell
.\tests\test_dso_acq8_topology_contract.exe
.\tests\test_dso_acq8_dual_dma_contract.exe
.\tests\test_dso_process_hs8_freq_policy.exe
```

Expected after implementation:

- topology contract updated for tri support
- DMA contract still green for dual, plus new tri checks
- process policy no longer tri-false-rejects due to dual-specific heuristics

- [ ] **Step 3: Capture a manual firmware diff summary**

Record the final changed files and make sure each one maps back to this scope:

- `User/dso/dso_config.h`
- `User/dso/dso_acq_8bit.c`
- `User/dso/dso_timebase.c`
- `User/dso/dso_process.c`
- `User/dso/dso_serial_debug.c`
- `tests/...`

Do not broaden beyond these unless hardware integration reveals a genuine blocker.
