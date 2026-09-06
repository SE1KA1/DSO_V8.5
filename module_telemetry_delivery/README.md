# 设备模块数据包交接包 — 已上线 5 模块（供前端 / 后端整合）

本包为设备端已实现并经真机验证的 **5 个功能模块**的以太网上行数据包格式协议 + Python 参考编解码。设备每 1s 经 TCP 长连接（服务器默认 `192.168.100.100:5005`）推送一行一帧 JSON，后端按行读取、按 `module` 分派即可。

> 交接对象：**后端**（帧解析/入库/告警）与**前端**（状态展示/阈值/声光语义）。

## 目录结构

```
module_telemetry_delivery/
├─ README.md                    # 本文件：总览 / 五模块清单 / 交付指引
├─ packet_specs/                # 数据包格式协议（首读 00_envelope_framework.md）
│  ├─ 00_envelope_framework.md  # 统一信封协议（所有模块公共，必读）
│  ├─ 02_pcal6524…md            # 模块 2：PCAL6524 开关/LED（含声光联动总则）
│  ├─ 03_sht40…md               # 模块 3：SHT40 温度监测
│  ├─ 04_ina226_u10…md          # 模块 4：过桥板电流监测（U10）
│  ├─ 05_ina226_u44…md          # 模块 5：核心板电流监测（U44）
│  └─ 06_bgm_adc_dac…md         # 模块 6：BGM 模拟通道采样读数
└─ python_ref/                  # Python 参考编解码（纯 stdlib，后端可直接 import）
   ├─ README.md
   ├─ envelope.py               # 信封编解码 + 公共校验 + 自测
   └─ module_{xxx}.py           # 每模块一个：帧构造 / 解析 / schema 校验 + 自测
```

## 模块清单与验证状态（全部真机已验帧）

| # | 模块 | 硬件/引脚 | 周期 | 状态 |
|---|---|---|---|---|
| 2 | PCAL6524：13 开关 + 5 LED(YDS1-5) | I2C 0x22 | 1s | ✅ 已上线（sw/yds/eth/usb + 声光联动） |
| 3 | SHT40 温湿度 | I2C 0x44 | 1s | ✅ 已上线（temp/level + sensor_lost） |
| 4 | U10-INA226 过桥板电流 | I2C 0x41 / 100mΩ | 1s | ✅ 已上线（cur_ma/level + overcurrent 事件） |
| 5 | U44-INA226 核心板电流 | I2C 0x40 / 20mΩ | 1s | ✅ 已上线（cur_ma/level） |
| 6 | BGM_ADC_DAC 采样读数 | DSO 采样 | 1s | ✅ 已上线（频率计快照 + 采样态） |

未含：模块 1（ATECC 密码芯片，硬件待验证）、模块 7（BGM_DDS，待做）、模块 8（看门狗，最后单独配置交付）——**勿将本包当全量 8 模块清单**。

## 声光联动速览（前端展示用，详见 02 规格 §2）

- **等级 `level`**（0..3，各模块 data 内）：0 正常 / 1 预警 / 2 告警 / 3 紧急。
- **硬件灯**：电流报警红 YDS1、电流正常绿 YDS2、温度正常蓝 YDS3、温度报警橙 YDS4、USB 通讯青 YDS5（闪烁档见 02）。
- **蜂鸣器**：等级 = max(两路电流, 温度)，按档节奏响；**GUI `set_buzzer {on:1}` 静音**（LED 不受影响）。
- **最新电流阈值（实测标定）**：U10 额定 35mA → 预警 29.75 / 告警 35 / 紧急 52.5 mA；U44 额定 135mA → 预警 115 / 告警 135 / 紧急 202.5 mA。温度：预警 58 / 告警 60 / 紧急 65 ℃。调整需同步固件宏（dso_ina226.h / dso_ina226_u44.h / dso_sht40.h）。

## 给后端同事的接入指引

1. 读 `packet_specs/00_envelope_framework.md`（信封/传输），再按模块读 `packet_specs/0X_*.md`（每模块的 data schema 与阈值）。
2. 跑通参考实现（零第三方依赖）：
   ```bash
   python envelope.py
   python module_pcal6524.py    # 5 个 module_*.py 各自可独立跑自测
   python module_sht40.py
   python module_ina226_u10.py
   python module_ina226_u44.py
   python module_bgm_adc_dac.py
   ```
3. 服务端每行：`envelope.parse(line)` 校验信封 → 按 `module` 分派对应 `module_xxx.decode(line)`；非法帧抛 `ValueError`，丢弃并计数。
4. 告警用 `data.level`（比 `over_cur`/`hi_alarm` 更细）；`seq` 检丢帧；`ts_ms` 为设备单调 ms（墙上时钟换算按 device.status 心跳校准）。

## 给前端同事的接入指引

- 温度：模块 3 `temp_dezi`（÷10 得 ℃）+ `temp_ok`；状态色建议按 `level`：0 正常/蓝、1 预警/黄、2 告警/橙、3 紧急/红。
- 电流：模块 4/5 `cur_ma` + `level`，红色告警 = 两路最高级；数值与阈值见上表（供范围/颜色映射）。
- 下行控制（开关/静音）走 **GUI 独立 TCP/串口通道**（`pc_switch_panel`，get_switch/set_sw/set_buzzer），不在本上行遥测包内，另见《STM32 与服务器设备协议 v1.0》。

## 帧速览

```json
{"ver":"1.0","type":"module.data","module":"sht40","seq":10,"ts_ms":1500000,"data":{"temp_dezi":245,"temp_ok":1,"hi_alarm":0,"level":0}}
{"ver":"1.0","type":"module.data","module":"ina226_u10","seq":20,"ts_ms":2100000,"data":{"cur_ma":12,"over_cur":0,"valid":1,"level":0}}
{"ver":"1.0","type":"module.data","module":"pcal6524","seq":30,"ts_ms":3300000,"data":{"sw":16382,"yds":17,"eth_link":1,"usb_active":0}}
```
