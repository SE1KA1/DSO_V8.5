# 8-Bit Three-Phase Redesign Design

**Goal:** Keep the working 8-bit `single-safe` path, remove the legacy 8-bit `dual` path and the current mixed experimental three-phase path, then rebuild one clean 8-bit three-phase acquisition architecture for future three-lane alternating sampling.

## Scope

- Keep the existing 8-bit `single-safe` acquisition pipeline behavior unchanged.
- Remove the 8-bit `dual` acquisition topology and all fallback logic that depends on it.
- Replace the current mixed 8-bit `triple_experimental` implementation with a clean three-phase backend.
- Keep serial/debug visibility for the new architecture, but remove stale topology semantics.
- Preserve the current fixed 12-bit pipeline.

## Non-Goals

- Do not redesign the 12-bit acquisition architecture.
- Do not refactor unrelated UI, rendering, or frequency-analysis code.
- Do not split the entire 8-bit backend into many new modules unless required for correctness.
- Do not broaden this change into a full acquisition subsystem rewrite.

## Current Problems

### Mixed topology state

`User/dso/dso_acq_8bit.c` currently carries three backend modes at once:

- `DSO_ACQ8_TOPOLOGY_SINGLE_SAFE`
- `DSO_ACQ8_TOPOLOGY_DUAL_FAST_PRODUCTION`
- `DSO_ACQ8_TOPOLOGY_TRIPLE_EXPERIMENTAL`

This forces the backend selector, DMA start/stop flow, snapshot logic, sample-copy logic, and serial diagnostics to keep branching on old paths that are no longer part of the intended product direction.

### Engine rollback to deleted architecture

`User/dso/dso_engine.c` still contains logic that falls back from the three-lane path to the old `dual` topology when lane-2 solve failures accumulate. Once the dual path is retired, this fallback becomes invalid and must be removed or replaced.

### Debug semantics no longer match the desired architecture

Current debug output still exposes topology names and lane semantics tied to `dual` and `triple_experimental`. If the backend is simplified but the debug model is left unchanged, future bring-up will be harder because the logs will describe states that no longer exist.

### Preview and lane-2 responsibilities are mixed into core backend flow

The current three-phase path includes lane-2 DMA, optional preview capture, and active topology control in the same control block. That makes it harder to reason about what is part of the core sampling chain and what is only an auxiliary service.

## Target Architecture

### Topology model

The 8-bit backend will expose only two valid topology states:

1. `single-safe`
2. `three-phase`

The old `dual` topology will be removed completely from configuration, runtime state, engine fallback, and serial/debug reporting.

The current `triple_experimental` meaning will be replaced by a production three-phase topology. The configuration contract for the final code should expose a production name such as `DSO_ACQ8_TOPOLOGY_THREE_PHASE`, not an experimental label.

### 8-bit `single-safe`

This path remains the safety baseline:

- one ADC path
- existing timer trigger model
- existing halfword decode path
- existing DMA ring handling

No functional behavior should change here except for any minimal compile-time or API adjustments required by removing the old dual/triple branching.

### 8-bit `three-phase`

This becomes the only high-speed multi-lane architecture:

- ADC1, ADC2, ADC3 run independently
- TIM1 `CC1/CC2/CC3` generate the three phase-shifted trigger points
- each ADC owns its own DMA ring
- sample reconstruction consumes three aligned byte lanes

The backend must be treated as a first-class architecture, not as an optional experiment layered on top of old code.

## Data Flow

### `single-safe`

```text
TIM8 trigger
  -> ADC1
  -> DMA ring (u16 storage, 8-bit decode)
  -> snapshot position
  -> copy recent/new samples
  -> engine/process/UI
```

### `three-phase`

```text
TIM1 CC1/CC2/CC3
  -> ADC1 / ADC2 / ADC3
  -> three byte DMA rings
  -> common-delta snapshot
  -> aligned 3-lane sample merge
  -> engine/process/UI
```

The core three-phase data path must not depend on the removed dual path for fallback, synchronization, or packing/unpacking.

## Component Boundaries

### `User/dso/dso_config.h`

Responsible for the external topology contract.

Required outcome:

- remove `DSO_ACQ8_TOPOLOGY_DUAL_FAST_PRODUCTION`
- reduce the configuration surface to `single-safe` and `three-phase`
- keep default topology explicit

### `User/dso/dso_acq_8bit.c`

Responsible for:

- topology sanitization
- backend init/deinit
- DMA start/stop
- position snapshot
- recent/new sample copy
- preview state lifecycle

Required outcome:

- remove `configure_dual_hw_backend()`
- remove `start_dual_hw_streaming()` / `stop_dual_hw_streaming()`
- remove `snapshot_dual_hw_position()`
- remove all `topology_is_dual_hw()` branches
- collapse backend dispatch to `single-safe` or `three-phase`

The three-phase backend should stay in this file for now, but with cleaner internal boundaries than the current mixed implementation.

### `User/dso/dso_engine.c`

Responsible for high-speed lane calibration policy and topology reactions.

Required outcome:

- remove fallback to the deleted dual topology
- keep lane-cal diagnostics
- replace invalid fallback with a policy that stays within the two-topology model

The policy for this redesign is:

- if three-phase lane solving fails repeatedly, mark the calibration state accordingly
- do not jump into a deleted backend
- do not introduce a new automatic fallback in this redesign
- keep the acquisition topology unchanged and surface the fault through diagnostics

### `User/dso/dso_serial_debug*.c/.h`

Responsible for making the new architecture observable.

Required outcome:

- remove stale references to the deleted dual topology
- keep lane windows and topology reporting meaningful
- ensure debug labels reflect the new two-topology model

### `User/dso/dso_lane_merge.c`

This module already provides the common-delta and aligned-copy logic needed by the three-phase path. It should remain the merge primitive for the rebuilt backend rather than duplicating equivalent logic inside `dso_acq_8bit.c`.

## Preview and Lane-2 Policy

The three-phase architecture and the preview mechanism must be treated as separate responsibilities.

### Core rule

The core three-phase sampling chain is:

- ADC1 lane 0
- ADC2 lane 1
- ADC3 lane 2

This chain must remain valid without requiring preview capture to run.

### Preview rule

If CH2 preview is still required, it must remain an auxiliary service:

- explicitly stop/borrow/restore ADC3 only when preview capture is requested
- not alter the steady-state definition of the core three-phase topology
- not leak old `dual` semantics into the three-phase control path

If later analysis shows preview is no longer needed, it can be removed as a separate cleanup. This redesign does not require that extra scope.

## Error Handling

### Initialization errors

Current acquisition code uses `while (1) {}` on HAL init failure. This redesign does not change that policy.

### Synchronization errors

Three-phase common-delta and phase-sync diagnostics remain valid and should stay visible in runtime/debug state.

### Topology requests

Invalid topology requests must sanitize to `single-safe`, not to any deleted topology.

## Testing Strategy

### Must stay green

- `tests\\test_dso_serial_debug.exe`
- `tests\\test_dso_hf_pipeline.exe`

### Additional verification focus

- confirm 8-bit `single-safe` sample decode path still uses the fixed low-byte/shift/high-byte detection correctly
- confirm 8-bit topology request API no longer accepts or routes into the removed dual backend
- confirm serial debug no longer prints stale dual-topology behavior

### Firmware validation targets

- 8-bit external `100 kHz / 1 Vpp / 1.65 V offset` input remains stable on `single-safe`
- 8-bit grounded-input behavior remains quiet and valid
- rebuilt three-phase path can be observed distinctly from `single-safe` through serial logs

## Implementation Constraints

- preserve the existing working 8-bit `single-safe` behavior
- prefer deleting obsolete branches over layering wrappers around them
- reuse existing merge and unpack helpers instead of duplicating logic
- keep the patch reviewable: remove old path first, then rebuild the new three-phase path in the reduced architecture

## Success Criteria

The redesign is complete when all of the following are true:

- the 8-bit backend exposes only `single-safe` and `three-phase`
- no runtime branch or fallback targets the deleted dual topology
- `single-safe` keeps its current stable behavior
- the new three-phase backend is the only multi-lane 8-bit architecture
- debug output matches the new architecture
- host tests pass and firmware logs reflect the intended topology model
