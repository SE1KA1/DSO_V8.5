# dsosim — DSO_V8.5 设备模拟器 v1.0

无需实板的 DSO 设备模拟器，用于驱动 PC 客户端主要页面和异常流程。

## 功能

- **双通道波形**：正弦 / 方波 / 三角 / 噪声，可设频率、幅度、占空比
- **测量**：频率 / Vpp / Vrms / 占空比 / 最大值最小值
- **触发**：AUTO / NORMAL / SINGLE，边沿 / 源 / 电平 / 位置
- **FFT**：2048 点参考 FFT（PC 端本地计算）
- **AWG**：波形 / 频率 / 占空比 / 启停 / DMA 欠载
- **状态机**：SAFE / LOCAL / PC / CLOUD / DEBUG / CONFIG / RECOVERY
- **异常事件**：断线、TCP 断开、溢出、相位同步、校准失败等
- **协议**：与《接口协议 v0.9》JSON 帧逐字段对齐

## 安装

```bash
pip install numpy pytest        # 纯标准库实现，numpy 可选
```

## 使用

```python
from dsosim import DSOSimulator

dev = DSOSimulator(seed=42)
dev.set_waveform(1, "sine", freq_hz=2000, amplitude_v=1.0)
dev.set_waveform(2, "square", freq_hz=500, amplitude_v=0.5)

frame = dev.read_frame()
print(frame.ch1["freq_hz"])       # 2000.0
print(len(frame.ch1["y"]))        # 608

# 控制指令
ack = dev.send_cmd("set_run", {"running": False})

# 异常注入
dev.disconnect_ethernet()         # 模拟网线拔出
```

## CLI

```bash
python -m dsosim.cli --server --port 5005   # 启动 TCP 服务器
python -m dsosim.cli --dump                 # 打印一帧 JSON
python -m dsosim.cli --interactive          # 交互式会话
```

## 测试

```bash
cd dsosim && python -m pytest tests/ -v
```

## 语义保真

- `y[608]` 与 `DSO_Frame.y_coords[608]` 逐列对应（水平像素 = 显示列）
- JSON 字段名/类型与《接口协议 v0.9》冻结 schema 一致
- 状态迁移遵循《云桥安全状态机 v0.9》迁移矩阵
- 错误码遵循 `E001..E009` 定义

## 目录结构

```
dsosim/
├── __init__.py     # 公共 API
├── device.py       # 核心状态机 + 遥测帧组装 + 控制命令
├── waveform.py     # 波形合成 + 测量
├── fft.py          # FFT / 频谱
├── awg.py          # AWG 模型
├── errors.py       # 错误码 + 错误事件
├── protocol.py     # JSON 序列化/反序列化
├── server.py       # TCP 服务器 (port 5005)
├── cli.py          # 命令行入口
└── tests/          # pytest 测试
```
