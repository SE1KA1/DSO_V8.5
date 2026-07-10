# DSO Frontend Vpp Scale Design

## Purpose

Align the project-reported `CH1 Vpp` and `CH2 Vpp` values with an external Tektronix oscilloscope `Pk-Pk` measurement for the current hardware setup.

The existing measurement core is correct at the ADC-code level:

```text
vpp_code = vmax_code - vmin_code
adc_vpp = vpp_code * adc_lsb_v
```

The missing piece is a fixed front-end scale between ADC-pin voltage and input-terminal voltage. Current bench observations show the project reports roughly twice the Tektronix `Pk-Pk` value, so the first hardware calibration coefficient should be `0.50f`.

## Scope

Implement a fixed, compile-time, per-channel input voltage scale:

```text
input_voltage = adc_voltage * channel_scale
```

Initial values:

```text
CH1 scale = 0.50
CH2 scale = 0.50
```

This scope intentionally does not introduce UI editing, EEPROM persistence, probe-dependent calibration tables, or automatic calibration.

## Architecture

Add a small DSO front-end calibration module under `User/dso/`:

```text
User/dso/dso_frontend_cal.h
User/dso/dso_frontend_cal.c
```

The module owns the fixed channel scale constants and exposes helpers for converting ADC-code deltas into input-terminal volts.

The existing `DSO_UI_CodeDeltaToVolts()` remains unchanged and continues to mean raw ADC voltage conversion. UI-facing measurement helpers will use the new input-voltage helper for Vpp and Vrms display.

## Data Flow

```text
DSO_Process_MeasureSamples / MeasureDisplay
  -> frame.vpp stores ADC code delta
  -> frame.adc_lsb_v stores ADC volts/code
  -> UI_Waveform_GetMeasurements
  -> DSO_FrontendCal_CodeDeltaToInputVolts(channel, code_delta, adc_lsb_v)
  -> MainTask / measurement page display input-terminal volts
```

## Affected Measurements

Apply the fixed scale to:

- CH1 Vpp
- CH2 Vpp
- CH1 Vrms
- CH2 Vrms
- Measurement page Vpp/Vrms paths that use UI measurement helpers or `ui_measure_units.h`

Do not change:

- waveform Y mapping
- V/div grid scale
- trigger voltage logic
- raw ADC capture
- frequency
- duty cycle
- `dso_process.c` max/min measurement

## Tests

Extend host coverage so both conversion meanings are explicit:

- Raw ADC conversion remains `code_delta * adc_lsb_v`.
- Input-terminal conversion is `code_delta * adc_lsb_v * 0.50f`.
- CH1 and CH2 both use the fixed per-channel scale.
- Missing ADC metadata still converts to zero.

Run at least:

```text
./tests/test_dso_ui_amplitude_units.exe
./tests/test_dso_serial_debug.exe
./tests/test_dso_hf_pipeline.exe
```

## Future Work

If later hardware data shows different coefficients per channel, probe attenuation, V/div range, or board revision, this module can grow into a table-driven calibration layer. That should be a separate design because it needs persistence, UI, and hardware documentation.
