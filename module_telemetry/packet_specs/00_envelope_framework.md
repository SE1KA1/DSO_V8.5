# 统一信封协议（envelope framework）— 所有模块公共

本文定义设备经以太网 TCP 上传服务器的**统一数据包格式**。各模块 `packet_specs/0X_*.md` 只描述其 `data` 字段内容（schema），其余一律遵守本文件。

## 1. 传输层

- **传输**：TCP 长连接（设备为客户端主动连接服务器）。
- **端点默认值**：`192.168.100.100:5005`（与现有遥测任务 dso_fmc_telemetry 一致，可配置）。
- **帧**：每帧一行 JSON 文本，行尾 `\r\n`（CRLF）。后端按行读取即得一帧。
- **可靠性**：依赖 TCP；无应用层重传。设备侧断线重连；`seq` 帮助后端检出丢帧。
- **推送节奏**：见各模块规格（周期上报/事件即时/变化即时）。建议周期帧与现有 device.status(1s) 通道分开独立的模块周期（各模块定义）。

## 2. 帧类型（envelope）

| type | 方向 | 语义 |
|---|---|---|
| `device.status` | 上行 | 设备 1s 心跳/全量状态（既有，不变） |
| `telemetry.io` / `telemetry.mode` / `event.key` | 上行 | 既有 FMC16 遥测/事件（不变） |
| **`module.data`** | 上行 | 某模块周期/变化上报（本包新增） |
| **`module.event`** | 上行 | 某模块事件（本包新增） |
| `command.execute` | 下行 | 服务器→设备命令请求（既有信封，见协议文档 §3） |
| `command.result` | 上行 | 命令应答（既有） |

> `module.data`/`module.event` 与既有帧**独立追加、互不冲突**；后端可用 `type` 分派。

## 3. 公共字段

```json
{
  "ver": "1.0",
  "type": "module.data",
  "module": "atecc608b",
  "seq": 42,
  "ts_ms": 1234567,
  "data": { ... 模块 schema ... }
}
```

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `ver` | string | ✓ | 协议版本，固定 `"1.0"` |
| `type` | string | ✓ | 上表帧类型之一 |
| `module` | string | ✓（module.* 时） | 模块标识：**小写字母+数字，≤24 字符**（`atecc608b`、`sht40`、`ina226_u10`、`ina226_u44`、`pcal6524`、`bgm_adc_dac`、`bgm_dds`、`watchdog`） |
| `seq` | int | ✓ | 设备侧自增序号（同一连接单调递增，检出丢帧） |
| `ts_ms` | int | ✓ | 设备运行时间戳（ms，FreeRTOS tick 或等效单调时钟）；与墙上时钟的换算由后端按 `device.status` 校准 |
| `data` | object | ✓ | 模块负载，schema 见各模块规格 |

## 4. 公共约定

- 字段命名 **snake_case**。
- 数值的单位/缩放**必须**在模块规格中写明（如温度用十分之一 ℃ 的 int）。
- **未知字段忽略**（向后兼容扩展）；**缺必填字段或类型错误的帧按坏帧丢弃**并记日志/告警计数。
- 布尔尽量用 0/1 int（C 侧友好）；枚举用字符串并列出合法值。
- 长字符串（如证书/指纹/序列号）用小写 hex 字符串。

## 5. 示例（完整帧，一行一帧）

```json
{"ver":"1.0","type":"module.data","module":"atecc608b","seq":42,"ts_ms":1234567,"data":{"present":1,"comm_ok":1,"chip_info":{"serial4":"a2c4f16d","part":"608B","rev":0,"lock_config":1,"lock_data":1},"login_state":"LOGGED_OUT","fail_cnt":0}}
{"ver":"1.0","type":"module.event","module":"atecc608b","seq":43,"ts_ms":1238700,"data":{"event":"login","level":"info","detail":"login ok"}}
```

## 6. 与既有协议的关系

- 底层信封沿用《STM32 与服务器设备协议 v1.0》（Markdown/STM32 与服务器设备协议.md）风格：`type` 分派、行帧、`seq`、`command.execute/result` 双向命令沿用原定义。
- 本框架只**新增两种上行帧类型**并约定 `module`/`data` 载荷组织方式；既有帧保持字节级兼容。
- 各模块下行命令（`command.execute` 的 `type` 参数）在各模块规格中列骨架；**加密登录握手等需与后端协同的流程标注 TBD**，联调时定稿。
