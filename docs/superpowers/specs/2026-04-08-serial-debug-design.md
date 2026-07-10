# Serial Debug Design

**Goal:** Add structured serial diagnostics that let us verify acquisition, processing, and UI-bound waveform data end-to-end without flooding UART1 or changing scope behavior more than necessary.

## Scope

- Add periodic summary lines for `ACQ`, `PROC`, and `UI`.
- Add anomaly-triggered snapshot dumps for small sample windows at key pipeline stages.
- Reuse `COM1` and FIFO-backed UART output.
- Avoid mixing this debug stream with VOFA frame dumps by default.

## Design

### Data flow

`DSO_Engine_Task` already owns the full waveform path, so the debug tap points stay there:

1. Raw/recent samples after acquisition copy
2. Processed analysis after trigger/frequency/render decisions
3. Final frame values and mapped UI Y coordinates before `DSO_UI_SendFrame`

### Output model

- Summary lines are emitted at a fixed interval.
- Snapshot lines are emitted only when anomalies are detected or when a force-snapshot macro is enabled.
- Output is tag-based, one record per line, easy to capture and parse later.

### Modules

- `dso_serial_debug_format.[ch]`
  - Pure formatter and anomaly decision helpers
  - Host-testable
- `dso_serial_debug.[ch]`
  - Runtime stats collection
  - UART send wrapper
  - Summary/snapshot scheduling

### Default behavior

- Serial debug enabled by compile-time switch
- VOFA frame streaming suppressed while serial debug is enabled
- CH1 summaries enabled by default
- CH2 summaries optional through a config macro

## Log Content

### `ACQ`

- frame id
- channel
- sample count
- real sample rate
- min/max/mean
- zero crossings
- clip flags
- interleave calibration state

### `PROC`

- measured frequency and validity
- trigger index and sub-sample trigger position
- samples per cycle
- jitter
- repetitive confidence
- reconstruction mode
- ETS active/building

### `UI`

- UI-bound frequency/value fields
- effective sample rate shown to UI
- timebase and vertical scale metadata
- mapped Y min/max

### Snapshot

- `RAW`: recent raw acquisition window
- `CAL`: CH1 after interleave correction
- `REN`: rendered display samples
- `UIY`: final UI Y coordinates

## Constraints

- UART bandwidth is limited; no continuous raw-stream dumping.
- Snapshot size must stay small and infrequent.
- Logging must be compile-time gated and cheap when disabled.

