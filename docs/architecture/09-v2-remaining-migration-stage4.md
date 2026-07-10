# V2 剩余底层迁移记录

本文记录本轮在当前主工程中继续迁移 V2 底层的实际落地结果，重点覆盖用户新增约束：

- 测频模块默认开启。
- 默认上电格式从 `10-bit` 改为 `8-bit high-speed` 三路交替采样。
- 底层架构只保留 `8-bit` 三路交替采样和 `10-bit` 双路交替 + CH2 实时。
- RUN/STOP 波形冻结修复保持生效。

## 本轮迁移范围

### 默认上电格式

默认采样格式现在定义为：

```c
#define DSO_SAMPLE_FORMAT_DEFAULT DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED
```

影响链路：

- `dso_sample_spec.c` 初始化 active format。
- `dso_acq.c` 初始化 facade active/pending format。
- 上电后默认进入 8-bit 三路交替采样。
- 系统设置页仍可切换到 `10-bit`，但不会恢复 `8-bit (2L)` 或 single-safe 产品路径。

## 触发底层迁移

本轮从 V2 迁入并接通：

- `DSO_TRIGGER_SLOPE_BOTH`
- `DSO_TRIGGER_SOURCE_CH1`
- `DSO_TRIGGER_SOURCE_CH2`
- `trigger_level_enabled`
- `DSO_AcquireCtrl_SetTriggerSource()`
- `DSO_AcquireCtrl_NormalizeTriggerSource()`
- `dso_trigger_source_bridge.c`

### Engine 行为

`DSO_Engine_Task` 每轮会根据当前通道可用性计算 effective trigger source：

- CH1：active analysis 为 CH1。
- CH2：仅在 `DSO_Acq_IsCh2Available()` 为 true 时使用 CH2。
- 8-bit：CH2 不可用，engine 会把触发源规整回 CH1。

Normal/Fast 路径中，如果 CH2 是触发源：

- CH2 recent/view analysis 决定 trigger、Normal/Single publish gating 和 single capture result。
- CH2 触发总索引按采样率映射到 CH1 record 时间轴，再调用 `DSO_Record_SetTrigger()`。
- 渲染前用 `DSO_TriggerDisplaySync_MapByRate()` 将 CH2 trigger position 映射到 CH1 render analysis，保证显示窗口与真实触发源对齐。

### UI 行为

当前 UI 布局不随 V2 覆盖，只做必要桥接：

- 触发源只支持 CH1/CH2。
- BOTH 边沿写入 `DSO_TRIGGER_SLOPE_BOTH`。
- 触发电平档位为 `+1.25V`、`0.00V`、`OFF`。
- `OFF` 不改变保存的电压值，只关闭 `trigger_level_enabled`。
- 每帧 `UI_Trigger_SyncFrameState(&s_frame)` 同步 engine 规整后的 source 和 level 状态。

## PB15/TIM12 外部测频

迁入文件：

- `User/dso/dso_freq_meter.c`
- `User/dso/dso_freq_meter.h`
- `User/dso/dso_freq_meter_math.c`
- `User/dso/dso_freq_meter_math.h`
- `User/dso/dso_freq_meter_hw_tim12.c`
- `User/dso/dso_freq_meter_hw_tim12.h`
- `User/ui/ui_freq_format.c`
- `User/ui/ui_freq_format.h`

Engine 默认启动：

```c
DSO_FreqMeter_Init();
DSO_FreqMeter_Start();
```

主循环中：

```c
DSO_FreqMeter_Service(now);
```

发布 frame 前写入：

- `frame.ext_freq_valid`
- `frame.ext_freq_state`
- `frame.ext_freq_mode`
- `frame.ext_freq_millihz`
- `frame.ext_freq_gate_us`

UI header 用 `UI_FreqFormat_ExtMilliHz()` 显示 `EXT:%s`。这一路是 PB15 外部输入，不覆盖 `frame.freq_hz` 或 `frame.ch2_freq_hz`。

## PB15 资源边界

PB15 现在默认属于 TIM12_CH2 外部测频输入。为避免 LED 初始化驱动测频输入：

- `User/bsp/src/bsp_led.c` 默认 `BSP_LED4_PB15_ENABLE = 0u`。
- LED4 的 `case 4` 和 GPIOB PB15 初始化均受该宏保护。
- LED1、LED2、LED3 行为不变。

如果后续硬件确认不使用 PB15 测频，才允许显式打开 LED4。

## 工程文件同步

Keil 工程 `Project/MDK-ARM(uV5)/project.uvprojx` 已新增：

- `dso_freq_meter.c`
- `dso_freq_meter_math.c`
- `dso_freq_meter_hw_tim12.c`
- `dso_trigger_source_bridge.c`
- `ui_freq_format.c`

当前 checkout 没有 `Project/EWARMv8/project.ewp`，因此本轮没有 IAR 工程可同步。

## 当前收口状态

阶段 4 后续迁移已经继续收口，当前状态如下：

- PA4/DAC 波形发生器已迁入，默认 `DSO_WAVEGEN_ENABLE=1`，路径为 PA4 / DAC1_CH1 / TIM5_TRGO / DMA1_Stream5，默认 profile 为 250kHz 方波。
- ADC 时钟/相位已按 V2 收口：核心目标 75MHz，Rev.V 使用 150MHz 输入并通过 divider 回到 75MHz，10-bit TIM 相位为 `1/4、3/4、1/2`。
- 串口基础波特率、`DSO_SERIAL_DEBUG_UART_BAUD` 和 `DSO_VOFA_FIREWATER_UART_BAUD` 已统一到 1152000；`DSO_SERIAL_DEBUG_ENABLE` 仍默认 0，没有默认打开大量串口诊断。
- 触发类型在 UI 侧固定为 Edge，不再保留 Pulse/Video 类型切换路径。
- 8-bit 三路交替校准 ready 状态已回填到 `frame.ch1_interleave_cal_valid`，方便 UI/诊断判断 HS8 校准是否 ready。
- 垂直默认挡位为 500mV/div，避免上电默认幅度过大。
- V2 的 UI 几何、header 高度、页面布局和显示尺寸仍不迁入。

## 验证摘要

本轮新增或重跑的 host 验证覆盖：

- 默认上电格式为 8-bit。
- trigger source、BOTH slope、level OFF。
- CH2 trigger source 在 engine 中进入 active analysis。
- trigger frame metadata。
- PB15/TIM12 频率计数学层、状态机契约、硬件契约、engine/UI 接线契约。
- 外部测频 UI 格式化。
- 8-bit tri-only timebase 与系统设置页两项切换。
- 8-bit HS8 reentry / calibration-ready metadata contract。
- 垂直默认 500mV/div 与全 8-bit ADC range 映射 contract。
- 触发类型 Edge-only UI contract。
- 既有 serial debug、hf pipeline、RUN/STOP contract。

Keil 固件构建需要在本地 Keil 环境中继续执行；当前命令行环境未提供 Keil/IAR 编译器。
