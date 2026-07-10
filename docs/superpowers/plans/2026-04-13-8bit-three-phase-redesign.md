# 8-Bit Three-Phase Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Preserve the working 8-bit `single-safe` path, remove the obsolete 8-bit `dual` path, and rebuild one clean 8-bit `three-phase` acquisition backend that becomes the only multi-lane 8-bit architecture.

**Architecture:** Shrink the 8-bit topology contract from three modes to two modes, then simplify `dso_acq_8bit.c` so backend dispatch, DMA flow, snapshot logic, and sample copy logic branch only between `single-safe` and `three-phase`. Remove engine fallback to the deleted dual path, keep lane-merge helpers as the merge primitive, and update serial/debug output so the observable runtime model matches the new architecture.

**Tech Stack:** C, STM32H7 HAL, FreeRTOS project code, host-side C tests, serial debug helpers, Keil/IAR embedded firmware projects.

---

### Task 1: Collapse the 8-bit topology contract to `single-safe` and `three-phase`

**Files:**
- Modify: `D:\DSO_V5\DSO_V4\User\dso\dso_config.h`
- Modify: `D:\DSO_V5\DSO_V4\User\dso\dso_acq_8bit.c`
- Modify: `D:\DSO_V5\DSO_V4\User\dso\dso_acq.c`
- Modify: `D:\DSO_V5\DSO_V4\User\dso\dso_acq.h`

- [ ] **Step 1: Replace the public 8-bit topology constants**
  - Remove `DSO_ACQ8_TOPOLOGY_DUAL_FAST_PRODUCTION`.
  - Rename `DSO_ACQ8_TOPOLOGY_TRIPLE_EXPERIMENTAL` to a production contract such as `DSO_ACQ8_TOPOLOGY_THREE_PHASE`.
  - Keep `DSO_ACQ8_TOPOLOGY_DEFAULT` explicit and valid.

- [ ] **Step 2: Simplify topology sanitization and helper predicates in `dso_acq_8bit.c`**
  - Remove `topology_is_dual_hw()`.
  - Update `sanitize_topology()`, `topology_lane_count()`, `topology_uses_lane2()`, `topology_uses_tim8()`, and `topology_profile_name()` so only two backend states remain.
  - Preserve `single-safe` behavior exactly.

- [ ] **Step 3: Keep the public acquisition API stable while removing deleted topology assumptions**
  - Verify `DSO_Acq_GetHs8Topology()` and `DSO_Acq_RequestHs8Topology()` still compile cleanly.
  - Ensure no public header or wrapper still advertises the deleted dual mode.

- [ ] **Step 4: Build-check the topology contract locally through focused file review**
  - Run: `rg -n "DUAL_FAST_PRODUCTION|TRIPLE_EXPERIMENTAL" User/dso -g"*.[ch]"`
  - Expected after this task: no remaining topology contract references outside places intentionally pending cleanup in later tasks.

### Task 2: Remove the deleted dual backend from the 8-bit acquisition core

**Files:**
- Modify: `D:\DSO_V5\DSO_V4\User\dso\dso_acq_8bit.c`
- Keep unchanged: `D:\DSO_V5\DSO_V4\User\dso\dso_acq8_sample_unpack.c`
- Keep unchanged: `D:\DSO_V5\DSO_V4\User\dso\dso_acq8_sample_unpack.h`

- [ ] **Step 1: Delete dual-backend initialization and streaming code**
  - Remove `configure_dual_hw_backend()`.
  - Remove `start_dual_hw_streaming()` and `stop_dual_hw_streaming()`.
  - Remove dual-only DMA storage and any dead helper that exists only for packed dual mode if it is no longer referenced.

- [ ] **Step 2: Collapse backend dispatch to two branches**
  - Update `configure_active_backend()`, `start_active_streaming()`, and `stop_active_streaming()` so they branch only between `single-safe` and `three-phase`.
  - Keep the `single-safe` branch untouched in behavior.

- [ ] **Step 3: Remove dual snapshot and dual sample-copy branches**
  - Delete `snapshot_dual_hw_position()`.
  - Update `snapshot_common_position()` so it dispatches only to `single-safe` or `three-phase`.
  - Remove all `topology_is_dual_hw()` branches from `DSO_Acq8_CopyRecentSamples()`, `DSO_Acq8_CopyNewSamples()`, and serial snapshot collection.

- [ ] **Step 4: Verify no deleted dual path remains in the core acquisition file**
  - Run: `rg -n "dual|MultiMode|ADC_DUALMODE_INTERL|s_dma_dual|topology_is_dual_hw" User/dso/dso_acq_8bit.c`
  - Expected after this task: no runtime dual backend code remains.

### Task 3: Rebuild the 8-bit three-phase backend as the only multi-lane path

**Files:**
- Modify: `D:\DSO_V5\DSO_V4\User\dso\dso_acq_8bit.c`
- Reuse unchanged unless required: `D:\DSO_V5\DSO_V4\User\dso\dso_lane_merge.c`
- Reuse unchanged unless required: `D:\DSO_V5\DSO_V4\User\dso\dso_lane_merge.h`

- [ ] **Step 1: Rename and normalize the three-phase backend entry points**
  - Rename `configure_triple_exp_backend()` to a production three-phase name.
  - Rename `snapshot_triple_exp_position()` similarly.
  - Update callers so the file no longer carries experimental naming.

- [ ] **Step 2: Keep the three-phase core path focused on steady-state lane acquisition**
  - Preserve ADC1/ADC2/ADC3 independent trigger setup on `TIM1 CC1/CC2/CC3`.
  - Preserve three independent DMA rings (`lane0`, `lane1`, `lane2`).
  - Preserve common-delta snapshot through `DSO_LaneMerge_CommonDelta3()`.
  - Preserve aligned 3-lane sample merge through `DSO_Acq8_CopyAligned3Bytes()`.

- [ ] **Step 3: Separate preview responsibility from core topology semantics**
  - Keep preview capture as an auxiliary borrow/restore flow if still required.
  - Ensure preview logic does not define lane count, topology name, or fallback behavior.
  - Ensure the core three-phase path remains valid even when preview is disabled.

- [ ] **Step 4: Re-check timebase/timer assumptions after the contract shrink**
  - Confirm `single-safe` still uses `TIM8`.
  - Confirm `three-phase` still uses `TIM1`.
  - Confirm `DSO_Acq8_SetTimArr()` still updates the correct timer and CCR registers under the new two-topology model.

### Task 4: Remove engine fallback to the deleted dual architecture

**Files:**
- Modify: `D:\DSO_V5\DSO_V4\User\dso\dso_engine.c`

- [ ] **Step 1: Delete the “triple fail -> dual fallback” policy**
  - Remove the branch that calls `DSO_Acq_RequestHs8Topology(DSO_ACQ8_TOPOLOGY_DUAL_FAST_PRODUCTION)`.
  - Remove any now-invalid fail-streak behavior that only existed to support that rollback.

- [ ] **Step 2: Keep diagnostics without changing topology automatically**
  - Preserve the lane-cal failure accounting that is still useful for serial/debug visibility.
  - Keep the acquisition topology unchanged on repeated lane-2 solve failures.

- [ ] **Step 3: Verify no engine logic still targets deleted topology names**
  - Run: `rg -n "DUAL_FAST_PRODUCTION|TRIPLE_EXPERIMENTAL" User/dso/dso_engine.c`
  - Expected after this task: engine policy references only the retained topology model.

### Task 5: Align serial/debug output with the new two-topology architecture

**Files:**
- Modify: `D:\DSO_V5\DSO_V4\User\dso\dso_serial_debug.c`
- Modify: `D:\DSO_V5\DSO_V4\User\dso\dso_serial_debug_format.c`
- Modify: `D:\DSO_V5\DSO_V4\User\dso\dso_serial_debug_format.h`
- Modify if required: `D:\DSO_V5\DSO_V4\User\dso\dso_acq.h`

- [ ] **Step 1: Remove stale deleted-topology wording from runtime/debug summaries**
  - Ensure topology IDs and labels now describe only `single-safe` and `three-phase`.
  - Preserve lane windows, delta reporting, and preview visibility where still valid.

- [ ] **Step 2: Keep the logs useful for three-phase bring-up**
  - Continue exposing lane positions, common delta, and phase-sync error diagnostics.
  - Ensure no field name suggests that a deleted dual mode still exists.

- [ ] **Step 3: Re-run the serial debug host test**
  - Run: `.\tests\test_dso_serial_debug.exe`
  - Expected: pass with no regression from the topology cleanup.

### Task 6: Add or update focused host-side verification for the reduced architecture

**Files:**
- Modify or create: `D:\DSO_V5\DSO_V4\tests\test_dso_serial_debug.c`
- Modify or create if needed: `D:\DSO_V5\DSO_V4\tests\test_dso_acq8_tim_trigger.c`
- Keep existing coverage: `D:\DSO_V5\DSO_V4\tests\test_dso_acq8_sample_unpack.c`

- [ ] **Step 1: Preserve current 8-bit unpack coverage**
  - Run: `.\tests\test_dso_acq8_sample_unpack.exe`
  - Expected: pass; this confirms the earlier single-safe decode fix still stands.

- [ ] **Step 2: Extend host-visible assertions that depend on topology/debug contract if required**
  - Add or update a small assertion set so deleted topology labels and values are not still emitted by debug formatting helpers.
  - Keep the test deterministic and independent from firmware hardware state.

- [ ] **Step 3: Re-run the high-frequency pipeline regression**
  - Run: `.\tests\test_dso_hf_pipeline.exe`
  - Expected: pass; no regression in the high-speed processing path.

### Task 7: Final integration verification and manual firmware check list

**Files:**
- No new code required in this task; verification only.

- [ ] **Step 1: Search the tree for deleted topology residue**
  - Run: `rg -n "DUAL_FAST_PRODUCTION|TRIPLE_EXPERIMENTAL|8bit_dual_hw|8bit_triple_exp" User tests`
  - Expected: no stale runtime references remain after the rename/cleanup.

- [ ] **Step 2: Re-run the full required host checks**
  - Run: `.\tests\test_dso_acq8_sample_unpack.exe`
  - Run: `.\tests\test_dso_serial_debug.exe`
  - Run: `.\tests\test_dso_hf_pipeline.exe`
  - Expected: all pass.

- [ ] **Step 3: Manual firmware validation checklist**
  - Confirm 8-bit `single-safe` still produces the already-verified clean `100 kHz / 1 Vpp / 1.65 V offset` waveform.
  - Confirm grounded-input 8-bit behavior remains quiet and valid.
  - Confirm the rebuilt three-phase backend reports the new topology name and no deleted dual semantics in UART logs.

- [ ] **Step 4: Diff checkpoint**
  - Since this checkout is not a git worktree, capture a manual diff summary instead of a commit.
  - Record which files changed and verify each one maps back to the approved design scope.
