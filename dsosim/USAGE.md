# dsosim v1.0 使用说明与测试顺序

---

## 一、环境准备

```bash
pip install pytest          # 仅测试需要；模拟器本体纯标准库
cd D:\DSO_V8.5
```

所有命令在 `D:\DSO_V8.5` 目录执行（保证 `dsosim` 包可导入）。

---

## 二、Python API 使用

### 2.1 最小示例

```python
from dsosim import DSOSimulator

dev = DSOSimulator(seed=42)          # seed 保证波形可复现

# 配置双通道波形
dev.set_waveform(1, "sine",    freq_hz=2000, amplitude_v=1.0)
dev.set_waveform(2, "square",  freq_hz=500,  amplitude_v=0.5, duty_pct=30)

# 读取一帧遥测
frame = dev.read_frame()
print(frame.ch1["freq_hz"])           # 2000.0
print(len(frame.ch1["y"]))            # 608
print(frame.ch2["duty_pct"])          # 30.0

# 下发控制指令
ack = dev.send_cmd("set_run", {"running": False})
print(ack.result)                     # "ok"
```

### 2.2 波形类型

| kind | 参数 | 说明 |
|------|------|------|
| `"sine"` | freq_hz, amplitude_v | 正弦 |
| `"square"` | freq_hz, amplitude_v, duty_pct | 方波，占空比 % |
| `"triangle"` | freq_hz, amplitude_v | 三角 |
| `"noise"` | noise_v | 高斯噪声，freq_valid=False |

### 2.3 控制命令清单

| 命令 | params | 示例 |
|------|--------|------|
| `set_run` | running(bool) | `{"running": False}` |
| `set_trig_mode` | mode(0/1/2) | Auto/Normal/Single |
| `set_trig_slope` | slope(0/1/2) | Rise/Fall/Both |
| `set_trig_source` | source(0/1) | CH1/CH2 |
| `set_trig_level` | level_v(float) | |
| `set_trig_pos` | pos_pct(0-100) | |
| `set_ch_coupling` | ch, coupling(0/1/2) | DC/AC/GND |
| `set_ch_probe` | ch, probe(1/10) | |
| `set_ch_offset` | ch, offset_v | |
| `set_ch_vdiv` | ch, vdiv_index | 10mV..1V 阶梯 |
| `set_acq_mode` | mode(0-3) | Sample/Peak/HiRes/Avg |
| `set_awg_start/stop` | — | |
| `set_awg_wave` | wave(0/1/2) | Sine/Triangle/Square |
| `set_awg_hz` | hz | |
| `set_awg_duty` | permille | |
| `set_state` | target | 状态切换 |
| `get_config` | — | |
| `reboot` | — | 需 CONFIG/RECOVERY 状态 |

### 2.4 异常事件注入

```python
from dsosim import DSOErrorCode

dev.disconnect_ethernet()              # 模拟网线拔出 → E003
dev.disconnect_tcp()                   # 模拟 TCP 客户端断开 → E004
dev.inject_error(DSOErrorCode.OVR)     # 模拟 ADC 溢出 → E001
dev.inject_error(DSOErrorCode.AWG_UNDERRUN)

frame = dev.read_frame()
print(frame.status["link_up"])         # False
print(frame.status["errors"])          # 错误列表

dev.clear_errors()                     # 清除全部错误
```

### 2.5 状态机

```python
dev.set_state("PC")       # LOCAL→PC    ✓
dev.set_state("CLOUD")    # PC→CLOUD    ✓
dev.safe_mode()           # →SAFE
dev.set_state("CLOUD")    # SAFE→CLOUD  ✗ (非法迁移)
```

---

## 三、CLI 使用

```bash
# 打印一帧 JSON（验证 schema）
python -m dsosim.cli --dump

# 启动 TCP 服务器（模拟真实 DSO 端口 5005）
python -m dsosim.cli --server --port 5005

# 交互式会话
python -m dsosim.cli --interactive
```

交互式命令：

```
wave 1 sine 2000 1.0      # CH1 正弦 2kHz 1V
wave 2 square 500 0.5     # CH2 方波 500Hz 0.5V
vdiv 1 500                # CH1 500mV/div
run 0                     # STOP
frame                     # 打印一帧 JSON
state PC                  # 切到 PC 状态
err E001                  # 注入溢出错误
quit
```

---

## 四、测试顺序

### 阶段 1：单元测试（pytest）

```bash
python -m pytest dsosim/tests/ -v
```

应显示 **22 passed**。测试覆盖：

| 类别 | 用例 | 验证点 |
|------|------|--------|
| 波形 | test_sine_wave_has_expected_length | 608 点，Y 范围 0-479 |
| 波形 | test_square_wave_duty_cycle | 占空比 50% 上下各半 |
| 波形 | test_noise_is_reproducible_with_seed | 同 seed 同输出 |
| 测量 | test_measurement_sine_freq_reported | 正弦 freq_valid=True |
| 测量 | test_measurement_noise_freq_invalid | 噪声 freq_valid=False |
| FFT | test_fft_sine_single_bin | 纯正弦能量集中单 bin |
| 状态 | test_state_transition_legal/illegal | 迁移矩阵校验 |
| 控制 | test_set_run_command | 指令生效 |
| 异常 | test_inject_ethernet_link_down | 断线 E003 |
| 协议 | test_frame_json_schema_complete | JSON 字段完整 |

### 阶段 2：协议 schema 冒烟

```bash
python -m dsosim.cli --dump | python -m json.tool
```

人工检查：`ver=0.9`、`ch1/ch2/tb/trig/awg/status/sys/freq_meter` 八个分组齐全，`ch1.y` 长度 608。

### 阶段 3：TCP 端到端（可选，无实板验证协议链路）

1. 终端 A 启动服务器：
   ```bash
   python -m dsosim.cli --server --port 5005
   ```
2. 终端 B 用 NetAssist 或 `telnet localhost 5005` 连接。
3. 发送控制指令，观察 ack：
   ```json
   {"cmd":"set_run","params":{"running":false},"seq":1}
   ```
   预期 ack：`{"ver":"0.9","type":"ack","seq":1,"cmd":"set_run","result":"ok","error":""}`

### 阶段 4：页面驱动流程验证

按 LCD 页面顺序，逐页用模拟器驱动 PC 客户端：

| 页面 | 模拟器操作 | 预期 PC 端表现 |
|------|-----------|---------------|
| Scope | `set_waveform` 双通道 | 波形折线 + 测量读数刷新 |
| Channel | `set_ch_coupling/probe/vdiv/offset` | 通道面板参数联动 |
| Trigger | `set_trig_mode/level/slope` | 触发面板 + 电平线 |
| Measure | 读 `frame.ch1.vpp_v/vrms_v` | 测量读数显示 |
| FFT | 读 `frame.ch1.y` → 本地 FFT | 频谱图 |
| AWG | `set_awg_start/wave/hz` | AWG 面板状态 |
| 频率计 | 读 `frame.status.ext_freq_*` | 频率计读数 |
| System | 读 `frame.sys.*` | 版本/状态 |

### 阶段 5：异常流程验证

| 异常 | 模拟器操作 | 预期 PC 端表现 |
|------|-----------|---------------|
| 网线拔出 | `disconnect_ethernet()` | link_up=False，告警 |
| TCP 断开 | `disconnect_tcp()` | 客户端断开提示 |
| ADC 溢出 | `inject_error(OVR)` ×3 | 溢出计数，可触发 SAFE |
| AWG 欠载 | `set_awg_hz(500000)` + start | underrun 告警 |
| 状态回退 | `set_state("CLOUD")` → `disconnect_ethernet()` | 状态迁移到 LOCAL/SAFE |

---

## 五、语义保真注意事项

1. **不要直接改 JSON 字段名**——schema 已冻结（v0.9），改名会导致 PC 客户端解析失败。
2. **波形按显示时间窗生成**——`y[608]` 是屏幕像素列，不是 ADC 原始采样。用 `time_per_pixel_ns` 控制水平分辨率。
3. **状态迁移有约束**——`set_state` 返回 False 表示非法迁移，调用方需处理。
4. **错误可恢复性**——`DSOErrorCode.recoverable` 区分可恢复/不可恢复，模拟真实固件的 SAFE 回退逻辑。
5. **seed 保证可复现**——测试用固定 seed，生产用随机 seed。
