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
