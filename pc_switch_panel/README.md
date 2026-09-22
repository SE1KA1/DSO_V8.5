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
# 连 dsosim 模拟器（免硬件联调；另开终端先跑：python -m dsosim.cli --server）
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
| GUI → DSO | `{"cmd":"set_buzzer","params":{"on":1},"seq":9}`（MUTE 滑块：1=静音） |
| GUI → DSO | `{"cmd":"reboot","params":{"mode":"soft"},"seq":10}`（Reset 键：远程复位） |
| DSO → GUI | `{"ver":"0.9","type":"ack","seq":..,"cmd":"set_sw","result":"ok","error":""}` |
| DSO → GUI | `{"ver":"0.9","type":"ack","seq":..,"cmd":"get_switch","result":"ok","error":"","data":{"sw":16383,"led":5,"temp":245,"temp_ok":1,"eth_link":1,"usb_active":0,"buzzer_mute":0}}` |
| DSO → GUI | `{"ver":"0.9","type":"ack","seq":..,"cmd":"reboot","result":"ok","error":"","data":{"mode":"soft"}}` |

- `sw` = 14-bit bitmap（bit0=SW1 … bit13=SW14）；`led` = 3-bit（bit0=心跳, bit1=温度告警, bit2=电流告警）
- `temp` = 过桥板 SHT40 温度（十分之一 ℃，245 = 24.5℃）；`temp_ok` = 传感器有效（0 时 GUI 显示 `--.- ℃`）
- `eth_link`/`usb_active` = 以太网链路 / USB 串口活动；`buzzer_mute` = 蜂鸣器静音态（与设备侧为准同步 MUTE 滑块）
- 点击开关 → 翻转目标态 → `set_sw`；500ms 轮询 `get_switch` 收敛设备侧改动 + 刷新温度。

### Reset（远程复位）

- 连接后 Reset 键可用；点击弹出二次确认，确认后发 `reboot`，设备软复位
  （`NVIC_SystemReset`，等效按复位键；复位原因 `SFTRSTF`）。
- 设备重启期间 GUI 自动重连（2s 一次，最长 30s）：状态栏显示"设备复位中，自动重连…"，
  恢复后回到 `online`；超时则提示手动 Connect。
- 真机的另一条路径 `mode:"hard"`（停喂狗 → SP706T 拉低 NRST，真硬件复位）暂未实现，留作钩子。
