# DSO Optimization Design

Date: 2026-04-29
Status: Draft for project review

## Context

The project is a dual-channel oscilloscope firmware for STM32H743IIT6. The current product path has two explicit acquisition formats:

- `8-bit (3L)` high-speed three-lane interleaved mode, currently the default format.
- `10-bit` mode with CH1 dual-lane interleaving and CH2 realtime sampling.

The architecture documents under `docs/architecture/` are the current source of truth. Older notes under `Doc/` are useful historical context, especially for display rotation and PlotBuf work, but some dimensions and performance notes no longer match the current code.

The existing external frequency meter path is accurate and must remain intact. That path uses PB15/TIM12 and feeds `DSO_Frame.ext_freq_millihz`, which the scope header currently formats into the CH1 frequency field. This optimization does not redesign or replace that frequency meter.

## Goals

1. Make amplitude-related UI measurements trustworthy in both 8-bit and 10-bit formats.
2. Make SDRAM ownership explicit so deep record, roll history, GUI heap, and VRAM cannot silently overlap.
3. Improve 10-bit acquisition readiness and diagnostics without adding hidden fallback paths.
4. Reduce misleading legacy names, comments, and documentation that still describe old 12-bit or old 8-bit paths.
5. Preserve the frozen 8-bit acquisition architecture and current external frequency meter behavior.

## Non-Goals

- Do not modify the 8-bit acquisition, calibration, enhanced rendering, or lane architecture to make 10-bit work.
- Do not add automatic fallback from 10-bit to 8-bit, from 8-bit to another topology, or from a failed path to a hidden compatibility path.
- Do not change PB15/TIM12 external frequency measurement math or hardware setup.
- Do not redesign the UI layout in this pass.
- Do not introduce a broad refactor of vendor code, emWin, FreeRTOS, HAL, or project files unrelated to the optimization.

## Recommended Route

Use a steady correction route:

1. Fix numeric trust issues first.
2. Lock down SDRAM boundaries second.
3. Add explicit 10-bit readiness diagnostics third.
4. Clean stale architecture wording last.

This sequence avoids destabilizing high-speed acquisition while fixing issues that can mislead users or future developers.

## Design Principles

- Each data path has one explicit implementation. If a state is invalid, report it instead of hiding it behind a fallback.
- UI measurement conversion must use metadata from the same frame being displayed, not a global value that can change after the frame was produced.
- Shared integration changes should be minimal and additive.
- 8-bit behavior remains frozen except for tests or documentation that describe the existing behavior more accurately.
- Current architecture documents and code comments should agree about default mode, available modes, memory layout, and hardware ownership.

## Phase 1: Amplitude Measurement Correctness

### Problem

Several UI measurement paths still convert ADC codes to volts using `DSO_ADC_MAX`, which is currently `4095u`. That value is a 12-bit-era constant. Current samples are either 8-bit (`adc_max = 255`) or 10-bit (`adc_max = 1023`). If raw `vpp`, `vrms`, `vmin`, and `vmax` values are converted with 4095, amplitude values shown by the UI can be substantially low.

This problem is separate from external frequency measurement. Frequency display based on `ext_freq_millihz` stays unchanged.

### Design

Add frame-local ADC scale metadata to `DSO_Frame`:

- `sample_format`
- `adc_bits`
- `adc_max`
- `adc_midscale`
- `adc_lsb_v`

The DSO engine populates these fields from `DSO_SampleSpec_GetActive()` when building each frame. The UI uses the metadata from `s_display_frame` or a local `DSO_Frame fr` copy when formatting amplitude values. This keeps conversion consistent with the actual frame, even if the user switches sample format while an older frame is still on screen.

The UI conversion rule is:

```c
volts = adc_code_delta * frame.adc_lsb_v;
```

For absolute sample-code values displayed as `Vmax` or `Vmin`, the initial correction keeps existing behavior and only replaces the stale full-scale denominator. It does not introduce a new offset model. If a later hardware calibration pass defines front-end gain/offset correction, that should be a separate design.

### Files

- Modify `User/dso/dso_process.h` to add the frame metadata fields.
- Modify `User/dso/dso_engine.c` to fill the metadata in frame construction.
- Modify `User/ui/UI_WaveformCtrl.c` to use `s_display_frame.adc_lsb_v`.
- Modify `User/ui/ui_screens.c` to use the copied frame's `adc_lsb_v`.
- Update tests under `tests/` for 8-bit and 10-bit amplitude conversion.

### Acceptance

- 8-bit Vpp and Vrms conversion uses `3.3 / 255`.
- 10-bit Vpp and Vrms conversion uses `3.3 / 1023`.
- External frequency display still uses `ext_freq_millihz`.
- No UI amplitude path depends on `DSO_ADC_MAX` for active 8-bit or 10-bit frame conversion.

## Phase 2: SDRAM Layout Boundaries

### Problem

The architecture documents already flag a memory ownership risk: deep record and roll history can both claim large SDRAM regions, while GUI heap and physical framebuffers also live in SDRAM. The current layout needs explicit boundary checks so future changes cannot create silent overlap.

### Design

Create a single checked SDRAM layout contract from the existing layout headers:

- GUI heap region
- DSO record region
- roll history region
- logical framebuffer
- physical framebuffer 0
- physical framebuffer 1
- sentinel regions

The first implementation should add compile-time assertions where all region starts and ends are constant. If any region cannot be expressed as a compile-time constant, add a small host-side layout test that includes the relevant headers and validates non-overlap numerically.

If overlap exists, resolve it by changing one explicit owner boundary, preferably the DSO record capacity declaration, rather than adding runtime avoidance logic.

### Files

- Modify `User/bsp/inc/bsp_sdram_layout.h`.
- Modify `User/bsp/inc/bsp_vram_layout.h` only if framebuffer ownership needs clearer exported bounds.
- Modify `User/dso/dso_record.c` only if record capacity must be reduced to respect the layout.
- Modify or add a host test under `tests/`, such as `test_dso_sdram_layout.c`.
- Update `docs/architecture/06-memory-bsp-build-test.md`.

### Acceptance

- SDRAM regions have documented start, end, and size.
- Deep record and roll history do not overlap.
- GUI heap does not overlap DSO record, roll history, or VRAM.
- A test or compile-time assertion fails if a future change creates overlap.

## Phase 3: 10-bit Readiness and Diagnostics

### Problem

The 8-bit backend has stricter multi-lane readiness behavior than the current 10-bit backend. The 10-bit path should report whether CH1 lanes have both advanced far enough to produce a coherent common window. It should also expose clear diagnostic reasons for not-ready states.

### Design

Keep the 10-bit hardware path unchanged:

- CH1 lane 0 remains ADC1.
- CH1 lane 1 remains ADC2.
- CH2 remains ADC3.
- TIM1 remains the sample trigger timer.

Improve readiness as a visible state machine:

- CH1 is ready only after both CH1 lanes have advanced and a nonzero common delta can be resolved.
- CH2 availability remains separate from CH1 readiness; CH2 absence must not block CH1 display if the active 10-bit path defines CH1 as usable.
- DMA overrun, stalled lane, and phase mismatch counters are reported through the existing debug snapshot or a minimal additive field.
- `DSO_Acq10_IsDataReady()` returns false during the initial wait instead of treating `running` alone as readiness.

No fallback is introduced. If a lane stalls or phase sync fails, the firmware reports the failure and keeps the path explicit.

### Files

- Modify `User/dso/dso_acq_10bit.c`.
- Modify `User/dso/dso_acq_10bit.h` only for additive diagnostic definitions if needed.
- Extend existing lane-merge or acquisition tests under `tests/`.
- Update serial debug formatting only if a new diagnostic field needs to be visible.

### Acceptance

- After 10-bit acquisition start, readiness waits for both CH1 lanes.
- A stalled lane can be distinguished from normal startup wait in diagnostics.
- No 10-bit readiness fix changes 8-bit behavior.
- No automatic fallback path is added.

## Phase 4: Architecture Cleanup

### Problem

Some names and comments still reflect older architecture stages: 12-bit constants, TIM8-era field names, old 8-bit dual-interleaved references, and historical PlotBuf dimensions. These do not always affect runtime behavior, but they increase the chance of future wrong changes.

### Design

Clean only misleading text, dead declarations, and documentation that no longer describe the active architecture. Avoid large file movement or behavior refactors.

The cleanup should distinguish between:

- Active code and active tests.
- Historical design documents that should be labeled historical.
- Current architecture documents that should describe the product path accurately.

### Files

- Review `User/dso/dso_config.h`.
- Review `User/dso/dso_acq_model.h`.
- Review `docs/architecture/*.md`.
- Review `Doc/*.md` and add a short historical-status note where appropriate.

### Acceptance

- Current docs state that default mode is 8-bit high-speed three-lane.
- Current docs state that UI selection is 10-bit or 8-bit (3L).
- Old 8-bit dual-interleaved text is clearly historical or removed from active architecture docs.
- `DSO_ADC_MAX` is not presented as the active ADC full-scale value for both supported formats.

## Testing Strategy

Run the existing smoke checks after each implementation stage:

```powershell
.\tests\test_dso_serial_debug.exe
.\tests\test_dso_hf_pipeline.exe
```

Add or extend focused host tests:

- Frame metadata population for 8-bit and 10-bit.
- UI measurement conversion using frame-local `adc_lsb_v`.
- SDRAM non-overlap.
- 10-bit readiness with both lanes advancing, one lane stalled, and initial startup wait.

For firmware-facing changes, rebuild the touched Keil target and confirm project-file or generated-hex diffs are intentional.

## Rollout Plan

1. Implement Phase 1 and tests.
2. Implement Phase 2 and tests.
3. Update architecture docs for the completed Phases 1 and 2.
4. Review hardware logs before implementing Phase 3.
5. Implement Phase 3 with tests and serial diagnostics.
6. Perform Phase 4 cleanup after behavior-affecting changes are stable.

## Risks

- Frame struct changes affect UI, serial debug, tests, and any code that copies `DSO_Frame`. This is acceptable because all copies use `sizeof(DSO_Frame)`, but compile errors should be expected where explicit initializers exist.
- Absolute voltage fields such as Vmax and Vmin may still need a later front-end calibration design. This optimization corrects ADC full-scale conversion only.
- SDRAM overlap resolution may reduce available deep-record capacity. That is preferable to silent memory corruption.
- 10-bit readiness tightening may reveal startup timing issues that were previously hidden by the loose `running` check.

## Review Checklist

- The external frequency meter path remains unchanged.
- The 8-bit architecture remains frozen.
- All new behavior has one explicit path.
- UI amplitude conversion follows the displayed frame metadata.
- SDRAM boundaries are mechanically checked.
- Documentation matches the current architecture.
