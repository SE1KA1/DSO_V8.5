# 8-Bit Dual Interleaved Design

**Goal:** Rebuild the 8-bit high-speed acquisition backend around STM32H743 native `ADC1 + ADC2` dual interleaved mode, using the hardware common-data path (`ADC12_CDR`) and a single DMA stream. Keep the current 12-bit acquisition architecture unchanged.

## Scope

- Replace the current 8-bit multi-lane high-speed backend with a two-ADC hardware interleaved backend.
- Keep the existing 8-bit `single-safe` path as a fallback and bring-up baseline.
- Remove the current 8-bit three-phase runtime sampling chain from the active architecture.
- Preserve the existing upper-layer engine, processing, and UI pipeline interfaces as much as possible.
- Add 8-bit-specific packed-dual unpack logic that matches the STM32H7 hardware data format.

## Non-Goals

- Do not redesign or refactor the 12-bit acquisition implementation in `User/dso/dso_acq.c`.
- Do not reuse the previous 8-bit three-phase lane-merge architecture.
- Do not add a second, software-merged dual-ADC fallback path.
- Do not broaden this work into UI, rendering, or full engine refactoring beyond what the new 8-bit acquisition contract requires.

## Current Problems

### Current 8-bit backend is built around the wrong hardware model

`User/dso/dso_acq_8bit.c` currently exposes:

- `DSO_ACQ8_TOPOLOGY_SINGLE_SAFE`
- `DSO_ACQ8_TOPOLOGY_THREE_PHASE`

The high-speed mode is implemented as three independent ADC DMA rings plus software alignment, merge, and calibration. This is not the correct architecture for the new 8-bit dual-interleaved target.

### Three-phase path is deeply coupled to lane-merge and lane-calibration logic

The current 8-bit path depends on:

- `ADC3` as a third lane
- `dso_lane_merge`
- `dso_hs8_lane_cal`
- `lane2`-specific debug and fallback behavior

That coupling is only useful for a true three-phase design. It becomes unnecessary and harmful if the target becomes a hardware dual-interleaved path.

### Existing cleanup tests block the correct hardware solution

`tests/test_dso_acq8_legacy_cleanup.c` currently forbids:

- `ADC_DUALMODE_INTERL`
- `HAL_ADCEx_MultiModeConfigChannel`
- `HAL_ADCEx_MultiModeStart_DMA`
- `HAL_ADCEx_MultiModeStop_DMA`

Those symbols are exactly what STM32H743 requires for a correct 8-bit dual-interleaved implementation, so the current test contract must be replaced.

## Hardware Basis

This design is based on STM32H743 reference manual RM0433 and ST's official ADC documentation.

### Required RM0433 facts

- Dual interleaved mode is selected by `ADC_CCR.DUAL = ADC_DUALMODE_INTERL`.
- The slave ADC starts after a programmable delay set by `ADC_CCR.DELAY`.
- For multi-ADC DMA data packing, `ADC_CCR.DAMDF` must be configured.
- For 8-bit dual interleaved mode, `DAMDF = 0b11`, exposed by HAL as `ADC_DUALMODEDATAFORMAT_8_BITS`.
- In this mode, each DMA read from `ADC12_CDR` returns four ordered 8-bit samples:
  - `[7:0]   = MST t0`
  - `[15:8]  = SLV t0`
  - `[23:16] = MST t1`
  - `[31:24] = SLV t1`
- In multi-ADC mode with packed DMA data, the master ADC must use DMA-managed conversion data (`DMNGT` set for DMA mode).
- If one ADC overruns in multi-ADC mode, DMA requests stop to preserve validity.

### Architectural consequence

For 8-bit high-speed dual acquisition, the correct chain is:

- `ADC1` master
- `ADC2` slave
- timer-triggered regular conversion
- common data register `ADC12_CDR`
- single DMA attached to `ADC1`
- software unpack from packed 32-bit words into ordered 8-bit samples

This is fundamentally different from the current three-phase software merge approach.

## Recommended Topology Model

The 8-bit backend should expose only two topology states:

1. `DSO_ACQ8_TOPOLOGY_SINGLE_SAFE`
2. `DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED`

The current `DSO_ACQ8_TOPOLOGY_THREE_PHASE` runtime architecture should be retired from the active 8-bit sampling backend.

### Default behavior

This document defines the supported architecture states, not the final product boot default.

The implementation must support:

- `single-safe` for low-risk bring-up and fallback
- `dual-interleaved` as the intended high-speed production path

## Target Architecture

### 8-bit `single-safe`

This path stays close to the current implementation:

- `ADC1`
- `TIM8 TRGO`
- one circular DMA ring
- existing decode path for single ADC storage

No functional redesign is required here beyond any API alignment needed to coexist with the new dual-interleaved topology contract.

### 8-bit `dual-interleaved`

This becomes the only high-speed 8-bit multi-ADC architecture:

- `ADC1` is master
- `ADC2` is slave
- both sample the same CH1 analog input
- `TIM8 TRGO` drives the regular conversion trigger
- common ADC12 control register configures interleaving and data packing
- one DMA stream reads `ADC12_CDR`
- software unpacks each 32-bit word into four ordered 8-bit samples

`ADC3` is not part of the 8-bit dual high-speed acquisition path.

## Data Flow

### `single-safe`

```text
TIM8 TRGO
  -> ADC1
  -> single DMA ring
  -> snapshot position
  -> decode/copy samples
  -> engine/process/UI
```

### `dual-interleaved`

```text
TIM8 TRGO
  -> ADC1 regular trigger (master)
  -> ADC12 common dual-interleaved mode
  -> ADC2 delayed sample (slave)
  -> ADC12_CDR packed 32-bit word
  -> ADC1 master DMA circular ring
  -> unpack 4 ordered 8-bit samples per word
  -> engine/process/UI
```

## Register and HAL Contract

### ADC configuration

`ADC1` and `ADC2` must both be configured for:

- 8-bit resolution
- single regular conversion
- same input channel
- same sample time
- same single-ended mode
- external trigger enabled
- no independent continuous conversion

`ADC1` must use DMA conversion data management in circular mode.

`ADC2` participates as slave in multi-mode and does not own a separate data DMA stream.

### Multi-mode configuration

`ADC1` common configuration must use:

- `Mode = ADC_DUALMODE_INTERL`
- `DualModeData = ADC_DUALMODEDATAFORMAT_8_BITS`
- `TwoSamplingDelay = dedicated 8-bit interleave delay setting`

The design keeps the delay configurable because 8-bit timing margins differ from the existing 12-bit path.

### Trigger source

The recommended trigger source remains `TIM8 TRGO`, matching the existing timer-triggered acquisition strategy already used successfully in the 12-bit path.

This keeps the timing model consistent with the rest of the product and avoids introducing a second timer architecture for no gain.

## Buffer Model

### Why the buffer model must differ from 12-bit

12-bit dual interleaved uses:

- one 32-bit DMA word
- two samples per word

8-bit dual interleaved uses:

- one 32-bit DMA word
- four samples per word

Therefore the 8-bit packed-dual ring and copy helpers cannot simply reuse the 12-bit assumptions.

### Recommended 8-bit DMA ring contract

Introduce a dedicated 8-bit dual packed DMA word count:

- `DSO_ACQ8_DUAL_WORD_COUNT`

Recommended sizing rule:

- `DSO_ACQ8_DUAL_WORD_COUNT = DSO_DMA_BUF_SIZE / 2`

This gives:

- `DSO_ACQ8_DUAL_WORD_COUNT` packed DMA words in memory
- `DSO_DMA_BUF_SIZE * 2` logical 8-bit samples after unpack

That matches the current upper-layer working buffer size in `dso_engine.c`, avoiding unnecessary growth in engine-side arrays.

### Position semantics

For dual-interleaved 8-bit mode:

- `GetPos()` returns the DMA word-ring position, not the logical unpacked sample index.
- `CopyRecentSamples()` and `CopyNewSamples()` convert from word position to unpacked sample stream internally.

This mirrors the current 12-bit dual behavior and keeps the acquisition API stable from the engine's point of view.

## Sample Unpack Contract

The current helper path:

- `DSO_Acq8_CopyAligned3Bytes(...)`

must be replaced in dual mode by a packed-dual unpack helper.

### Required helper behavior

For each 32-bit DMA word:

- extract `sample0 = bits[7:0]`
- extract `sample1 = bits[15:8]`
- extract `sample2 = bits[23:16]`
- extract `sample3 = bits[31:24]`

The helper must preserve the exact acquisition order:

`MST t0 -> SLV t0 -> MST t1 -> SLV t1`

This unpack order is the core correctness contract for all downstream frequency, trigger, and rendering logic.

## Runtime Model

The current 8-bit runtime model assumes:

- `single-safe`: `sample_rate = trigger_rate`
- `three-phase`: `sample_rate = 3 * trigger_rate`

The new model must become:

- `single-safe`: `sample_rate = trigger_rate`
- `dual-interleaved`: `sample_rate = 2 * trigger_rate`

This affects:

- runtime info fields
- serial debug frequency/sample-rate reporting
- timebase-dependent window calculations

## Component Boundaries

### `User/dso/dso_config.h`

Responsible for the 8-bit topology contract.

Required outcome:

- replace `THREE_PHASE` topology with `DUAL_INTERLEAVED`
- remove stale lane-2 configuration that only exists for the retired three-phase backend
- keep the single-safe path available

### `User/dso/dso_acq_8bit.c`

Responsible for:

- 8-bit topology sanitization
- hardware init/deinit
- DMA start/stop
- position snapshot
- sample copy
- runtime info reporting

Required outcome:

- retain `single-safe`
- remove active three-phase backend configuration and streaming flow
- add native dual-interleaved backend
- stop using `ADC3` in high-speed 8-bit runtime acquisition

### `User/dso/dso_acq8_sample_unpack.[ch]`

Responsible for conversion from raw DMA storage into ordered logical samples.

Required outcome:

- keep any single-safe support that is still needed
- replace three-lane aligned-copy behavior with 8-bit packed-dual unpack support

### `User/dso/dso_serial_debug*.c/.h`

Responsible for making the new topology observable.

Required outcome:

- report `top = dual-interleaved`
- report `lc = 2`
- stop printing lane-2-only semantics from the retired three-phase backend
- keep enough packed-dual debug visibility to diagnose order and rate problems

### `tests/*`

Required outcome:

- remove the current false contract that forbids multi-mode APIs in 8-bit code
- add tests that verify packed 8-bit dual unpack order
- keep existing serial-debug and high-frequency pipeline tests green

## What Must Remain Unchanged

- `User/dso/dso_acq.c` 12-bit dual interleaved acquisition path
- existing 12-bit DMA and unpack assumptions
- existing engine/UI public behavior outside the 8-bit backend contract

This redesign is intentionally scoped so the 12-bit path remains the stable reference implementation and does not become entangled with 8-bit bring-up.

## Error Handling

### Initialization failures

Current acquisition modules use infinite-loop fail-stop behavior on HAL init failure. This design keeps that policy unchanged.

### Overrun behavior

The design must respect RM0433 behavior:

- if either ADC overruns in dual mode, DMA requests stop
- software must treat this as a hard acquisition fault and surface it through existing diagnostics

### Invalid topology requests

Invalid topology requests must sanitize to `single-safe`.

## Testing Strategy

### Host-side tests

Keep these existing checks green:

- `.\tests\test_dso_serial_debug.exe`
- `.\tests\test_dso_hf_pipeline.exe`

Add focused 8-bit dual tests for:

- packed 32-bit word to 4-sample unpack order
- recent-sample copy from packed ring
- new-sample copy across ring wrap
- runtime sample-rate math (`2x trigger`)
- topology contract (`single-safe` and `dual-interleaved` only)

### Firmware validation

Validation targets:

1. Grounded input:
   - low `vpp`
   - no false high-frequency lock
   - stable baseline
2. 100 kHz / 1 Vpp / 1.65 V offset sine:
   - measured frequency near 100 kHz
   - `samples_per_cycle` follows timebase changes logically
   - waveform shape remains smooth
3. Timebase sweep:
   - no broken sample-rate scaling
   - no old three-phase `phase_err` semantics leaking into dual mode

## Risks

### HAL multi-mode assumptions differ between 8-bit and 12-bit

The hardware supports both, but the data packing format differs. Reusing 12-bit helper assumptions without redesign will silently break sample ordering.

### Existing debug/test layers still assume three lanes

The 8-bit debug layer currently exposes three-lane fields. That contract must be simplified carefully so diagnostics remain useful without keeping dead architecture alive.

### Timer-rate math must stay coherent

The new architecture doubles the effective sample rate, not triples it. Any stale `3x` assumption in runtime info or debug formatting will produce misleading logs even if the hardware path is correct.

## Recommendation

Implement the 8-bit high-speed backend as a clean hardware dual-interleaved architecture:

- `ADC1 + ADC2`
- `ADC_DUALMODE_INTERL`
- `ADC_DUALMODEDATAFORMAT_8_BITS`
- one DMA reading `ADC12_CDR`
- dedicated packed-dual unpack path

Do not keep the current three-phase backend alive in parallel. It adds coupling, dead logic, and debug ambiguity without helping the target architecture.
