# DSO 复用接口说明

> 基于 DSO_V8.5 源码提取。所有接口遵循"内核无修改"原则，新增功能只通过适配层接入。

## 1. DSO_Frame — 波形帧（只读）

**定义**: `dso_process.h:115-175` / `dso_ui_bridge.h:117-201`

每帧从 Engine 产出，经 2-slot 临界区环形缓冲传递给 UI。外部适配层可直接读取。

### 波形数据

| 字段 | 类型 | 说明 |
|------|------|------|
| `y_coords[608]` | `int16_t` | CH1 每像素列 Y 坐标 |
| `y_coords_ch2[608]` | `int16_t` | CH2 每像素列 Y 坐标 |
| `waveform_column_envelope_ch1/ch2` | `bool` | 密集模式下启用列包络 |
| `y_column_env_top/bot_ch1[608]` | `int16_t` | CH1 列包络上下边界 |
| `y_column_env_top/bot_ch2[608]` | `int16_t` | CH2 列包络上下边界 |

### 测量值

| 字段 | 类型 | 说明 |
|------|------|------|
| `freq_hz` | `float` | CH1 频率 (Hz) |
| `freq_valid` | `bool` | CH1 频率有效 |
| `vpp` | `uint16_t` | CH1 峰峰值 (ADC code) |
| `vmin` / `vmax` | `uint16_t` | CH1 最小值/最大值 |
| `vrms` | `float` | CH1 有效值 (V) |
| `duty_cycle` | `float` | CH1 占空比 (0.0-1.0, -1=不可测) |
| `ch2_freq_hz` | `float` | CH2 频率 |
| `ch2_freq_valid` | `bool` | CH2 频率有效 |
| `ch2_vpp/vmin/vmax` | `uint16_t` | CH2 幅值 |
| `ch2_vrms` | `float` | CH2 有效值 |
| `ch2_duty_cycle` | `float` | CH2 占空比 |
| `ext_freq_valid` | `bool` | 外部频率计有效 |
| `ext_freq_millihz` | `uint64_t` | 外部频率 (mHz) |

### 时基/垂直/触发快照

| 字段 | 类型 | 说明 |
|------|------|------|
| `sample_rate` | `uint32_t` | CH1 采样率 (Hz) |
| `ch2_sample_rate` | `uint32_t` | CH2 采样率 (Hz) |
| `tb_sec_div_ns` | `uint32_t` | 时基 (ns/div) |
| `tb_mode` | `uint8_t` | 时基模式 |
| `tb_time_per_pixel_ns` | `float` | 每像素时间 (ns) |
| `vert_mv_per_div` | `uint32_t` | CH1 mV/div |
| `vert_scale_min_v` / `max_v` | `float` | CH1 电压范围 |
| `ch2_vert_mv_per_div` | `uint32_t` | CH2 mV/div |
| `ch2_vert_scale_min_v/max_v` | `float` | CH2 电压范围 |
| `trig_mode` | `uint8_t` | 触发模式 |
| `trig_source` | `uint8_t` | 触发源 |
| `trig_level_v` | `float` | 触发电平 (V) |
| `trig_level_enabled` | `bool` | 触发电平线启用 |
| `triggered` | `bool` | 本帧已触发 |
| `adc_bits` | `uint8_t` | ADC 位宽 (8/10) |
| `adc_lsb_v` | `float` | ADC LSB 电压 |

### Roll/校准/状态

| 字段 | 类型 | 说明 |
|------|------|------|
| `roll_ring_coherent_engine` | `bool` | Roll 环引擎一致 |
| `roll_resync_in_progress` | `bool` | Roll 重同步中 |
| `roll_fill_ch1/ch2` | `uint16_t` | Roll 填充进度 |
| `live_record_paused` | `bool` | 实时录制暂停 |
| `cal_state` | `uint8_t` | 校准状态 |
| `cal_progress_pct` | `uint8_t` | 校准进度 % |
| `ch1_interleave_cal_valid` | `bool` | CH1 交织校准有效 |
| `ets_active` | `bool` | ETS 启用 |
| `ets_building` | `bool` | ETS 构建中 |
| `recon_mode` | `uint8_t` | 重建模式 |
| `display_quality` | `uint8_t` | 显示质量 |
| `effective_sample_rate_hz` | `float` | 等效采样率 |

**获取方式**: 通过 `UI_Waveform_IsStreamActive()` / `UI_Waveform_PopFrame()`（`UI_WaveformCtrl.c`）

---

## 2. 渲染元数据（只读）

### DSO_RenderMeta
**定义**: `dso_timebase.h:54-98`

| 字段 | 类型 | 说明 |
|------|------|------|
| `recon_mode` | `DSO_ReconMode` | 重建模式 |
| `display_quality` | `DSO_DisplayQuality` | 显示质量 |
| `ets_active` / `ets_building` | `bool` | ETS 状态 |
| `fast_mode` | `bool` | 快速模式 |
| `column_envelope` | `bool` | 列包络模式 |
| `samples_per_cycle` | `float` | 每周期采样数 |
| `effective_sample_rate_hz` | `float` | 等效采样率 |

### DSO_RenderPolicyInput/Decision
**定义**: `dso_render_policy.h:35-63`

| 字段 | 类型 | 说明 |
|------|------|------|
| `fast_mode` / `sample_mode` | `bool` | 渲染模式标志 |
| `allow_ets` | `bool` | 允许 ETS |
| `freq_valid` | `bool` | 频率有效 |
| `clipped` | `bool` | 信号削波 |
| `repetitive_confidence` | `float` | 重复信号置信度 |
| `recon_mode` | `DSO_ReconMode` | 决策输出：重建模式 |
| `display_quality` | `DSO_DisplayQuality` | 决策输出：显示质量 |

---

## 3. 测量接口（只读）

### DSO_ProcessAnalysis
**定义**: `dso_process.h:77-103`

| 字段 | 类型 | 说明 |
|------|------|------|
| `freq_hz` | `float` | 频率 |
| `freq_valid` | `bool` | 频率有效 |
| `vmin` / `vmax` / `vavg` | `uint16_t`/`float` | 幅度统计 |
| `trigger_index` | `int32_t` | 触发点索引 |
| `trigger_pos_samples` | `float` | 触发点位置 |
| `triggered` | `bool` | 已触发 |
| `samples_per_cycle` | `float` | 每周期采样数 |
| `crossing_count` | `uint32_t` | 过零次数 |
| `period_jitter_pct` | `float` | 周期抖动 % |
| `repetitive_confidence` | `float` | 重复置信度 |
| `clipped` | `bool` | 削波标志 |

**API**: `DSO_Process_AnalyzeSamples()`, `DSO_Process_AnalyzeSamplesEx()`, `DSO_Process_MeasureDisplay()`, `DSO_Process_MeasureSamples()`

---

## 4. 触发配置（可读写）

### DSO_TriggerMode
**定义**: `dso_acquire_control.h:16-23`

| 值 | 说明 |
|----|------|
| `AUTO` (0) | 自动触发 |
| `NORMAL` (1) | 正常触发 |
| `SINGLE` (2) | 单次触发 |

### DSO_TriggerSlope
**定义**: `dso_acquire_control.h:25-32`

| 值 | 说明 |
|----|------|
| `RISING` (0) | 上升沿 |
| `FALLING` (1) | 下降沿 |
| `BOTH` (2) | 双沿 |

### DSO_TriggerSource
**定义**: `dso_acquire_control.h:34-39`

| 值 | 说明 |
|----|------|
| `CH1` (0) | 通道 1 |
| `CH2` (1) | 通道 2 |

### DSO_TriggerConfig
**定义**: `dso_process.h:105-112`

| 字段 | 类型 | 说明 |
|------|------|------|
| `level_volts` | `float` | 触发电平 (V) |
| `level_enabled` | `bool` | 电平显示 |
| `slope` | `DSO_TriggerSlope` | 边沿 |

### DSO_AcquireCtrlInfo（完整采集控制状态）
**定义**: `dso_acquire_control.h:52-81`

| 字段 | 类型 | RW | 说明 |
|------|------|-----|------|
| `trigger_mode` | `DSO_TriggerMode` | RW | 触发模式 |
| `trigger_slope` | `DSO_TriggerSlope` | RW | 触发边沿 |
| `trigger_source` | `DSO_TriggerSource` | RW | 触发源 |
| `trigger_level_v` | `float` | RW | 触发电平 |
| `trigger_level_enabled` | `bool` | RW | 电平线启用 |
| `trigger_position_pct` | `uint8_t` | RW | 触发位置 % |
| `acquire_mode` | `DSO_AcquireMode` | RW | 采集模式 |
| `average_depth` | `uint8_t` | RW | 平均深度 |
| `record_depth_index` | `uint8_t` | RW | 记录深度 |
| `pretrigger_pct` | `uint8_t` | RW | 预触发 % |
| `record_view_offset` | `int32_t` | RW | 记录视图偏移 |
| `single_captured` / `single_armed` | `bool` | RO | 单次状态 |
| `run_requested` | `bool` | RW | RUN/STOP |

**API**:
- `DSO_AcquireCtrl_GetInfo()` — 读取当前状态
- `DSO_AcquireCtrl_SetTriggerMode/LevelVolts/Slope/Source/PositionPct()` — 设置
- `DSO_AcquireCtrl_SetRunRequested(bool)` — RUN/STOP
- `DSO_AcquireCtrl_ApplyPending()` — 应用待定配置

---

## 5. 通道垂直设置（可读写）

### DSO_VerticalInfo（每通道快照）
**定义**: `dso_vertical.h:36-45`

| 字段 | 类型 | RW | 说明 |
|------|------|-----|------|
| `vdiv_index` | `uint8_t` | RO | V/div 档位索引 |
| `mv_per_div` | `uint32_t` | RO | mV/div |
| `volts_per_div` | `float` | RO | V/div |
| `offset_v` | `float` | **RW** | 垂直偏移 (V) |
| `probe` | `DSO_ProbeAtten` | **RW** | 探头倍率 (x1/x10) |
| `coupling` | `DSO_CouplingMode` | **RW** | 耦合 (DC/AC/GND) |
| `scale_min_v` | `float` | RO | 量程最小值 |
| `scale_max_v` | `float` | RO | 量程最大值 |

### DSO_CouplingMode
**定义**: `dso_vertical.h:25-29`

| 值 | 说明 |
|----|------|
| `DC` (0) | 直流耦合 |
| `AC` (1) | 交流耦合 |
| `GND` (2) | 接地 |

### DSO_ProbeAtten
**定义**: `dso_vertical.h:14-17`

| 值 | 说明 |
|----|------|
| `X1` (1) | 1x 探头 |
| `X10` (10) | 10x 探头 |

### DSO_ChannelId
`CH1` (0), `CH2` (1)

**API**:
- `DSO_Vertical_SetIndexForChannel(ch, index)` — 设置 V/div 档位
- `DSO_Vertical_SetOffsetForChannel(ch, offset_v)` — 设置垂直偏移
- `DSO_Vertical_SetProbeForChannel(ch, atten)` — 设置探头
- `DSO_Vertical_SetCouplingForChannel(ch, coupling)` — 设置耦合
- `DSO_Vertical_GetInfoForChannel(ch)` — 读取完整快照
- `DSO_Vertical_VoltageAtPixelYForChannel(ch, y)` — 像素→电压

---

## 6. FFT（未实现）

当前代码仅有 `DSO_FFT_SIZE 2048` 常量预留（`dso_config.h:525`），无运行时 FFT 功能。

---

## 7. AWG / 波形发生器（可读写配置 + 只读状态）

### DSO_WavegenWaveKind
**定义**: `dso_wavegen_types.h:16-20`

| 值 | 说明 |
|----|------|
| `SINE` (0) | 正弦波 |
| `TRIANGLE` (1) | 三角波 |
| `SQUARE` (2) | 方波 |

### DSO_WavegenProfile（配置）
**定义**: `dso_wavegen_types.h:30-36`

| 字段 | 类型 | RW | 说明 |
|------|------|-----|------|
| `wave` | `DSO_WavegenWaveKind` | RW | 波形类型 |
| `name` | `const char *` | RO | 名称 |
| `wave_hz` | `uint32_t` | RW | 频率 (Hz) |
| `lut_count` | `uint16_t` | RO | 查找表点数 |
| `duty_permille` | `uint16_t` | RW | 占空比 (‰) |

### DSO_WavegenSnapshot（运行时状态，只读轮询）
**定义**: `dso_wavegen_types.h:38-46`

| 字段 | 类型 | 说明 |
|------|------|------|
| `running` | `bool` | 运行中 |
| `dma_underrun` | `bool` | DMA 欠载 |
| `requested_update_hz` | `uint32_t` | 请求更新率 |
| `actual_update_hz` | `uint32_t` | 实际更新率 |
| `tim_psc` / `tim_arr` | `uint32_t` | 定时器参数 |
| `lut_count` | `uint16_t` | LUT 点数 |

### DSO_WavegenStatus（错误码）
**定义**: `dso_wavegen_types.h:22-28`

| 值 | 说明 |
|----|------|
| `OK` (0) | 正常 |
| `INVALID` (1) | 无效配置 |
| `TIMER_ERROR` (2) | 定时器错误 |
| `HAL_ERROR` (3) | HAL 错误 |
| `DMA_UNDERRUN` (4) | DMA 欠载 |

**API**:
- `DSO_Wavegen_GetSelectedProfile()` — 读取当前配置
- `DSO_Wavegen_StartFromConfig()` / `DSO_Wavegen_Stop()` — 启停
- `DSO_Wavegen_Poll(DSO_WavegenSnapshot *out)` — 轮询状态

---

## 8. RUN/STOP 控制（可读写）

### DSO_AcquireMode
**定义**: `dso_acquire_control.h:41-50`

| 值 | 说明 |
|----|------|
| `SAMPLE` (0) | 普通采样 |
| `PEAK_DETECT` (1) | 峰值检测 |
| `HIRES` (2) | 高分辨率 |
| `AVERAGE` (3) | 平均 |

**API**:
- `DSO_AcquireCtrl_SetRunRequested(bool)` — 设置 RUN/STOP
- `DSO_AcquireCtrl_IsRunRequested()` — 查询 RUN/STOP
- `DSO_AcquireCtrl_RearmSingle()` — 单次触发重新预触发
- `DSO_Acq_Start()` / `DSO_Acq_Stop()` / `DSO_Acq_IsRunning()` — 采集控制

---

## 9. 错误与状态（只读遥测）

### 溢出计数器（DSO_AcqDebugSnapshot）
**定义**: `dso_acq.h:15-140`

| 字段 | 类型 | 说明 |
|------|------|------|
| `overrun_count` | `uint32_t` | 总溢出次数 |
| `lane0/1/2_overrun_count` | `uint32_t` | 各 lane 溢出 |
| `phase_sync_errors` | `uint32_t` | 相位同步错误 |
| `phase_sync_error` | `bool` | 当前有同步错误 |

### 外部频率计状态（DSO_FreqMeterState）
**定义**: `dso_freq_meter.h:15-23`

| 值 | 说明 |
|----|------|
| `STOPPED` (0) | 停止 |
| `WAIT_EDGE` (1) | 等待边沿 |
| `MEASURING` (2) | 测量中 |
| `VALID` (3) | 有效 |
| `NO_SIGNAL` (4) | 无信号 |
| `OVERRANGE` (5) | 超量程 |
| `FAULT` (6) | 故障 |

### 校准错误（DSO_10bitLaneCalError / DSO_HS8LaneSolveFail）
**定义**: `dso_10bit_lane_cal.h:30-37`, `dso_hs8_lane_cal.h:35-43`

| 错误码 | 说明 |
|--------|------|
| NONE | 无错误 |
| NO_SIGNAL | 无信号 |
| NOT_REPETITIVE | 非重复 |
| CLIPPED | 削波 |
| SOLVE / SINGULAR / GAIN / SKEW / OFFSET | 校准求解失败 |

**API**: `DSO_Acq_GetDebugSnapshot()`, `DSO_Acq_CheckOverrun()`, `DSO_Acq_GetOverrunCount()`

---

## 10. ADC 原始数据访问（只读）

### DMA 位置与缓冲区
**API** (`dso_acq.h`):
- `DSO_Acq_GetPos()` → CH1 DMA 当前位置
- `DSO_Acq_GetBuf()` → CH1 DMA 缓冲区基址 (`const volatile uint32_t *`)
- `DSO_Acq_GetCh2Pos()` / `DSO_Acq_GetCh2Buf()` → CH2
- `DSO_Acq_CopyRecentSamples(buf, n)` → 复制最近 n 个样点
- `DSO_Acq_CopyNewSamples(buf, n)` → 复制自上次以来的新样点
- `DSO_Acq_GetSampleRate()` → 当前采样率
- `DSO_Acq_GetRuntimeInfo()` → ADC 时钟模型 (`DSO_AcqRuntimeInfo`)

### DSO_SampleSpec
**定义**: `dso_sample_spec.h:32-49`

| 字段 | 类型 | 说明 |
|------|------|------|
| `adc_bits` | `uint8_t` | ADC 位宽 |
| `adc_max` | `uint16_t` | ADC 满量程 |
| `midscale` | `uint16_t` | 中值 |
| `vref` | `float` | 参考电压 |
| `lane_count` | `uint8_t` | lane 数 |
| `ch2_mode` | `DSO_Ch2Mode` | CH2 模式 |
| `interleaved` | `bool` | 是否交织 |

---

## 适配层接入原则

1. **只读数据**（DSO_Frame, RenderMeta, 测量值, 错误状态）→ 直接读取全局变量或调用 Get* API
2. **可控参数**（触发, 通道, RUN/STOP, AWG）→ 通过 Set* API 设置，ApplyPending 提交
3. **轮询状态**（AWG 快照, 采集调试快照）→ 通过 Poll/GetSnapshot API 获取
4. **所有 API 均为同步调用**，无回调注册机制。外部适配层需自行实现轮询或中断通知
5. **禁止直接修改 DSO 内核变量**，所有写入必须通过已发布的 Set* 函数
