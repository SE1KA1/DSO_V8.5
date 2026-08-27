# XO2 FMC 完整板数据对接

## 使用范围

完整板代码位于：

```text
include/fmc16_full_board.h
src/fmc16_full_board.c
```

它负责公开数据包的构建和解析，不包含 XO2 RTL、位流或板级业务实现。接入前先运行 `fmc16_minimal_smoke_run_with_capabilities()`。返回 0 且能力字包含 `0x00000003`，才继续处理完整板消息。

完整板当前能力字为 `0x00002BF7`。不要按该值做精确匹配，应按所用功能检查对应能力位。

## 接收流程

每次先调用通用收包函数：

```c
uint16_t packet[FMC16_MAX_PACKET_WORDS];
size_t words;

fmc16_result_t result = fmc16_capture_packet(
    fmc16_stm32h743_read_word,
    NULL,
    packet,
    FMC16_MAX_PACKET_WORDS,
    500000UL,
    &words);
```

`result != FMC16_OK` 时不要读取 payload。校验通过后按消息类型分派：

```c
fmc16_io_snapshot_t io;
fmc16_mode_snapshot_t mode;
fmc16_event_t event;
fmc16_response_t response;

switch (fmc16_packet_type(packet)) {
case FMC16_TYPE_IO_SNAPSHOT:
    result = fmc16_parse_io_snapshot(packet, words, &io);
    break;
case FMC16_TYPE_MODE_SNAPSHOT:
    result = fmc16_parse_mode_snapshot(packet, words, &mode);
    break;
case FMC16_TYPE_EVENT:
    result = fmc16_parse_event(packet, words, &event);
    break;
case FMC16_TYPE_RESPONSE:
    result = fmc16_parse_response(packet, words, &response);
    break;
default:
    break;
}
```

解析函数会检查 CRC、包长、方向、通道、schema、保留位和结果字数。不要绕过解析函数直接强转结构体。

## 启动包

XO2 复位后依次发送：

```text
RESET_NOTICE -> LINK_HELLO -> DEVICE_INFO
```

分别使用：

```c
fmc16_parse_reset_notice(...);
fmc16_parse_link_hello(...);
fmc16_parse_device_info(...);
```

收到 RESET_NOTICE 后，作废未完成事务和旧的 PI/PIO 镜像。LINK_HELLO 与 GET_CAPABILITIES RESPONSE 的能力字应一致。

## 读取 PI、PO、PIO

`IO_SNAPSHOT` 使用 `fmc16_io_snapshot_t`：

```c
fmc16_io_snapshot_t io;

if (fmc16_parse_io_snapshot(packet, words, &io) == FMC16_OK) {
    if ((io.validity & (1U << 1)) != 0U) {
        uint16_t pi_applied = io.pi_applied;
    }
    if ((io.validity & (1U << 2)) != 0U &&
        (io.business_flags & (1U << 1)) != 0U) {
        uint32_t po = io.po;
    }
    if ((io.validity & (1U << 3)) != 0U &&
        (io.business_flags & (1U << 2)) != 0U) {
        uint16_t pio = io.pio;
    }
}
```

各字段含义：

| 字段 | 用法 |
|---|---|
| `pi_requested` | STM32最近要求的 PI 值 |
| `pi_applied` | STM32确认 MCP23017 写成功后通知 XO2 保存的镜像 |
| `po` | XC7A 输出的 `PO[31:0]`，只读 |
| `pio` | 当前 `PIO[15:0]` 采样值 |
| `pio_output_enable` | XO2 当前驱动的 PIO 位 |
| `pio_drive_value` | XO2 对输出位驱动的值 |
| `pio_writable_mask` | 远程可写位；当前必须为 0 |
| `*_generation` | 对应数据成功发布后的版本号 |

`STATUS_SNAPSHOT` 可用于兼容原 96-bit SPI 状态。新代码优先使用 `IO_SNAPSHOT` 和 `MODE_SNAPSHOT`。

## 写入 PI 镜像

PI 的实际输出由 STM32 通过 MCP23017/FMCU_PI 完成。顺序不能颠倒：

1. STM32 写 MCP23017。
2. 确认写操作成功。
3. 增加 PI generation。
4. 构建 `IO_CONTROL`，把已应用值通知 XO2。
5. 连续写完整数据包。
6. 接收 ACK 和 RESPONSE。
7. 解析返回的 PI 镜像与 generation。

构包示例：

```c
size_t words;
static uint32_t sequence = 1U;
static uint32_t next_transaction = 1U;
uint32_t transaction_id = next_transaction++;

fmc16_result_t result = fmc16_build_io_control_pi(
    packet,
    FMC16_MAX_PACKET_WORDS,
    sequence++,
    transaction_id,
    FMC16_FLAG_ACK_REQUIRED,
    50U,
    pi_update_mask,
    pi_applied_value,
    expected_generation,
    new_generation,
    &words);

if (result == FMC16_OK) {
    result = fmc16_write_packet(
        fmc16_stm32h743_write_word, NULL, packet, words);
}
```

响应使用 `fmc16_parse_io_control_response()`。状态为 `STALE_GENERATION` 时，先读取新的 IO_SNAPSHOT，再决定是否重试。重试必须复用原 transaction ID，并设置 `FMC16_FLAG_RETRY`。

该接口只允许 PI 更新，构出的 PIO mask、output enable 和 drive value 都是 0。远程 PIO 写没有开放。

## PO 和 PIO

PO 只能读取。协议没有 PO 写入命令。

PIO 当前只允许读取状态和本地驱动信息：

- M1 时 `pio_output_enable` 可以为 `0x000F`；
- 其它模式通常为 `0x0000`；
- `pio_writable_mask` 必须为 `0x0000`；
- 接收方不得自行构造远程 PIO 写请求。

## 模式与业务状态

`fmc16_parse_mode_snapshot()` 返回：

- M0 至 MC 的 mode ID；
- 消抖后的按键状态；
- experiment ID；
- 时钟选择状态；
- legacy control payload；
- mode 和 clock generation。

完整板只上报模式编号，不传显示名称。当前模式由 XO2 本地按键控制，远程 SET_MODE、SET_CLK_SELECT 和 SET_CONTROL_OWNER 没有开放。

## 周期快照和事件

允许设置周期的消息类型：

```text
LINK_STATUS
STATUS_SNAPSHOT
IO_SNAPSHOT
MODE_SNAPSHOT
```

使用 `fmc16_build_set_publish_period()`。当前 `threshold` 必须为 0。

事件订阅使用：

```c
uint32_t mask = FMC16_EVENT_MASK_PI_APPLIED |
                FMC16_EVENT_MASK_PO_CHANGED |
                FMC16_EVENT_MASK_PIO_CHANGED;

fmc16_build_subscribe_events(
    packet, FMC16_MAX_PACKET_WORDS,
    sequence++, next_transaction++,
    FMC16_FLAG_ACK_REQUIRED,
    mask, 100U, &words);
```

100 ms 是相邻事件的最小间隔。冷却期内只保留最近一次事件。高频 PO/PIO 建议读周期快照，不要全部改成事件。

## 主机状态

STM32 用 `fmc16_build_host_status()` 上报运行时间、PI 请求值、PI 已应用值、按键灯状态、FMC 服务状态和最近完成的事务号。

`validity` 控制哪些字段有效。未使用的字段保持 0，不能放 HAL 错误码、指针、调用栈或调试计数。

## 当前支持的构包接口

| 接口 | 完整板行为 |
|---|---|
| `fmc16_build_get_link_status` | 返回链路状态 |
| `fmc16_build_get_status_snapshot` | 返回兼容状态快照 |
| `fmc16_build_set_publish_period` | 设置周期上报 |
| `fmc16_build_subscribe_events` | 订阅事件 |
| `fmc16_build_unsubscribe_events` | 取消事件 |
| `fmc16_build_link_resync` | 请求链路重新同步 |
| `fmc16_build_io_control_pi` | 提交 PI 已应用镜像 |
| `fmc16_build_host_status` | 上报 STM32 状态 |

未提供 SET_MODE、SET_CLK_SELECT、SET_CONTROL_OWNER、PIO 写和 DATA_BLOCK 构包接口，因为完整板当前会拒绝这些操作。

查询命令的 RESPONSE 使用对应解析函数：

| 查询 | 解析函数 |
|---|---|
| GET_CAPABILITIES | `fmc16_parse_capability_response` |
| GET_LINK_STATUS | `fmc16_parse_link_status_response` |
| GET_STATUS_SNAPSHOT | `fmc16_parse_status_snapshot_response` |
| IO_CONTROL | `fmc16_parse_io_control_response` |

其它 RESPONSE 使用 `fmc16_parse_response()`，先检查 `status`，再读取 `result_words`。ACK 和 NACK 使用 `fmc16_parse_ack()`。

## 分阶段验收

第一阶段只跑最小通信：

```text
RESET_NOTICE
LINK_HELLO
DEVICE_INFO
GET_CAPABILITIES
ACK
RESPONSE
NACK/BAD_OPCODE
```

第二阶段才验证完整板数据：

```text
LINK_STATUS
STATUS_SNAPSHOT
IO_SNAPSHOT
MODE_SNAPSHOT
IO_CONTROL PI
HOST_STATUS
EVENT subscribe/unsubscribe
LINK_RESYNC
```

PC 上的 `test_full_board.c` 通过只说明构包和解析与当前协议一致。完整板还要使用时序合格的位流，在实板上逐项完成第二阶段测试。
