# PC Switch Panel

PySide6 GUI that replicates the LCD **Switch panel** page (3 LED + 14 DIP
switches) and drives the real switches on the DSO over **TCP 5005** with JSON
control commands (aligned with `dsosim/protocol.py` v0.9).

## Setup

```bash
cd pc_switch_panel
python -m venv .venv
.venv\Scripts\pip install -r requirements.txt
```

## Run

```bash
# 连 dsosim 模拟器（免硬件联调，另开终端先跑 python -m dsosim.server）
.venv\Scripts\python main.py 127.0.0.1 5005

# 连真实 DSO（改成 DSO 的 IP）
.venv\Scripts\python main.py 192.168.100.x 5005
```

也可以在 GUI 的 Host/Port 输入框里直接改，点 Connect。

## 协议

| 方向 | 内容 |
|------|------|
| GUI → DSO | `{"cmd":"set_sw","params":{"sw":5,"on":1},"seq":7}` |
| GUI → DSO | `{"cmd":"get_switch","params":{},"seq":8}` |
| DSO → GUI | `{"ver":"0.9","type":"ack","seq":..,"cmd":"set_sw","result":"ok","error":""}` |
| DSO → GUI | `{"ver":"0.9","type":"ack","seq":..,"cmd":"get_switch","result":"ok","error":"","data":{"sw":16383,"led":5}}` |

- `sw` = 14-bit bitmap（bit0=SW1 … bit13=SW14）；`led` = 3-bit（bit0=心跳, bit1=电源, bit2=报警）
- 点击开关 → 翻转目标态 → `set_sw`；500ms 轮询 `get_switch` 收敛设备侧改动。
