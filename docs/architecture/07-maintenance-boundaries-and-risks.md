# 维护边界、已知风险与审计清单

本文记录当前架构中的硬边界、历史遗留点和后续改动前应检查的风险。这里的内容不是功能需求，而是维护时避免误改和架构漂移的清单。

## 硬边界

### 8-bit 架构收敛边界

V2 底层移植后，8-bit 架构目标是只保留三路交替采样。以下 8-bit 专属文件可以为“三路交替采样单一路径”做收敛式修改，但不应继续维护 single safe / dual interleaved 作为产品路径，也不应新增隐藏 fallback：

- `User/dso/dso_acq_8bit.c`
- `User/dso/dso_acq8_fault_policy.c`
- `User/dso/dso_acq8_fault_policy.h`
- `User/dso/dso_acq8_refresh_policy.c`
- `User/dso/dso_acq8_refresh_policy.h`
- `User/dso/dso_hs8_lane_cal.c`
- `User/dso/dso_hs8_lane_cal.h`
- `User/dso/dso_hs8_render.c`
- `User/dso/dso_hs8_render.h`

允许的共享接入面：

- `dso_acq.c/.h`
- `dso_sample_spec.c/.h`
- `dso_engine.c`
- `dso_timebase.c/.h`
- `dso_render_policy.c/.h`
- `dso_acquire_control.c/.h`
- `dso_process.c/.h`
- `dso_serial_debug*.c/.h`
- `ui_system_setup.c/.h`
- `UI_WaveformCtrl.c/.h`

共享接入应是最小增量，不要把 10-bit 硬件细节写进 8-bit 文件，也不要把 8-bit 校准/渲染策略拿来硬套 10-bit。

### 不新增 fallback 层

本项目当前维护原则是：

- 单一路径。
- 显式错误或诊断。
- 针对根因修复。

不建议新增：

- 隐藏 fallback。
- 并行新旧路径。
- “不确定时试另一个路径”的兼容分支。
- 只是为了掩盖不确定性的 wrapper。

已有 8-bit 历史模块中存在 `auto_fallback_to_12bit` 这类命名和策略字段，但当前默认 false，且属于旧 8-bit 范围。不要把这种模式扩散到新的 10-bit 主线。

### RUN/STOP 控制权边界

V2 已修复 RUN/STOP 波形冻结问题，迁移时必须保持单一控制权：

- UI 只表达用户意图，读写 `DSO_AcquireCtrl_IsRunRequested()` / `DSO_AcquireCtrl_SetRunRequested()`。
- Engine 消费 `run_requested` 并决定是否生成和发布新 `DSO_Frame`。
- 手动 STOP 是逻辑暂停，不直接物理停止高速 ADC/DMA。
- 手动 RUN 需要同步 ingest/roll 游标并清除旧 trigger anchor，避免恢复后仍显示旧窗口。
- SINGLE 捕获完成后的 frozen 状态可以保留物理 stop 语义，但不能与手动 STOP 混为一条状态路径。

不要让 `UI_Waveform_ToggleRunStop()`、触发页面或其他 UI 入口重新直接调用 `DSO_Acq_Start()` / `DSO_Acq_Stop()` 实现手动 RUN/STOP；否则会重新引入 V2 文档中记录的 ADC/DMA restart 不稳定和 UI/engine 状态不一致问题。

## 已知风险和历史遗留

### `DSO_ADC_MAX` 是 legacy 常量

`User/dso/dso_config.h` 中仍定义：

```c
#define DSO_ADC_MAX 4095u
```

但当前产品默认上电采样格式是 8-bit high-speed：

```text
adc_max = 255
midscale = 128
```

10-bit 是：

```text
adc_max = 1023
midscale = 512
```

核心处理和垂直映射大多已经使用 `DSO_SampleSpec_GetActive()`。UI 幅度换算使用 `DSO_Frame.adc_lsb_v`，不要用 legacy `DSO_ADC_MAX` 推断当前采样格式的满量程。

### `dso_config.h` 中仍有 12-bit 语义注释

`dso_config.h` 里有多处历史注释仍写 `12-bit interleaved timing`、`SAR_CYCLES_X10_12BIT` 等。部分宏可能只是历史命名，部分可能影响当前模型。

需要区分：

- 只是注释/命名陈旧。
- 仍被当前 10-bit 路径使用。

例如 10-bit 后端构建 runtime info 时传入的是：

```text
DSO_ADC_SAR_CYCLES_X10_10BIT
```

但全局 `DSO_ADC_CONVERSION_CYCLES_X10` 仍用 12-bit SAR 宏。若某路径仍依赖这个全局宏，需要单独确认。

### UI 系统设置注释陈旧

V2 移植目标下，`User/ui/ui_system_setup.c` 应收敛为：

```text
10-bit -> 8-bit (3L) -> 10-bit
```

如果代码中仍保留 `8-bit (2L)`、single safe、dual interleaved 或旧的 `12-bit` 说法，应在对应实现阶段同步清理，避免误导后续维护者。

当前第三阶段已经把系统设置页模式切换收敛为 `10-bit` 与 `8-bit (3L)` 两项；后续若重新加入 8-bit 其他 topology，必须重新定义产品需求，而不能通过旧 enum 或旧 label 偷偷恢复。

### Trigger source 已进入 engine，trigger type 仍未进入底层

`User/ui/ui_trigger_ui.c` 中：

- source 已通过 `dso_trigger_source_bridge.c` 转到 `DSO_AcquireCtrl_SetTriggerSource()`。
- slope/mode/level 已实际转到 `DSO_AcquireCtrl`。
- type 仍只是 UI 状态和弱 hook。

`DSO_TriggerConfig` 当前包含：

```c
float level_volts;
bool level_enabled;
DSO_TriggerSlope slope;
```

engine 会根据 `DSO_AcquireCtrlInfo.trigger_source` 选择 CH1 或 CH2 的 active analysis；CH2 不可用时规整回 CH1。当前仍没有 Pulse/Video 等 trigger type 策略，若要实现这些类型，需要扩展明确的数据模型，而不是只在 UI 状态里增加选项。

### PB15 被外部测频占用

PB15 当前默认作为 TIM12_CH2 外部测频输入，不再作为 LED4 输出。维护时必须注意：

- `bsp_led.c` 中 `BSP_LED4_PB15_ENABLE` 默认 0。
- 不要在其他 BSP 初始化中重新把 PB15 配成普通输出。
- `TIM8_BRK_TIM12_IRQn` 已由 `dso_freq_meter_hw_tim12.c` 实现；如果未来 TIM8 break 需要独立保护逻辑，应在同一 IRQ 中明确合并处理，而不是新增另一个同名 handler。

### Channel enable 还不是采集开关

`User/ui/ui_channel_ctrl.c` 维护 CH1/CH2 enable 状态，并提供弱回调 `UI_ChannelCtrl_OnEnableChanged()`。

当前 10-bit 采集层仍会采 ADC3 CH2；UI 的 solo/显示策略和 channel enable 不是同一个概念。若产品要求“关闭通道就停止采集/处理/显示”，需要定义清楚：

- 关闭通道是否停止 ADC/DMA？
- 是否停止 record ingest？
- 是否隐藏 UI trace？
- 是否隐藏测量和采样率？
- 8-bit tri 中 ADC3 被 CH1 lane 占用时，CH2 enable 如何显示？

### LCDConf 和 main 的逻辑 framebuffer 注释有历史差异

`main.c` 的 `Framebuffer_Init()` 注释提到两个逻辑子帧；实际 `emWin/Config/LCDConf_Lin_Template.c` 中：

```c
#define NUM_BUFFERS 1
```

当前实际显示模型是：

- emWin 单逻辑 framebuffer。
- LCDConf 手动旋转/贴 PlotBuf 到物理 ping/pong framebuffer。
- LTDC 扫描物理 ping/pong。

这个差异未必是 bug，但改显示链路时应以 LCDConf 和 `bsp_vram_layout.h` 为准。

### Record buffer 与 RollHistory 存在 SDRAM 末端预算重叠风险

`User/bsp/inc/bsp_sdram_layout.h` 当前定义：

```text
SDRAM_RECORD_BUF  = 0xC0A00000
SDRAM_RECORD_SIZE = SDRAM_APP_SIZE - 6 MB
```

按 32 MB SDRAM 计算，record 区会延伸到 SDRAM 末端 `0xC2000000`。同时 `User/dso/roll_history_mem.c` 使用：

```text
ROLL_HIST_BLOCK_BYTES = 3 MB
base = EXT_SDRAM_ADDR + EXT_SDRAM_SIZE - 3 MB
```

也就是从 `0xC1D00000` 到 `0xC2000000`。这段地址落在当前 deep record 定义的尾部范围内。若 deep record 和 RollHistory 同时按最大容量使用，存在覆盖风险。

后续建议：

- 在 `bsp_sdram_layout.h` 中显式扣除 RollHistory 预留区。
- 或让 RollHistory 使用 record 区外的独立固定地址，并用编译期断言检查不重叠。
- 或限制 deep record 容量，让其上限止于 RollHistory base。

### IAR 工程当前不作为维护目标

当前 checkout 只存在：

```text
Project/MDK-ARM(uV5)
```

未发现：

```text
Project/EWARMv8
```

IAR 工程当前不作为维护目标；新增源文件只需要同步当前维护目标 Keil 工程和 host tests。历史文档中提到的 `Project/EWARMv8` 只作为来源背景，不作为本阶段待办。

## 审计清单

### 修改采样位宽或 sample format 时

- [ ] 更新 `dso_sample_spec.h/.c`。
- [ ] 确认 `adc_max` 和 `midscale` 与实际 ADC 分辨率一致。
- [ ] 搜索 `DSO_ADC_MAX`，确认没有固定 12-bit 换算残留。
- [ ] 确认 `DSO_Vertical` 使用 active sample spec。
- [ ] 确认 `DSO_Process` 触发电平、clip、测量使用 active sample spec。
- [ ] 确认 UI 测量和单位换算不再写死 4095。
- [ ] 更新 `test_dso_sample_spec.c`。

### 修改采集后端时

- [ ] 不在 `dso_acq.c` 放硬件细节。
- [ ] 10-bit 代码保持在 `dso_acq_10bit*`、`dso_10bit*`、`dso_hs10*` 等专属文件。
- [ ] 8-bit 专属文件只围绕三路交替采样收敛；不要继续维持 2L/single 旧路径。
- [ ] 8-bit sample spec 保持 `lane_count=3`、`interleaved=true`、`ch2_mode=DISABLED`。
- [ ] 8-bit CH2 对外保持不可用：`DSO_Acq8_IsCh2Available()==false`、CH2 sample rate 为 0。
- [ ] DMA buffer 保持 32 字节对齐。
- [ ] DMA 启动前对 RX buffer 做 clean+invalidate。
- [ ] half/full callback 覆盖正确半区。
- [ ] `CopyRecentSamples()` / `CopyNewSamples()` 读取 DMA ring 前对精确窗口做 invalidate。
- [ ] 采样格式切换或后端重启后，engine 等待 `DSO_Acq_IsDataReady()` 再分析和发布新帧。
- [ ] `GetPos()` 与 `CopyNewSamples()` 的 ring 语义一致。
- [ ] `GetSampleRate()` 与真实有效采样率一致。
- [ ] CH2 可用性通过 `DSO_Acq_IsCh2Available()` 统一判断。
- [ ] 串口初始化日志中 8-bit CH2 应显示 `hs8_ch2=0`，不要恢复旧 `hs8_dual_ch2` 字段。
- [ ] 新增源文件同步到 Keil 工程。

### 修改 RUN/STOP 或采集控制时

- [ ] UI 运行状态显示读取 `DSO_AcquireCtrl_IsRunRequested()`，不是 `DSO_Acq_IsRunning()`。
- [ ] 手动 STOP 不直接调用 `DSO_Acq_Stop()`。
- [ ] Engine 在 `run_requested == false` 时不发布新波形帧，并保留最后一帧。
- [ ] RUN 边沿同步 `ingest_prev_pos` / `roll_prev_pos`。
- [ ] RUN 边沿清除旧 trigger anchor，避免 record display window 锚在 STOP 前触发点。
- [ ] 空 record window 不发布成一条直线。
- [ ] SINGLE frozen 的物理 stop 路径与手动 STOP 逻辑暂停路径分开。
- [ ] 更新或运行 `test_dso_engine_record_anchor_policy.c`、`test_dso_engine_run_resume_policy.c`、`test_dso_runstop_control_contract.c`。

### 修改时基或渲染时

- [ ] 10-bit 表和 8-bit tri 表分开维护。
- [ ] `active_8bit_table()` 固定指向 tri 表，除非产品重新定义 8-bit 多 topology。
- [ ] `active_table()` 选择逻辑仍清晰。
- [ ] `DSO_Timebase_GetWindowSampleCount()` 与 sample rate 单位一致。
- [ ] HS10 只在 CH1 上启用，除非明确扩展 CH2 策略。
- [ ] Roll 模式不引入 ETS/HS8/HS10 状态依赖。
- [ ] column envelope 的 low/high buffer 与 UI 绘制方向一致。

### 修改 UI 显示时

- [ ] `DSO_DISPLAY_WIDTH/HEIGHT` 与 `MainTask` plot 区、PlotBuf、LCDConf 一致。
- [ ] PlotBuf 几何改动同步 `dma2d_wave.h` 和 LCDConf 中的物理 plot 区。
- [ ] header/body/full rotate 请求语义不被破坏。
- [ ] DMA2D 与 CPU 旋转路径不会并发配置同一个 DMA2D 外设。
- [ ] 修改 header 逻辑像素后调用 `LCDConf_RequestHeaderRotate()`。
- [ ] STOP/RUN 下帧率和延时符合预期。

### 修改输入或页面时

- [ ] 不要只改 UI 状态而忘记 DSO 控制层。
- [ ] 触发 source 已生效；若要让 trigger type 生效，需要新增明确的 Pulse/Video 等 engine 分析策略。
- [ ] channel enable 若要生效，需要定义采集、记录、测量、显示四层语义。
- [ ] EC1/EC2 按住组合逻辑避免互相误触发。

## 推荐的后续修正优先级

1. 清理 `dso_config.h` 和 `ui_system_setup.c` 中 12-bit 时代注释，避免误导。
2. 明确 trigger type 是否属于后续产品范围；source 已在当前阶段纳入 CH1/CH2。
3. 明确 channel enable 与 solo/display 的关系。
