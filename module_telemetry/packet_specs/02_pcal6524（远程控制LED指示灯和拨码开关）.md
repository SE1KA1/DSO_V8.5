# 模块 2：PCAL6524HEAZ（13 开关 + 5 硬件 LED YDS1-5）

## 1. 硬件与用途

| 项 | 值 |
|---|---|
| 器件 | PCAL6524HEAZ（I2C IO 扩展，24 IO = P0/P1/P2） |
| I2C 7 位地址 | **0x22**（SDA=PB7, SCL=PB8，同一条软件 I2C 总线） |
| 开关 | P0/P1 经 4×SN74LVC257 复用器驱动 13 个 DIP（B1..B9、B11..B14；B10 悬空恒 0） |
| 5 LED | P2_0..P2_4 = 物理 YDS1..YDS5，**输出低电平点亮**（固件已归一化为 1=亮） |

## 2. 声光联动总则（LED YDS1-5 + 蜂鸣器，供 03/04/05 共同遵循）

**分级档位**（各模块统一语义）：
| level | 语义 | LED 闪烁（亮/灭半周期） | 蜂鸣器节奏 |
|---|---|---|---|
| 0 | 正常 | 相应 LED 常亮（绿/蓝）或灭 | **静音** |
| 1 | 预警 | 慢闪：500ms/500ms（1 次/秒） | 响 250ms → 停 1750ms（2s 周期） |
| 2 | 告警 | 快闪：250ms/250ms（2 次/秒） | 响 250ms → 停 250ms |
| 3 | 紧急 | 爆闪：100ms/100ms（5 次/秒） | 响 100ms → 停 100ms |

YDS 颜色映射与点亮条件：
| YDS | 颜色 | 含义 | 点亮条件 |
|---|---|---|---|
| YDS1 | 红 | 电流告警 | **U10 与 U44 电流取最高等级**闪烁（慢闪/快闪/爆闪） |
| YDS2 | 绿 | 电流正常 | 两路电流均 level=0 时常亮 |
| YDS3 | 蓝 | 温度正常 | SHT40 level=0 时常亮 |
| YDS4 | 橙 | 温度告警 | SHT40 分级闪烁（同三频） |
| YDS5 | 青 | USB 通讯 | USB-UART 通道最近有活动（≤5s 内收到行） |

**蜂鸣器**：
- 接 U10 INA226 ALERT 引脚（开漏低电平响）；由固件节奏引擎驱动（100ms 粒度写 U10 ALERT_LIMIT 通断），**等级 = max(两路电流, SHT40 温度) 三者最高档**（温度报警同样驱动蜂鸣器）。
- 节奏与同档 LED 闪烁同步（见上表）。
- **静音**：GUI/串口 `set_buzzer {on:1}` 可让蜂鸣器完全静音，**LED 闪烁不受影响**；当前静音态经 get_switch 应答 `buzzer_mute` 回读（0 响/1 静音）。
- 判级纯按实时数值（芯片 AFF 锁存不强制等级），恢复正常后 ≤3 个采样周期蜂鸣自动停。

## 3. 周期上报 `type: module.data`（1s 周期）

```json
"data": {
  "sw": 16382,          // int  13 开关位图（bit0=B1..bit8=B9, bit10=B11..bit13=B14；bit9 恒 0；1=ON）
  "yds": 31,            // int  5 位点亮状态（bit0=YDS1 …bit4=YDS5；1=亮，与硬件低电平点亮已归一化）
  "eth_link": 1,        // int 0/1  以太网链路（netif up）
  "usb_active": 1       // int 0/1  USB 串口 ≤5s 内收到行
}
```

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `sw` | int | ✓ | 0..16383；bit9(B10)恒 0 |
| `yds` | int | ✓ | 0..31；bit_i=1 表示 YDS(i+1) 亮（LED 实际状态，含闪烁过程） |
| `eth_link` | int | ✓ | 与 yds bit3 同源（冗余便于后端直接判读） |
| `usb_active` | int | ✓ | 与 yds bit4 同源 |

> `yds` 是 1s 上报时刻 LED 的实时位图，闪烁档的 LED 在 yds 中会随上报相位出现 0/1；后端如需"告警与否"直接用各模块帧的 `level` 字段，勿用 yds 单点判级。

## 4. 事件 `type: module.event`（可选）

```json
"data": {"event":"switch_changed","level":"info","detail":"sw 0x3FFE"}
```
| event | 触发 |
|---|---|
| `switch_changed` | 开关位图相对上次上报变化（可选实现，防高频刷屏可省略） |

## 5. 示例完整帧

```json
{"ver":"1.0","type":"module.data","module":"pcal6524","seq":30,"ts_ms":3300000,"data":{"sw":16382,"yds":17,"eth_link":1,"usb_active":0}}
```

## 6. 后端解析要点
- 用 `module=="pcal6524"` 分派；校验用 `python_ref/module_pcal6524.py`。
- `sw` 与 GUI 开关通道 `get_switch` 的 `sw` 同源；`yds` 为逻辑点亮位，后端只管 1=亮。
- 声光联动是否处于静音态：订阅 GUI 通道 get_switch 的 `buzzer_mute`，或经下行 `set_buzzer` 控制（协议见《STM32 与服务器设备协议 v1.0》§4）。
