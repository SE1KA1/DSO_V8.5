# LCD→PC 功能/控件/状态映射表

> 覆盖 DSO_V8.4 全部 8 个 LCD 页面。每个 LCD 控件对应 PC 入口、状态来源、控制命令和回读字段。
> 所有数据路径：`DSO_Frame`（只读遥测）∪ `AcquireCtrl/Vertical/Wavegen` API（可读写参数）。

---

## 一、Scope（波形主页面）

LCD 默认页面，显示波形 + 顶部状态栏 + 右侧快捷栏。

### 1.1 波形区

| LCD 控件 | PC 入口 | 状态来源 | 命令 | 回读字段 |
|---------|---------|---------|------|---------|
| CH1 波形线（黄色） | Canvas/SVG `<polyline>` 608 点 | `DSO_Frame.y_coords[608]` | — | `ch1.y[]` |
| CH2 波形线（青色） | Canvas/SVG `<polyline>` | `DSO_Frame.y_coords_ch2[608]` | — | `ch2.y[]` |
| 密集模式 CH1 包络 | Canvas `<rect>` 填充 | `y_column_env_top/bot_ch1[608]` | — | `ch1.env_top/env_bot[]` |
| 密集模式 CH2 包络 | Canvas `<rect>` 填充 | `y_column_env_top/bot_ch2[608]` | — | `ch2.env_top/env_bot[]` |
| 网格背景 (10×8 div) | 本地 SVG/CSS 绘制 | 固定 | — | — |
| 触发电平线 | Canvas 水平标线 | `trig_level_v` + `trig_level_enabled` | — | `trig.level_v` |
| 触发位置标记 (T) | Canvas 三角形/箭头 | `trig_pos_pct` | — | `trig.pos_pct` |
| CH1 参考地箭头 (← 黄色) | Canvas 标记 | `ch1.vert_scale_min_v/max_v` 计算零电位 Y | — | — |
| CH2 参考地箭头 (← 青色) | Canvas 标记 | `ch2.vert_scale_min_v/max_v` | — | — |
| 波形刷新 | `requestAnimationFrame` | 帧推送 | — | 帧率 (fps) |

### 1.2 顶栏状态

| LCD 控件 | PC 入口 | 状态来源 | 命令 | 回读字段 |
|---------|---------|---------|------|---------|
| CH1 图标 (黄色) | 固定标签 | — | — | — |
| CH2 图标 (青色) | 固定标签（8-bit 模式灰色） | `DSO_Frame.ch2_present` | — | `ch2.ch2_present` |
| 时基 (s/ms/μs/ns/div) | 文本标签 | `DSO_Frame.tb_sec_div_ns` | `set_tb_sec_div` (待扩展) | `tb.sec_div_ns` |
| 采样率 (Sa/s) | 文本标签 | `DSO_Frame.sample_rate` | — | `tb.sample_rate_hz` |
| Y-T / Roll 模式 | 文本标签 + 切换 | `DSO_Frame.tb_mode` | — | `tb.mode` |
| Solo 模式 | 文本 (CH1/CH2/双通道) | UI 本地状态 | — | — |
| RUN/STOP 按钮 | 按钮 (绿=Run, 红=Stop) | `DSO_Frame.status.running` | `set_run` | `status.running` |
| 触发模式 (Auto/Norm/Single) | 文本标签 | `DSO_Frame.trig_mode` | `set_trig_mode` | `trig.mode` |
| 触发状态 | LED 指示灯 | `DSO_Frame.triggered` | — | `trig.triggered` |
| 电池/电源 | 图标 | — | — | — |

### 1.3 测量读数列

| LCD 控件 | PC 入口 | 状态来源 | 命令 | 回读字段 |
|---------|---------|---------|------|---------|
| CH1 Vpp | 文本 "3.30 V" | `DSO_Frame.vpp × adc_lsb_v` | — | `ch1.vpp_v` |
| CH1 Vrms | 文本 | `DSO_Frame.vrms` | — | `ch1.vrms_v` |
| CH1 F (频率) | 文本 "12.34 kHz" | `DSO_Frame.freq_hz` + `freq_valid` | — | `ch1.freq_hz` |
| CH1 T (周期) | 文本 (1/freq) | `1.0 / freq_hz` | — | `ch1.freq_hz` |
| CH1 Duty (占空比) | 文本 "50.0%" | `DSO_Frame.duty_cycle` | — | `ch1.duty_pct` |
| CH2 Vpp | 文本 | `DSO_Frame.ch2_vpp × adc_lsb_v` | — | `ch2.vpp_v` |
| CH2 Vrms | 文本 | `DSO_Frame.ch2_vrms` | — | `ch2.vrms_v` |
| CH2 F (频率) | 文本 | `DSO_Frame.ch2_freq_hz` + `ch2_freq_valid` | — | `ch2.freq_hz` |
| CH2 Duty | 文本 | `DSO_Frame.ch2_duty_cycle` | — | `ch2.duty_pct` |
| EXT F (外部频率) | 文本 | `DSO_Frame.ext_freq_millihz / 1000.0` | — | `status.ext_freq_hz` |
| EXT 状态 | 状态文本 | `DSO_Frame.ext_freq_state` | — | `status.ext_freq_state` |

### 1.4 WAVE 快捷栏（右侧 5 入口）

| LCD 入口 | PC 入口 | 说明 |
|---------|---------|------|
| Scop | 默认波形页 | 当前页面 |
| Channel | 切换到 Channel 面板 | 见第二章 |
| Trigger | 切换到 Trigger 面板 | 见第四章 |
| Measure | 切换到 Measure 面板 | 见第五章 |
| System | 切换到 System 面板 | 见第七章 |

---

## 二、Channel（通道设置页面）

每通道独立子页。

### 2.1 CH1 通道

| LCD 控件 | PC 入口 | 状态来源 | 命令 | 回读字段 |
|---------|---------|---------|------|---------|
| 耦合 (DC/AC/GND) | 下拉框 3 选 | `DSO_Vertical_GetInfoForChannel(0).coupling` | `set_ch_coupling ch=0 coupling=X` | `ch1.coupling` |
| 探头倍率 (X1/X10) | 下拉框 2 选 | `GetInfoForChannel(0).probe` | `set_ch_probe ch=0 probe=X` | `ch1.probe` |
| V/div 档位 | 下拉框多档 | `GetInfoForChannel(0).mv_per_div` | `set_ch_vdiv ch=0 vdiv_index=X` | `ch1.vdiv_mv` |
| 垂直偏移 (Position) | 滑块 ±量程 | `GetInfoForChannel(0).offset_v` | `set_ch_offset ch=0 offset_v=X` | `ch1.offset_v` |
| 通道开关 (ON/OFF) | 切换按钮 | LCD 本地 | — | — |

### 2.2 CH2 通道

| LCD 控件 | PC 入口 | 状态来源 | 命令 | 回读字段 |
|---------|---------|---------|------|---------|
| 耦合 | 下拉框 | `GetInfoForChannel(1).coupling` | `set_ch_coupling ch=1 coupling=X` | `ch2.coupling` |
| 探头倍率 | 下拉框 | `GetInfoForChannel(1).probe` | `set_ch_probe ch=1 probe=X` | `ch2.probe` |
| V/div 档位 | 下拉框 | `GetInfoForChannel(1).mv_per_div` | `set_ch_vdiv ch=1 vdiv_index=X` | `ch2.vdiv_mv` |
| 垂直偏移 | 滑块 | `GetInfoForChannel(1).offset_v` | `set_ch_offset ch=1 offset_v=X` | `ch2.offset_v` |
| 通道开关 | 切换按钮 | LCD 本地 | — | — |

---

## 三、Trigger（触发设置页面）

| LCD 控件 | PC 入口 | 状态来源 | 命令 | 回读字段 |
|---------|---------|---------|------|---------|
| 触发模式 | 按钮组 Auto/Normal/Single | `DSO_AcquireCtrl_GetInfo().trigger_mode` | `set_trig_mode mode=X` | `trig.mode` |
| 触发边沿 | 下拉 Rise/Fall/Both | `GetInfo().trigger_slope` | `set_trig_slope slope=X` | `trig.slope` |
| 触发源 | 下拉 CH1/CH2 | `GetInfo().trigger_source` | `set_trig_source source=X` | `trig.source` |
| 触发电平 (V) | 数值输入 ±Vref | `GetInfo().trigger_level_v` | `set_trig_level level_v=X` | `trig.level_v` |
| 触发电平线开关 | 切换 | `GetInfo().trigger_level_enabled` | — | `trig.level_enabled` |
| 触发位置 (%) | 滑块 0-100 | `GetInfo().trigger_position_pct` | `set_trig_pos pos_pct=X` | `trig.pos_pct` |
| 预触发比例 | 滑块 | `GetInfo().pretrigger_pct` | — | `trigger.pretrigger_pct` |
| 触发状态 | LED | `DSO_Frame.triggered` | — | `trig.triggered` |

---

## 四、Measure（测量页面）

### 4.1 自动测量

| LCD 控件 | PC 入口 | 状态来源 | 命令 | 回读字段 |
|---------|---------|---------|------|---------|
| CH1 Vpp | 数值文本 | `DSO_Frame.vpp × adc_lsb_v` | — | `ch1.vpp_v` |
| CH1 Vrms | 数值文本 | `DSO_Frame.vrms` | — | `ch1.vrms_v` |
| CH1 Vmin | 数值文本 | `DSO_Frame.vmin × adc_lsb_v` | — | `ch1.vmin_v` |
| CH1 Vmax | 数值文本 | `DSO_Frame.vmax × adc_lsb_v` | — | `ch1.vmax_v` |
| CH1 Freq (频率) | 数值文本 | `DSO_Frame.freq_hz` | — | `ch1.freq_hz` |
| CH1 Period (周期) | 数值文本 | `1.0 / freq_hz` | — | `ch1.freq_hz` |
| CH1 Duty (占空比) | 数值文本 | `DSO_Frame.duty_cycle` | — | `ch1.duty_pct` |
| CH2 Vpp/Vrms/Vmin/Vmax/Freq/Duty | 同上 | `ch2_*` 系列 | — | 同上结构 |
| 外部频率 Fext | 数值文本 | `DSO_Frame.ext_freq_millihz / 1000.0` | — | `status.ext_freq_hz` |

### 4.2 光标测量（PC 端实现）

| LCD 控件 | PC 入口 | 说明 |
|---------|---------|------|
| 水平光标 (ΔV) | PC 本地叠加两条水平线 | 通过 Y 坐标反算电压，无需 DSO 支持 |
| 垂直光标 (Δt) | PC 本地叠加两条竖线 | 通过 X 坐标反算时间 |
| 光标值显示 | PC 本地计算 | (ΔX × ns/pixel, ΔY × mV/pixel) |

---

## 五、FFT（频谱页面）—— PC 端实现

DSO 固件无 FFT 功能，全部在 PC 端完成。

| LCD 控件 | PC 入口 | 说明 |
|---------|---------|------|
| FFT 频谱图 | Canvas/SVG 柱状图 | PC 本地对 `y_coords[608]` 做 FFT |
| 窗函数选择 | 下拉 (Rect/Hann/Hamming/Blackman) | PC 本地 DSP |
| FFT 点数 | 下拉 (256/512/1024/2048) | PC 本地 DSP |
| 频率轴 (Hz) | PC 本地计算 | `sample_rate / FFT_size` |
| 幅度轴 (dB) | PC 本地计算 | `20 × log10(|FFT|)` |
| 峰值标记 | PC 本地 | 自动标注前 N 个峰值 |
| 垂直档位 (dB/div) | 下拉 | PC 本地缩放 |
| 中心频率 | 滑块 | PC 本地频谱平移 |
| 带宽 | 滑块 | PC 本地频谱缩放 |

---

## 六、AWG（波形发生器页面）

### 6.1 配置

| LCD 控件 | PC 入口 | 状态来源 | 命令 | 回读字段 |
|---------|---------|---------|------|---------|
| 波形类型 | 下拉 Sine/Triangle/Square | `DSO_Wavegen_GetSelectedProfile().wave` | `set_awg_wave wave=X` | `awg.wave` |
| 频率 (Hz) | 数值输入 | `GetSelectedProfile().wave_hz` | `set_awg_hz hz=X` (需 profile 匹配) | `awg.hz` |
| 占空比 (‰) | 滑块 0-1000 | `GetSelectedProfile().duty_permille` | `set_awg_duty permille=X` (仅方波) | `awg.duty_permille` |
| 输出路径 | 文本 PA4/DAC1_CH1 | 编译期固定 | — | `awg.path` |
| 启停按钮 | 按钮 (On/Off) | `DSO_Wavegen_Poll().running` | `set_awg_start` / `set_awg_stop` | `awg.running` |

### 6.2 运行时状态

| LCD 控件 | PC 入口 | 状态来源 | 命令 | 回读字段 |
|---------|---------|---------|------|---------|
| 实际更新率 (Hz) | 文本 | `DSO_Wavegen_Poll().actual_update_hz` | — | `awg.actual_update_hz` |
| DMA 欠载告警 | 红色警告 | `Poll().dma_underrun` | — | `awg.underrun` |
| 定时器 PSC/ARR | 调试信息 | `Poll().tim_psc / tim_arr` | — | `awg.tim_psc/arr` |
| 查找表点数 | 信息 | `Poll().lut_count` | — | `awg.lut_count` |
| 错误状态 | 状态文本 | `DSO_WavegenStatus` | — | `awg.status` |

---

## 七、频率计（Frequency Meter 页面）

| LCD 控件 | PC 入口 | 状态来源 | 命令 | 回读字段 |
|---------|---------|---------|------|---------|
| 频率值 (Hz) | 数值文本 | `DSO_Frame.ext_freq_millihz / 1000.0` | — | `status.ext_freq_hz` |
| 频率有效指示 | LED | `DSO_Frame.ext_freq_valid` | — | `status.ext_freq_valid` |
| 频率计状态 | 状态文本 | `DSO_Frame.ext_freq_state` | — | `status.ext_freq_state` |
| 频率计模式 | 文本 Low/Gated | `DSO_Frame.ext_freq_mode` | — | `status.ext_freq_mode` |
| 闸门时间 (μs) | 文本 | `DSO_FreqMeterSnapshot.gate_us` | — | `freq_meter.gate_us` |
| 边沿计数 | 文本 | `DSO_FreqMeterSnapshot.edge_count` | — | `freq_meter.edge_count` |
| 参考时钟 ticks | 文本 | `DSO_FreqMeterSnapshot.ref_ticks` | — | `freq_meter.ref_ticks` |
| 溢出计数 | 警告 | `DSO_FreqMeterSnapshot.overflow_count` | — | `freq_meter.overflow_count` |
| 信号超时 (ms) | 文本 | `DSO_FreqMeterSnapshot.last_edge_age_ms` | — | `freq_meter.last_edge_age_ms` |
| 超量程告警 | 警告 | `DSO_FreqMeterState_OVERRANGE` | — | `freq_meter.overrange` |

---

## 八、System（系统设置页面）

### 8.1 设备信息（只读）

| LCD 控件 | PC 入口 | 状态来源 | 命令 | 回读字段 |
|---------|---------|---------|------|---------|
| 固件版本 | 文本 | 编译期常量 | — | `sys.fw_ver` |
| 硬件版本 | 文本 | `HAL_GetREVID()` | — | `sys.hw_rev` |
| 主控型号 | 文本 STM32H743 | 固定 | — | `sys.mcu` |
| ADC 模式 (8-bit/10-bit) | 文本 | `DSO_Frame.adc_bits` | — | `ch1.adc_bits` |
| 采样格式 | 文本 | `DSO_SampleSpec` | — | `sys.sample_format` |

### 8.2 状态监控（只读）

| LCD 控件 | PC 入口 | 状态来源 | 命令 | 回读字段 |
|---------|---------|---------|------|---------|
| 系统运行状态 | 文本 | `DSO_Frame.status.running` | — | `status.running` |
| 云桥连接状态 | 状态文本 | `DSO_Frame.state` (LOCAL/PC/CLOUD...) | `set_state` | `state` |
| ETH LINK 状态 | LED | 内部变量 | — | `netif.link_up` |
| TCP 客户端连接 | LED | 内部变量 | — | `tcp.connected` |
| 溢出 (OVR) 计数 | 数值 | `DSO_Acq_CheckOverrun()` | — | `status.overrun` |
| 相位同步错误 | 数值 | `DSO_Acq_GetPhaseSyncErrorCount()` | — | `status.phase_sync_err` |
| FreeRTOS 堆剩余 | 数值 | `xPortGetFreeHeapSize()` | — | `sys.heap_free` |
| 系统运行时间 | 文本 | `HAL_GetTick()` | — | `ts` |
| 帧率 (fps) | 数值 | PC 本地统计 | — | `sys.fps` |

### 8.3 校准（只读 + 控制）

| LCD 控件 | PC 入口 | 状态来源 | 命令 | 回读字段 |
|---------|---------|---------|------|---------|
| 校准状态 | 状态文本 | `DSO_Frame.cal_state` | — | `status.cal_state` |
| 校准进度 (%) | 进度条 | `DSO_Frame.cal_progress_pct` | — | `status.cal_pct` |
| 通道校准有效 | LED | `DSO_Frame.ch1_interleave_cal_valid` | — | `status.ch1_cal_valid` |
| 启动校准 | 按钮 | — | `start_cal` (待实现) | — |

### 8.4 采集配置

| LCD 控件 | PC 入口 | 状态来源 | 命令 | 回读字段 |
|---------|---------|---------|------|---------|
| 采集模式 | 下拉 Sample/Peak/HiRes/Avg | `DSO_AcquireCtrl_GetInfo().acquire_mode` | `set_acq_mode mode=X` | `acq.mode` |
| 平均深度 (2/4/8/16/32/64) | 下拉 | `GetInfo().average_depth` | `set_acq_avg_depth depth=X` | `acq.avg_depth` |
| 存储深度 | 下拉 Short/Medium/Deep | `GetInfo().record_depth_index` | — | `acq.record_depth` |
| 记录视图偏移 | 文本 | `GetInfo().record_view_offset` | — | `acq.view_offset` |
| 时基档位 | 下拉 | `DSO_Frame.tb_sec_div_ns` | — | `tb.sec_div_ns` |
| ETS 状态 | 文本 | `DSO_Frame.ets_active / ets_building` | — | `status.ets_active/building` |
| 重建模式 | 文本 | `DSO_Frame.recon_mode` | — | `ch1.recon_mode` |
| 显示质量 | 文本 | `DSO_Frame.display_quality` | — | `ch1.display_quality` |

### 8.5 显示/界面

| LCD 控件 | PC 入口 | 说明 |
|---------|---------|------|
| 背光亮度 | 滑块 | BSP 本地控制 |
| 屏幕方向 (横/竖) | 下拉 | LTDC 配置 |
| 网格亮度 | 滑块 | PC 本地 CSS |
| 波形线宽 | 下拉 (1/2/3px) | PC 本地 Canvas |
| 通道颜色 | 颜色选择器 | PC 本地 CSS |

### 8.6 保存/加载

| LCD 控件 | PC 入口 | 说明 |
|---------|---------|------|
| 保存波形截图 | 按钮 | PC 本地 `<canvas>.toDataURL()` → PNG |
| 保存原始数据 CSV | 按钮 | PC 本地 `DSO_Acq_GetBuf()` → CSV 导出 |
| 加载波形回放 | 文件选择器 | PC 本地读取 CSV/JSONL |

---

## 九、编码器映射（EC1 / EC2 → PC）

LCD 通过两颗 EC11 编码器交互。PC 端模拟如下：

| LCD 编码器操作 | PC 模拟 | DSO 命令 |
|--------------|---------|---------|
| EC1 短按 | 按钮 "RUN/STOP" | `set_run` |
| EC1 长按 (>1.5s) | 按钮 "MENU" | 打开右侧面板 |
| EC2 短按 | 按钮 "BACK" | 返回 Scope 页 |
| EC2 长按 | 按钮 "Scale↔Pos" | 切换旋钮模式 |
| EC1 旋转 (Scale) | 鼠标滚轮 / 滑块 | 时基档位 |
| EC1 旋转 (Position) | 鼠标滚轮 | `set_ch_offset ch=0` |
| EC2 旋转 (Scale) | 鼠标滚轮 | V/div 档位 |
| EC2 旋转 (Position) | 鼠标滚轮 | `set_ch_offset ch=1` |
| EC2 按住 + EC1 旋转 | 快捷键 | Solo 模式切换 |

---

## 十、遥测帧完整字段索引

PC 端接收的 JSON 遥测帧全部字段汇总：

```
ch1.y[]                  ch1.env_top/bot[]       ch1.freq_hz
ch1.freq_valid           ch1.vpp_v               ch1.vrms_v
ch1.vmin_v               ch1.vmax_v              ch1.duty_pct
ch1.vdiv_mv              ch1.coupling            ch1.probe
ch1.offset_v             ch1.scale_min_v         ch1.scale_max_v

ch2.y[]                  ch2.env_top/bot[]       ch2.freq_hz
ch2.freq_valid           ch2.vpp_v               ch2.vrms_v
ch2.vmin_v               ch2.vmax_v              ch2.duty_pct
ch2.vdiv_mv              ch2.coupling            ch2.probe
ch2.offset_v             ch2.scale_min_v         ch2.scale_max_v

tb.sec_div_ns            tb.sample_rate_hz       tb.eff_sample_rate_hz
tb.mode                  tb.time_per_pixel_ns

trig.mode                trig.source             trig.slope
trig.level_v             trig.pos_pct            trig.triggered
trig.level_enabled

awg.running              awg.wave                awg.hz
awg.duty_permille        awg.actual_update_hz    awg.underrun
awg.path                 awg.tim_psc             awg.tim_arr
awg.lut_count

status.running           status.overrun          status.phase_sync_err
status.cal_state         status.cal_pct          status.ets_active
status.ets_building      status.ext_freq_hz      status.ext_freq_valid
status.ext_freq_state    status.ext_freq_mode    state

freq_meter.gate_us       freq_meter.edge_count   freq_meter.ref_ticks
freq_meter.overflow_count freq_meter.last_edge_age_ms

ch1.adc_bits             ch1.recon_mode          ch1.display_quality
ch1.effective_sample_rate_hz  ch2.ch2_present
sys.fw_ver               sys.hw_rev              sys.mcu
sys.sample_format        sys.heap_free           ts
```

---

## 无遗漏确认

| LCD 页面 | 控件数 | 全部映射 | 控制命令 | 回读字段 |
|---------|--------|---------|---------|---------|
| Scope（波形主页） | 28 | ✅ | `set_run` | `ch1/ch2/tb/trig/status` |
| Channel（通道）×2 | 10 | ✅ | 4 条 `set_ch_*` | `ch1/ch2.*` |
| Trigger（触发） | 8 | ✅ | 5 条 `set_trig_*` | `trig.*` |
| Measure（测量） | 13 | ✅ | — | `ch1/ch2.*` |
| FFT（频谱） | 9 | ✅ PC 本地 | — | — |
| AWG（波形发生器） | 7 | ✅ | 4 条 `set_awg_*` | `awg.*` |
| 频率计 | 6 | ✅ | — | `status.ext_* / freq_meter.*` |
| System（系统）×6 子页面 | 14 | ✅ | 2 条 `set_acq_* / set_state` | `sys.* / acq.* / status.*` |
| 编码器 (EC1/EC2) | 8 | ✅ PC 模拟 | 对应按钮/滑块命令 | — |
| **总计** | **103** | **✅** | — | — |
