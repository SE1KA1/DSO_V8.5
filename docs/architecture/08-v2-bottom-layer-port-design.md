# V2 底层移植设计方案

本文基于当前工程 `/mnt/d/DSO_V8.1` 与 `V2/` 目录下实际源码状态整理。目标是把 V2 中已经重构过的底层能力迁移到当前主工程，同时保护当前 UI、显示布局和交互代码不被 V2 的旧 UI 状态覆盖。

## 结论先行

不建议直接把 `V2/User/dso/`、`V2/User/bsp/` 或 `V2/User/ui/` 整包覆盖到当前工程。V2 的底层确实包含目标所需的 8-bit 三路交替采样收敛，但它也同时改变了默认启动格式、显示几何和 UI 交互：

- 当前工程已经按新约束将默认采样格式改为 `8-bit high-speed`，即上电进入三路交替采样；`10-bit` 仍保留为系统设置页可切换模式。
- V2 将 8-bit 路径收敛成 tri interleaved 单一路径；这与本次明确的目标架构一致。
- V2 将 `DSO_DISPLAY_HEIGHT` 从当前 `248` 改为 `395`，并把 `MainTask.c` 头部布局回退到旧的 `80px` 高度；当前工程已在 UI/显示方向做了新的布局和保护逻辑。
- V2 删除或回退了当前工程中的部分 UI/BSP 调试保护，例如 `ui_ec_poll_task`、`vram_sentinels`、增强版 MemManage 诊断等。
- V2 默认打开或提高了多个诊断/波特率/波形发生器配置，直接继承会改变当前串口和运行时负载；当前工程已按阶段迁入 PB15/TIM12 测频默认启动、HS8 诊断格式、串口 1152000 波特率、ADC 75/150MHz 时钟/10-bit 相位和 PA4 波形发生器，但仍不整包继承 V2 的 UI 几何、页面布局和串口调试默认打开行为。

推荐采用“目标采样架构优先 + 分阶段选择性移植”的方案：底层最终只保留两条主线，`8-bit` 三路交替采样和 `10-bit` 双路交替 + CH2 实时；UI 布局、绘制和页面结构默认不改。RUN/STOP 波形冻结修复是例外，它只允许在 `UI_WaveformCtrl.c` 和 `ui_trigger_ui.c` 做控制接口层的最小桥接，不迁移 V2 的 UI 几何、菜单和显示风格。

## 当前落地状态

截至本轮迁移，当前主工程已经落地以下 V2 底层能力：

- 默认上电格式：`DSO_SAMPLE_FORMAT_DEFAULT = DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED`。
- 采样架构：`8-bit` 固定三路交替采样，`10-bit` 保留 CH1 双路交替 + CH2 实时。
- RUN/STOP：手动 STOP 走 `run_requested=false` 逻辑暂停，engine 冻结帧发布。
- 触发：支持 CH1/CH2 trigger source、rising/falling/both、触发电平 ON/OFF；8-bit 下 CH2 不可用时自动规整为 CH1。
- 外部测频：PB15/TIM12 频率计已迁入，engine 默认 `Init/Start/Service`，结果进入 `DSO_Frame.ext_freq_*`，UI header 显示 `EXT:%s`。
- PB15 资源：LED4/PB15 默认关闭，避免板级 LED 初始化驱动 TIM12_CH2 输入。
- HS8 重入：三路 8-bit 在上电、格式/拓扑变化后先进入 settle/calibrating，校准 ready 前禁止 HS8/ETS 增强重构；ready 边沿会清空 HS8/ETS 累积，避免 RUN/STOP 或重启后沿用旧相位桶。
- HS8 低频识别：CH1 校准前后都会用 lane0 独立窗口做低频 bootstrap/refine，限制在 `<=1.5 MHz`、高重复度、足够 samples-per-cycle 的条件下参与校准和频率显示。
- 串口诊断：`HS8D` 已补齐 data-ready、DMA rate/count、per-lane accept、trigger divider、per-lane overrun 等字段；`HS8W` 同步输出 `ORD/ORDP` 三路排列频率估算；测频诊断新增 `[FREQ]`/`[FM]` 格式化能力。
- 串口默认：`DSO_SERIAL_DEBUG_UART_BAUD` 和 `DSO_VOFA_FIREWATER_UART_BAUD` 调整为 `1152000`，HS8 诊断默认压缩到 `HS8D stage=1`，默认关闭 `RAWD` 和 phase-only，避免三路窗口/原始 dump 常态占用串口。

已迁移但仍需保留边界：

- PA4/DAC 波形发生器链路已迁入，默认开启，且与旧 `dso_testgen` 做编译期互斥。
- V2 ADC 75 MHz / 150 MHz 输入时钟策略和 10-bit TIM phase 改动已作为独立阶段迁入，仍需要实机采样质量验证。
- V2 的 UI 几何、header 高度、页面布局、显示尺寸和串口调试默认打开行为不迁入。

## 当前工程保护边界

### 必须保护的 UI 文件

以下文件属于当前 UI 现状，不应被 V2 覆盖：

- `User/ui/MainTask.c`
- `User/ui/UI_WaveformCtrl.c`
- `User/ui/ui_screens.c`
- `User/ui/ui_system_setup.c`
- `User/ui/ui_nav.h`
- `User/ui/ui_trigger_ui.c`
- `User/ui/ui_trigger_ui.h`
- `User/ui/ui_theme.h`
- `User/ui/dma2d_wave.c`
- `User/ui/dma2d_wave.h`
- `User/ui/ui_ec_poll_task.c`
- `User/ui/ui_ec_poll_task.h`

例外边界：

- `User/ui/UI_WaveformCtrl.c` 和 `User/ui/ui_trigger_ui.c` 需要纳入 RUN/STOP 波形冻结修复，但修改范围仅限控制契约：
  - UI 不再直接调用 `DSO_Acq_Start()` / `DSO_Acq_Stop()` 实现手动 RUN/STOP。
  - UI 的运行状态显示和切换动作统一读写 `DSO_AcquireCtrl_IsRunRequested()` / `DSO_AcquireCtrl_SetRunRequested()`。
  - 不改变当前 header、plot 区、菜单布局、旋钮语义和页面绘制。

原因：

- 当前 UI 已采用 `HDR_H=160` 的头部与 `DSO_DISPLAY_HEIGHT=248` 的波形区配合，V2 的 `HDR_H=80` / `DSO_DISPLAY_HEIGHT=395` 与当前显示链路不一致。
- 当前工程已有 PlotBuf、DMA2D、Header/Body/Full rotate、VRAM sentinels 等显示保护；V2 的 UI 文件会回退这些工作。
- 当前用户明确要求“不影响当前 UI 相关代码”。

### 必须保护的 BSP/系统保护

以下当前工程能力也不应被 V2 回退：

- `User/bsp/stm32h7xx_it.c` 中增强版 MemManage 诊断。
- `User/bsp/bsp_vram_layout.h`
- `User/bsp/vram_sentinels.c`
- `User/bsp/vram_sentinels.h`
- `User/bsp/dma2d_nlr.h`
- `User/bsp/y_t_stress_ab.h`
- `User/main.c` 中当前 emWin/DSO 任务栈配置、DSO engine task handle、VRAM sentinel 初始化等。

V2 中的 `stm32h7xx_it.c` 相比当前工程是回退版本，不能直接移入。

### 目标采样架构

本次目标架构已明确收敛为两条采集主线：

- `8-bit high-speed`：只保留 ADC1/ADC2/ADC3 三路交替采样，三路都服务 CH1，CH2 在 8-bit 模式不可用。
- `10-bit`：CH1 使用 ADC1/ADC2 双路交替采样，CH2 使用 ADC3 实时采样。

因此，V2 中以下 8-bit 策略应作为目标迁移，而不是视为风险项：

- `dso_acq.c` 中 8-bit topology 固定为 `DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED`。
- `DSO_Acq_RequestHs8Topology()` 不再暴露 2L 产品路径。
- `DSO_Acq8_IsCh2Available()` 在 8-bit 下返回 false。
- 删除 single safe / dual interleaved 后端产品路径。
- `active_8bit_table()` 只返回 tri table。

仍需谨慎处理的是 V2 的 UI 布局、BSP 调试回退、显示几何和串口调试默认打开行为；这些不属于当前已确认的采样架构目标，不能跟随整包覆盖进入当前工程。

## V2 可迁移能力清单

### 1. 纯逻辑模块

这类模块没有硬件副作用，适合优先迁移并用 host test 锁定。

| 能力 | V2 文件 | 建议 |
| --- | --- | --- |
| PB15 频率计数学层 | `V2/User/dso/dso_freq_meter_math.c/.h` | 可直接迁入，先接 host test |
| lane window 解析 | `V2/User/dso/dso_lane_merge.c/.h` | 可选择性迁入，保持现有 API 向后兼容 |
| 波形发生器 profile/math/LUT | `V2/User/dso/dso_wavegen.c/.h`, `dso_wavegen_lut.c/.h`, `dso_wavegen_timer_math.c/.h`, `dso_wavegen_types.h` | 已迁入，并接入 PA4/DAC1 硬件路径；默认 profile 为 250kHz 方波 |
| 触发显示按采样率映射 | `V2/User/dso/dso_10bit_display_sync.c/.h` | 可迁入新增通用函数 `DSO_TriggerDisplaySync_MapByRate()`，保留旧函数 |
| both-edge/level-off 触发算法 | `V2/User/dso/dso_process.c/.h` | 可迁入，但要保持默认 `level_enabled=true` |

### 2. 采集后端底层修正

V2 对采集后端的改动分为三类：可以直接迁移的底层质量修正、已经确认要迁移的 8-bit 三路收敛、以及仍需硬件验证的时钟/相位参数。

可迁移的底层质量修正：

- DMA ring 读取前按精确窗口 invalidate D-Cache，而不是只依赖 half/full 回调。
- DMA 启动前对 RX buffer 做 clean+invalidate。
- `DSO_Acq_IsDataReady()` / `DSO_Acq10_IsDataReady()` / `DSO_Acq8_IsDataReady()` ready gate。
- `DSO_AcqDebugSnapshot` 增加 lane DMA 计数、ready 状态、accepted trigger div 等诊断字段。
- 10-bit CH1 lane 合并由 `dso_acq10_stream.c` 转为使用 `DSO_LaneMerge_ResolveCommon*Window2()` 的方式，便于统一窗口语义。
- `dso_acq_hal_stm32h7.c` 增加 `DMA1_Stream0_IRQHandler()` 与 `DMA2_Stream0_IRQHandler()`，仅当 8-bit tri DMA stream 映射迁移时需要。

已迁入但仍需实机硬件验证的行为变化：

- ADC core target 从 `50 MHz` 改为 `75 MHz`，输入 clock 从 `100 MHz` 档扩展为 `150 MHz`。
- 10-bit TIM1 三相从当前 `1/6, 1/2, 5/6` 改为 V2 的 `1/4, 3/4, 1/2`。
- 8-bit tri ARR 从当前路径切到 V2 的 `period=18`，等效 `33.333 MSPS`。
- 8-bit DMA stream 重新分布为 ADC1 `DMA2_Stream0`、ADC2 `DMA1_Stream0`、ADC3 `DMA2_Stream1`。

这些变化会直接影响采样率、相位和示波器实际表现；当前源码已经按 V2 参数收口，后续验收重点应放在实机波形质量、相位一致性、DMA 计数推进和触发稳定性上。

### 3. 触发控制与运行控制

V2 新增的控制模型包括：

- `DSO_TRIGGER_SLOPE_BOTH`
- `DSO_TRIGGER_SOURCE_CH1` / `DSO_TRIGGER_SOURCE_CH2`
- `trigger_level_enabled`
- `run_requested`
- `DSO_AcquireCtrl_SetRunRequested()`
- `DSO_AcquireCtrl_NormalizeTriggerSource()`

当前迁移状态：

- 在 `DSO_AcquireCtrlInfo` 中增加字段，但默认值保持当前行为：
  - `trigger_source = CH1`
  - `trigger_level_enabled = true`
  - `run_requested = true`
- `dso_process` 已支持 BOTH 边沿和 `trigger_level_enabled=false`。
- `dso_engine` 已根据 CH1/CH2 trigger source 选择 active analysis，并在 CH2 不可用时规整为 CH1。
- `dso_trigger_source_bridge.c` 已接入当前触发页 source 选择；UI 页面布局未整包迁移。

该迁移只改变底层契约和触发页的必要桥接，不迁移 V2 的触发页布局。

### 4. RUN/STOP 逻辑暂停与波形冻结修复

V2 文档 `V2/output/doc/RUN_STOP波形冻结问题修复总结.docx` 已确认该问题在 V2 中修复。该修复必须随底层迁移一起纳入当前工程，否则采样链路重构后仍可能出现 STOP 后再 RUN 画面不恢复、显示旧触发窗口或空 record 窗口直线的问题。

当前状态：已在当前工程落地 RUN/STOP 逻辑暂停主链路，涉及 `dso_acquire_control.*`、`dso_engine.c`、`UI_WaveformCtrl.c`、`ui_trigger_ui.c`。本次只迁移控制契约和 engine 帧冻结逻辑，不迁移 V2 的 UI 布局、菜单或显示几何。

问题根因：

- UI 和 engine 同时直接控制采集硬件启停，`DSO_Acq_IsRunning()` 与用户期望的 RUN/STOP 逻辑状态混用。
- 手动 STOP 物理停止 ADC/DMA 后，再 RUN 可能遇到 STM32H7 ADC/DMA restart 状态不稳定，导致 UI 显示 RUN 但帧不再连续更新。
- STOP 前的旧 `record_info.trigger_valid` 或 analysis trigger 状态可能继续锚定显示窗口，RUN 后看起来仍停在旧波形。
- RUN 边沿清空 record 后，第一帧可能是空 record window，旧逻辑把 recent samples 清成 1 点，显示为直线。
- V2 后续改成逻辑暂停后，若 `UI_Waveform_ToggleRunStop()` 仍读硬件 running 状态，则因为 STOP 不再停硬件，第二次按键会继续提交 STOP。

目标修复语义：

- 手动 STOP 是逻辑暂停：不物理停止高速 ADC/DMA，只冻结 engine 帧发布，保留 UI 上一帧。
- 手动 RUN 重新允许 engine 发布帧，并同步 `ingest_prev_pos` / `roll_prev_pos`，清理旧触发锚点。
- SINGLE 捕获完成后的 frozen 状态仍可物理停止采集，保留单次捕获语义。
- UI 的 RUN/STOP 状态来源是 `run_requested`，不是 `DSO_Acq_IsRunning()`。
- 空 record window 不应发布成有效显示帧；应等待有样本后再更新 recent/display frame。

建议迁移文件：

- 修改 `User/dso/dso_acquire_control.h`
- 修改 `User/dso/dso_acquire_control.c`
- 修改 `User/dso/dso_engine.c`
- 修改 `User/ui/UI_WaveformCtrl.c`
- 修改 `User/ui/ui_trigger_ui.c`

建议测试：

- `test_dso_engine_record_anchor_policy.c`
- `test_dso_engine_run_resume_policy.c`
- `test_dso_runstop_control_contract.c`
- 现有 `test_dso_serial_debug.exe`
- 现有 `test_dso_hf_pipeline.exe`

当前已新增并生成对应 `.exe` runner：

- `tests/test_dso_engine_record_anchor_policy.c/.exe`
- `tests/test_dso_engine_run_resume_policy.c/.exe`
- `tests/test_dso_runstop_control_contract.c/.exe`

### 5. PB15/TIM12 外部频率计

V2 新增完整的外部频率计链路：

- `dso_freq_meter.c/.h`
- `dso_freq_meter_math.c/.h`
- `dso_freq_meter_hw_tim12.c/.h`
- PB15 / TIM12_CH2 / AF2
- `TIM8_BRK_TIM12_IRQn`
- 低频 input capture，高频 gated count，`1 kHz` / `2 kHz` hysteresis
- `NO_SIGNAL` / `OVERRANGE` / `FAULT` 显式状态

迁移时必须同步处理：

- 当前 `User/bsp/src/bsp_led.c` 把 PB15 当 LED4 输出。若启用 PB15 频率计，必须禁用 LED4 对 PB15 的初始化和驱动。
- `TIM8_BRK_TIM12_IRQHandler()` 是 TIM8 break 与 TIM12 共用 IRQ。V2 在 `dso_freq_meter_hw_tim12.c` 里实现该 IRQ，并清理 TIM8 break flags。当前工程没有同名 IRQ，但需要确认其他模块不会期望使用 TIM8 break。
- Engine 默认 `Init/Start/Service` 频率计，并把结果写入 `DSO_Frame.ext_freq_*`。
- `ui_freq_format.c/.h` 已迁入，`MainTask` header 用 `EXT:%s` 显示外部测频。
- `bsp_led.c` 中 `BSP_LED4_PB15_ENABLE` 默认 0，PB15 默认归 TIM12_CH2 测频输入使用。

### 6. PA4/DAC 波形发生器

V2 新增 PA4/DAC1_CH1/TIM5_TRGO/DMA1_Stream5 波形发生器：

- `dso_wavegen.c/.h`
- `dso_wavegen_types.h`
- `dso_wavegen_lut.c/.h`
- `dso_wavegen_timer_math.c/.h`
- `dso_wavegen_dac_pa4.c/.h`

迁移风险：

- 当前工程已有 `dso_testgen.c` 使用 PA4 / DAC1_CH1 / DMA1_Stream5，只是默认 `DSO_TESTGEN_ENABLE=0`。
- BSP 中 `bsp_cpu_dac.c` 也有 PA4/DAC1/DMA1_Stream5 能力。
- V2 `main.c` 默认在 `DSO_WAVEGEN_ENABLE` 时创建 `Wavegen` 任务；直接迁移会改变系统负载并占用 PA4。

当前状态：

- 纯逻辑、LUT、timer math 和 PA4/DAC1_CH1/TIM5_TRGO/DMA1_Stream5 硬件路径均已迁入。
- `DSO_WAVEGEN_ENABLE` 默认 1，默认 profile 为 `DSO_WAVEGEN_PROFILE_SQUARE_250K`。
- `DSO_TESTGEN_ENABLE` 与 `DSO_WAVEGEN_ENABLE` 已做编译期互斥，不能同时为 1。
- `main.c` 在 `DSO_WAVEGEN_ENABLE` 打开时创建 `Wavegen` 任务。

## 三种移植路径

### 方案 A：整包替换 V2 底层

做法：

- 拷贝 V2 的 `User/dso/` 主要文件到当前工程。
- 同步 V2 `Project/MDK-ARM(uV5)/project.uvprojx` 中新增源文件。
- 避免拷贝 `User/ui/`。

优点：

- 初始移植速度最快。
- 能最大程度复现 V2 当前行为。

缺点：

- 会把 V2 的默认启动格式、ADC clock、TIM phase、串口诊断和波形发生器默认配置一起带进当前工程。
- 会覆盖或间接冲击当前 UI/BSP 保护。
- 即使不拷贝 UI 文件，底层 frame/control 契约变化也会间接要求 UI 适配。

结论：不推荐作为默认方案。

### 方案 B：选择性迁移，保持当前 UI 与目标采样架构

做法：

- 先迁移纯逻辑和新增独立模块。
- 再迁移 shared contract 字段，并用默认值保持当前行为。
- 然后把 8-bit 收敛为三路交替采样单一路径。
- 最后迁移采集后端质量修正；10-bit 时钟/相位等硬件参数单独验证。

优点：

- 符合“不影响当前 UI”的要求。
- 符合“8-bit 只保留三路交替采样，10-bit 保留双路交替 + CH2 实时”的目标架构。
- 每一步都能通过 host test 和 Keil build 验证。
- 能把 V2 的确定收益先拿进来。

缺点：

- 比整包覆盖慢。
- 需要拆分 V2 中混在一起的底层修正和产品策略。

结论：推荐。

### 方案 C：并行保留 V2 core，通过编译开关选择

做法：

- 新增 `User/dso_v2/` 或类似目录。
- 当前 core 与 V2 core 并行存在。
- 通过宏选择使用哪个 core。

优点：

- 便于对照验证。
- 回退简单。

缺点：

- 形成长期双路径。
- 与当前“不新增 fallback 层/并行旧新逻辑”的架构约束冲突。
- UI/engine/shared contract 更容易分叉。

结论：不推荐，除非只作为短期实验分支，且最终必须删除其中一条路径。

## 推荐实施阶段

### 阶段 0：冻结边界与基线验证

目标：

- 确认当前 UI 文件和 BSP 保护文件不在本轮迁移修改范围。
- 确认当前 host test 基线。
- 确认 V2 的 RUN/STOP 修复文档已经纳入迁移范围。

动作：

- 阅读并摘录 `V2/output/doc/RUN_STOP波形冻结问题修复总结.docx`，将其作为阶段 2 的验收依据之一。
- 运行当前已有 host checks：
  - `./tests/test_dso_serial_debug.exe`
  - `./tests/test_dso_hf_pipeline.exe`
- 记录 Keil 工程当前可编译状态。
- 在后续每个阶段检查 `User/ui/` 是否出现非预期 diff；阶段 2 允许的 UI diff 仅限 RUN/STOP 控制桥接。

### 阶段 1：迁移纯逻辑模块

目标：

- 把无硬件副作用的 V2 能力移入当前工程。

建议文件：

- 新增 `User/dso/dso_freq_meter_math.c`
- 新增 `User/dso/dso_freq_meter_math.h`
- 新增 `User/dso/dso_wavegen_types.h`
- 新增 `User/dso/dso_wavegen_lut.c`
- 新增 `User/dso/dso_wavegen_lut.h`
- 新增 `User/dso/dso_wavegen_timer_math.c`
- 新增 `User/dso/dso_wavegen_timer_math.h`
- 修改 `User/dso/dso_lane_merge.c`
- 修改 `User/dso/dso_lane_merge.h`
- 修改 `User/dso/dso_10bit_display_sync.c`
- 修改 `User/dso/dso_10bit_display_sync.h`

测试：

- 引入并运行 V2 中对应 host tests：
  - `test_dso_freq_meter_math.c`
  - `test_dso_wavegen_lut.c`
  - `test_dso_wavegen_timer_math.c`
  - `test_trigger_source_rate_mapping.c`

### 阶段 2：迁移控制契约与 RUN/STOP 冻结修复

目标：

- 让底层具备 V2 的触发源、both slope、level off、run requested 数据模型。
- 保持当前 UI 布局和页面结构不变，只接入底层必须暴露的控制字段。
- 纳入 V2 已修复的 RUN/STOP 波形冻结问题，采用逻辑暂停而不是手动物理停采。

建议文件：

- 修改 `User/dso/dso_acquire_control.h`
- 修改 `User/dso/dso_acquire_control.c`
- 修改 `User/dso/dso_process.h`
- 修改 `User/dso/dso_process.c`
- 修改 `User/dso/dso_engine.c`
- 修改 `User/ui/UI_WaveformCtrl.c`
- 修改 `User/ui/ui_trigger_ui.c`

原则：

- 默认 trigger source 固定 CH1。
- 默认 trigger level enabled。
- 当前 UI 接入 `dso_trigger_source_bridge.c`，但只支持 CH1/CH2 两个实际底层源。
- UI 布局和页面结构不改；只做 RUN/STOP、source/slope/level/mode 的必要控制接口桥接。
- `UI_Waveform_IsRunning()` 和 `UI_Waveform_ToggleRunStop()` 统一读写 `DSO_AcquireCtrl_IsRunRequested()` / `DSO_AcquireCtrl_SetRunRequested()`。
- UI 手动 RUN/STOP 不直接调用 `DSO_Acq_Start()` / `DSO_Acq_Stop()`。
- Engine 每轮读取 `run_requested`。STOP 时跳过帧生成和 `DSO_UI_SendFrame()`，保留最后一帧；RUN 边沿同步 ingest/roll 游标并清除旧触发锚点。
- RUN 后 record window 未形成有效样本前，不发布空窗口直线帧。
- SINGLE 捕获 frozen 可以继续使用物理 stop，不与手动 STOP 逻辑暂停混淆。

测试：

- `test_dso_acquire_control_trigger_source.c`
- `test_dso_acquire_control_trigger_slope_both.c`
- `test_trigger_level_off_contract.c`
- `test_dso_process_trigger_both.c`
- `test_dso_engine_record_anchor_policy.c`
- `test_dso_engine_run_resume_policy.c`
- `test_dso_runstop_control_contract.c`

### 阶段 3：迁移采样架构收敛、ready gate 与 DMA cache 窗口

目标：

- 将 8-bit 收敛为三路交替采样单一路径。
- 修正高速 DMA 读取时的 cache 一致性。
- 切换采样模式或重启采样后，engine 等待数据 ready 再分析。

建议文件：

- 修改 `User/dso/dso_acq.h`
- 修改 `User/dso/dso_acq.c`
- 修改 `User/dso/dso_acq_8bit.h`
- 修改 `User/dso/dso_acq_8bit.c`
- 修改 `User/dso/dso_acq_10bit.h`
- 修改 `User/dso/dso_acq_10bit.c`
- 修改 `User/dso/dso_engine.c`

原则：

- `8-bit` 固定为 tri interleaved，删除 single safe / dual interleaved 产品路径。
- `DSO_Acq_IsCh2Available()` 在 8-bit 下返回 false，10-bit 下保留 CH2 实时。
- UI 格式切换最终收敛为 `8-bit (3L) -> 10-bit -> 8-bit (3L)`；上电默认进入 8-bit。
- 10-bit 可以优先迁移 V2 的 `prepare_all_dma_rx_buffers()`、`invalidate_dma_u16_window()`、`invalidate_dma_u16_recent()`。
- 不默认迁移 10-bit TIM phase 改动；相位改动作为独立硬件验证项。

测试：

- `test_dso_acq_ready_contract.c`
- `test_dso_engine_8bit_ready_contract.c`
- `test_dso_acq10_sampling_chain_contract.c`

### 阶段 4：接入 PB15/TIM12 频率计

目标：

- 迁移 V2 外部频率计底层，按当前约束默认上电开启，并在当前 UI header 中做最小显示接入。

建议文件：

- 新增 `User/dso/dso_freq_meter.c`
- 新增 `User/dso/dso_freq_meter.h`
- 新增 `User/dso/dso_freq_meter_hw_tim12.c`
- 新增 `User/dso/dso_freq_meter_hw_tim12.h`
- 修改 `User/dso/dso_config.h`
- 修改 `User/dso/dso_process.h`
- 修改 `User/dso/dso_engine.c`
- 新增 `User/ui/ui_freq_format.c`
- 新增 `User/ui/ui_freq_format.h`
- 修改 `User/ui/MainTask.c`
- 修改 `User/bsp/src/bsp_led.c`
- 修改 `Project/MDK-ARM(uV5)/project.uvprojx`

必要保护：

- `BSP_LED4_PB15_ENABLE` 默认 0 或在频率计启用时强制 LED4 不占 PB15。
- 频率计默认由 engine 初始化和启动；不要再引入默认关闭的隐式启用开关。
- `TIM8_BRK_TIM12_IRQHandler()` 只新增一次，不与其他模块重复定义。

测试：

- `test_dso_freq_meter_math.c`
- `test_dso_freq_ui_format.c`
- `test_dso_freq_meter_contract.c`
- `test_dso_freq_meter_engine_contract.c`
- `test_dso_freq_meter_state_contract.c`
- `test_dso_freq_meter_hw_contract.c`
- Keil build。

### 阶段 5：接入 PA4 波形发生器

目标：

- 按当前需求接入 V2 wavegen 硬件层，提供默认板载测试源。

建议文件：

- 新增 `User/dso/dso_wavegen.c`
- 新增 `User/dso/dso_wavegen.h`
- 新增 `User/dso/dso_wavegen_dac_pa4.c`
- 新增 `User/dso/dso_wavegen_dac_pa4.h`
- 修改 `User/dso/dso_config.h`
- 修改 `User/main.c`
- 修改 `Project/MDK-ARM(uV5)/project.uvprojx`

必要保护：

- `DSO_WAVEGEN_ENABLE` 默认 1。
- 默认 profile 为 `DSO_WAVEGEN_PROFILE_SQUARE_250K`。
- 编译期禁止 `DSO_WAVEGEN_ENABLE && DSO_TESTGEN_ENABLE`。
- 默认创建 `Wavegen` 任务。
- 确认 PA4 没被当前测试源或外部用途占用。

测试：

- `test_dso_wavegen_config.c`
- `test_dso_wavegen_lut.c`
- `test_dso_wavegen_timer_math.c`
- Keil build。

### 阶段 6：硬件行为验证项

以下内容不建议混入前五个阶段，应单独审批或单独验证：

- 是否接受 V2 的 `75 MHz` ADC core target。
- 是否把 10-bit TIM1 phase 改为 `1/4, 3/4, 1/2`。
- 是否改变默认启动格式；当前已经按新约束改为 `8-bit high-speed`。
- 是否扩大外部测频 UI 展示；当前只在 header 显示 `EXT:%s`。
- 是否继续扩展 trigger type；当前只接入 edge trigger 的 CH1/CH2 source、slope、level。

## 工程文件同步

Keil 工程需要增量加入新源文件。V2 工程中已注册的新文件包括：

- `dso_freq_meter.c`
- `dso_freq_meter_math.c`
- `dso_freq_meter_hw_tim12.c`
- `dso_wavegen.c`
- `dso_wavegen_lut.c`
- `dso_wavegen_timer_math.c`
- `dso_wavegen_dac_pa4.c`
- `dso_trigger_source_bridge.c`
- `ui_freq_format.c`

当前推荐只按阶段加入需要的文件。本轮已加入 `dso_trigger_source_bridge.c` 和 `ui_freq_format.c`，但仅做当前 UI 的必要桥接和外部测频显示，不迁移 V2 UI 布局。

当前 checkout 未发现 `Project/EWARMv8/`，若 IAR 仍需支持，需要先恢复该工程目录再同步。

## 已确认的关键决策

用户已明确目标采样架构：

- `8-bit` 只保留三路交替采样。
- `10-bit` 保留 CH1 双路交替采样 + CH2 实时采样。
- V2 中已修复的 RUN/STOP 波形冻结问题必须纳入迁移：手动 STOP 采用逻辑暂停，UI 状态读 `run_requested`，engine 冻结帧发布而不是重启 ADC/DMA。

因此，本轮实现不再维护 `8-bit (2L)`、single safe 或 dual interleaved 作为产品路径。根据新的产品约束，默认启动格式已经从 `10-bit` 改为 `8-bit high-speed`。

## 当前落地状态

截至阶段 4 完成，当前主工程已经落地以下内容：

- 阶段 2 RUN/STOP 冻结修复已迁入：
  - `DSO_AcquireCtrl` 增加 `run_requested`。
  - UI RUN/STOP 入口只读写逻辑运行意图，不再直接物理启停 ADC/DMA。
  - Engine 在手动 STOP 时冻结帧发布，RUN 边沿同步 record/roll 游标并避免空 record window 直线帧。
- 阶段 3 采样架构收敛已迁入：
  - `DSO_SAMPLE_FORMAT_DEFAULT` 已改为 `DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED`，上电默认 8-bit 三路交替采样。
  - `10-bit` 保持 CH1 ADC1/ADC2 双路交替 + CH2 ADC3 实时。
  - `8-bit high-speed` 的 sample spec 固定为 `lane_count=3`、`interleaved=true`、`CH2 disabled`。
  - `dso_acq.c` 将所有 8-bit topology request 收敛到 `DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED`。
  - `dso_acq_8bit.c` 采用 ADC1/ADC2/ADC3 三路独立 DMA：ADC1 `DMA2_Stream0`、ADC2 `DMA1_Stream0`、ADC3 `DMA2_Stream1`。
  - 8-bit ADC3 使用 `ADC_CHANNEL_1` 作为 CH1 第三 lane，不再使用 `ADC_CHANNEL_0` 做 8-bit CH2。
  - `dso_timebase.c` 删除 8-bit single/dual 产品表选择，`active_8bit_table()` 固定返回 tri 表。
  - `ui_system_setup.c` 的模式切换只在 `10-bit` 与 `8-bit (3L)` 之间循环。
  - `DSO_Acq_IsDataReady()`、10-bit/8-bit 后端 ready gate、debug snapshot ready 字段已接入。
  - 10-bit 和 8-bit DMA ring 读取前都加入精确窗口 D-Cache invalidate，DMA 启动前加入 RX buffer clean+invalidate。
  - 串口初始化日志把旧 `hs8_dual_ch2` 改为 `hs8_ch2=0`，避免继续暗示 8-bit dual + CH2 实时路径。
- 阶段 4 触发源和外部测频已迁入：
  - `DSO_TRIGGER_SLOPE_BOTH`、`DSO_TRIGGER_SOURCE_CH1/CH2`、`trigger_level_enabled` 已进入 acquire/process/engine 链路。
  - `dso_trigger_source_bridge.c` 已接入 UI source 选择；CH2 不可用时自动规整为 CH1。
  - `dso_freq_meter*` 已迁入，PB15/TIM12 测频默认由 engine 启动并 service。
  - `ui_freq_format.c/.h` 已迁入，`MainTask` header 显示 `EXT:%s`。
  - `bsp_led.c` 默认关闭 LED4/PB15，避免与 TIM12_CH2 输入冲突。
  - Keil `.uvprojx` 已注册 `dso_freq_meter.c`、`dso_freq_meter_math.c`、`dso_freq_meter_hw_tim12.c`、`dso_trigger_source_bridge.c` 和 `ui_freq_format.c`。

阶段 5 已继续迁入的 V2 行为：

- ADC 时钟基线已从 50MHz core 改为 V2 的 75MHz core。
- Rev.Y / unknown 使用 75MHz ADC input；Rev.V 使用 150MHz ADC input 并通过 core divider 2 回到 75MHz。
- `dso_acq_model.h` 已按 independent ADC lane 建模，不再保留旧的双 ADC 打包降级速率分支。
- `dso_acq_10bit.c` 已把 10-bit PLL2 限定为 75MHz/150MHz 两个输入档。
- 10-bit TIM 相位已迁移为 `1/4、3/4、1/2`。
- `dso_wavegen*` 已迁入，默认 `DSO_WAVEGEN_ENABLE=1`，PA4/DAC1_CH1 由 TIM5_TRGO + DMA1_Stream5 输出默认 250kHz 方波。
- `DSO_TESTGEN_ENABLE` 与 `DSO_WAVEGEN_ENABLE` 编译期互斥，避免两个内部源同时抢 PA4/DAC1/DMA1_Stream5。

仍未迁入的 V2 行为：

- 没有整包覆盖当前 UI 布局、显示几何或页面。
- 串口基础波特率、`DSO_SERIAL_DEBUG_UART_BAUD` 和 `DSO_VOFA_FIREWATER_UART_BAUD` 已统一到 1152000，但没有启用 V2 的串口调试默认打开行为。
- 没有迁移 V2 的显示几何差异。

## 推荐下一步

阶段 5 之后，建议先做硬件侧确认和工程文件同步，而不是马上继续扩大功能面：

- 在 Keil 中 rebuild `Project/MDK-ARM(uV5)/project.uvprojx` 的 `Flash` target，确认新增/修改源文件已经被工程纳入。
- 用串口 snapshot 确认 8-bit 初始化日志为 `hs8_top=3`、`lc=3`、`hs8_ch2=0`、`lane2_ch=1`。
- 用实际信号确认 8-bit 三路 DMA 计数都在推进，`data_ready=1` 后才开始发布新帧。
- 用 RUN/STOP 反复切换确认 STOP 保持最后一帧、RUN 后恢复动态波形。
- 用 PB15 外部方波验证 TIM12_CH2 测频状态：无信号时应显示 `--` 或测量中状态，有信号时 header 应显示 `EXT:<频率>`。
- PA4 波形发生器已作为阶段 5 纳入；后续只需要按实机需要调整 profile、开关宏或输出幅度相关参数。
