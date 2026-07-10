# LTDC DSOEng Flicker Diagnosis Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Isolate why enabling `DSO_Engine_Task` causes LTDC FIFO underrun and visible flicker while a frozen UI frame is being scanned.

**Architecture:** Keep the display in the already-proven frozen first-frame diagnostic mode so LTDC scans static VRAM. Re-enable only one DSOEng sub-path per build, using `User/bsp/y_t_stress_ab.h` A/B switches plus the existing host contract test to prove the intended diagnostic state. If a diagnostic has no effect, restore that switch before trying the next one.

**Tech Stack:** STM32H743 firmware, FreeRTOS, emWin/LTDC/DMA2D, ADC DMA acquisition, Keil MDK ARMCC, host C contract tests.

---

## Current Evidence

- Static LTDC diagnostic does not flicker.
- Full D-Cache disabled does not flicker, but targeted logical/physical cache clean did not solve it.
- VBR reload did not solve it.
- Freeze first scope frame still flickered when DSOEng was enabled.
- Physical framebuffer signature stayed stable during freeze, so LTDC was scanning unchanged content.
- Flicker events correlate with `g_lcdconf_ltdc_fu_cnt` increments.
- Wavegen-off did not solve it.
- MainTask periodic log-off did not solve it.
- `Y_T_AB_DISABLE_DSO_ENGINE_TASK=1` stops flicker; current visible waveform is `ADC_FakeData_Task`, not real ADC data.

## Working Hypothesis

DSOEng causes LTDC underrun through one of these paths:

1. ADC DMA itself contends with LTDC memory access or bus arbitration.
2. ADC DMA half/full callbacks perform DCache invalidation bursts that stall LTDC reads.
3. DSOEng CPU processing/copying reads DMA buffers heavily enough to starve LTDC.
4. DSOEng writes SDRAM record/roll-history buffers and competes with LTDC.
5. A peripheral interrupt storm or priority issue delays LTDC servicing or SDRAM access.

## Task 1: DSOEng Runs, Acquisition Never Starts

**Files:**
- Modify: `User/bsp/y_t_stress_ab.h`
- Modify: `User/dso/dso_acq.c`
- Modify: `tests/test_lcd_render_latch_cache_contract.c`

- [ ] **Step 1: Restore DSOEng task creation**

Set:

```c
#define Y_T_AB_DISABLE_DSO_ENGINE_TASK  0
```

- [ ] **Step 2: Add `Y_T_AB_DISABLE_ACQ_START`**

Add to `User/bsp/y_t_stress_ab.h`:

```c
/* 1 = keep DSOEng alive but prevent ADC/DMA acquisition from starting. */
#ifndef Y_T_AB_DISABLE_ACQ_START
#define Y_T_AB_DISABLE_ACQ_START  1
#endif
```

- [ ] **Step 3: Gate `DSO_Acq_Start()`**

In `User/dso/dso_acq.c`, include `y_t_stress_ab.h` and change:

```c
void DSO_Acq_Start(void) { s_backend->Start(); }
```

to:

```c
void DSO_Acq_Start(void)
{
#if (Y_T_AB_DISABLE_ACQ_START == 0)
    s_backend->Start();
#endif
}
```

- [ ] **Step 4: Update contract test**

Require:

```c
"#define Y_T_AB_DISABLE_DSO_ENGINE_TASK  0"
"#define Y_T_AB_DISABLE_ACQ_START  1"
"#if (Y_T_AB_DISABLE_ACQ_START == 0)"
```

- [ ] **Step 5: Run**

```bash
"/mnt/e/mingw64/bin/gcc.exe" -Wall -Wextra -std=c99 tests/test_lcd_render_latch_cache_contract.c -o tests/test_lcd_render_latch_cache_contract.exe && ./tests/test_lcd_render_latch_cache_contract.exe
"/mnt/e/Keil5 MDKcore/UV4/UV4.exe" -b "D:\DSO_V8.4\Project\MDK-ARM(uV5)\project.uvprojx" -t Flash
```

Expected: Keil log shows `0 Error(s)`.

**Hardware decision:**
- No flicker: root is ADC/DMA start path or later acquisition activity.
- Flicker: root is DSOEng init/service without acquisition; inspect FreqMeter, record init, UI publish, CPU load.

## Task 2: Acquisition Starts, DMA Callbacks Do Minimal Work

**Files:**
- Modify: `User/bsp/y_t_stress_ab.h`
- Modify: `User/dso/dso_acq_8bit.c`
- Modify: `User/dso/dso_acq_10bit.c`
- Modify: `tests/test_lcd_render_latch_cache_contract.c`

- [ ] **Step 1: Restore Task 1 if no effect**

Set:

```c
#define Y_T_AB_DISABLE_ACQ_START  0
```

- [ ] **Step 2: Add callback cache A/B**

```c
/* 1 = skip DCache invalidation in ADC DMA half/full callbacks. Diagnostic only. */
#ifndef Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE
#define Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE  1
#endif
```

- [ ] **Step 3: Gate callback invalidates**

Wrap each DMA half/full callback `DSO_AcqCommon_InvalidateDCacheWindow(...)` in:

```c
#if (Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE == 0)
    DSO_AcqCommon_InvalidateDCacheWindow(...);
#endif
```

**Hardware decision:**
- No flicker: callback cache maintenance bursts are starving LTDC.
- Flicker: ADC DMA traffic or later DSOEng CPU work remains suspect.

## Task 3: Acquisition Runs, DSOEng Does Not Copy/Process Frames

**Files:**
- Modify: `User/bsp/y_t_stress_ab.h`
- Modify: `User/dso/dso_engine.c`
- Modify: `tests/test_lcd_render_latch_cache_contract.c`

- [ ] **Step 1: Restore Task 2 if no effect**

Set:

```c
#define Y_T_AB_DISABLE_ACQ_DMA_CALLBACK_CACHE_INVALIDATE  0
```

- [ ] **Step 2: Add DSOEng service-only A/B**

```c
/* 1 = after DSO_Acq_Service(), skip DSO frame copy/process/publish loop body. */
#ifndef Y_T_AB_DSO_ENGINE_ACQ_SERVICE_ONLY
#define Y_T_AB_DSO_ENGINE_ACQ_SERVICE_ONLY  1
#endif
```

- [ ] **Step 3: Gate engine loop**

Immediately after `DSO_Acq_Service();` in `DSO_Engine_Task`:

```c
#if (Y_T_AB_DSO_ENGINE_ACQ_SERVICE_ONLY != 0)
        vTaskDelay(pdMS_TO_TICKS(DSO_ENGINE_SERVICE_INTERVAL_MS));
        continue;
#endif
```

**Hardware decision:**
- No flicker: CPU copy/process/record/UI publish is the problem.
- Flicker: raw ADC DMA/peripheral traffic is enough to starve LTDC.

## Task 4: Disable SDRAM Record/Roll Writes Only

**Files:**
- Modify: `User/bsp/y_t_stress_ab.h`
- Modify: `User/dso/dso_engine.c`

- [ ] **Step 1: Restore Task 3 if no effect**

Set:

```c
#define Y_T_AB_DSO_ENGINE_ACQ_SERVICE_ONLY  0
```

- [ ] **Step 2: Add SDRAM write A/B**

```c
/* 1 = skip DSO_Record_Ingest* and DSO_EngineRoll_RingFeed SDRAM writes. */
#ifndef Y_T_AB_DISABLE_DSO_SDRAM_HISTORY_WRITES
#define Y_T_AB_DISABLE_DSO_SDRAM_HISTORY_WRITES  1
#endif
```

- [ ] **Step 3: Gate record/roll writes**

Wrap:

```c
DSO_Record_Ingest(...)
DSO_Record_IngestCh2(...)
DSO_EngineRoll_RingFeed(...)
```

with:

```c
#if (Y_T_AB_DISABLE_DSO_SDRAM_HISTORY_WRITES == 0)
    ...
#endif
```

**Hardware decision:**
- No flicker: SDRAM record/roll writes are starving LTDC.
- Flicker: CPU read/process or ADC DMA remains suspect.

## Likely Fix Directions After Isolation

- If ADC DMA traffic alone causes flicker: reduce active ADC DMA bandwidth, move DMA buffers to a less contentious SRAM region, lower DMA priority, adjust DMA burst/FIFO settings, or relax LTDC pixel clock/blanking.
- If DMA callback cache maintenance causes flicker: replace large half-buffer invalidates with smaller recent-window invalidates outside IRQ, or move DMA buffers to non-cacheable MPU region.
- If CPU processing causes flicker: throttle DSOEng during active LTDC scan, chunk processing, or schedule heavy work after VSYNC.
- If SDRAM history writes cause flicker: keep display-protect mode stricter, defer history writes, write in small chunks during blanking, or disable live record/roll prefeed at high bandwidth.

## Stop Conditions

- Do not stack more than one new diagnostic switch at a time.
- If a switch has no effect, restore it before the next burn.
- If a switch fixes flicker, keep it only long enough to split its sub-path further, then replace it with a root-cause fix.
