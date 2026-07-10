# BSP、内存、缓存、构建与测试

本文记录 STM32H743 BSP 初始化、内存布局、cache/DMA 约束、构建和测试入口。

## MCU 和系统基础

目标芯片：STM32H743IIT6。

系统配置入口：

- `User/bsp/bsp.c`
  - `System_Init()`
  - `SystemClock_Config()`
  - `MPU_Config()`
  - `CPU_CACHE_Enable()`
- `User/FreeRTOSConfig.h`
  - `configCPU_CLOCK_HZ = 400000000`
  - `configTICK_RATE_HZ = 1000`
  - `configTOTAL_HEAP_SIZE = 120 KB`
  - `configCHECK_FOR_STACK_OVERFLOW = 2`
  - `configGENERATE_RUN_TIME_STATS = 1`

`System_Init()` 顺序：

```text
MPU_Config
  -> CPU_CACHE_Enable
  -> HAL_Init
  -> SystemClock_Config
```

## MPU 和 Cache

当前 MPU 关键区域：

| Region | 地址 | 大小 | 属性 | 用途 |
| --- | --- | --- | --- | --- |
| 0 | `0x24000000` | 512 KB | Write-back, cacheable | AXI SRAM / `.RAM_D1` |
| 1 | `0x60000000` | 64 KB | Device / non-cacheable | FMC 扩展 IO |
| 2 | `0xC0000000` | 32 MB | Write-through, cacheable | SDRAM，含逻辑/物理 framebuffer、GUI heap、record、RollHistory |

设计含义：

- AXI SRAM 是 WB，需要 DMA 写入后 invalidate，DMA 读取前 clean。
- SDRAM 是 WT，CPU 写 framebuffer 更容易被 LTDC/DMA2D 看到，减少 dirty cache 问题。
- DMA2D/ LTDC 是总线主设备，不经过 CPU L1 Cache；因此涉及 AXI SRAM 或 cacheable buffer 时必须明确 clean/invalidate。

## SDRAM 布局

基础定义在 `User/bsp/inc/bsp_fmc_sdram.h`：

```text
EXT_SDRAM_ADDR = 0xC0000000
EXT_SDRAM_SIZE = 32 MB
SDRAM_LCD_SIZE = 2 MB
SDRAM_LCD_LAYER = 2
SDRAM_APP_BUF = 0xC0400000
```

显示 framebuffer 精确布局在 `User/bsp/inc/bsp_vram_layout.h`：

| 名称 | 地址 | 尺寸 |
| --- | --- | --- |
| `VRAM_LOGICAL_ADDR` | `0xC0000000` | 800 x 480 x 2 bytes |
| `VRAM_PHYSICAL_0_ADDR` | `0xC0200000` | 480 x 800 x 2 bytes |
| `VRAM_PHYSICAL_1_ADDR` | `0xC0300000` | 480 x 800 x 2 bytes |
| `VRAM_SENTINEL0_ADDR` | phys0 末尾 | 哨兵 |
| `VRAM_SENTINEL1_ADDR` | phys1 末尾 + 0x800 | 哨兵 |

Record buffer 布局在 `User/bsp/inc/bsp_sdram_layout.h`：

```text
0xC0000000  Display frame buffers   4 MB
0xC0400000  SDRAM_APP_BUF / GUI heap 预留最多 6 MB
0xC0A00000  SDRAM_RECORD_BUF
0xC1D00000  SDRAM_ROLL_HISTORY_BUF  末尾 3 MB
0xC2000000  SDRAM_TOTAL_END
```

RollHistory 通过 `SDRAM_ROLL_HISTORY_BUF..SDRAM_ROLL_HISTORY_END` 拥有 SDRAM 末端 3 MB block：

- CH1 ring：256K samples。
- CH2 ring：256K samples。
- CH1 pull：256K samples。
- CH2 pull：256K samples。
- CH1/CH2 scratch。
- CH1/CH2 roll line。

`SDRAM_RECORD_BUF..SDRAM_RECORD_END` is the deep-record region.
`SDRAM_ROLL_HISTORY_BUF..SDRAM_ROLL_HISTORY_END` is the RollHistory region.
`SDRAM_RECORD_END == SDRAM_ROLL_HISTORY_BUF`，因此两个区域相邻且不重叠。Host 测试 `tests/test_dso_sdram_layout.c` 验证 VRAM、GUI heap、record 和 RollHistory 的顺序与非重叠关系。

维护内存布局时要同时考虑：

- framebuffer 固定地址。
- GUI_ALLOC heap。
- record buffer。
- RollHistory 末端 block。
- DMA2D/Cache 对齐。

## DSO RAM 放置

常见放置：

- `.RAM_D1`
  - engine recent/display/mode scratch。
  - timebase state。
  - acquire control state。
  - record state。
- `.RAM_D2`
  - 10-bit ADC DMA ring：`s_dma_lane0`、`s_dma_lane1`、`s_dma_ch2`。
  - 8-bit ADC DMA ring。
- SDRAM
  - framebuffer。
  - GUI heap。
  - record deep storage。
  - RollHistory。

采集 DMA buffer 必须保持 32 字节对齐，否则 D-Cache range invalidate 可能踩到邻近数据。

## DMA 和 Cache 约束

### ADC DMA

ADC DMA 写入 D2 SRAM。half/full callback 中：

- half transfer：invalidate buffer 前半区。
- transfer complete：invalidate buffer 后半区。

10-bit 后端用 `DSO_CacheRange_Align()` 将地址和长度扩展到 cache line 边界后调用 `SCB_InvalidateDCache_by_Addr()`。

### DMA2D

DMA2D 在显示链路中承担：

- PlotBuf 背景恢复：M2M。
- PlotBuf 到物理 framebuffer：M2M。
- 物理前台到后台：M2M。
- 波形线段：R2M 填色。
- emWin Fill/Copy：LCDConf 中可走 DMA2D。

DMA2D 共享同一个外设寄存器，因此需要：

- `DMA2D_Bus_RecursiveLock()`。
- `DMA2D_Wait_TransferComplete()`。
- 在 DMA2D 读取 PlotBuf 前做必要 cache 同步。

### LTDC

LTDC 只扫描物理 framebuffer。切换地址要走：

1. 写 layer CFBAR shadow。
2. 等待 line interrupt / VFP。
3. immediate reload。
4. 更新 active physical buffer 标记。

## LCD 和显存

`User/bsp/src/bsp_lcd_rgb.c` 初始化 LTDC：

- LTDC GPIO AF14。
- PixelFormat RGB565。
- 开启 LTDC IRQ。
- 初始 framebuffer 地址随后在 `main.c` 被强制设为 `VRAM_PHYSICAL_0_ADDR`。

实际 emWin LCD 配置在：

```text
emWin/Config/LCDConf_Lin_Template.c
```

不要误以为 `emWin/DisplayDriver/LCDConf_Lin_Template.c` 是当前工程使用的配置；它更像模板/源样例。

## PB15/TIM12 测频与 LED4 冲突

V2 外部测频链路使用 PB15 作为 TIM12_CH2 输入：

- `User/dso/dso_freq_meter_hw_tim12.c` 将 PB15 配置为 `GPIO_AF2_TIM12`。
- `TIM8_BRK_TIM12_IRQHandler()` 在该文件中实现，并在处理 TIM12 后清理 TIM8 break flags。
- `User/dso/dso_engine.c` 上电默认初始化并启动频率计。

当前板级 LED 映射中 LED4 也曾使用 PB15。为了避免上电时 GPIO 输出驱动测频输入，`User/bsp/src/bsp_led.c` 默认定义：

```c
#define BSP_LED4_PB15_ENABLE 0u
```

因此 LED1-3 正常初始化，LED4/PB15 默认不再作为 LED 输出。只有在确认 PB15 不接外部测频输入时，才可以显式把 `BSP_LED4_PB15_ENABLE` 改为 1。

## 构建

### Keil

Keil 工程：

```text
Project/MDK-ARM(uV5)/project.uvprojx
```

目标：

```text
Flash
```

构建后输出：

```text
Project/output(mdk).hex
```

当前 `.uvprojx` 已注册主要 DSO/UI/BSP 文件，包括：

- `dso_acq_10bit.c`
- `dso_acq_8bit.c`
- `dso_sample_spec.c`
- `dso_engine.c`
- `dso_freq_meter.c`
- `dso_freq_meter_math.c`
- `dso_freq_meter_hw_tim12.c`
- `dso_trigger_source_bridge.c`
- `dso_process.c`
- `dso_timebase.c`
- `dso_vertical.c`
- `MainTask.c`
- `ui_freq_format.c`
- `dma2d_wave.c`
- `emWin/Config/LCDConf_Lin_Template.c`

新增源文件时要同步更新 Keil 工程。

### IAR

IAR 工程当前不作为维护目标；当前维护和验证以 Keil `Project/MDK-ARM(uV5)/project.uvprojx` 与 host tests 为准。仓库中的历史说明可能仍提到 `Project/EWARMv8/project.ewp`，但本轮优化不恢复或同步 IAR 工程。

## Host 测试

仓库说明要求至少运行：

```powershell
.\tests\test_dso_serial_debug.exe
.\tests\test_dso_hf_pipeline.exe
```

`tests/` 下还有大量面向模块的测试：

- 10-bit：
  - `test_dso_acq10_stream.c`
  - `test_dso_acq10_tim_trigger.c`
  - `test_dso_acq10_api_contract.c`
  - `test_dso_10bit_lane_cal.c`
  - `test_dso_10bit_display_sync.c`
  - `test_dso_hs10_render.c`
  - `test_dso_timebase_10bit.c`
- 8-bit：
  - `test_dso_acq8_topology_contract.c`
  - `test_dso_acq8_sample_unpack.c`
  - `test_dso_acq8_tim_trigger.c`
  - `test_dso_acq8_fault_policy.c`
  - `test_dso_hs8_lane_cal.c`
  - `test_dso_hs8_render.c`
- 共享链路：
  - `test_dso_sample_spec.c`
  - `test_dso_acquire_control.c`
  - `test_dso_acquire_control_trigger_source.c`
  - `test_dso_acquire_control_trigger_slope_both.c`
  - `test_dso_acquire_control_trigger_level_enable.c`
  - `test_dso_hf_pipeline.c`
  - `test_dso_process_trigger_both.c`
  - `test_dso_process_hs8_freq_policy.c`
  - `test_dso_serial_debug.c`
  - `test_dso_lane_merge.c`
  - `test_dso_cache_range.c`
  - `test_dso_engine_roll_freq_policy.c`
- 外部测频/触发桥：
  - `test_dso_freq_meter_math.c`
  - `test_dso_freq_ui_format.c`
  - `test_dso_freq_meter_*_contract.c`
  - `test_trigger_*_contract.c`
  - `test_trigger_source_bridge.c`
  - `test_trigger_source_rate_mapping.c`

当前文档更新没有改 C 代码，因此不要求重新构建固件；但如果后续架构文档伴随源码调整，应运行对应 host 测试，并在固件相关改动后重建 Keil 工程。

## 常用维护检查

新增 DSO 源文件时：

1. 文件放在正确目录。
2. header/source 成对。
3. 添加或更新 host test。
4. 更新 Keil `.uvprojx`。
5. 更新本文档集中对应模块说明。

显示链路改动时：

1. 同时核对 `bsp_vram_layout.h`、`LCDConf_Lin_Template.c`、`dma2d_wave.h`、`MainTask.c` 的几何尺寸。
2. 确认 `DSO_DISPLAY_WIDTH/HEIGHT` 与 PlotBuf 和 UI plot 区一致。
3. 确认 DMA2D 使用前后有等待和 cache 同步。
4. 确认 LTDC 只扫描物理 framebuffer。

采集链路改动时：

1. 确认 DMA buffer section 和 alignment。
2. 确认 half/full callback 的 invalidate 范围。
3. 确认 `DSO_Acq_GetSampleRate()` 和 `GetCh2SampleRate()` 对应真实采样率。
4. 确认 `DSO_SampleSpec` 的 adc_max/midscale 与样本实际位宽一致。
5. 不要为不确定问题添加隐藏 fallback；应定位根因并显式报告错误。

## PA4 波形发生器资源归属

V2 波形发生器已迁移为 `User/dso/dso_wavegen*` 模块：

- 输出脚：PA4 / DAC1_CH1
- 触发源：TIM5_TRGO
- DMA：DMA1_Stream5
- 默认 profile：250kHz 方波，50% duty
- 默认更新预算：15MSPS

`DSO_TESTGEN_ENABLE` 与 `DSO_WAVEGEN_ENABLE` 互斥。两者都使用 PA4/DAC1/DMA1_Stream5，不能同时开启。
