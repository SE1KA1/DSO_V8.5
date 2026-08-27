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
print(ack.result)                     # "ok