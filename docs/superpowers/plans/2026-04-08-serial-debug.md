# Serial Debug Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add structured UART diagnostics for acquisition, processing, and UI output so waveform correctness can be analyzed from captured serial logs.

**Architecture:** Keep formatting and anomaly decisions in a pure helper module, then integrate a runtime collector into `DSO_Engine_Task`. Emit periodic summaries and anomaly-triggered snapshots using `COM1`, while suppressing VOFA traffic during serial-debug sessions.

**Tech Stack:** C, STM32 HAL/FreeRTOS project code, UART FIFO driver, host-side C tests, Keil MDK build.

---

### Task 1: Add host-testable formatting and anomaly helpers

**Files:**
- Create: `D:\DSO_V4\User\dso\dso_serial_debug_format.h`
- Create: `D:\DSO_V4\User\dso\dso_serial_debug_format.c`
- Create: `D:\DSO_V4\tests\test_dso_serial_debug.c`

- [ ] **Step 1: Write the failing test**
- [ ] **Step 2: Run the test to verify formatting/anomaly helpers fail before implementation**
- [ ] **Step 3: Implement the minimal formatter helpers**
- [ ] **Step 4: Run the host test to verify it passes**

### Task 2: Add runtime serial debug module

**Files:**
- Create: `D:\DSO_V4\User\dso\dso_serial_debug.h`
- Create: `D:\DSO_V4\User\dso\dso_serial_debug.c`
- Modify: `D:\DSO_V4\User\dso\dso_config.h`
- Modify: `D:\DSO_V4\User\dso\dso_engine.c`

- [ ] **Step 1: Add compile-time config for interval, snapshots, CH2 logging, and VOFA coexistence**
- [ ] **Step 2: Implement runtime stats collection and UART line emission**
- [ ] **Step 3: Wire summary/snapshot emission into `DSO_Engine_Task`**
- [ ] **Step 4: Keep the disabled path cheap and side-effect free**

### Task 3: Register new files and verify builds

**Files:**
- Modify: `D:\DSO_V4\Project\MDK-ARM(uV5)\project.uvprojx`
- Modify: `D:\DSO_V4\Project\MDK-ARM(uV5)\project.uvoptx`

- [ ] **Step 1: Add new DSO source files to the Keil project**
- [ ] **Step 2: Run host-side test executable build and verify it passes**
- [ ] **Step 3: Run Keil build and verify the embedded project still compiles cleanly**
