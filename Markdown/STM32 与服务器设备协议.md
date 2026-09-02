# STM32 与服务器设备协议 v1.0

> 定义 STM32（DSO 板卡）与后端服务器之间的设备消息协议。
> 传输：TCP 5005，CRLF 定界，一行一 JSON。STM32 主动连接服务器（客户端），单连接双向。

## 1. 传输与连接

| 项 | 值 |
|---|---|
| 传输 | TCP，端口 5005 |
| 定界 | CRLF，一行一 JSON |
| 角色 | STM32 = 客户端（主动连服务器）；服务器 = 监听端 |
| 双向 | STM32 推 telemetry/event/result；服务器在**同一连接**下发 command |
| 断线 | STM32 5s 退避重连；重连后 `bootId` 不变、`seq` 继续递增 |
| 心跳 | `device.status` 每 1s 一次；服务器 3s 未收到则判离线 |

## 2. 通用信封（所有消息）

```json
{
  "protocolVersion": "1.0",
  "messageType": "telemetry.io | telemetry.mode | event.key | device.status | command.execute | command.result",
  "deviceId": "board-001",
  "sessionId": "...",
  "bootId": 42,
  "seq": 1024,
  "timestampMs": 1787625600123,
  "commandId": "uuid-if-command",
  "payload": {}
}
```

| 字段 | 说明 |
|---|---|
| `protocolVersion` | 协议版本 `"1.0"` |
| `messageType` | 消息类型（见 §3） |
| `deviceId` | 板卡编号（配置，默认 `board-001`） |
| `sessionId` | 实验会话编号（服务器下发，会话级） |
| `bootId` | 本次启动编号（每次启动 +1） |
| `seq` | 本次启动内递增的设备消息序号（单调） |
| `timestampMs` | 设备时间戳（ms） |
| `commandId` | 仅命令相关消息使用（`command.execute` / `command.result`） |
| `payload` | 消息具体数据 |

`seq` 规则：所有 STM32 主动发送的消息共用一个递增序号（周期帧、事件、result 都计）。`commandId` 由服务器生成，STM32 原样回填。

## 3. 消息类型

### 3.1 `device.status`（设备状态，STM32 → 服务器）

上线立即发送一次，此后每 1s 心跳。服务器 3s 未收到 → 判离线（拒绝新命令）。

```json
{"protocolVersion":"1.0","messageType":"device.status","deviceId":"board-001",
 "sessionId":"...","bootId":42,"seq":1,"timestampMs":1787625600000,
 "payload":{"online":true,"buildId":"v0.9.1","fmcReady":true,
            "bootId":42,"uptimeMs":12345,"lastError":""}}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `online` | bool | 设备在线 |
| `buildId` | str | 固件版本 |
| `fmcReady` | bool | FMC16 链路是否就绪（`g_fmc16.link.state==FMC16_LINK_UP`） |
| `bootId` | u32 | 本次启动编号（与信封一致） |
| `uptimeMs` | u32 | 运行时长 |
| `lastError` | str | 最近错误（空 = 无） |

### 3.2 `telemetry.io`（PI/PO/PIO 监测，STM32 → 服务器）

默认 10 Hz 周期发送；数据变化时立即发送。`validity` 标志各字段是否有效。

```json
{"protocolVersion":"1.0","messageType":"telemetry.io","deviceId":"board-001",
 "sessionId":"...","bootId":42,"seq":2,"timestampMs":1787625600100,
 "payload":{"pi":5,"po":305441742,"pio":15,
            "piGeneration":7,"poGeneration":8,"pioGeneration":9,
            "validity":14,"timestampMs":1787625600100}}
```

| 字段 | 类型 | 来源（Fmc16IOSnapshot） |
|---|---|---|
| `pi` | u16 | `pi_applied`（已确认镜像） |
| `po` | u32 | `po_sample`（XC7A 输出，只读） |
| `pio` | u16 | `pio_sample` |
| `piGeneration/poGeneration/pioGeneration` | u32 | 对应 generation |
| `validity` | u16 | bit1=PI 一致, bit2=PO 一致, bit3=PIO 一致 |
| `timestampMs` | u32 | XO2 采样时间戳 |

### 3.3 `telemetry.mode`（模式/按键/时钟，STM32 → 服务器）

状态变化立即发送；重连时发送全量。

```json
{"protocolVersion":"1.0","messageType":"telemetry.mode","deviceId":"board-001",
 "sessionId":"...","bootId":42,"seq":3,"timestampMs":1787625600100,
 "payload":{"mode":2,"experimentId":1,"clockProfile":"PLL_50M","hz":50000000,
            "keyState":0,"generation":3}}
```

| 字段 | 类型 | 来源（Fmc16ModeSnapshot） |
|---|---|---|
| `mode` | u8 | `mode`（M0~MC） |
| `experimentId` | u8 | `experiment_id` |
| `clockProfile` | str | 时钟档位名（本地映射 `clk_sel`） |
| `hz` | u32 | 时钟频率（本地映射） |
| `keyState` | u16 | `key_state`（消抖后按键 [9:0]） |
| `generation` | u32 | `mode_generation` |

### 3.4 `event.key`（按键变化事件，STM32 → 服务器）

```json
{"protocolVersion":"1.0","messageType":"event.key","deviceId":"board-001",
 "sessionId":"...","bootId":42,"seq":4,"timestampMs":1787625600123,
 "payload":{"keyState":4,"changedMask":4,"pressMask":4,
            "source":"web","generation":4}}
```

| 字段 | 说明 |
|---|---|
| `keyState` | 当前按键状态 |
| `changedMask` | 本次发生变化的按键位 |
| `pressMask` | 本次按下的按键位 |
| `source` | `web`（网页/服务器）或 `physical`（实体按键） |
| `generation` | 状态版本号 |

### 3.5 `command.execute`（服务器 → STM32）

```json
{"protocolVersion":"1.0","messageType":"command.execute","deviceId":"board-001",
 "sessionId":"...","bootId":42,"seq":0,"timestampMs":1787625600500,
 "commandId":"uuid-command-001",
 "payload":{"type":"set_sw","args":{"sw":5,"on":1},"deadlineMs":5000}}
```

| 字段 | 说明 |
|---|---|
| `commandId` | 服务器生成的唯一 ID；**相同 ID 不重复执行**，返回第一次结果 |
| `type` | 命令类型（见 §4） |
| `args` | 命令参数（对象） |
| `deadlineMs` | 命令失效时限（ms），超时返回 `TIMEOUT` |

### 3.6 `command.result`（STM32 → 服务器）

只确认命令执行；PO/PIO 结果仍由 `telemetry.io` 确认。

```json
{"protocolVersion":"1.0","messageType":"command.result","deviceId":"board-001",
 "sessionId":"...","bootId":42,"seq":5,"timestampMs":1787625600510,
 "commandId":"uuid-command-001",
 "payload":{"status":"OK","detail":"","appliedGeneration":7,"fmcTransactionId":3}}
```

| 字段 | 说明 |
|---|---|
| `status` | `OK` / `BAD_REQUEST` / `NOT_SUPPORTED` / `BUSY` / `TIMEOUT` / `STALE_GENERATION` / `OFFLINE` |
| `detail` | 补充说明 |
| `appliedGeneration` | 执行后状态版本号（若适用） |
| `fmcTransactionId` | 对应 FMC 请求编号（若适用） |

## 4. 命令集（command.execute `type`）

| type | args | 行为 | 对应实现 |
|---|---|---|---|
| `get_status` | `{}` | 返回全量状态（device.status + telemetry.io/mode） | 立即回 result + 推送快照 |
| `set_sw` | `{sw, on}` | 开关控制（sw=0..8,10..13；on=0/1） | `DSO_Switch_Set` |
| `set_mode` | `{mode}` | 远程切模式 | FMC16 完整板（**当前位流未开放 → NOT_SUPPORTED**） |
| `set_clk_select` | `{profile}` | 远程切时钟 | 同上（未开放 → NOT_SUPPORTED） |
| `pi_control` | `{mask, value, expectedGen, newGen}` | 提交 PI 已应用镜像 | FMC16 `IO_CONTROL`（完整板） |
| `reboot` | `{}` | 软件复位 | `NVIC_SystemReset()`（需 CONFIG/RECOVERY 态） |

## 5. 状态机（连接/租约）

```
OFFLINE ──connect──> HANDSHAKE ──device.status──> ONLINE
    ▲                    │ 超时                    │
    └─────3s 无心跳──────┘                        │
    ▲                                             │
    └──────LEASE_EXPIRED 或 断开 后拒绝新命令────────┘
```

- **ONLINE**：接受命令。`command.execute` 到达后校验 `sessionId`、`deviceId`、`deadlineMs`。
- **OFFLINE/LEASE_EXPIRED**：拒绝新命令，`command.result` 回 `OFFLINE`。
- 命令幂等：`commandId` 去重——相同 ID 返回第一次执行的结果（STM32 侧维护最近 N 个 ID 缓存）。

## 6. 错误码

| status | 场景 |
|---|---|
| `OK` | 执行成功 |
| `BAD_REQUEST` | 参数非法（sw 越界、on≠0/1、缺字段） |
| `NOT_SUPPORTED` | 命令/能力未开放（set_mode/set_clk_select 当前位流不支持） |
| `BUSY` | FMC 总线忙 / 命令队列满 |
| `TIMEOUT` | 超过 `deadlineMs` 未完成 |
| `STALE_GENERATION` | PI 写入的 expectedGen 过期（需重读快照后重试） |
| `OFFLINE` | 设备离线/租约过期，拒绝控制 |

## 7. 与现有实现映射

| 现有模块 | 状态 |
|---|---|
| `dso_fmc_telemetry.c` | ✅ 已对齐：客户端任务推 `device.status`(1s) + `telemetry.io`/`telemetry.mode`(500ms) + `event.key`(FMC16 事件)；同一连接收 `command.execute` → 回 `command.result` |
| `dso_switch_cmd.c` | ✅ 已暴露 `DSO_Switch_Cmd_JsonGetStr/Int` 供协议解析；`set_sw` 经 `command.execute` 执行（串口/旧 TCP 通道保留） |
| `fmc16_integration.c` | ✅ 事件经 `DSO_FmcTelemetry_OnEvent` 推送；`fmcReady`/快照为 `telemetry.*` 数据源 |
| 心跳/租约 | ✅ `device.status` 1s 心跳已实现；离线判定/租约拒绝由服务器侧执行（本协议定义） |

## 8. 待定 / 后续

- `serverSeq` 由后端生成的语义（`telemetry.io`）——建议改为设备自增 `seq` 统一，后端按需记录。
- `set_mode`/`set_clk_select` 等完整板命令待 XO2 位流开放后启用。
- `clockProfile`/`hz` 的本地映射表待定（clk_sel → 名称/频率）。
