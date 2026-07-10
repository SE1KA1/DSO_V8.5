# DSO 10-Bit Mode Design

**Goal:** Replace the current `12-bit stable` architecture with a new `10-bit` mode, keep the existing `8-bit (2L)` and `8-bit (3L)` high-speed modes, and make `10-bit` the default boot mode. The new `10-bit` path must be implemented in dedicated files with low coupling, must align its processing, measurement, and rendering behavior with the current 8-bit pipeline where that pipeline already produces better visual results, and must do so without modifying the existing 8-bit architecture files.

## Scope

- Remove the current `12-bit stable` product mode from the active architecture.
- Introduce a new `10-bit` product mode:
  - `CH1`: `PA6`, `ADC1 + ADC2`, both on channel 3, dual interleaved
  - `CH2`: `PC2`, `ADC3`, channel 0, single triggered
- Keep both existing 8-bit high-speed modes:
  - `8-bit (2L)`
  - `8-bit (3L)`
- Make `10-bit` the default active and pending mode after power-on.
- Keep existing 8-bit architecture code frozen during this work. The `10-bit` migration may integrate with shared mode-selection and dispatch surfaces, but it must not edit 8-bit-specific backend modules.
- Preserve the existing upper-layer product structure:
  - acquisition
  - engine
  - trigger / measurement
  - record
  - timebase
  - vertical mapping
  - UI frame transport
  - emWin rendering
- Include first-phase parity features for 10-bit:
  - persistent CH1 lane calibration
  - raw-sample measurement support
  - `Sample / Peak / Average / HiRes`
  - enhanced repetitive reconstruction
  - serial/debug visibility
  - host-side regression coverage

## Non-Goals

- Do not redesign, rewrite, or patch the existing 8-bit dual or tri architecture files as part of this 10-bit migration.
- Do not perform unrelated refactors in vendor code or UI layout code.
- Do not generalize all acquisition backends into one abstract mega-layer in this phase.
- Do not change the current 8-bit topology behavior or default 8-bit lane ownership.
- Do not introduce fallback scaffolding, hidden compatibility branches, or layered "just in case" backup paths to compensate for unclear design boundaries.

## Current Problems

### The current 12-bit path is architecturally different from the 8-bit path

The current `12-bit stable` mode in `User/dso/dso_acq.c` uses STM32H743 hardware multi-mode packing (`ADC_DUALMODE_INTERL`) with a 12-bit-specific calibration module in `User/dso/dso_interleave_cal.c`.

The current 8-bit high-speed path in `User/dso/dso_acq_8bit.c` uses independent ADC lanes, independent DMA streams, timer phase offsets, software merge, separate lane calibration in `User/dso/dso_hs8_lane_cal.c`, and a dedicated repetitive reconstruction path in `User/dso/dso_hs8_render.c`.

These are not the same architecture, and simply changing 12-bit resolution constants to 10-bit would preserve the old 12-bit behavior rather than the better-looking 8-bit behavior.

### The current 12-bit processing chain does not fully match the 8-bit chain

Current 8-bit behavior is better because it already benefits from:

- raw-domain `HiRes` / `Average` preprocessing before display rendering
- raw-sample measurement for CH1
- `HS8` repetitive enhanced reconstruction when signal confidence is good
- lane-specific calibration state tuned for multi-lane sampling

The current 12-bit path does not consistently use those same decisions. If 10-bit reused the old 12-bit treatment, it would likely keep the older visual character instead of the current 8-bit result the user prefers.

### The current codebase contains 12-bit-specific coupling in names and policies

Examples already present in product code and tests:

- `DSO_SAMPLE_FORMAT_12BIT_STABLE`
- `dso_interleave_cal.[ch]`
- `s_table_12bit`
- `auto_fallback_to_12bit`
- UI strings and tests that explicitly assume `12-bit`

Leaving those in place after adding 10-bit would create misleading architecture and future maintenance errors.

## Chosen Approach

### Approach A: rename the current 12-bit path to 10-bit and adjust ADC resolution

- lowest code churn
- highest behavior mismatch with the current 8-bit pipeline
- leaves old calibration and measurement assumptions in place

This is rejected.

### Approach B: add a dedicated 10-bit backend and align its processing model with 8-bit

- keeps 8-bit working code isolated
- keeps 8-bit architecture files untouched
- removes 12-bit-specific coupling cleanly
- allows 10-bit to inherit the 8-bit-style measurement and render behavior that already looks good
- limits changes in shared files to dispatch, mode selection, and controlled extension points

This is the selected design.

### Approach C: refactor 8-bit and 10-bit into one generic interleaved framework now

- best long-term purity
- highest first-phase risk
- too likely to destabilize the current 8-bit path the user already trusts

This is rejected for phase 1.

## Target Architecture

### Product sample modes

The product mode cycle becomes:

1. `10-bit`
2. `8-bit (2L)`
3. `8-bit (3L)`

Boot default must enter `10-bit`.

### File boundaries

The implementation must create dedicated 10-bit modules instead of adding large 10-bit branches inside existing 8-bit files.

#### Frozen 8-bit architecture files

The following 8-bit-specific modules are frozen and must not be modified as part of this work:

- `User/dso/dso_acq_8bit.c`
- `User/dso/dso_acq8_fault_policy.c`
- `User/dso/dso_acq8_fault_policy.h`
- `User/dso/dso_acq8_refresh_policy.c`
- `User/dso/dso_acq8_refresh_policy.h`
- `User/dso/dso_hs8_lane_cal.c`
- `User/dso/dso_hs8_lane_cal.h`
- `User/dso/dso_hs8_render.c`
- `User/dso/dso_hs8_render.h`

If a design idea would require editing one of these files, that idea is out of scope for this migration and must be rejected.

#### New files

- `User/dso/dso_acq_10bit.h`
- `User/dso/dso_acq_10bit.c`
- `User/dso/dso_10bit_lane_cal.h`
- `User/dso/dso_10bit_lane_cal.c`
- `User/dso/dso_hs10_render.h`
- `User/dso/dso_hs10_render.c`

#### Shared files to modify

- `User/dso/dso_acq.c`
- `User/dso/dso_acq.h`
- `User/dso/dso_sample_spec.h`
- `User/dso/dso_sample_spec.c`
- `User/dso/dso_engine.c`
- `User/dso/dso_timebase.c`
- `User/dso/dso_timebase.h`
- `User/dso/dso_render_policy.h`
- `User/dso/dso_render_policy.c`
- `User/dso/dso_acquire_control.c`
- `User/dso/dso_serial_debug.c`
- `User/dso/dso_serial_debug_format.c`
- `User/dso/dso_process.c`
- `User/ui/ui_system_setup.c`
- `User/ui/ui_system_setup.h`
- project files under both toolchains for source registration
- relevant host tests under `tests/`

#### Old files to retire from the active design

- `User/dso/dso_interleave_cal.h`
- `User/dso/dso_interleave_cal.c`

If these files are kept temporarily during migration, they must end the work as unused and unreferenced. The preferred final state is full removal from both build systems.

### Shared architectural rule

`User/dso/dso_acq.c` remains the top-level acquisition facade and mode dispatcher only. It must not absorb detailed 10-bit hardware configuration or 10-bit calibration algorithms.

Shared-file changes are allowed only when they are additive integration for the new `10-bit` mode and do not alter the existing 8-bit architecture implementation.

The dedicated 10-bit files own:

- timer and ADC setup specific to 10-bit mode
- 10-bit DMA ring semantics
- CH1 dual-lane merge
- CH2 single-lane capture contract
- persistent lane calibration
- 10-bit enhanced repetitive reconstruction logic

This keeps 8-bit and 10-bit independent enough to evolve separately.

## 10-Bit Acquisition Design

### CH1 hardware model

`CH1` in 10-bit mode is a two-lane interleaved software-merge path, intentionally matching the current 8-bit dual-lane topology style rather than the old 12-bit hardware-packed multi-mode style.

- input pin: `PA6`
- `ADC1` regular channel 3
- `ADC2` regular channel 3
- both ADCs run in 10-bit resolution
- each lane has its own DMA ring
- sampling is timer-triggered with phase offset
- merged output is a single ordered CH1 sample stream

This is the most important architectural decision in the design.

### CH2 hardware model

`CH2` in 10-bit mode is a single triggered real-time path:

- input pin: `PC2`
- `ADC3` regular channel 0
- 10-bit resolution
- single DMA ring
- synchronized to the same timer base used by the 10-bit mode

CH2 remains a true active channel in 10-bit mode, unlike current 8-bit high-speed operation where CH2 is effectively disabled at the sample-spec level.

### Timer and trigger model

The 10-bit mode will reuse the trigger style from the current 8-bit dual-lane backend and will standardize on `TIM1`:

- `TIM1 CC1` drives `ADC1` for CH1 lane 0
- `TIM1 CC2` drives `ADC2` for CH1 lane 1
- `TIM1 CC3` drives `ADC3` for CH2

This keeps the 10-bit timing model aligned with the existing software-merged dual-lane architecture and avoids any dependency on the old `TIM8 + hardware multimode` path.

### Buffer contract

10-bit mode uses independent lane buffers, then merges them into ordered 16-bit sample arrays for the engine.

The acquisition facade seen by the engine remains consistent:

- `GetPos()`
- `GetPosCh2()`
- `GetBuf()`
- `GetBufCh2()`
- `CopyRecentSamples()`
- `CopyNewSamples()`
- `CopyRecentCh2Samples()`
- `CopyNewCh2Samples()`

But the 10-bit backend owns the internal semantics for:

- lane DMA positions
- lane merge order
- effective sample-rate calculation
- CH2 trigger-rate relationship

### Effective sample rate contract

For 10-bit mode:

- `CH1 sample rate = lane0 trigger rate + lane1 trigger rate`
- `CH2 sample rate = CH2 trigger rate`

Those reported rates must be accurate because the engine, trigger analysis, frequency measurement, and timebase reconstruction depend on them.

## 10-Bit Calibration Design

### Why 10-bit needs its own calibration module

The old `dso_interleave_cal` module is tied to the old 12-bit architecture. Reusing it would keep hidden assumptions about sample packing, calibration state flow, and format identity.

10-bit instead gets a dedicated CH1 lane-calibration module:

- `dso_10bit_lane_cal.h`
- `dso_10bit_lane_cal.c`

### Calibration responsibilities

The 10-bit lane calibration module must:

- solve CH1 lane-to-lane offset, gain, and skew
- maintain runtime state across frames
- reset when signal conditions invalidate the model
- expose readiness / validity information to the engine and UI
- persist coefficients to existing nonvolatile storage if such storage is already used for calibration in this codebase

### Calibration scope

- CH1 dual-lane only
- no interleave calibration on CH2
- calibration validity must surface through the existing frame diagnostics field `ch1_interleave_cal_valid` unless a small generic rename is performed across the whole UI/debug stack in one pass

The recommended first phase keeps the existing frame field name for compatibility and changes only the backing module.

## Processing and Rendering Alignment

### Design rule

10-bit must follow the 8-bit-style data treatment wherever that treatment is already demonstrably better than the old 12-bit path.

### Acquire mode support

The current 12-bit mode only cycles:

- `Sample`
- `Peak`
- `Average`

10-bit will support the same full set currently allowed by 8-bit:

- `Sample`
- `Peak`
- `HiRes`
- `Average`

This requires `dso_acquire_control.c` to treat 10-bit as a mode that allows `HiRes`, rather than inheriting the current 12-bit restriction.

### Measurement source

The old 12-bit path tends to measure from display columns, while the current 8-bit path measures CH1 from raw samples.

10-bit should follow the 8-bit decision:

- prefer raw-sample measurement for CH1
- use display-column measurement only when raw measurement is unavailable or inappropriate
- keep CH2 measurement consistent with its actual rendered source

This is required to keep 10-bit frequency and amplitude behavior closer to the current 8-bit experience.

### Raw-domain preprocessing

10-bit should enter the same kind of pre-render source preparation that 8-bit uses today:

- optional `HiRes` boxcar pass in raw domain
- optional averaging in raw domain

This behavior belongs in the engine control path, not inside the low-level acquisition module.

### Enhanced repetitive reconstruction

10-bit gets a dedicated enhanced repetitive reconstruction helper:

- `dso_hs10_render.h`
- `dso_hs10_render.c`

This module mirrors the responsibility split already used by `dso_hs8_render`, but uses thresholds and assumptions tuned for the 10-bit dual-lane mode.

10-bit rendering should therefore choose among:

- direct/raw window rendering
- ETS fallback when applicable
- `HS10` enhanced reconstruction when repetitive confidence and sample density allow it

This is a deliberate divergence from the old 12-bit path and is required to match the user's preference for the current 8-bit visual effect.

## Timebase Design

`User/dso/dso_timebase.c` currently selects among `s_table_12bit`, `s_table_8bit`, `s_table_8bit_dual`, and `s_table_8bit_tri`.

The new design replaces `s_table_12bit` with:

- `s_table_10bit`

Timebase selection becomes:

- `10-bit` -> `s_table_10bit`
- `8-bit (2L)` -> `s_table_8bit_dual`
- `8-bit (3L)` -> `s_table_8bit_tri`
- `8-bit single-safe` internal fallback may continue using `s_table_8bit` when needed

The 10-bit table must be tuned for the real CH1 effective sample rate and CH2 single-lane rate, not copied blindly from the old 12-bit table.

## Sample Spec Design

`User/dso/dso_sample_spec.[ch]` becomes the source of truth for the new product mode identity.

Required state after this migration:

- `DSO_SAMPLE_FORMAT_10BIT` replaces `DSO_SAMPLE_FORMAT_12BIT_STABLE`
- `DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED` remains
- `DSO_SAMPLE_FORMAT_DEFAULT = DSO_SAMPLE_FORMAT_10BIT`

The 10-bit spec must report:

- `adc_bits = 10`
- `adc_max = 1023`
- correct `midscale`
- `lane_count = 2`
- `interleaved = true`
- `ch2_mode = DSO_CH2_MODE_REALTIME`
- `start_immediately = true`

Current default boot and active-format tests must be updated accordingly.

## UI and Boot Behavior

### Power-on default

After initialization completes, both the pending and active sample format must be `10-bit`.

That includes:

- `dso_sample_spec` default format
- `dso_acq` initial active and pending mode state
- UI pending label state

### UI cycle order

`UI_System_Ec2Detent()` becomes:

- forward: `10-bit -> 8-bit (2L) -> 8-bit (3L) -> 10-bit`
- reverse: `10-bit -> 8-bit (3L) -> 8-bit (2L) -> 10-bit`

The label text becomes:

- `10-bit`
- `8-bit (2L)`
- `8-bit (3L)`

## Naming Cleanup

The migration must remove 12-bit-specific product naming from active code and tests.

Required examples:

- `DSO_SAMPLE_FORMAT_12BIT_STABLE` -> `DSO_SAMPLE_FORMAT_10BIT`
- `auto_fallback_to_12bit` -> a neutral name such as `auto_fallback_to_base_mode`, or remove the fallback entirely if the policy remains disabled
- `s_table_12bit` -> `s_table_10bit`
- `dso_interleave_cal` includes and tests -> 10-bit replacements

The objective is not cosmetic. Neutral and correct names reduce future coupling errors.

## Test Strategy

### Replace 12-bit assumptions with 10-bit expectations

Existing tests that currently assume 12-bit behavior must be rewritten around 10-bit:

- `tests/test_dso_sample_spec.c`
- `tests/test_dso_acquire_control.c`
- `tests/test_dso_hf_pipeline.c`
- `tests/test_dso_process_hs8_freq_policy.c`
- any tests that include `dso_interleave_cal.h`

### Add 10-bit-specific host tests

New host tests should cover at least:

- 10-bit sample spec values and default boot mode
- 10-bit acquire-mode cycle including `HiRes`
- 10-bit CH1 merge ordering
- 10-bit CH2 rate / position semantics
- 10-bit lane calibration solve / apply / readiness behavior
- 10-bit timebase table selection
- 10-bit render policy gating for enhanced reconstruction
- UI mode-cycle labels and order

### Keep existing 8-bit coverage green

This migration must not regress the currently passing 8-bit contract tests. Existing 8-bit tests remain required evidence that 10-bit work did not destabilize dual or tri mode behavior.

## Migration Sequence

The code changes should follow this order:

1. define the new 10-bit format identity and make it the default
2. add dedicated 10-bit acquisition and calibration modules
3. switch `dso_acq.c` dispatch from old 12-bit backend to 10-bit backend
4. align engine measurement and render-source handling for 10-bit
5. add `HS10` timebase / render-policy integration
6. update UI labels and cycle order
7. remove old 12-bit module references and names
8. update both toolchain project files
9. run host regression tests for both 10-bit and preserved 8-bit behavior

## Final Architecture Summary

After this migration, the product architecture should look like this:

```text
UI format select / power-on default
  -> 10-bit | 8-bit(2L) | 8-bit(3L)
  -> dso_sample_spec
  -> dso_acq.c dispatch
     -> dso_acq_10bit.c
        -> CH1 dual-lane ADC1/ADC2 on PA6 CH3
        -> CH2 single-lane ADC3 on PC2 CH0
        -> software-merged 10-bit CH1 stream
        -> realtime CH2 stream
     -> dso_acq_8bit.c
        -> existing 8-bit dual / tri paths
  -> engine
     -> mode-specific lane calibration
        -> dso_10bit_lane_cal.c for 10-bit
        -> dso_hs8_lane_cal.c for 8-bit
     -> dso_process.c
     -> mode-specific enhanced reconstruction
        -> dso_hs10_render.c for 10-bit
        -> dso_hs8_render.c for 8-bit
     -> dso_timebase.c
     -> dso_vertical.c
     -> frame transport
  -> UI / MainTask
```

The important outcome is that 10-bit becomes a first-class mode with its own files and its own behavior, while 8-bit remains intact and the old 12-bit architecture disappears from the active product code.
