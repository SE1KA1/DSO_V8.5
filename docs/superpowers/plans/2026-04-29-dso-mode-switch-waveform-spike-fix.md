# DSO Mode Switch Waveform Spike Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix persistent 10-bit waveform spikes and 8-bit degradation after 8-bit/10-bit mode switching by correcting 10-bit TIM1 startup and 10-bit DMA IRQ cleanup.

**Architecture:** Keep acquisition mode switching as a single explicit path through `dso_acq.c`. Modify only the 10-bit backend for synchronized TIM1 CC startup and explicit cleanup of its DMA IRQs. Treat serial output as a validation preflight because the hardware proof depends on lane/IRQ diagnostics.

**Tech Stack:** STM32H743 HAL C firmware, FreeRTOS, Keil MDK project, host-side C contract tests under `tests/`.

---

## File Map

- Modify: `User/dso/dso_acq_10bit.c`
  - Add 10-bit-local synchronized TIM1 PWM start helper.
  - Add 10-bit-local DMA IRQ cleanup helper.
  - Use synchronized PWM start in the PWM1 path.
  - Clear 10-bit DMA IRQs during deinit.

- Modify: `tests/test_dso_acq10_sampling_chain_contract.c`
  - Extend static contract coverage for synchronized TIM1 PWM startup.
  - Extend static contract coverage for 10-bit DMA IRQ cleanup.

- No planned changes: `User/dso/dso_acq_8bit.c`
  - 8-bit architecture is frozen.

- No planned changes: `User/ui/*`
  - UI clearing or frame invalidation is out of scope for this fix.

---

### Task 0: Serial Diagnostic Preflight

**Files:**
- Read: `User/dso/dso_config.h`
- Read: `Project/MDK-ARM(uV5)/project.uvprojx`
- Read: `User/bsp/inc/bsp_uart_fifo.h`
- Read: `User/bsp/src/bsp_uart_fifo.c`
- Read: `User/dso/dso_wavegen.c`

- [ ] **Step 1: Confirm current serial build configuration**

Run:

```bash
rg -n "DSO_SERIAL_DEBUG_ENABLE|DSO_APP_INFO_PRINTF_ENABLE|DSO_VOFA_FIREWATER_ENABLE|DSO_FREQ_METER_SERIAL_DEBUG_ENABLE|DSO_WAVEGEN_ENABLE" User/dso/dso_config.h Project/MDK-ARM\\(uV5\\)/project.uvprojx
```

Expected observations:

- `DSO_SERIAL_DEBUG_ENABLE` defaults to `0u`.
- `DSO_APP_INFO_PRINTF_ENABLE` defaults to `0u`.
- `DSO_VOFA_FIREWATER_ENABLE` defaults to `0u`.
- `DSO_FREQ_METER_SERIAL_DEBUG_ENABLE` defaults to `0u`.
- `DSO_WAVEGEN_ENABLE` defaults to `1u`.
- Keil project currently defines `USE_HAL_DRIVER`, `STM32H743xx`, `DSO_PLOTBUF_GUARD_ENABLE=1`, and `MAINTASK_STACK_LOG_INTERVAL_MS=2000`, but does not enable serial debug.

- [ ] **Step 2: Confirm COM1 physical and baud assumptions**

Run:

```bash
rg -n "UART1_BAUD|COM1|USART1_TX_PIN|USART1_RX_PIN|fputc|comSetBaud" User/bsp/inc/bsp_uart_fifo.h User/bsp/src/bsp_uart_fifo.c
```

Expected observations:

- COM1 is USART1.
- USART1 TX/RX are PA9/PA10.
- `UART1_BAUD` is `1152000`.
- `fputc()` routes `printf` to `comSendChar(COM1, ch)`.

- [ ] **Step 3: Classify the no-output report**

Use this decision table before changing code:

| Observation | Meaning | Action |
|---|---|---|
| No serial debug frames, but wavegen/startup prints appear | Expected default config; serial diagnostics are disabled | Enable diagnostics only for the validation build |
| Absolutely no bytes at 1152000 baud | printf/COM1/link/baud issue | Check terminal baud, PA9 wiring, USART1 init, and whether the flashed image includes current code |
| Output appears at 115200 but not 1152000 | Terminal baud mismatch or project macro override | Use 1152000 or inspect build defines |
| Output stops after enabling serial debug | bandwidth or debug silencing issue | Reduce diagnostic interval or use phase-only mode |

- [ ] **Step 4: Choose validation build flags**

For the hardware validation build, use Keil preprocessor defines rather than changing product defaults:

```text
DSO_SERIAL_DEBUG_ENABLE=1
DSO_SERIAL_DEBUG_FORCE_SNAPSHOT=1
DSO_SERIAL_DEBUG_HS8_STAGE=2
DSO_APP_INFO_PRINTF_ENABLE=1
```

Do not commit these as default product values unless explicitly requested. Product defaults stay quiet.

---

### Task 1: Write Failing Contract Test for 10-bit Startup and IRQ Cleanup

**Files:**
- Modify: `tests/test_dso_acq10_sampling_chain_contract.c`

- [ ] **Step 1: Add helper checks if missing**

Ensure the test file has `require_contains`, `require_not_contains`, and `require_ordered`. They already exist in the current file. Do not duplicate them.

- [ ] **Step 2: Add synchronized PWM startup assertions**

Add these assertions after the existing phase assertions:

```c
    ok &= require_contains(acq10_path, acq10, "static void start_tim1_pwm_channels_synchronized(void)");
    ok &= require_contains(acq10_path, acq10, "TIM_CCxChannelCmd(tim, TIM_CHANNEL_1, TIM_CCx_ENABLE);");
    ok &= require_contains(acq10_path, acq10, "TIM_CCxChannelCmd(tim, TIM_CHANNEL_2, TIM_CCx_ENABLE);");
    ok &= require_contains(acq10_path, acq10, "TIM_CCxChannelCmd(tim, TIM_CHANNEL_3, TIM_CCx_ENABLE);");
    ok &= require_contains(acq10_path, acq10, "__HAL_TIM_MOE_ENABLE(&s_acq10.htim1);");
    ok &= require_contains(acq10_path, acq10, "__HAL_TIM_ENABLE(&s_acq10.htim1);");
    ok &= require_ordered(acq10_path,
                          acq10,
                          "TIM_CCxChannelCmd(tim, TIM_CHANNEL_3, TIM_CCx_ENABLE);",
                          "__HAL_TIM_ENABLE(&s_acq10.htim1);");
    ok &= require_contains(acq10_path, acq10, "start_tim1_pwm_channels_synchronized();");
    ok &= require_not_contains(acq10_path, acq10, "HAL_TIM_PWM_Start(&s_acq10.htim1, TIM_CHANNEL_1)");
```

This intentionally fails on the current source because 10-bit PWM startup is sequential.

- [ ] **Step 3: Add 10-bit DMA IRQ cleanup assertions**

Add these assertions after the DMA/cache assertions:

```c
    ok &= require_contains(acq10_path, acq10, "static void reset_dma_irq(IRQn_Type irqn)");
    ok &= require_contains(acq10_path, acq10, "HAL_NVIC_DisableIRQ(irqn);");
    ok &= require_contains(acq10_path, acq10, "HAL_NVIC_ClearPendingIRQ(irqn);");
    ok &= require_contains(acq10_path, acq10, "reset_dma_irq(DMA1_Stream1_IRQn);");
    ok &= require_contains(acq10_path, acq10, "reset_dma_irq(DMA1_Stream2_IRQn);");
    ok &= require_contains(acq10_path, acq10, "reset_dma_irq(DMA2_Stream1_IRQn);");
```

- [ ] **Step 4: Run the test and verify it fails**

Run:

```bash
./tests/test_dso_acq10_sampling_chain_contract.exe
```

Expected result before implementation:

```text
User/dso/dso_acq_10bit.c: missing "static void start_tim1_pwm_channels_synchronized(void)"
User/dso/dso_acq_10bit.c: missing "reset_dma_irq(DMA1_Stream1_IRQn);"
```

The exact line order may differ, but the failure must be about missing synchronized startup and IRQ cleanup.

---

### Task 2: Implement 10-bit DMA IRQ Cleanup

**Files:**
- Modify: `User/dso/dso_acq_10bit.c`

- [ ] **Step 1: Add local IRQ cleanup helper**

Add this helper near the existing DMA handle setup helpers, after `init_dma_handle()`:

```c
static void reset_dma_irq(IRQn_Type irqn)
{
    HAL_NVIC_DisableIRQ(irqn);
    HAL_NVIC_ClearPendingIRQ(irqn);
}
```

- [ ] **Step 2: Call cleanup from `DSO_Acq10_Deinit()`**

In `DSO_Acq10_Deinit()`, after the three `HAL_DMA_DeInit()` blocks and before `memset(&s_acq10, 0, sizeof(s_acq10));`, add:

```c
    reset_dma_irq(DMA1_Stream1_IRQn);
    reset_dma_irq(DMA1_Stream2_IRQn);
    reset_dma_irq(DMA2_Stream1_IRQn);
```

- [ ] **Step 3: Run the contract test**

Run:

```bash
./tests/test_dso_acq10_sampling_chain_contract.exe
```

Expected result at this point:

- IRQ cleanup assertions pass.
- The test still fails on synchronized PWM startup assertions.

---

### Task 3: Implement 10-bit Synchronized TIM1 PWM Startup

**Files:**
- Modify: `User/dso/dso_acq_10bit.c`

- [ ] **Step 1: Add synchronized PWM startup helper**

Add this helper immediately before `start_phase_timer()`:

```c
static void start_tim1_pwm_channels_synchronized(void)
{
    TIM_TypeDef *tim = s_acq10.htim1.Instance;

    tim->CR1 &= ~TIM_CR1_CEN;
    tim->CNT = 0u;
    tim->EGR = TIM_EGR_UG;
    TIM_CCxChannelCmd(tim, TIM_CHANNEL_1, TIM_CCx_ENABLE);
    TIM_CCxChannelCmd(tim, TIM_CHANNEL_2, TIM_CCx_ENABLE);
    TIM_CCxChannelCmd(tim, TIM_CHANNEL_3, TIM_CCx_ENABLE);
    __HAL_TIM_MOE_ENABLE(&s_acq10.htim1);
    __HAL_TIM_ENABLE(&s_acq10.htim1);
}
```

- [ ] **Step 2: Replace sequential PWM startup**

In `start_phase_timer()`, replace the PWM1 branch:

```c
    if (DSO_Acq10_GetTimTriggerMode() == DSO_ACQ10_TIM_TRIGGER_MODE_PWM1) {
        if (HAL_TIM_PWM_Start(&s_acq10.htim1, TIM_CHANNEL_1) != HAL_OK) {
            while (1) {}
        }
        if (HAL_TIM_PWM_Start(&s_acq10.htim1, TIM_CHANNEL_2) != HAL_OK) {
            while (1) {}
        }
        if (HAL_TIM_PWM_Start(&s_acq10.htim1, TIM_CHANNEL_3) != HAL_OK) {
            while (1) {}
        }
    } else {
```

with:

```c
    if (DSO_Acq10_GetTimTriggerMode() == DSO_ACQ10_TIM_TRIGGER_MODE_PWM1) {
        start_tim1_pwm_channels_synchronized();
    } else {
```

Leave the non-PWM `HAL_TIM_OC_Start()` branch unchanged.

- [ ] **Step 3: Run the contract test**

Run:

```bash
./tests/test_dso_acq10_sampling_chain_contract.exe
```

Expected result:

```text
<no output>
```

Exit code must be `0`.

---

### Task 4: Run Existing Host Regression Contracts

**Files:**
- No source changes.

- [ ] **Step 1: Run 10-bit sampling chain contract**

Run:

```bash
./tests/test_dso_acq10_sampling_chain_contract.exe
```

Expected: exit code `0`.

- [ ] **Step 2: Run 8-bit protection contract**

Run:

```bash
./tests/test_dso_acq8_sampling_chain_protection.exe
```

Expected: exit code `0`.

- [ ] **Step 3: Run engine sample-restart readiness contract**

Run:

```bash
./tests/test_dso_engine_8bit_ready_contract.exe
```

Expected output:

```text
all engine sample-restart readiness contract tests passed
```

Exit code must be `0`.

- [ ] **Step 4: Run repository smoke tests from AGENTS.md if available**

Run:

```bash
./tests/test_dso_serial_debug.exe
./tests/test_dso_hf_pipeline.exe
```

Expected: both exit with code `0`.

If either `.exe` cannot run in the current shell environment, record the failure text and continue to firmware build validation.

---

### Task 5: Firmware Build and Hardware Validation

**Files:**
- Build: `Project/MDK-ARM(uV5)/project.uvprojx`

- [ ] **Step 1: Build Keil target**

Open `Project/MDK-ARM(uV5)/project.uvprojx` in Keil and build target `Flash`.

Expected:

- Firmware builds without compile errors.
- Any change to `Project/output(mdk).hex` is intentional.

- [ ] **Step 2: Prepare serial terminal**

Connect terminal to COM1/USART1:

```text
Pins: PA9 TX, PA10 RX
Baud: 1152000
Format: 8N1
Line ending display: CRLF
```

If there is still no output, first check for wavegen startup lines:

```text
[WAVEGEN] start ...
```

Absence of this line suggests a UART/printf/baud/flashed-image issue, not merely disabled DSO serial debug.

- [ ] **Step 3: Enable validation diagnostics for the debug build only**

Use Keil preprocessor defines for the validation build:

```text
DSO_SERIAL_DEBUG_ENABLE=1
DSO_SERIAL_DEBUG_FORCE_SNAPSHOT=1
DSO_SERIAL_DEBUG_HS8_STAGE=2
DSO_APP_INFO_PRINTF_ENABLE=1
```

Do not commit these product-default changes unless requested.

- [ ] **Step 4: Validate 10-bit waveform**

On hardware:

1. Boot in default `8-bit (3L)`.
2. Switch to `10-bit`.
3. Observe waveform quality and serial snapshot fields.

Expected:

- 10-bit visible spikes are reduced.
- `phase_sync_errors` does not grow continuously after startup.
- `lane0_delta`, `lane1_delta`, and `common_delta` show coherent progress.
- `overrun_count` does not grow continuously.

- [ ] **Step 5: Validate 8-bit after mode round trip**

On hardware:

1. Switch from `10-bit` back to `8-bit (3L)`.
2. Observe waveform quality and serial snapshot fields.

Expected:

- 8-bit waveform does not degrade after returning from 10-bit.
- No burst of 10-bit residual DMA IRQ behavior appears in 8-bit diagnostics.
- 8-bit lane0/lane1/lane2 DMA position rates remain plausible.

---

### Task 6: Commit Implementation

**Files:**
- Stage only:
  - `User/dso/dso_acq_10bit.c`
  - `tests/test_dso_acq10_sampling_chain_contract.c`

- [ ] **Step 1: Review diff**

Run:

```bash
git diff -- User/dso/dso_acq_10bit.c tests/test_dso_acq10_sampling_chain_contract.c
```

Expected:

- 10-bit local synchronized PWM helper added.
- 10-bit local DMA IRQ cleanup helper added.
- `DSO_Acq10_Deinit()` clears only `DMA1_Stream1_IRQn`, `DMA1_Stream2_IRQn`, and `DMA2_Stream1_IRQn`.
- 8-bit files are untouched.

- [ ] **Step 2: Commit**

Run:

```bash
git add User/dso/dso_acq_10bit.c tests/test_dso_acq10_sampling_chain_contract.c
git commit -m "dso: synchronize 10bit mode switching"
```

Expected:

- Commit includes only the two planned files.

---

## Self-Review

- Spec coverage:
  - 10-bit synchronized TIM1 startup is covered by Task 1 and Task 3.
  - 10-bit DMA IRQ cleanup is covered by Task 1 and Task 2.
  - 8-bit frozen boundary is covered by File Map and Task 6 diff review.
  - UI non-change is covered by File Map and the design scope.
  - Serial validation blocker is covered by Task 0 and Task 5.

- Placeholder scan:
  - No unresolved implementation placeholders are intentionally present.

- Type and symbol consistency:
  - Uses existing `TIM_CCxChannelCmd`, `TIM_CHANNEL_1/2/3`, `TIM_CCx_ENABLE`, `__HAL_TIM_MOE_ENABLE`, and `__HAL_TIM_ENABLE`, matching the existing 8-bit implementation pattern.
  - Uses existing IRQ names from the 10-bit DMA configuration.
