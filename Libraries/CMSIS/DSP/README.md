# CMSIS-DSP import

This folder vendors the Keil Pack CMSIS-DSP runtime library used by the STM32H743 MDK build.

- Headers: `Include/`
- Library: `Lib/ARM/arm_cortexM7lfsp_math.lib`
- Source: `E:\Keil5 MDKcore\ARM\PACK\ARM\CMSIS\5.7.0\CMSIS\DSP\Lib\ARM\arm_cortexM7lfsp_math.lib`
- Target: Cortex-M7, little-endian, single-precision FPU

The MDK project links this library through the linker input path. Future FFT/spectrum work should include `arm_math.h` and use the CMSIS-DSP real FFT APIs documented in the official Arm reference.
