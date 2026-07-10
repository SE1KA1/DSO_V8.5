# 8-Bit Dual Interleaved Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the 8-bit three-phase runtime sampling chain with an STM32H743-native ADC1+ADC2 dual-interleaved backend while keeping the 12-bit acquisition path unchanged.

**Architecture:** Keep `8bit single-safe` as the fallback path and introduce one new `8bit dual-interleaved` backend that uses `ADC_DUALMODE_INTERL`, `ADC_DUALMODEDATAFORMAT_8_BITS`, one master DMA ring, and a dedicated packed-8bit unpack path. Remove the active three-phase runtime chain and align serial-debug/runtime math to a 2-lane model.

**Tech Stack:** STM32H7 HAL ADC multi-mode, TIM8 trigger, C host regression tests, existing DSO engine/serial debug pipeline.

---

### Task 1: Update 8-bit topology contract tests first

**Files:**
- Modify: `tests/test_dso_acq8_topology_contract.c`
- Modify: `tests/test_dso_acq8_legacy_cleanup.c`

- [ ] **Step 1: Write the failing topology expectations**

Change the topology contract test to require:

- `DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED` exists
- `DSO_ACQ8_TOPOLOGY_THREE_PHASE` no longer exists
- default topology is not forced to three-phase

- [ ] **Step 2: Run the focused contract test and verify it fails**

Run: `cl /nologo /I. /IUser tests\\test_dso_acq8_topology_contract.c /Fe:tests\\test_dso_acq8_topology_contract_new.exe`

Expected: compile failure or runtime failure because code still exposes `THREE_PHASE`.

- [ ] **Step 3: Remove false legacy-cleanup bans on ADC multimode APIs**

Edit `tests/test_dso_acq8_legacy_cleanup.c` so it no longer forbids:

- `ADC_DUALMODE_INTERL`
- `HAL_ADCEx_MultiModeConfigChannel`
- `HAL_ADCEx_MultiModeStart_DMA`
- `HAL_ADCEx_MultiModeStop_DMA`

Keep bans for truly deleted 8-bit three-phase/legacy names.

- [ ] **Step 4: Run the cleanup test build and verify it still compiles**

Run: `cl /nologo /I. /IUser tests\\test_dso_acq8_legacy_cleanup.c /Fe:tests\\test_dso_acq8_legacy_cleanup_new.exe`

Expected: build succeeds.

### Task 2: Add failing unpack tests for 8-bit packed dual DMA words

**Files:**
- Modify: `tests/test_dso_acq8_sample_unpack.c`
- Modify: `User/dso/dso_acq8_sample_unpack.h`

- [ ] **Step 1: Add a failing test for one packed 32-bit word**

Add a test that asserts one packed word decodes in order:

`MST t0, SLV t0, MST t1, SLV t1`

Example packed word:

`0x44332211 -> 0x11, 0x22, 0x33, 0x44`

- [ ] **Step 2: Add a failing ring-copy test across word boundaries**

Add a test that copies recent/new samples from a packed word ring and checks ordered output across wrap.

- [ ] **Step 3: Build the unpack test and verify it fails**

Run: `cl /nologo /I. /IUser tests\\test_dso_acq8_sample_unpack.c User\\dso\\dso_acq8_sample_unpack.c /Fe:tests\\test_dso_acq8_sample_unpack_new.exe`

Expected: build or runtime failure because packed-dual helpers do not exist yet.

### Task 3: Replace the 8-bit topology contract in configuration

**Files:**
- Modify: `User/dso/dso_config.h`

- [ ] **Step 1: Replace three-phase topology definitions**

Change the 8-bit topology section so it defines:

- `DSO_ACQ8_TOPOLOGY_SINGLE_SAFE`
- `DSO_ACQ8_TOPOLOGY_DUAL_INTERLEAVED`

Remove:

- `DSO_ACQ8_TOPOLOGY_THREE_PHASE`
- `DSO_ACQ8_LANE2_CHANNEL_INDEX`

- [ ] **Step 2: Keep default topology explicit**

Set the default to the existing low-risk choice unless later validation requires otherwise.

- [ ] **Step 3: Rebuild the topology contract test**

Run: `cl /nologo /I. /IUser tests\\test_dso_acq8_topology_contract.c /Fe:tests\\test_dso_acq8_topology_contract_new.exe`

Expected: build succeeds and runtime passes after later tasks complete.

### Task 4: Implement packed-dual 8-bit unpack helpers

**Files:**
- Modify: `User/dso/dso_acq8_sample_unpack.h`
- Modify: `User/dso/dso_acq8_sample_unpack.c`
- Test: `tests/test_dso_acq8_sample_unpack.c`

- [ ] **Step 1: Add helper APIs**

Add helper declarations for:

- unpack one 32-bit packed dual word into 4 `uint16_t` samples
- copy recent samples from a packed dual ring
- copy new samples from a packed dual ring

- [ ] **Step 2: Implement minimal helpers**

Implement only the order guaranteed by RM0433:

- byte0 = MST t0
- byte1 = SLV t0
- byte2 = MST t1
- byte3 = SLV t1

- [ ] **Step 3: Run the unpack tests**

Run: `cl /nologo /I. /IUser tests\\test_dso_acq8_sample_unpack.c User\\dso\\dso_acq8_sample_unpack.c /Fe:tests\\test_dso_acq8_sample_unpack_new.exe`

Then run: `.\tests\test_dso_acq8_sample_unpack_new.exe`

Expected: PASS

### Task 5: Replace 8-bit three-phase backend with dual-interleaved backend

**Files:**
- Modify: `User/dso/dso_acq_8bit.c`

- [ ] **Step 1: Replace topology helpers**

Update topology sanitization and lane-count helpers to support only:

- single-safe -> lane count 1
- dual-interleaved -> lane count 2

- [ ] **Step 2: Remove active three-phase runtime state**

Remove or neutralize:

- `lane2_adc_channel()`
- active `ADC3` runtime DMA for 8-bit high-speed sampling
- TIM1 CC1/CC2/CC3 trigger chain
- three-lane common-position/merge flow

- [ ] **Step 3: Add dual-interleaved hardware init**

Configure:

- `ADC1` master, trigger `TIM8 TRGO`, DMA circular
- `ADC2` slave, same input channel and sample time
- `ADC_DUALMODE_INTERL`
- `ADC_DUALMODEDATAFORMAT_8_BITS`
- dedicated `TwoSamplingDelay`

- [ ] **Step 4: Add master DMA packed-word ring**

Use one `uint32_t` DMA ring sized for 8-bit packed dual acquisition.

- [ ] **Step 5: Replace copy/recent/new sample paths**

Route dual topology through the new packed-dual unpack helpers instead of `CopyAligned3Bytes`.

- [ ] **Step 6: Fix runtime sample-rate math**

Make dual topology report:

`sample_rate_hz = 2 * trigger_rate_hz`

not `3 * trigger_rate_hz`.

### Task 6: Remove three-phase-only engine/debug coupling

**Files:**
- Modify: `User/dso/dso_engine.c`
- Modify: `User/dso/dso_serial_debug.c`
- Modify: `User/dso/dso_serial_debug_format.c`
- Modify: `User/dso/dso_serial_debug_format.h`
- Modify: `User/dso/dso_acq.h`

- [ ] **Step 1: Remove three-phase-only engine fallback**

Delete the runtime path in `dso_engine.c` that reacts specifically to `DSO_ACQ8_TOPOLOGY_THREE_PHASE`.

- [ ] **Step 2: Keep debug structures compatible with lane count 2**

Do not break the existing snapshot/reporting API, but make formatting behave correctly when `hs8_lane_count == 2`.

- [ ] **Step 3: Stop advertising lane2 hardware config as an active 8-bit requirement**

Serial debug should still print lane-count and topology, but no longer depend on `lane2_ch` as an architectural control input.

### Task 7: Run host verification

**Files:**
- Test: `tests/test_dso_acq8_topology_contract_new.exe`
- Test: `tests/test_dso_acq8_legacy_cleanup_new.exe`
- Test: `tests/test_dso_acq8_sample_unpack_new.exe`
- Test: `tests/test_dso_serial_debug.exe`
- Test: `tests/test_dso_hf_pipeline.exe`

- [ ] **Step 1: Run focused new tests**

Run:

- `.\tests\test_dso_acq8_topology_contract_new.exe`
- `.\tests\test_dso_acq8_legacy_cleanup_new.exe`
- `.\tests\test_dso_acq8_sample_unpack_new.exe`

Expected: PASS

- [ ] **Step 2: Run existing regression tests**

Run:

- `.\tests\test_dso_serial_debug.exe`
- `.\tests\test_dso_hf_pipeline.exe`

Expected: PASS

- [ ] **Step 3: Review remaining risks**

Call out:

- any remaining serial-debug fields still carrying legacy names
- any runtime assumptions still tied to old three-phase semantics
- firmware-only validation still needed on hardware
