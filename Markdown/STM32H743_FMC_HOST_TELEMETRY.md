# STM32H743 FMC 主机遥测协议（读 XO2 → 以太网推送）

> 版本 0.1（草稿）。对齐 dsosim protocol v0.9（JSON、CRLF 定界）。
> 实现：`User/dso/dso_fmc_telemetry.h/c`；配置见 `User/dso/fmc16/fmc16_conf.h`。

## 1. 架构

```
XO2 FPGA ──FMC16(0x60000000)──> STM32H743 ──TCP 客户端──> 服务器(PC)
             (快照/事件包)        (FMC16_Service_Task   (默认 192.168.100.100:5005)
                                  解析+缓存)            断线 5s 退避重连
                                    │
                                    └─ dso_fmc_telemetry 任务：500ms 周期推快照帧；
                                       事件到达即时推事件帧
```

- STM32 为 **TCP 客户端**，主动连接服务器；服务器无需回包（单向遥测）。
- 数据源为 FMC16_Service_Task 已解析的 XO2 快照（`fmc16_get_io_snapshot()` 等）；
  快照未收到时对应字段为 0，`link/host` 字段始终有效。

## 2. 周期快照帧（每 500ms）

```json
{"ver":"0.9","seq":N,"ts":N,"src":"DSO","fpga":{
  "link":{"state":"UP"},
  "io":{"pi_requested":N,"pi_applied":N,"po":N,"pio":N,
        "validity":N,"pi_gen":N,"po_gen":N,"pio_gen":N},
  "mode":{"mode_id":N,"keys":N,"experiment_id":N,"clk_sel":N},
  "host":{"uptime_ms":N,"packets_rx":N,"packets_tx":N,"fmc_state":N}}}
```

| 字段 | 类型 | 来源（Fmc16IOSnapshot / Fmc16ModeSnapshot） |
|---|---|---|
| `link.state` | "UP"/"DOWN" | `g_fmc16.link.state == FMC16_LINK_UP` |
| `io.pi_requested` | u16 | `pi_requested`（STM32 最近要求的 PI 值） |
| `io.pi_applied` | u16 | `pi_applied`（已确认镜像） |
| `io.po` | u32 | `po_sample`（XC7A 输出 PO[31:0]，只读） |
| `io.pio` | u16 | `pio_sample`（当前 PIO[15:0] 采样） |
| `io.validity` | u16 | `validity`（bit1=PI 一致, bit2=PO 一致, bit3=PIO 一致） |
| `io.pi_gen/po_gen/pio_gen` | u32 | 对应 generation |
| `mode.mode_id` | u8 | `mode`（M0~MC） |
| `mode.keys` | u16 | `key_state`（消抖后按键 [9:0]） |
| `mode.experiment_id` | u8 | `experiment_id` |
| `mode.clk_sel` | u8 | `clk_sel` |
| `host.uptime_ms` | u32 | `HAL_GetTick()` |
| `host.packets_rx/tx` | u32 | `g_fmc16.packets_rx/tx` |
| `host.fmc_state` | i32 | `g_fmc16.sys_state`（FMC16SystemState） |

## 3. 事件帧（即时）

```json
{"ver":"0.9","type":"fpga_event","seq":N,"ts":N,
 "event":{"code":"0xNNNN","ts":N,"data":[w0,w1,...]}}
```

- `code`：事件码（PI_APPLIED / PO_CHANGED / PIO_CHANGED 等，FMC16_EVENT_MASK_*）
- `data`：事件数据 words（0..8 个）
- 由 `FMC16_Service_Task` 收到 XO2 EVENT 包时经 `DSO_FmcTelemetry_OnEvent()` 推送

## 4. 传输细节

- TCP：服务器 IP `FMC_TEL_SERVER_IP0..3`（默认 192.168.100.100）、端口 `FMC_TEL_SERVER_PORT`（5005）
- 帧以 `\r\n` 定界，一行一 JSON；服务器用 `json.loads(line)` 即可解析
- 断线 5s 退避重连；重连后 `seq` 不重置（单调递增）
- 周期 `FMC_TEL_PERIOD_MS`（默认 500ms）

## 5. 配置（fmc16_conf.h）

```c
#define FMC_TEL_SERVER_IP0  192  /* 服务器 IP */
#define FMC_TEL_SERVER_IP1  168
#define FMC_TEL_SERVER_IP2  100
#define FMC_TEL_SERVER_IP3  100
#define FMC_TEL_SERVER_PORT 5005U
#define FMC_TEL_PERIOD_MS   500U
```

## 6. 验证方法

PC 端收帧脚本（Python）：

```python
import socket, json
s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("0.0.0.0", 5005)); s.listen(1)
conn, _ = s.accept()
buf = b""
while True:
    buf += conn.recv(4096)
    while b"\n" in buf:
        line, buf = buf.split(b"\n", 1)
        line = line.strip()
        if not line: continue
        obj = json.loads(line)          # 解析失败会抛异常 → 帧格式问题
        print(obj.get("type", "snapshot"), obj.get("fpga", {}).get("link"))
```

预期：每 500ms 收到一条 snapshot 帧；拨动物理开关/PO 变化时收到 `fpga_event` 帧。

## 7. 后续扩展（待 XO2 位流支持）

- `SET_PUBLISH_PERIOD` / `SUBSCRIBE_EVENTS` 主动配置 XO2 周期上报与事件订阅
- `GET_STATUS_SNAPSHOT` 等查询命令（经 `fmc16_command_send`）
- 服务器下行控制命令（读 FMC 数据之外的写操作需等完整板接口开放）
