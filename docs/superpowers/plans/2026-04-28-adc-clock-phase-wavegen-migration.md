# ADC Clock Phase Wavegen Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 V2 的 ADC 75MHz 时钟/相位模型与 PA4 DAC 波形发生器迁移到当前主工程，同时保持当前 UI 和已收束的 8-bit/10-bit 架构边界不变。

**Architecture:** 采用单路径迁移：共享采样模型切到 V2 的 75MHz ADC core 设计，10-bit 采样路径迁移 V2 的 PLL2 与 TIM 相位，波形发生器作为 `User/dso/dso_wavegen*` 独立底层模块接入 `main.c`。不引入旧路径兼容层，不恢复 packed dual-interleaved fallback，不改 UI 布局。

**Tech Stack:** STM32H743 HAL、FreeRTOS、Keil MDK uVision 工程、host C tests with `/mnt/e/mingw64/bin/gcc.exe`。

---

## Scope And Guardrails

本计划只迁移底层与最小启动集成：

- `User/dso/dso_config.h`：ADC 75MHz 宏、wavegen 宏、`DSO_TESTGEN_ENABLE` 与 `DSO_WAVEGEN_ENABLE` 互斥保护。
- `User/dso/dso_acq_model.h`：V2 independent ADC lane timing model。
- `User/dso/dso_acq_10bit.c`：V2 PLL2 75/150MHz 输入选择与 TIM 相位点。
- `User/dso/dso_wavegen*.c/.h`：从 V2 迁移 PA4 DAC wavegen 模块。
- `User/main.c`：创建 `DSO_Wavegen_Task`。
- `Project/MDK-ARM(uV5)/project.uvprojx`：注册新 wavegen 源文件。
- `tests/`：迁入 V2 clock/wavegen 合约测试，并新增一个硬件路径文本合约测试。
- `docs/architecture/`：更新中文架构文档。

硬约束：

- 保持上电默认 `8-bit 三路交替采样`。
- 保持当前 10-bit 架构为 `双路交替 + CH2 实时`。
- 不重构 UI，不改波形显示布局。
- `DSO_TESTGEN_ENABLE` 和 `DSO_WAVEGEN_ENABLE` 不允许同时为 1，因为两者都占用 `PA4 / DAC1_CH1 / DMA1_Stream5`。
- `DSO_WAVEGEN_ENABLE` 默认按 V2 为 1，默认 profile 为 `DSO_WAVEGEN_PROFILE_SQUARE_250K`。
- `TIM5` 归 wavegen 使用；不要把 BSP PWM/TIM5 路径并行接到同一运行链路。
- `V2/` 是参考目录，不纳入主仓库版本控制。

## File Map

- Modify: `User/dso/dso_config.h`
- Modify: `User/dso/dso_acq_model.h`
- Modify: `User/dso/dso_acq_10bit.c`
- Modify: `User/main.c`
- Modify: `Project/MDK-ARM(uV5)/project.uvprojx`
- Modify: `docs/architecture/03-acquisition-and-sample-modes.md`
- Modify: `docs/architecture/06-memory-bsp-build-test.md`
- Modify: `docs/architecture/08-v2-bottom-layer-port-design.md`
- Create: `docs/architecture/10-adc-clock-phase-wavegen-migration.md`
- Create: `User/dso/dso_wavegen.h`
- Create: `User/dso/dso_wavegen.c`
- Create: `User/dso/dso_wavegen_types.h`
- Create: `User/dso/dso_wavegen_lut.h`
- Create: `User/dso/dso_wavegen_lut.c`
- Create: `User/dso/dso_wavegen_timer_math.h`
- Create: `User/dso/dso_wavegen_timer_math.c`
- Create: `User/dso/dso_wavegen_dac_pa4.h`
- Create: `User/dso/dso_wavegen_dac_pa4.c`
- Create: `tests/test_dso_acq_model_75mhz.c`
- Create: `tests/test_dso_acq8_clock_contract.c`
- Create: `tests/test_dso_acq10_clock_contract.c`
- Create: `tests/test_dso_acq10_sampling_chain_contract.c`
- Create: `tests/test_dso_wavegen_config.c`
- Create: `tests/test_dso_wavegen_timer_math.c`
- Create: `tests/test_dso_wavegen_lut.c`
- Create: `tests/test_dso_wavegen_profile.c`
- Create: `tests/test_dso_wavegen_dac_pa4_contract.c`

### Task 1: Import RED Contract Tests

**Files:**
- Create: `tests/test_dso_acq_model_75mhz.c`
- Create: `tests/test_dso_acq8_clock_contract.c`
- Create: `tests/test_dso_acq10_clock_contract.c`
- Create: `tests/test_dso_acq10_sampling_chain_contract.c`
- Create: `tests/test_dso_wavegen_config.c`
- Create: `tests/test_dso_wavegen_timer_math.c`
- Create: `tests/test_dso_wavegen_lut.c`
- Create: `tests/test_dso_wavegen_profile.c`
- Create: `tests/test_dso_wavegen_dac_pa4_contract.c`

- [ ] **Step 1: Copy V2 clock and wavegen tests into current tests directory**

Run:

```bash
cp V2/tests/test_dso_acq_model_75mhz.c tests/test_dso_acq_model_75mhz.c
cp V2/tests/test_dso_acq8_clock_contract.c tests/test_dso_acq8_clock_contract.c
cp V2/tests/test_dso_acq10_clock_contract.c tests/test_dso_acq10_clock_contract.c
cp V2/tests/test_dso_acq10_sampling_chain_contract.c tests/test_dso_acq10_sampling_chain_contract.c
cp V2/tests/test_dso_wavegen_config.c tests/test_dso_wavegen_config.c
cp V2/tests/test_dso_wavegen_timer_math.c tests/test_dso_wavegen_timer_math.c
cp V2/tests/test_dso_wavegen_lut.c tests/test_dso_wavegen_lut.c
cp V2/tests/test_dso_wavegen_profile.c tests/test_dso_wavegen_profile.c
```

- [ ] **Step 2: Add PA4 DAC hardware path contract test**

Create `tests/test_dso_wavegen_dac_pa4_contract.c` with:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_text_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    long length;
    char *text;

    if (fp == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        exit(1);
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(fp);
        exit(1);
    }
    length = ftell(fp);
    if (length < 0) {
        fprintf(stderr, "failed to tell %s\n", path);
        fclose(fp);
        exit(1);
    }
    rewind(fp);

    text = (char *)malloc((size_t)length + 1u);
    if (text == NULL) {
        fprintf(stderr, "failed to allocate %ld bytes for %s\n", length + 1, path);
        fclose(fp);
        exit(1);
    }
    if (fread(text, 1u, (size_t)length, fp) != (size_t)length) {
        fprintf(stderr, "failed to read %s\n", path);
        free(text);
        fclose(fp);
        exit(1);
    }
    text[length] = '\0';
    fclose(fp);
    return text;
}

static int require_contains(const char *path, const char *text, const char *needle)
{
    if (strstr(text, needle) == NULL) {
        fprintf(stderr, "%s: missing \"%s\"\n", path, needle);
        return 0;
    }
    return 1;
}

static int require_not_contains(const char *path, const char *text, const char *needle)
{
    if (strstr(text, needle) != NULL) {
        fprintf(stderr, "%s: forbidden \"%s\"\n", path, needle);
        return 0;
    }
    return 1;
}

int main(void)
{
    const char *path = "User/dso/dso_wavegen_dac_pa4.c";
    char *text = read_text_file(path);
    int ok = 1;

    ok &= require_contains(path, text, "DMA1_Stream5");
    ok &= require_contains(path, text, "DAC_TRIGGER_T5_TRGO");
    ok &= require_contains(path, text, "__HAL_RCC_TIM5_CLK_ENABLE();");
    ok &= require_contains(path, text, "TIM5->CR2 = TIM_CR2_MMS_1;");
    ok &= require_contains(path, text, "DMA_PRIORITY_VERY_HIGH");
    ok &= require_contains(path, text, "SCB_CleanDCache_by_Addr");
    ok &= require_contains(path, text, "TIM5->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;");
    ok &= require_not_contains(path, text, "DAC_TRIGGER_T6_TRGO");
    ok &= require_not_contains(path, text, "DAC_TRIGGER_T8_TRGO");

    free(text);
    return ok ? 0 : 1;
}
```

- [ ] **Step 3: Run RED verification**

Run:

```bash
set -euo pipefail
CC=/mnt/e/mingw64/bin/gcc.exe

$CC -I. -IUser tests/test_dso_acq_model_75mhz.c -o tests/test_dso_acq_model_75mhz.exe || true
./tests/test_dso_acq_model_75mhz.exe || true

$CC -I. -IUser tests/test_dso_acq8_clock_contract.c -o tests/test_dso_acq8_clock_contract.exe
./tests/test_dso_acq8_clock_contract.exe

$CC -I. -IUser tests/test_dso_acq10_clock_contract.c -o tests/test_dso_acq10_clock_contract.exe
./tests/test_dso_acq10_clock_contract.exe || true

$CC -I. -IUser tests/test_dso_acq10_sampling_chain_contract.c -o tests/test_dso_acq10_sampling_chain_contract.exe
./tests/test_dso_acq10_sampling_chain_contract.exe || true

$CC -I. -IUser tests/test_dso_wavegen_config.c -o tests/test_dso_wavegen_config.exe || true
$CC -I. -IUser tests/test_dso_wavegen_dac_pa4_contract.c -o tests/test_dso_wavegen_dac_pa4_contract.exe || true
```

Expected:

- `test_dso_acq_model_75mhz` fails because current shared ADC macros are still 50/100MHz.
- `test_dso_acq8_clock_contract` may already pass because current 8-bit file already matches V2 clock setup.
- `test_dso_acq10_clock_contract` fails because current 10-bit file still uses 100/120MHz-style PLL2 branch.
- `test_dso_acq10_sampling_chain_contract` fails because current 10-bit phase points still include `period_clocks / 6u`.
- `test_dso_wavegen_config` fails or does not compile because wavegen macros are not in current `dso_config.h`.
- `test_dso_wavegen_dac_pa4_contract` fails or does not compile because `User/dso/dso_wavegen_dac_pa4.c` is not present yet.

- [ ] **Step 4: Commit imported failing contracts**

Run:

```bash
git add tests/test_dso_acq_model_75mhz.c \
        tests/test_dso_acq8_clock_contract.c \
        tests/test_dso_acq10_clock_contract.c \
        tests/test_dso_acq10_sampling_chain_contract.c \
        tests/test_dso_wavegen_config.c \
        tests/test_dso_wavegen_timer_math.c \
        tests/test_dso_wavegen_lut.c \
        tests/test_dso_wavegen_profile.c \
        tests/test_dso_wavegen_dac_pa4_contract.c
git commit -m "test: add adc clock and wavegen contracts"
```

### Task 2: Migrate Shared ADC Clock Model

**Files:**
- Modify: `User/dso/dso_config.h`
- Modify: `User/dso/dso_acq_model.h`
- Test: `tests/test_dso_acq_model_75mhz.c`

- [ ] **Step 1: Change shared ADC clock targets in `User/dso/dso_config.h`**

Replace:

```c
/* Dual interleaved timing target: 50 MHz effective ADC core clock, DELAY=1. */
#define DSO_ADC_CORE_TARGET_HZ      50000000u
#define DSO_ADC_INPUT_CLK_DEFAULT_HZ 50000000u
#define DSO_ADC_INPUT_CLK_REVV_HZ   100000000u

/* Compatibility alias: logical "working ADC clock" target. */
#define DSO_ADC_CLK_HZ              DSO_ADC_CORE_TARGET_HZ
```

With:

```c
/* Independent-lane acquisition target: 75 MHz effective ADC core clock, DELAY=1. */
#define DSO_ADC_CORE_TARGET_HZ       75000000u
#define DSO_ADC_INPUT_CLK_DEFAULT_HZ 75000000u
#define DSO_ADC_INPUT_CLK_REVV_HZ   150000000u

/* Compatibility alias: logical "working ADC clock" target. */
#define DSO_ADC_CLK_HZ               DSO_ADC_CORE_TARGET_HZ
```

- [ ] **Step 2: Replace required-period math in `User/dso/dso_acq_model.h`**

Replace the full `DSO_AcqModel_RequiredPeriodClocks` body with:

```c
static inline uint32_t DSO_AcqModel_RequiredPeriodClocks(uint32_t tim8_clk_hz,
                                                         uint32_t adc_core_clk_hz,
                                                         uint32_t sample_cycles_x10,
                                                         uint32_t sar_cycles_x10,
                                                         uint32_t delay_cycles)
{
    uint64_t lane_cycles_x10;

    if ((tim8_clk_hz == 0u) || (adc_core_clk_hz == 0u)) {
        return 0u;
    }

    /*
     * High-speed paths use independent ADC lane timing. Each lane receives one
     * trigger per timer period, so the minimum period is one conversion plus the
     * explicit trigger-delay margin.
     */
    lane_cycles_x10 = (uint64_t)sample_cycles_x10 +
                      (uint64_t)sar_cycles_x10 +
                      ((uint64_t)delay_cycles * 10u);

    return (uint32_t)((lane_cycles_x10 * (uint64_t)tim8_clk_hz +
                       ((uint64_t)adc_core_clk_hz * 10ull) - 1ull) /
                      ((uint64_t)adc_core_clk_hz * 10ull));
}
```

- [ ] **Step 3: Replace non-single sample-rate branch in `DSO_AcqModel_Build`**

Replace the `else` branch after `if (single_adc)` with:

```c
    } else {
        out.lane_rate_hz = out.trigger_rate_hz;
        out.sample_rate_hz = (out.period_clocks != 0u)
                           ? ((2u * tim8_clk_hz) / out.period_clocks)
                           : 0u;
    }
    out.profile_valid = (required_period_clocks != 0u) &&
                        (out.period_clocks >= required_period_clocks);
```

Make sure the old comments mentioning `packed DMA word` and `fallback-invalid` are removed from this function.

- [ ] **Step 4: Run ADC model tests**

Run:

```bash
set -euo pipefail
CC=/mnt/e/mingw64/bin/gcc.exe

$CC -I. -IUser tests/test_dso_acq_model_75mhz.c -o tests/test_dso_acq_model_75mhz.exe
./tests/test_dso_acq_model_75mhz.exe

$CC -I. -IUser tests/test_dso_acquire_control.c User/dso/dso_acquire_control.c User/dso/dso_sample_spec.c -o tests/test_dso_acquire_control_new.exe
./tests/test_dso_acquire_control_new.exe
```

Expected:

```text
all acq model 75mhz tests passed
all acquire control tests passed
```

- [ ] **Step 5: Commit shared model migration**

Run:

```bash
git add User/dso/dso_config.h User/dso/dso_acq_model.h
git commit -m "dso: switch adc model to 75mhz lanes"
```

### Task 3: Migrate 10-bit PLL2 And Phase Points

**Files:**
- Modify: `User/dso/dso_acq_10bit.c`
- Test: `tests/test_dso_acq10_clock_contract.c`
- Test: `tests/test_dso_acq10_sampling_chain_contract.c`

- [ ] **Step 1: Replace 10-bit PLL2 selection**

In `config_pll2(uint32_t adc_input_clk_hz)`, replace the old `if (adc_input_clk_hz >= 100000000u)` branch with:

```c
    switch (adc_input_clk_hz) {
    case 75000000u:
        periph_clk.PLL2.PLL2N = 150;
        periph_clk.PLL2.PLL2P = 10;
        break;
    case 150000000u:
        periph_clk.PLL2.PLL2N = 150;
        periph_clk.PLL2.PLL2P = 5;
        break;
    default:
        while (1) {}
    }
```

- [ ] **Step 2: Replace 10-bit TIM phase delay mapping**

In the 10-bit phase helper, replace the lane mapping with:

```c
    switch (which) {
    case 0u:
        return period_clocks / 4u;
    case 1u:
        return (period_clocks * 3u) / 4u;
    default:
        return period_clocks / 2u;
    }
```

- [ ] **Step 3: Run 10-bit clock and sampling-chain contracts**

Run:

```bash
set -euo pipefail
CC=/mnt/e/mingw64/bin/gcc.exe

$CC -I. -IUser tests/test_dso_acq10_clock_contract.c -o tests/test_dso_acq10_clock_contract.exe
./tests/test_dso_acq10_clock_contract.exe

$CC -I. -IUser tests/test_dso_acq10_sampling_chain_contract.c -o tests/test_dso_acq10_sampling_chain_contract.exe
./tests/test_dso_acq10_sampling_chain_contract.exe

$CC -I. -IUser tests/test_dso_10bit_display_sync.c User/dso/dso_10bit_display_sync.c -lm -o tests/test_dso_10bit_display_sync_new.exe
./tests/test_dso_10bit_display_sync_new.exe
```

Expected:

```text
all acq10 clock contract tests passed
all 10bit display sync tests passed
```

The sampling-chain contract exits with code 0 and prints no success line.

- [ ] **Step 4: Commit 10-bit clock and phase migration**

Run:

```bash
git add User/dso/dso_acq_10bit.c
git commit -m "dso: migrate 10bit adc clock phase"
```

### Task 4: Add Wavegen Configuration And Mutual Exclusion

**Files:**
- Modify: `User/dso/dso_config.h`
- Test: `tests/test_dso_wavegen_config.c`

- [ ] **Step 1: Add wavegen macros after `DSO_TESTGEN_ENABLE` in `User/dso/dso_config.h`**

Insert:

```c
/* DAC waveform generator: PA4 / DAC1_CH1 / TIM5_TRGO / DMA1_Stream5. */
#ifndef DSO_WAVEGEN_ENABLE
#define DSO_WAVEGEN_ENABLE                 1u
#endif

#define DSO_WAVEGEN_PATH_DAC_PA4           0u
#define DSO_WAVEGEN_PATH_PWM_TIM5          1u

#define DSO_WAVEGEN_WAVE_SINE              0u
#define DSO_WAVEGEN_WAVE_TRIANGLE          1u
#define DSO_WAVEGEN_WAVE_SQUARE            2u

#define DSO_WAVEGEN_PROFILE_SINE_32_468K       0u
#define DSO_WAVEGEN_PROFILE_SINE_16_937K       1u
#define DSO_WAVEGEN_PROFILE_SINE_10_1500K      2u
#define DSO_WAVEGEN_PROFILE_TRIANGLE_20_750K   3u
#define DSO_WAVEGEN_PROFILE_TRIANGLE_32_468K   4u
#define DSO_WAVEGEN_PROFILE_SQUARE_2_7500K     5u
#define DSO_WAVEGEN_PROFILE_BRINGUP_SQUARE_1K  6u
#define DSO_WAVEGEN_PROFILE_SQUARE_100K        7u
#define DSO_WAVEGEN_PROFILE_SQUARE_500K        8u
#define DSO_WAVEGEN_PROFILE_SQUARE_1M          9u
#define DSO_WAVEGEN_PROFILE_SQUARE_2M          10u
#define DSO_WAVEGEN_PROFILE_SQUARE_3M          11u
#define DSO_WAVEGEN_PROFILE_SQUARE_3500K       12u
#define DSO_WAVEGEN_PROFILE_SQUARE_250K        13u

#ifndef DSO_WAVEGEN_PATH
#define DSO_WAVEGEN_PATH                   DSO_WAVEGEN_PATH_DAC_PA4
#endif

#ifndef DSO_WAVEGEN_PROFILE
#define DSO_WAVEGEN_PROFILE                DSO_WAVEGEN_PROFILE_SQUARE_250K
#endif

#ifndef DSO_WAVEGEN_DAC_UPDATE_BUDGET_HZ
#define DSO_WAVEGEN_DAC_UPDATE_BUDGET_HZ   15000000u
#endif

#ifndef DSO_WAVEGEN_SQUARE_DUTY_PERMILLE
#define DSO_WAVEGEN_SQUARE_DUTY_PERMILLE   500u
#endif

#ifndef DSO_WAVEGEN_VREF_MV
#define DSO_WAVEGEN_VREF_MV                3287u
#endif

#ifndef DSO_WAVEGEN_POLL_MS
#define DSO_WAVEGEN_POLL_MS                500u
#endif

#ifndef DSO_WAVEGEN_HOST_SYNTAX
#define DSO_WAVEGEN_HOST_SYNTAX            0u
#endif

#if (DSO_WAVEGEN_ENABLE != 0u) && (DSO_TESTGEN_ENABLE != 0u)
#error "DSO_WAVEGEN_ENABLE and DSO_TESTGEN_ENABLE both use PA4/DAC1/DMA1_Stream5"
#endif

#if (DSO_WAVEGEN_PATH != DSO_WAVEGEN_PATH_DAC_PA4) && \
    (DSO_WAVEGEN_PATH != DSO_WAVEGEN_PATH_PWM_TIM5)
#error "DSO_WAVEGEN_PATH must be DAC_PA4 or PWM_TIM5"
#endif

#if DSO_WAVEGEN_PATH == DSO_WAVEGEN_PATH_PWM_TIM5
#error "DSO_WAVEGEN_PATH_PWM_TIM5 is reserved; current migration supports DAC_PA4 only"
#endif

#if (DSO_WAVEGEN_PROFILE > DSO_WAVEGEN_PROFILE_SQUARE_250K)
#error "DSO_WAVEGEN_PROFILE is out of range"
#endif

#if (DSO_WAVEGEN_SQUARE_DUTY_PERMILLE == 0u) || \
    (DSO_WAVEGEN_SQUARE_DUTY_PERMILLE >= 1000u)
#error "DSO_WAVEGEN_SQUARE_DUTY_PERMILLE must be in 1..999"
#endif
```

- [ ] **Step 2: Run wavegen config contract**

Run:

```bash
set -euo pipefail
CC=/mnt/e/mingw64/bin/gcc.exe

$CC -I. -IUser tests/test_dso_wavegen_config.c -o tests/test_dso_wavegen_config.exe
./tests/test_dso_wavegen_config.exe
```

Expected:

```text
all wavegen config tests passed
```

- [ ] **Step 3: Verify mutual exclusion fails when both generators are enabled**

Run:

```bash
/mnt/e/mingw64/bin/gcc.exe -I. -IUser -DDSO_TESTGEN_ENABLE=1 tests/test_dso_wavegen_config.c -o tests/test_dso_wavegen_config_conflict.exe
```

Expected: compiler exits nonzero and prints:

```text
DSO_WAVEGEN_ENABLE and DSO_TESTGEN_ENABLE both use PA4/DAC1/DMA1_Stream5
```

- [ ] **Step 4: Commit wavegen config**

Run:

```bash
git add User/dso/dso_config.h
git commit -m "dso: add wavegen configuration"
```

### Task 5: Migrate Wavegen Modules From V2

**Files:**
- Create: `User/dso/dso_wavegen.h`
- Create: `User/dso/dso_wavegen.c`
- Create: `User/dso/dso_wavegen_types.h`
- Create: `User/dso/dso_wavegen_lut.h`
- Create: `User/dso/dso_wavegen_lut.c`
- Create: `User/dso/dso_wavegen_timer_math.h`
- Create: `User/dso/dso_wavegen_timer_math.c`
- Create: `User/dso/dso_wavegen_dac_pa4.h`
- Create: `User/dso/dso_wavegen_dac_pa4.c`
- Test: `tests/test_dso_wavegen_timer_math.c`
- Test: `tests/test_dso_wavegen_lut.c`
- Test: `tests/test_dso_wavegen_profile.c`
- Test: `tests/test_dso_wavegen_dac_pa4_contract.c`

- [ ] **Step 1: Copy wavegen module files from V2**

Run:

```bash
cp V2/User/dso/dso_wavegen.h User/dso/dso_wavegen.h
cp V2/User/dso/dso_wavegen.c User/dso/dso_wavegen.c
cp V2/User/dso/dso_wavegen_types.h User/dso/dso_wavegen_types.h
cp V2/User/dso/dso_wavegen_lut.h User/dso/dso_wavegen_lut.h
cp V2/User/dso/dso_wavegen_lut.c User/dso/dso_wavegen_lut.c
cp V2/User/dso/dso_wavegen_timer_math.h User/dso/dso_wavegen_timer_math.h
cp V2/User/dso/dso_wavegen_timer_math.c User/dso/dso_wavegen_timer_math.c
cp V2/User/dso/dso_wavegen_dac_pa4.h User/dso/dso_wavegen_dac_pa4.h
cp V2/User/dso/dso_wavegen_dac_pa4.c User/dso/dso_wavegen_dac_pa4.c
```

- [ ] **Step 2: Host-verify timer math and LUT**

Run:

```bash
set -euo pipefail
CC=/mnt/e/mingw64/bin/gcc.exe

$CC -I. -IUser tests/test_dso_wavegen_timer_math.c User/dso/dso_wavegen_timer_math.c -o tests/test_dso_wavegen_timer_math.exe
./tests/test_dso_wavegen_timer_math.exe

$CC -I. -IUser tests/test_dso_wavegen_lut.c User/dso/dso_wavegen_lut.c -lm -o tests/test_dso_wavegen_lut.exe
./tests/test_dso_wavegen_lut.exe
```

Expected:

```text
all wavegen timer math tests passed
all wavegen lut tests passed
```

- [ ] **Step 3: Host-verify profile table without hardware link**

Run:

```bash
set -euo pipefail
CC=/mnt/e/mingw64/bin/gcc.exe

$CC -I. -IUser -DDSO_WAVEGEN_ENABLE=0 \
    tests/test_dso_wavegen_profile.c \
    User/dso/dso_wavegen.c \
    -o tests/test_dso_wavegen_profile.exe
./tests/test_dso_wavegen_profile.exe
```

Expected:

```text
all wavegen profile tests passed
```

- [ ] **Step 4: Host syntax-compile wavegen controller with hardware calls stubbed out**

Run:

```bash
/mnt/e/mingw64/bin/gcc.exe -I. -IUser -DDSO_WAVEGEN_HOST_SYNTAX=1 \
    -c User/dso/dso_wavegen.c \
    -o tests/test_dso_wavegen_controller_syntax.o
```

Expected: compiler exits 0.

- [ ] **Step 5: Verify PA4 DAC hardware path contract**

Run:

```bash
set -euo pipefail
CC=/mnt/e/mingw64/bin/gcc.exe

$CC -I. -IUser tests/test_dso_wavegen_dac_pa4_contract.c -o tests/test_dso_wavegen_dac_pa4_contract.exe
./tests/test_dso_wavegen_dac_pa4_contract.exe
```

Expected: exits 0 and prints no failure.

- [ ] **Step 6: Commit wavegen module migration**

Run:

```bash
git add User/dso/dso_wavegen.h \
        User/dso/dso_wavegen.c \
        User/dso/dso_wavegen_types.h \
        User/dso/dso_wavegen_lut.h \
        User/dso/dso_wavegen_lut.c \
        User/dso/dso_wavegen_timer_math.h \
        User/dso/dso_wavegen_timer_math.c \
        User/dso/dso_wavegen_dac_pa4.h \
        User/dso/dso_wavegen_dac_pa4.c
git commit -m "dso: add pa4 dac wavegen"
```

### Task 6: Integrate Wavegen Startup And MDK Project

**Files:**
- Modify: `User/main.c`
- Modify: `Project/MDK-ARM(uV5)/project.uvprojx`

- [ ] **Step 1: Include wavegen header in `User/main.c`**

Add near other DSO includes:

```c
#include "dso/dso_wavegen.h"
```

- [ ] **Step 2: Create wavegen task after DSO engine or fake ADC task creation**

Add this block before the `RTOS_CPU_STATS_ENABLE` block:

```c
#if DSO_WAVEGEN_ENABLE
    if (xTaskCreate(DSO_Wavegen_Task, "Wavegen", 2048, NULL, 2, NULL) != pdPASS) {
        printf("[BOOT] FAIL Wavegen\r\n");
        Error_Handler(__FILE__, __LINE__);
    }
#endif
```

- [ ] **Step 3: Register wavegen sources in MDK DSO group**

In `Project/MDK-ARM(uV5)/project.uvprojx`, add these `<File>` entries near `dso_testgen.c`:

```xml
            <File>
              <FileName>dso_wavegen.c</FileName>
              <FileType>1</FileType>
              <FilePath>..\..\User\dso\dso_wavegen.c</FilePath>
            </File>
            <File>
              <FileName>dso_wavegen_lut.c</FileName>
              <FileType>1</FileType>
              <FilePath>..\..\User\dso\dso_wavegen_lut.c</FilePath>
            </File>
            <File>
              <FileName>dso_wavegen_timer_math.c</FileName>
              <FileType>1</FileType>
              <FilePath>..\..\User\dso\dso_wavegen_timer_math.c</FilePath>
            </File>
            <File>
              <FileName>dso_wavegen_dac_pa4.c</FileName>
              <FileType>1</FileType>
              <FilePath>..\..\User\dso\dso_wavegen_dac_pa4.c</FilePath>
            </File>
```

- [ ] **Step 4: Verify project file registration**

Run:

```bash
rg -n "dso_wavegen\\.c|dso_wavegen_lut\\.c|dso_wavegen_timer_math\\.c|dso_wavegen_dac_pa4\\.c" Project/MDK-ARM\\(uV5\\)/project.uvprojx
```

Expected: all four source files appear once.

- [ ] **Step 5: Commit startup and project integration**

Run:

```bash
git add User/main.c Project/MDK-ARM\(uV5\)/project.uvprojx
git commit -m "dso: start wavegen task"
```

### Task 7: Update Chinese Architecture Documentation

**Files:**
- Modify: `docs/architecture/03-acquisition-and-sample-modes.md`
- Modify: `docs/architecture/06-memory-bsp-build-test.md`
- Modify: `docs/architecture/08-v2-bottom-layer-port-design.md`
- Create: `docs/architecture/10-adc-clock-phase-wavegen-migration.md`
- Modify: `docs/architecture/README.md`

- [ ] **Step 1: Update acquisition document**

In `docs/architecture/03-acquisition-and-sample-modes.md`, add a section with these exact facts:

```markdown
## ADC 时钟与相位基线

当前底层采用 V2 的 75MHz ADC core 基线：

- `DSO_ADC_CORE_TARGET_HZ = 75000000`
- Rev.Y / unknown 输入 ADC kernel clock 为 75MHz，core divider 为 1。
- Rev.V 输入 ADC kernel clock 为 150MHz，core divider 为 2，等效 ADC core 仍为 75MHz。
- 8-bit 路径保持三路交替采样，三路相位为 `1/6、1/2、5/6`。
- 10-bit 路径保持双路交替 + CH2 实时，TIM 相位为 `1/4、3/4、1/2`。

该模型按 independent ADC lane 建模，不再使用 packed dual-interleaved fallback。
```

- [ ] **Step 2: Update BSP/build document**

In `docs/architecture/06-memory-bsp-build-test.md`, add:

```markdown
## PA4 波形发生器资源归属

V2 波形发生器已迁移为 `User/dso/dso_wavegen*` 模块：

- 输出脚：PA4 / DAC1_CH1
- 触发源：TIM5_TRGO
- DMA：DMA1_Stream5
- 默认 profile：250kHz 方波，50% duty
- 默认更新预算：15MSPS

`DSO_TESTGEN_ENABLE` 与 `DSO_WAVEGEN_ENABLE` 互斥。两者都使用 PA4/DAC1/DMA1_Stream5，不能同时开启。
```

- [ ] **Step 3: Create stage document**

Create `docs/architecture/10-adc-clock-phase-wavegen-migration.md`:

```markdown
# V2 ADC 时钟/相位与波形发生器迁移记录

## 迁移目标

本阶段迁移 V2 的 ADC 75MHz 时钟/相位模型和 PA4 DAC 波形发生器，保持 UI 不重构。

## ADC 时钟

- 共享 ADC core 目标为 75MHz。
- Rev.Y / unknown 使用 75MHz ADC input。
- Rev.V 使用 150MHz ADC input，内部 core divider 为 2。
- 8-bit 三路交替路径沿用当前 V2 形态。
- 10-bit 双路交替路径迁移 V2 的 PLL2 与相位配置。

## TIM 相位

- 8-bit：`1/6、1/2、5/6`。
- 10-bit：`1/4、3/4、1/2`。

## 波形发生器

- 模块：`User/dso/dso_wavegen*`。
- 路径：PA4 / DAC1_CH1 / TIM5_TRGO / DMA1_Stream5。
- 默认开启：`DSO_WAVEGEN_ENABLE = 1`。
- 默认 profile：`DSO_WAVEGEN_PROFILE_SQUARE_250K`。
- `DSO_TESTGEN_ENABLE` 与 `DSO_WAVEGEN_ENABLE` 编译期互斥。

## 验证

host 合约覆盖 ADC 75MHz 模型、8-bit clock、10-bit clock、10-bit sampling chain、wavegen config、wavegen timer math、wavegen LUT、wavegen profile 和 PA4 DAC 硬件路径文本合约。Keil 固件构建需要在本机 IDE 中执行。
```

- [ ] **Step 4: Update README table of contents**

Add `10-adc-clock-phase-wavegen-migration.md` to `docs/architecture/README.md`.

- [ ] **Step 5: Commit docs**

Run:

```bash
git add docs/architecture/03-acquisition-and-sample-modes.md \
        docs/architecture/06-memory-bsp-build-test.md \
        docs/architecture/08-v2-bottom-layer-port-design.md \
        docs/architecture/10-adc-clock-phase-wavegen-migration.md \
        docs/architecture/README.md
git commit -m "docs: document adc clock wavegen migration"
```

### Task 8: Final Host Verification

**Files:**
- Verify all modified and created files from Tasks 1-7.

- [ ] **Step 1: Run full migration verification**

Run:

```bash
set -euo pipefail
CC=/mnt/e/mingw64/bin/gcc.exe

$CC -I. -IUser tests/test_dso_acq_model_75mhz.c -o tests/test_dso_acq_model_75mhz.exe
./tests/test_dso_acq_model_75mhz.exe

$CC -I. -IUser tests/test_dso_acq8_clock_contract.c -o tests/test_dso_acq8_clock_contract.exe
./tests/test_dso_acq8_clock_contract.exe

$CC -I. -IUser tests/test_dso_acq10_clock_contract.c -o tests/test_dso_acq10_clock_contract.exe
./tests/test_dso_acq10_clock_contract.exe

$CC -I. -IUser tests/test_dso_acq10_sampling_chain_contract.c -o tests/test_dso_acq10_sampling_chain_contract.exe
./tests/test_dso_acq10_sampling_chain_contract.exe

$CC -I. -IUser tests/test_dso_wavegen_config.c -o tests/test_dso_wavegen_config.exe
./tests/test_dso_wavegen_config.exe

$CC -I. -IUser tests/test_dso_wavegen_timer_math.c User/dso/dso_wavegen_timer_math.c -o tests/test_dso_wavegen_timer_math.exe
./tests/test_dso_wavegen_timer_math.exe

$CC -I. -IUser tests/test_dso_wavegen_lut.c User/dso/dso_wavegen_lut.c -lm -o tests/test_dso_wavegen_lut.exe
./tests/test_dso_wavegen_lut.exe

$CC -I. -IUser -DDSO_WAVEGEN_ENABLE=0 \
    tests/test_dso_wavegen_profile.c \
    User/dso/dso_wavegen.c \
    -o tests/test_dso_wavegen_profile.exe
./tests/test_dso_wavegen_profile.exe

$CC -I. -IUser -DDSO_WAVEGEN_HOST_SYNTAX=1 \
    -c User/dso/dso_wavegen.c \
    -o tests/test_dso_wavegen_controller_syntax.o

$CC -I. -IUser tests/test_dso_wavegen_dac_pa4_contract.c -o tests/test_dso_wavegen_dac_pa4_contract.exe
./tests/test_dso_wavegen_dac_pa4_contract.exe

$CC -I. -IUser tests/test_dso_sample_spec.c User/dso/dso_sample_spec.c -o tests/test_dso_sample_spec_new.exe
./tests/test_dso_sample_spec_new.exe

$CC -I. -IUser tests/test_dso_10bit_display_sync.c User/dso/dso_10bit_display_sync.c -lm -o tests/test_dso_10bit_display_sync_new.exe
./tests/test_dso_10bit_display_sync_new.exe

./tests/test_dso_runstop_control_contract.exe
./tests/test_dso_serial_debug.exe
./tests/test_dso_hf_pipeline.exe
./tests/test_dso_timebase_8bit.exe
./tests/test_dso_timebase_8bit_tri_only_contract.exe
./tests/test_dso_lane_merge.exe
./tests/test_dso_acq8_topology_contract.exe
```

Expected visible success lines include:

```text
all acq model 75mhz tests passed
all acq8 clock contract tests passed
all acq10 clock contract tests passed
all wavegen config tests passed
all wavegen timer math tests passed
all wavegen lut tests passed
all wavegen profile tests passed
all sample spec tests passed
all 10bit display sync tests passed
all serial debug tests passed
all hf pipeline tests passed
all 8-bit timebase tests passed
all 8-bit tri-only timebase contract tests passed
all lane merge tests passed
all acq8 topology contract tests passed
```

- [ ] **Step 2: Verify ignored generated outputs remain ignored**

Run:

```bash
git status --short --ignored | rg "tests/.*(_new|_revert|tmp_|controller_syntax).*|tests/.*\\.o|Project/output\\(mdk\\)\\.hex|V2/" || true
```

Expected: generated `_new.exe`, syntax `.o`, `V2/`, and hex outputs appear only as ignored (`!!`) entries or do not appear.

- [ ] **Step 3: Check project has no unresolved migration wording**

Run:

```bash
rg -n "50000000u|100000000u|packed DMA word|fallback-invalid" \
    User/dso/dso_config.h \
    User/dso/dso_acq_model.h \
    User/dso/dso_acq_10bit.c \
    docs/architecture

rg -n "DAC_TRIGGER_T6_TRGO|DAC_TRIGGER_T8_TRGO" \
    User/dso/dso_wavegen.c \
    User/dso/dso_wavegen_dac_pa4.c \
    User/main.c \
    docs/architecture
```

Expected: both commands exit 1 with no matches. The old trigger names may remain in disabled legacy files such as `User/dso/dso_testgen.c` or BSP examples, but not in the new wavegen path.

- [ ] **Step 4: Commit any final fixes**

Run:

```bash
git status --short
git add User tests docs Project/MDK-ARM\(uV5\)/project.uvprojx
git commit -m "dso: complete adc clock wavegen migration"
```

If `git status --short` is clean before this step, skip the commit and record that there were no final fixes.

### Task 9: Firmware Build And Board Validation Handoff

**Files:**
- Verify in Keil: `Project/MDK-ARM(uV5)/project.uvprojx`

- [ ] **Step 1: Build in Keil**

Open `Project/MDK-ARM(uV5)/project.uvprojx`, select target `Flash`, and rebuild.

Expected:

- No missing source file errors for `dso_wavegen*.c`.
- No compile-time conflict between `DSO_WAVEGEN_ENABLE` and `DSO_TESTGEN_ENABLE`.
- `Project/output(mdk).hex` refreshes if the Keil post-build step runs.

- [ ] **Step 2: Board smoke test**

Run on STM32H743IIT6 board:

- Power-on default enters `8-bit 三路交替采样`.
- Switching to `10-bit` still gives `双路交替 + CH2 实时`.
- PB15/TIM12 external frequency meter remains active.
- RUN/STOP frozen waveform behavior remains correct.
- PA4 outputs default 250kHz square wave from wavegen.
- CH1 can acquire PA4 output through the intended jumper/input path.
- Serial log contains one `[WAVEGEN] start ... wave=square ... wave_hz=250000 ...` line when runtime logging is enabled.

- [ ] **Step 3: Commit Keil project or generated-output decisions**

If Keil modifies only ignored generated outputs, do not commit them. If Keil modifies tracked project metadata intentionally, commit:

```bash
git add Project/MDK-ARM\(uV5\)/project.uvprojx
git commit -m "build: refresh mdk project metadata"
```

If no tracked files changed after Keil build, record:

```bash
git status --short
```

Expected: no tracked changes.

## Self-Review Notes

- Spec coverage: ADC 75MHz macros, independent-lane model, 10-bit PLL/phase, PA4 wavegen, startup integration, MDK project registration, docs, host tests, and board validation are covered.
- Scope check: UI layout and rendering behavior are outside this migration and are intentionally untouched.
- Type consistency: wavegen public types are `DSO_WavegenProfile`, `DSO_WavegenSnapshot`, `DSO_WavegenStatus`, and `DSO_WavegenWaveKind`; file names match V2 exactly.
- Resource consistency: `DSO_TESTGEN_ENABLE` and `DSO_WAVEGEN_ENABLE` compile-time conflict is explicit.
