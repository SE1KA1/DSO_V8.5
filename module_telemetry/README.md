# module_telemetry — 设备功能模块数据上云交接包

各功能模块的状态/事件数据经以太网 TCP 上传服务器。本目录为**数据包格式规格 + Python 参考编解码 + STM32 上报代码**的交接件，可直接整体压缩发给后端同事。

## 目录结构

```
module_telemetry/
├─ README.md                    # 本文件：总览 / 模块清单 / 打包 / 交接指引
├─ packet_specs/                # 数据包格式协议规格（给后端同事的首读文件）
│  ├─ 00_envelope_framework.md  # 统一信封协议（所有模块公共）
│  └─ 01_atecc608b.md           # 模块 1：密码芯片 ATECC608B（身份登录/防篡改）
│  └─ (后续 02_.._08_.md 按模块续增)
├─ python_ref/                  # Python 参考编解码（纯 stdlib，后端可直接 import 使用）
│  ├─ README.md
│  ├─ envelope.py               # 信封编解码 + 公共校验 + 自测
│  └─ module_atecc608b.py       # 模块 1 帧构造/解析/schema 校验 + 自测
│  └─ (后续每模块一个 module_xxx.py)
└─ stm32/                       # STM32 侧上报代码/骨架（供嵌入式侧集成，本轮不参与 Keil 构建）
   ├─ module_telemetry.h
   └─ module_atecc608b_upload.c/.h
```

## 模块清单与状态（8 项，按开发顺序）

| # | 模块 | 硬件/引脚 | 状态 |
|---|---|---|---|
| 1 | 密码芯片 ATECC608B-SSHDA-T（身份登录/防篡改） | I2C **0x60**（W=0xC0/R=0xC1），PB7/PB8 | 🔄 重新调试：SSHDA-T 默认 7-bit **0x60**，且**必须先 Wake**（≤100kHz 发 0x00 使 SDA 低 ≥60µs → 等 ≥1.5ms），否则 Sleep 中一律 NACK —— 此前从未在"规范的 0x00 Wake 之后"扫过 0x60，故扫不到；旧"0x7C 异常器件/判非标准 ATECC"结论作废。已按规范重做（0x60 主探 + 0x35/0x36 备选对照），待烧录验证 |
| 2 | PCAL6524HEAZ：13 开关 + 5 LED(YDS1-5) | I2C 0x22, PB7/PB8；INT# PH8 | ✅ 已上线：sw/yds/eth/usb 上报 + YDS1-5 声光联动点灯（02 规格含总则，阈值/蜂鸣已对齐最新） |
| 3 | SHT40 温湿度 | I2C 0x44, PB7/PB8 | ✅ 已上线：temp_dezi + **rh_centi**（湿度）+ level 上报 + 蜂鸣联动（58/60/65℃） |
| 4 | U10-INA226 过桥板电流 | I2C 0x41, PB7/PB8, 100mΩ | ✅ 已上线：cur_ma/level + overcurrent 事件；阈值按实测标定（正常 50mA → 额定 70mA，10-10 重标定） |
| 5 | U44-INA226 核心板电流 | I2C 0x40, PB7/PB8, 20mΩ | ✅ 已上线：cur_ma/level；阈值按实测标定（正常 118mA → 额定 160mA） |
| 6 | BGM_ADC_DAC 模拟通道控制 | （DSO 采样已实现） | ✅ 已上线：频率计快照+采样态上报 |
| 7 | BGM_DDS 接口控制（AD9833） | P2_6/PA4/PC10/PC12/PD2 | ⏳ 待做 |
| 8 | SP706T 看门狗（远程恢复） | WDI PB9 / PFO# PG10 | 🔶 规格+Python+固件(dso_watchdog)+上报已验帧；喂狗策略/配置参数待最后单独对齐（PFO# 语义待确认） |

## 打包命令（Windows PowerShell，在 D:\DSO_V8.5 下执行）

```powershell
Compress-Archive -Path module_telemetry -DestinationPath module_telemetry_pack.zip -Force
```

## 交接指引（给后端同事）

1. 先读 `packet_specs/00_envelope_framework.md`（传输与公共信封），再读对应模块 `packet_specs/0X_*.md`。
2. 快速跑通参考实现（无需任何第三方库）：
   ```bash
   python module_telemetry/python_ref/module_atecc608b.py
   ```
   会打印示例帧并跑自测（字段齐全/坏帧拒绝/未知模块拒绝）。
3. 在自己的服务端按规格解析即可；**未实现的下行命令（auth_*）在规格中标注 TBD，联调时再对齐**。
4. STM32 骨架在 `stm32/`，标注了与现有遥测任务（dso_fmc_telemetry，服务器 192.168.100.100:5005）的挂接点；当前嵌入式侧按模块逐轮接入真实数据。

## 约定速览

- 传输：TCP 长连接，CRLF 分行，一行一帧 JSON；服务器默认 192.168.100.100:5005（可配）。
- 信封：`{"ver":"1.0","type":"module.data|module.event","module":"atecc608b","seq":n,"ts_ms":n,"data":{...}}`
- 数值单位全部写进模块规格；未知字段忽略；缺必填字段按坏帧丢弃并告警。
