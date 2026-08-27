# STM32H743 ↔ XO2-4000HC FMC16 数据包协议与复用规范

## 1. 设计目标与非目标

将 XO2 作为 STM32 FMC 总线上的 16-bit 异步流式网关，统一承载 XO2 本地状态、XC7A `PO/PIO` 状态、扩展模块数据以及 STM32 下行命令。设计必须满足：

1. 完整包原子入队，不允许读到半包。
2. 不使用 `FMC_NWAIT`，依靠固定宽时序和信用流控。
3. 支持失步重同步、头校验、整包校验、命令去重和大数据分片。
4. 保留当前 96-bit SPI 状态快照作为兼容 payload，同时提供字段清晰的原生 MODE/IO 快照，外部读取方不需要理解 RTL 内部实现。
5. 对外数据只包含业务值、有效性、新鲜度、版本/能力和协议级结果；不传 RTL 内部寄存器、原始采样节点、HAL 返回值、LCD/UI 缓存、测试码、递增模式或专用 debug 消息。
6. 协议服务状态与业务数据分包；读取方只关心业务时，可只解析 `DEVICE_INFO`、`STATUS_SNAPSHOT`、`IO_SNAPSHOT`、`MODE_SNAPSHOT` 和 `EVENT`。

## 2. 传输层定义

| 项目 | 定义 |
|---|---|
| FMC 模式 | 异步 NOR/SRAM |
| 数据宽度 | 16-bit |
| 地址/数据复用 | 禁用 |
| 片选 | `FMC_NE1` |
| 建议基地址 | `0x60000000` |
| XO2 地址空间 | 无地址线；所有半字读写使用同一地址 |
| 读方向 | XO2 → STM32，`NE1=0, NOE=0, NWE=1` |
| 写方向 | STM32 → XO2，`NE1=0, NWE=0, NOE=1` |
| `FMC_CLK` | 不参与协议，XO2 端作普通输入且不加上下拉 |
| `FMC_NWAIT` | STM32 禁用，XO2 端高阻 |
| 空闲读取 | 返回 `0xFFFF`，TX 队列指针不前进 |
| 最大 payload | 256 words / 512 bytes |
| 默认未完成命令数 | 1，可通过能力包协商 |
| STM32 MPU | Device、non-cacheable、non-bufferable |

XO2 只能在有效读周期驱动总线：

```verilog
wire read_active = !fmc_ne1_n && !fmc_noe_n && fmc_nwe_n;
assign fmc_db = read_active ? tx_word : 16'hzzzz;
```

`NOE=0` 与 `NWE=0` 同时出现定义为非法周期。XO2 必须立即高阻并增加 `BUS_ILLEGAL_CYCLE` 计数，不得为任何一个方向提供宽泛 fallback。

## 3. 字序与数值规则

1. 物理传输单元为 16-bit word，STM32 每次从同一 FMC 地址读/写一个 `uint16_t`。
2. 32-bit 字段先传高 16 位，再传低 16 位。
3. CRC 内每个 word 按 bit15 到 bit0 输入。
4. STM32 解析器必须显式移位组合数值，不得直接把 FMC 缓冲区强转为 C packed struct。
5. 序号与事务号归各自的 Source 节点所有，溢出后按无符号 32-bit 自然回绕。

## 4. 统一包头

| Word | 字段 | 定义 |
|---:|---|---|
| W0 | `SOF0` | 固定 `0xA55A` |
| W1 | `SOF1` | 固定 `0x5AA5` |
| W2 | `VERSION_HEADER` | Major[15:12]、Minor[11:8]、HeaderWords[7:0] |
| W3 | `TOTAL_WORDS` | 整包 word 数，包含 Header、Payload 和 CRC32 |
| W4 | `TYPE_FLAGS` | MessageType[15:8]、Flags[7:0] |
| W5 | `SOURCE_DEST` | Source[15:8]、Destination[7:0] |
| W6 | `CHANNEL` | 逻辑通道 |
| W7 | `PAYLOAD_WORDS` | Payload word 数 |
| W8 | `SEQUENCE_H` | 32-bit 传输序号高 16 位 |
| W9 | `SEQUENCE_L` | 传输序号低 16 位 |
| W10 | `TRANSACTION_H` | 32-bit 事务号高 16 位 |
| W11 | `TRANSACTION_L` | 事务号低 16 位 |
| W12 | `HEADER_CRC16` | W2～W11 的 CRC16 |
| W13… | `PAYLOAD` | 0～256 words |
| 最后 2 words | `PACKET_CRC32` | CRC32 高 16 位先传 |

公共头固定：

```text
VERSION_HEADER = 0x100D
TOTAL_WORDS    = 13 + PAYLOAD_WORDS + 2
```

最短合法包为 15 words。接收端必须同时校验 `HeaderWords=13`、`PayloadWords<=256` 以及 `TotalWords=15+PayloadWords`。

## 5. CRC 定义

Header CRC 使用 CRC-16/CCITT-FALSE：

```text
Polynomial = 0x1021
Init       = 0xFFFF
RefIn      = false
RefOut     = false
XorOut     = 0x0000
Range      = W2..W11
```

整包 CRC 使用 CRC-32/MPEG-2：

```text
Polynomial = 0x04C11DB7
Init       = 0xFFFFFFFF
RefIn      = false
RefOut     = false
XorOut     = 0x00000000
Range      = W2..最后一个 payload word，包含 W12 Header CRC
```

W0/W1 用于重同步，不进入 CRC。Header CRC 通过前不得信任包长和事务号；Packet CRC 通过前不得执行 payload。

ACK golden vector（XO2→STM32，sequence=1，transaction=1；确认 `COMMAND/GET_CAPABILITIES`）：

```text
A55A 5AA5 100D 0013 0300 0100 0000 0004 0000 0001 0000 0001 0141
0030 0001 0001 0000 F1E8 829E
```

其中 Header CRC=`0x0141`，Packet CRC32=`0xF1E8829E`。双方实现必须首先通过该向量，再生成各 mode 与 IO_CONTROL 的共享向量。

## 6. 节点、通道、类型与标志

### 6.1 Source / Destination

| ID | 节点 |
|---:|---|
| `0x00` | STM32H743 |
| `0x01` | XO2 管理/网关 |
| `0x02` | XC7A 状态源 |
| `0x03` | 扩展模块聚合器 |
| `0x10～0xEF` | 具体扩展模块实例 |
| `0xFE` | Broadcast |
| `0xFF` | 非法/保留 |

### 6.2 Channel

| Channel | 用途 |
|---:|---|
| `0x0000` | Link management |
| `0x0001` | 周期状态 |
| `0x0002` | 控制命令 |
| `0x0003` | 异步事件 |
| `0x0004` | 分片/批量数据 |

### 6.3 Message Type

| Type | 名称 | 方向 |
|---:|---|---|
| `0x01` | `LINK_HELLO` | XO2 → STM32 |
| `0x02` | `LINK_STATUS` | XO2 → STM32 |
| `0x03` | `ACK` | 双向 |
| `0x04` | `NACK` | 双向 |
| `0x05` | `DEVICE_INFO` | 双向 |
| `0x10` | `STATUS_SNAPSHOT` | XO2 → STM32 |
| `0x11` | `IO_SNAPSHOT` | XO2 → STM32 |
| `0x12` | `MODE_SNAPSHOT` | XO2 → STM32 |
| `0x13` | `HOST_STATUS` | STM32 → XO2 |
| `0x20` | `EVENT` | XO2 → STM32 |
| `0x30` | `COMMAND` | STM32 → XO2 |
| `0x31` | `RESPONSE` | XO2 → STM32 |
| `0x40` | `DATA_BLOCK` | 双向 |
| `0x7F` | `RESET_NOTICE` | XO2 → STM32 |

### 6.4 Flags

| Bit | 名称 | 含义 |
|---:|---|---|
| 0 | `ACK_REQUIRED` | 长命令需先返回 ACK |
| 1 | `RESPONSE` | 当前包是事务响应 |
| 2 | `ERROR` | Payload 包含错误状态 |
| 3 | `FRAGMENTED` | Payload 包含分片前缀 |
| 4 | `FIRST_FRAGMENT` | 第一片 |
| 5 | `LAST_FRAGMENT` | 最后一片 |
| 6 | `URGENT` | 高优先级事件 |
| 7 | `RETRY` | 同一事务的重试包 |

## 7. `LINK_HELLO` 与能力协商

XO2 每次进入 FMC16 用户模式后先入队一个 `LINK_HELLO`：

| Payload | 定义 |
|---:|---|
| P0 | 设备类型 `0x584F`（`XO`） |
| P1 | 协议版本 `0x0100` |
| P2-P3 | 32-bit `build_id` |
| P4-P5 | 32-bit capabilities |
| P6 | `max_payload_words`，上限 `0x0100` |
| P7 | `max_outstanding`，默认 `1` |
| P8 | RX 队列总 word 数 |
| P9 | TX 队列总 word 数 |
| P10 | `reset_reason` |

Capabilities 位定义：

```text
bit0  asynchronous read
bit1  asynchronous write
bit2  legacy 96-bit snapshot
bit3  fragmentation
bit4  credit flow control
bit5  event publication
bit6  duplicate transaction cache
bit7  atomic snapshot
bit8  IO_CONTROL / IO_SNAPSHOT schema 1.0
bit9  PI mirror from STM32 FMCU_PI controller
bit10 PIO per-bit output enable
bit11 local mode/key engine compatible with current work_top
bit12 remote mode ownership
bit13 selectable-clock state reporting
bit14..31 reserved
```

只允许上报已实现且已验证的能力。不支持的 Destination 或 Opcode 必须显式返回 `NACK/UNSUPPORTED`，不得静默成功。

### 7.1 `DEVICE_INFO`

`DEVICE_INFO` 是给通用读取方使用的自描述信息。STM32 和 XO2 在建立链路后各发送一次，设备重启或构建版本变化后重发；固定 16-word payload：

| P | 字段 | 定义 |
|---:|---|---|
| 0 | `device_info_schema` | `0x0100` |
| 1 | `device_type` | STM32=`0x4837`（H7）；XO2=`0x584F`（XO） |
| 2 | `protocol_version` | `0x0100` |
| 3 | `role_personality` | role[15:8]：master=`0`、slave=`1`；personality[7:0]：LOCAL=`0`、FMC16=`1` |
| 4-5 | `build_id` | 32-bit，构建唯一标识 |
| 6 | `mode_schema_version` | `0x0100` |
| 7 | `io_schema_version` | `0x0100` |
| 8 | `experiment_schema_version` | `0x0100` |
| 9 | `board_revision` | 高8位主版本、低8位次版本；未知填 `0x0000` |
| 10-11 | `supported_mode_mask` | bit0..12 对应 M0..MC |
| 12-13 | `supported_experiment_class_mask` | 32-bit 实验类别能力位；未定义位必须为0 |
| 14 | `public_feature_flags` | bit0状态快照、bit1 IO快照、bit2模式快照、bit3事件、bit4命令 |
| 15 | `reserved` | 必须为0 |

读取方先检查 Major 版本，再按各 schema 解码；不得依据 `build_id` 猜测字段布局。

## 8. 状态快照包

`STATUS_SNAPSHOT` 固定使用 10-word payload：

| Payload | 定义 |
|---:|---|
| P0 | 状态 Schema，固定 `0x0100` |
| P1 | 有效性标志 |
| P2 | 现有 SPI 96-bit 帧 Word0 |
| P3 | 现有 SPI 96-bit 帧 Word1 |
| P4 | 现有 SPI 96-bit 帧 Word2 |
| P5 | `PO[31:16]` |
| P6 | `PO[15:0]` |
| P7 | `PIO[15:0]` |
| P8-P9 | 32-bit 采样时间，ms |

P1 定义：

```text
bit0 = XO2 本地字段有效
bit1 = PO 通过连续两拍一致性采样
bit2 = PIO 通过连续两拍一致性采样
bit3..15 = 0
```

当前 96-bit payload 继续使用：

```text
P2 = {legacy_crc6[5:4], freq_full_select, clk_sel[4:0],
      legacy_crc6[3:0], mode[3:0]}
P3 = {control_byte_one, control_byte_two}
P4 = {control_byte_three, control_byte_four}
P5/P6 = PO[31:0]
P7 = PIO[15:0]
```

标准状态包共 `13 + 10 + 2 = 25 words = 50 bytes`。状态包是非事务包，`transaction_id=0`。

### 8.1 PI / PO / PIO 方向与权限

| 总线 | 物理方向 | 协议权限 | 实际执行位置 |
|---|---|---|---|
| `PI[15:0]` | STM32/MCP23017 → XC7A | 可写、可读镜像 | STM32写 `FMCU_PI`；XO2只保存镜像 |
| `PO[31:0]` | XC7A → XO2 | **只读** | XO2连续两拍一致后上报 |
| `PIO[15:0]` | XC7A ↔ XO2 | 逐位输入/输出 | XO2按输出使能三态控制 |

对 `PO` 的写请求必须返回 `NACK/READ_ONLY`。PI 不能由已经占作 `FMC_DB[15:0]` 的 X2_IO 引脚直接驱动；STM32先通过现有 MCP23017/FMCU_PI 路径写入，再将已应用值和 generation 放入 `IO_CONTROL`，供 XO2建立状态镜像。

### 8.2 `IO_CONTROL` 命令

`COMMAND opcode=0x0100` 固定为 13-word payload：

| Payload | 字段 | 定义 |
|---:|---|---|
| P0 | opcode | `0x0100` |
| P1 | schema | `0x0100` |
| P2 | options | bit0 PI有效；bit1 PIO有效 |
| P3 | timeout | ms |
| P4 | `pi_update_mask` | 1表示更新该 PI 镜像位 |
| P5 | `pi_value` | STM32已经写入 `FMCU_PI` 的值 |
| P6 | `pio_update_mask` | 1表示更新该 PIO 位方向和值 |
| P7 | `pio_output_enable` | 1=XO2驱动，0=XO2高阻/输入 |
| P8 | `pio_drive_value` | 仅输出位有效 |
| P9-P10 | `expected_generation` | `0xFFFFFFFF` 表示不比较 |
| P11-P12 | `new_generation` | STM32单调递增的32-bit值 |

执行规则：PI mask非零前 STM32必须已经完成 MCP23017 写入；PIO越权位、旧 generation 或非法 option 使整包失败，禁止部分成功；PIO输入转输出时先装值后使能，输出转输入时先撤使能。成功响应返回 PI mirror、PIO输出使能、PIO驱动值和完整32-bit generation。

### 8.3 `IO_SNAPSHOT`

固定 19-word payload，分别给 PI、PO、PIO 独立 generation，避免某一路更新时让其它数据被误判为新值：

| P | 字段 |
|---:|---|
| 0 | schema=`0x0100` |
| 1 | validity：bit0 PI requested有效；bit1 PI applied有效；bit2 PO稳定；bit3 PIO稳定；bit4 PIO驱动状态有效；bit8 PO stale；bit9 PIO stale |
| 2 | `pi_requested[15:0]`，STM32要求值 |
| 3 | `pi_applied[15:0]`，STM32确认MCP23017写成功后通知XO2保存的镜像值；XO2不具备PI物理回读能力 |
| 4-5 | `po_sample[31:0]` |
| 6 | `pio_sample[15:0]` |
| 7 | `pio_output_enable[15:0]` |
| 8 | `pio_drive_value[15:0]` |
| 9 | `pio_writable_mask[15:0]` |
| 10-11 | `pi_generation[31:0]` |
| 12-13 | `po_generation[31:0]` |
| 14-15 | `pio_generation[31:0]` |
| 16-17 | `sample_timestamp_ms[31:0]`，XO2启动后的单调时间 |
| 18 | business flags：bit0 PI同步完成；bit1 PO可供业务读取；bit2 PIO可供业务读取；其它位为0 |

当前 `work_top.v` 只有 M1 驱动 `PIO[3:0]`，其它 mode 的 `pio_drive_mask=0`。首版兼容构建必须上报 M1输出使能 `0x000F`、其它 mode `0x0000`，且远程 `pio_writable_mask=0x0000`、capability bit10=0。只有完成显式仲裁、XC7A高阻审计和仿真后，才能开放远程 PIO 写。

generation 只在对应业务值成功发布后递增。`stale` 由发布端按配置的最大样本年龄产生，不允许读取方用“值是否变化”推断新鲜度。

### 8.4 `HOST_STATUS`

STM32 用 `HOST_STATUS` 把会影响业务解释的主机侧状态通知 XO2；固定 15-word payload，不携带原始 HAL 错误码、调用栈或调试计数：

| P | 字段 | 定义 |
|---:|---|---|
| 0 | `host_status_schema` | `0x0100` |
| 1 | validity | bit0运行时间；bit1 PI；bit2 KEY LED；bit3应用上下文；bit4最后事务 |
| 2-3 | `uptime_ms[31:0]` | STM32启动后的单调时间 |
| 4 | `pi_requested[15:0]` | 上层当前请求值 |
| 5 | `pi_applied[15:0]` | MCP23017已成功应用值 |
| 6-7 | `pi_generation[31:0]` | 成功应用后递增 |
| 8 | `key_led_mask[15:0]` | 仅表示当前用户可见按键灯状态，不传驱动内部缓存 |
| 9 | `host_state_flags` | bit0 PI初始化完成；bit1 PI当前有效；bit2 KEY LED初始化完成；bit3 KEY LED当前有效 |
| 10 | `fmc_service_flags` | bit0链路就绪；bit1允许命令；bit2允许周期快照；其它位为0 |
| 11-12 | `last_applied_transaction_id[31:0]` | 最近成功产生业务副作用的事务号 |
| 13 | `application_schema_version` | `0x0100` |
| 14 | `reserved` | 必须为0 |

失败状态通过相应 validity/state 位变为0或通过事务 `RESPONSE/NACK` 返回标准错误码；不得把厂商库错误枚举直接放进公开 payload。

## 9. 命令、响应与错误

### 9.1 `COMMAND`

```text
P0    opcode
P1    command_schema_version
P2    options
P3    requested_timeout_ms
P4..  arguments
```

| Opcode | 功能 |
|---:|---|
| `0x0001` | `GET_CAPABILITIES` |
| `0x0002` | `GET_LINK_STATUS` |
| `0x0003` | `GET_STATUS_SNAPSHOT` |
| `0x0010` | `SET_MODE` |
| `0x0011` | `SET_CLK_SELECT` |
| `0x0012` | `SET_PUBLISH_PERIOD` |
| `0x0013` | `SET_CONTROL_OWNER` |
| `0x0020` | `SUBSCRIBE_EVENTS` |
| `0x0021` | `UNSUBSCRIBE_EVENTS` |
| `0x00F0` | `LINK_RESYNC` |
| `0x0100` | `IO_CONTROL` |

当前并未确认 STM32 → XO2 → XC7A 的反向业务通道；对 XC7A 或扩展模块的命令只能在 Capability 明确上报后启用。

当前兼容实现的 mode owner 固定为 `XO2_LOCAL=0`：`key_in[9]` 在 M0→MC 循环，`key_in[0]` 控制频率档。`STM32_REMOTE=1` 仅在 capability bit12=1 时允许；第一版中 `SET_MODE`、`SET_CLK_SELECT`、`SET_CONTROL_OWNER` 返回 `UNSUPPORTED`。后续开放远程 owner 时必须带 `expected_mode_generation`，显式定义本地 key9 与远程命令优先级，不采用“最后写入者获胜”。

Opcode参数固定如下（P0-P3仍是COMMAND公共前缀）：

| Opcode | P4起参数 |
|---|---|
| GET_CAPABILITIES / GET_LINK_STATUS / GET_STATUS_SNAPSHOT | 无，payload_words必须为4 |
| SET_MODE | P4=new_mode；P5-P6=expected_mode_generation |
| SET_CLK_SELECT | P4=clk_sel/full_select；P5-P6=expected_clock_generation |
| SET_PUBLISH_PERIOD | P4=Message Type；P5-P6=period_ms；P7=change_threshold |
| SET_CONTROL_OWNER | P4=new_owner；P5-P6=expected_mode_generation |
| SUBSCRIBE_EVENTS | P4-P5=event_mask；P6-P7=min_interval_ms |
| UNSUBSCRIBE_EVENTS | P4-P5=event_mask |
| LINK_RESYNC | P4=reason；P5-P6=last_good_sequence |
| IO_CONTROL | 使用 §8.2 固定13-word payload |

### 9.2 `RESPONSE`

```text
P0    original_opcode
P1    status_code
P2    detail_code
P3    result_words
P4..  result
```

| Code | 含义 |
|---:|---|
| `0x0000` | OK |
| `0x0001` | ACCEPTED |
| `0x0002` | BAD_OPCODE |
| `0x0003` | BAD_LENGTH |
| `0x0004` | BAD_ARGUMENT |
| `0x0005` | BUSY |
| `0x0006` | NO_CREDIT |
| `0x0007` | TIMEOUT |
| `0x0008` | CRC_ERROR |
| `0x0009` | UNSUPPORTED_VERSION |
| `0x000A` | INTERNAL_ERROR |
| `0x000B` | READ_ONLY |
| `0x000C` | OWNERSHIP_CONFLICT |
| `0x000D` | STALE_GENERATION |

普通命令只返回一次 `RESPONSE`。长命令设置 `ACK_REQUIRED` 后先返回 `ACK/ACCEPTED`，最后再返回 `RESPONSE`。

重试包必须使用原 `transaction_id`、原 Opcode 和原参数，并设置 `RETRY`。XO2 必须返回缓存的上一事务响应，不得重复执行有副作用的命令。

### 9.3 `ACK` / `NACK`

固定 4-word payload：P0=原 Message Type，P1=原 Opcode（非命令填0），P2=ACK时填 `ACCEPTED`、NACK时填错误码，P3=ACK预计完成ms或NACK detail。必须复制原 `transaction_id`。只有固定头和 Header CRC 已通过、事务号可信时才能对 CRC错误回 NACK，否则静默丢弃并计数。

## 9A. 与当前 XO2/STM32 mode 代码对齐

### 9A.1 `MODE_SNAPSHOT`

固定 16-word payload。它只发布消抖后的业务状态，不发布同步链中间节点或原始按键采样：

| P | 字段 | 当前代码来源 |
|---:|---|---|
| 0 | schema=`0x0100` | 固定 |
| 1 | mode[3:0]、owner[9:8] | `work_top.mode`、默认 XO2_LOCAL |
| 2 | clk_sel[4:0]、full_select bit5、PLL locked bit6、clock_output_connected bit7 | 当前时钟状态 |
| 3 | `business_key_state[9:0]` | 仅消抖后的用户按键状态 |
| 4 | `experiment_id` | 当前兼容代码取 `PO[31:24]`；未知填0 |
| 5 | `experiment_schema_version` | 当前业务解释版本=`0x0100` |
| 6 | `experiment_page_id` | 无分页填0；页面切换时使用稳定公开编号 |
| 7 | `business_validity` | bit0 mode；bit1按键；bit2实验ID；bit3页面；bit4 legacy payload；bit5时钟选择状态 |
| 8-9 | `legacy_payload[31:0]` | 四个 control byte |
| 10-11 | `mode_generation[31:0]` | mode、owner、实验上下文变化后递增 |
| 12-13 | `clock_generation[31:0]` | clk_sel/full_select变化后递增 |
| 14 | `control_context` | last_control_source[7:0]、active_personality[15:8]；本地键=`0`、STM32命令=`1` |
| 15 | flags | bit0 legacy有效；bit1本地mode有效；bit2时钟选择有效；bit3物理时钟输出有效；bit4业务数据有效 |

FMC16下 IO142 已作 DB14且 `FPGA_CLK_SEL2=OPEN`，所以可继续上报 `clk_sel` 作为选择状态，但 `clock_output_connected=0`。STM32 UI 不得把“选择24 MHz”显示成“XC7A已收到24 MHz”。

当前 mode 名称固定为：M0 RESULT DISPLAY、M1 INPUT MAPPING、M2 DECODER SELECT、M3 EVENT TRIGGER、M4 PARALLEL INPUT、M5 SCAN DISPLAY、M6 SIGNAL SHAPING、M7 MATRIX KEYS、M8 ROTARY CONTROL、M9 NAV CONTROL、MA CONTROL PANEL、MB FULL STATUS VIEW、MC REMOTE CONTROL。协议只传 mode ID，不传字符串。

### 9A.2 四个 control byte

| Mode | byte1 | byte2 | byte3 | byte4 |
|---:|---|---|---|---|
| M0 | `{num_a2,num_a1}` | `{num_c4,num_c3}` | `{num_c6,num_c5}` | `{num_c8,num_c7}` |
| M1 | `{num_a4,num_a3}` | `{num_a2,num_a1}` | `0` | `0` |
| M2 | `{num_c2,num_c1}` | `{num_c4,num_c3}` | `{num_c6,num_c5}` | `{num_c8,num_c7}` |
| M3 | `{key2,key1}` | `{key4,key3}` | `{key6,key5}` | `{key8,key7}` |
| M4 | `PO[31:24]` | `PO[23:16]` | `PIO[15:8]` | `PIO[7:0]` |
| M5 | `PO[31:24]` | `PO[23:16]` | `PO[15:8]` | `PIO[7:0]` |
| M6 | `key[8:1]` | `0` | `0` | `0` |
| M7 | `PO[31:24]` | `PO[23:16]` | `PIO[15:8]` | `PO[7:0]` |
| M8/M9 | `0` | `0` | `0` | `0` |
| MA/MB/MC | `key[8:1]` | `0` | `0` | `0` |

M3按当前 RTL 使用 raw key，其它键值使用 debounced key。完整 `PO`、`PIO` 始终另在快照中传输，不能只解析 mode-dependent control byte。

### 9A.3 `BuildFmcuPiValue()` 精确规则

| Mode | `FMCU_PI[15:0]` |
|---:|---|
| M0 | `[7:0]=byte1`；[8]=byte2[0]；[9]=byte2[4]；[10]=byte3[0]；[11]=byte3[4]；[12]=byte4[0]；[13]=byte4[4]；[15:14]=0 |
| M1 | `{byte1,byte2}` |
| M2/M3 | PI[7:0]依次取 byte1[0], byte1[4], byte2[0], byte2[4], byte3[0], byte3[4], byte4[0], byte4[4] |
| M6/MA/MB/MC | `[7:0]=byte1`；高8位0 |
| M4/M5/M7/M8/M9 | `0x0000` |

当前源码注释把 M0 称为“7-bit”，但实际赋值覆盖 PI[13:0] 共14位。协议以代码实际赋值为准；实现时应修正注释和测试名称，不得把14位截成7位。

### 9A.4 现有 96-bit SPI 帧兼容封装

`STATUS_SNAPSHOT.P2..P7` 必须逐word等价于当前12字节帧：

```text
P2 = {crc6[5:4], full_select, clk_sel[4:0], crc6[3:0], mode[3:0]}
P3 = {control_byte_one, control_byte_two}
P4 = {control_byte_three, control_byte_four}
P5 = PO[31:16]
P6 = PO[15:0]
P7 = PIO[15:0]
```

STM32首版可复用现有 `Data_Process()` 与 `BuildFmcuPiValue()` 业务映射，再逐步迁移到原生 MODE/IO snapshot，避免一次重写13个 mode。

## 9B. `EVENT` 固定格式

P0=event code，P1=event source，P2-P3=timestamp_ms，P4-P5=event_generation，P6=`event_schema_version`，P7起为事件数据。事件码固定为：MODE_CHANGED=`0x0001`、CLOCK_CHANGED=`0x0002`、KEY_CHANGED=`0x0003`、EXPERIMENT_CHANGED=`0x0004`、PI_APPLIED=`0x0005`、PO_CHANGED=`0x0010`、PIO_CHANGED=`0x0011`。事件只携带事件发生后的公开业务值；不携带边沿检测寄存器、同步器级数、内部状态机编码或调试探针。高频 PO/PIO 默认只更新快照；订阅事件必须限速并合并，不能挤占命令响应队列。

## 10. `LINK_STATUS` 与无 NWAIT 流控

`LINK_STATUS` 固定为 8-word payload，只提供维持无 NWAIT 流控所需的信息，不对外传错误计数器：

```text
P0      schema = 0x0100
P1      link_state / error_flags
P2      STM32→XO2 RX free words
P3      XO2→STM32 TX used words
P4      RX free packet slots
P5      TX pending packets
P6-P7   uptime_ms
```

`P1` 只定义公开服务状态：bit0 link_ready、bit1 rx_accepting、bit2 tx_data_available、bit3 resync_required。CRC失败、包长错误、队列溢出等通过相关事务的标准 `NACK/RESPONSE` 或 `resync_required` 表达；实现内部可保留统计寄存器，但不得放入对外数据包。

流控规则：

1. STM32 写包前必须确认 `RX free words >= TOTAL_WORDS` 且存在空包槽。
2. 一个包必须连续写入；建议使用 DMA 或短临界区。
3. RX 包内相邻 word 默认最长间隔 10 ms；超时丢弃半包并回到 SOF 搜索。
4. XO2 只有在 Header CRC 和 Packet CRC 都通过后才能执行命令。
5. TX 包必须完整入队后才对 STM32 可见；队列不得在包内断流。
6. TX 空时返回 `0xFFFF`，不前进指针。
7. TX调度优先级固定为 RESET/HELLO > RESPONSE/NACK > LINK_STATUS > EVENT > 周期快照 > DATA_BLOCK；低优先级包不得阻塞命令响应。
8. 普通命令默认100 ms超时、最多重试3次；重试必须复用同一transaction_id并置RETRY，不创建会重复副作用的新事务。

### 10.1 `RESET_NOTICE`

固定 8-word payload：P0=schema `0x0100`，P1=reset_reason，P2-P3=旧 build_id，P4-P5=新 boot_id，P6=失效的最后事务数，P7=flags。复位后 XO2 清空半包和重复事务缓存，先发 `RESET_NOTICE`，再发 `LINK_HELLO`；STM32收到后必须作废未完成事务、PI/PIO镜像和旧 credit。

### 10.2 固定长度目录

| Type | Payload words | 整包words | 整包bytes | 备注 |
|---|---:|---:|---:|---|
| LINK_HELLO | 11 | 26 | 52 | 固定 |
| LINK_STATUS | 8 | 23 | 46 | 固定 |
| ACK/NACK | 4 | 19 | 38 | 固定 |
| DEVICE_INFO | 16 | 31 | 62 | 固定，双向自描述 |
| STATUS_SNAPSHOT | 10 | 25 | 50 | 兼容当前96-bit SPI |
| IO_SNAPSHOT | 19 | 34 | 68 | 固定 |
| MODE_SNAPSHOT | 16 | 31 | 62 | 固定 |
| HOST_STATUS | 15 | 30 | 60 | 固定，STM32→XO2 |
| RESET_NOTICE | 8 | 23 | 46 | 固定 |
| COMMAND/RESPONSE/EVENT/DATA_BLOCK | 可变 | `15+payload` | `2×整包words` | 必须满足各自最小长度 |

## 11. 分片数据

`FRAGMENTED=1` 时 payload 前缀固定为：

```text
P0-P1  object_id[31:0]
P2     fragment_index       // 从 0 开始
P3     fragment_count
P4-P5  total_object_words
P6..   fragment_data
```

每个分片独立具有 Header CRC 和 Packet CRC。接收端按 `object_id` 重组，缺片通过事务响应请求重传，不得使用 NWAIT 停住总线。

`DATA_BLOCK` 在非分片时也必须带 9-word 数据前缀：P0=schema、P1=data_kind、P2=element_format、P3=element_count、P4-P5=object_id、P6-P7=word_offset、P8=data_words，P9起为数据。`data_words` 必须等于实际剩余 payload word 数；未知 data_kind/format 返回 UNSUPPORTED，不把原始块强转为结构体。

## 12. 解析状态机

双方接收器都必须实现同等的严格解析器：

```text
HUNT_SOF0
  → HUNT_SOF1
  → READ_FIXED_HEADER
  → CHECK_VERSION_LENGTH_HEADER_CRC
  → READ_PAYLOAD_AND_CRC32
  → CHECK_PACKET_CRC32
  → DISPATCH_ATOMIC_PACKET
```

失败处理：

- `SOF1` 不匹配：保留当前 word 对 `SOF0` 的滑动窗口判断。
- Header CRC 失败：不信任长度，立即回到 SOF 搜索。
- Packet CRC 失败：整包丢弃；若 Header 已合法且事务号非零，可返回 `NACK/CRC_ERROR`。
- 未知 Major 版本：拒绝。已知 Major 下的未知 Message Type：事务包返回 `BAD_OPCODE/UNSUPPORTED`，非事务包丢弃并计数。
- 任何错误不得触发业务状态更新。

## 13. 21 根 FMC 信号与复用矩阵

| FMC 信号 | XO2 端 | 串联器件 | 其它复用/连接 | FMC16 模式要求 |
|---|---:|---|---|---|
| DB0 | IO2 | BR_R12 22Ω | STM32 SPI4 MOSI/PE6；BFPS PI 开关 | SPI4 禁用；BFPS 高阻 |
| DB1 | IO3 | BR_R13 22Ω | STM32 SPI4 MISO/PE5；BFPS PI 开关 | SPI4 禁用；BFPS 高阻 |
| DB2 | IO4 | BR_R14 22Ω | STM32 SPI4 NSS/PE4；BFPS PI 开关 | SPI4 禁用；BFPS 高阻 |
| DB3 | IO5 | BR_R15 22Ω | STM32 SPI4 CLK/PE2；BFPS PI 开关 | SPI4 禁用；BFPS 高阻 |
| DB4 | IO6 | BR_R16 22Ω | STM32 X2_INT0/PE3；BFPS PI 开关 | PE3 不得驱动；BFPS 高阻 |
| DB5 | IO17 | BR_R17 22Ω | BFPS PI 开关 | BFPS 高阻 |
| DB6 | IO52 | BR_R18 22Ω | BFPS PI 开关 | BFPS 高阻 |
| DB7 | IO65 | BR_R19 22Ω | BFPS PI 开关 | BFPS 高阻 |
| DB8 | IO70 | BR_R20 22Ω | 历史 `X2_SPI_SN`，对应电阻未装；BFPS | 不启用历史功能 |
| DB9 | IO89 | BR_R21 22Ω | BFPS PI 开关 | BFPS 高阻 |
| DB10 | IO117 | BR_R22 22Ω | BFPS PI 开关 | BFPS 高阻 |
| DB11 | IO125 | BR_R23 22Ω | XO2 SDA；BFPS PI 开关 | XO2 I2C 禁用 |
| DB12 | IO126 | BR_R24 22Ω | XO2 SCL；BFPS PI 开关 | XO2 I2C 禁用 |
| DB13 | IO141 | BR_R25 22Ω | BFPS PI 开关 | BFPS 高阻 |
| DB14 | IO142 | BR_R26 22Ω | `FPGA_CLK_SEL2` → XC7A B13_L13_P；BFPS | `FPGA_CLK_SEL2` OPEN |
| DB15 | IO143 | BR_R27 22Ω | BFPS；历史 LPF 曾作时钟出口 | FMC personality 不得再约束为 `clk` |
| FMC_CLK | IO91_CLK1 | BR_R28 0Ω | STM32 PD3/LED5 | 异步模式忽略；XO2 作输入 |
| FMC_NE1 | IO50_CLK2 | BR_R29 22Ω | `FPGA_CLK_SEL1` → XC7A GCLK；STM32 PD7/SPI1_MOSI | `SEL1` OPEN；PD7 作 FMC_NE1 |
| FMC_NOE | X2_TDI | BR_R30 22Ω | XO2 JTAG TDI | FMC 运行时 JTAG 不得活动 |
| FMC_NWE | X2_TCK | BR_R31 22Ω | XO2 JTAG TCK | FMC 运行时 JTAG 不得活动 |
| FMC_NWAIT | X2_TMS | BR_R32 22Ω | JTAG TMS；PC6 经 BR_R53 0Ω 连 FPGA_INIT_B | 禁用；XO2 高阻 |

## 14. FMC16 模式硬件配置

| 项目 | FMC16 配置 | 理由 |
|---|---|---|
| `FPGA_CLK_SEL1` | OPEN / 拔掉 | IO50 已作 `FMC_NE1` |
| `FPGA_CLK_SEL2` | OPEN / 拔掉 | IO142 已作 `FMC_DB14` |
| `BFPS_nOE_JP`（安全启动） | 1-2 | `nOE=1`，四片 PI 开关高阻；首次FMC联调使用 |
| `BFPS_SEL_JP`（PI运行） | 2-3 | 选择 STM32/MCP23017 `FMCU_PI` 侧，隔离 X2_IO/FMC DB 侧 |
| `BFPS_nOE_JP`（PI运行） | 2-3 | `nOE=0`；仅在 SEL 已确认2-3后使能 PI |
| `BSW_CTRL1` | 无 FMC 专用设置 | 不参与 21 根核心桥 |
| `BSW_CTRL2` | 无 FMC 专用设置 | 不参与 21 根核心桥 |
| XO2 JTAG | 程序下载后停止操作 | TDI/TCK/TMS 已复用 |
| 共享 FMC 扩展板 | 首次启用时拔除或确保严格高阻 | 避免多器件驱动总线 |

首次总线联调保持 `BFPS_nOE_JP=1-2`，完全隔离 XC7A PI。需要运行现有 PI 业务时，必须断电确认 `BFPS_SEL_JP=2-3` 选择 FMCU_PI，再将 `BFPS_nOE_JP` 改为2-3。禁止在 SEL 选择 X2_IO 侧时使能，否则 FMC_DB 跳变会直接进入 XC7A PI。

## 15. SPI、时钟与 XC7A 边界

### 15.1 SPI4

`DB0..DB4` 与 STM32 SPI4/INT0 硬复用。FMC16 上电前必须保证 PE2/PE3/PE4/PE5/PE6 不以旧功能推挽驱动这些网络。

### 15.2 SPI1

XO2 侧 SPI1 站点 40/44/45/71 不在 FMC 21 线内，但 STM32 `PD7` 同时是 `SPI1_MOSI/AF5` 和 `FMC_NE1/AF12`。因此：

- 当前全双工 `HAL_SPI_TransmitReceive_DMA` 驱动与 FMC_NE1 互斥。
- 当前 XO2 TX-only SPI RTL 不消费 MOSI，后续可单独评估不占 PD7 的接收模式；在 HAL 与上板证据完成前，不将 SPI1 写为 FMC 并行回退通道。

### 15.3 XC7A 时钟

`FPGA_CLK_SEL1/2` 全部断开后，XO2 到 XC7A 的两条本地时钟链都被物理切断。如果 FMC16 模式仍要读取有效 XC7A `PO/PIO`，必须先确认云端桥或其它独立时钟源已为 XC7A 提供合法系统时钟。

FMC 包校验通过但 XC7A 字段不变，不能单独据此判定 FMC 故障。

## 16. XO2 构建 Personality

不在同一个顶层用宽泛运行时 MUX 抢占这些管脚。建立两个明确构建目标：

```text
LOCAL_PERSONALITY
- XO2 驱动 XC7A 时钟
- 现有 SPI/I2C/PI 课程链路
- FMC 端口不存在或全高阻

FMC16_PERSONALITY
- FMC_DB[15:0] + NE1/NOE/NWE
- 异步、无 NWAIT、无同步 FMC_CLK
- SPI4、XO2 I2C、旧 PI 端口不实例化
- CLK_SEL1/2 必须 OPEN，BFPS 必须高阻
```

同一 SITE 如果被 `clk/PI/SPI/I2C/FMC` 重复占用，构建必须失败，不得静默裁剪。

## 17. 对外读取规则与禁止字段

第三方读取程序按以下固定顺序处理即可：

1. 在 16-bit word 流中搜索 `A55A 5AA5`。
2. 校验 Major=`1`、HeaderWords=`13`、长度关系和 Header CRC16。
3. 收齐整包并校验 Packet CRC32；失败包整体丢弃，绝不使用部分 payload。
4. 按 Message Type 选择固定字段表；先检查对应 schema Major，再读取 validity。
5. 32-bit 字段使用“高16位在前”组合；generation 相同表示同一版业务数据，timestamp 用于判断样本年龄。
6. 未识别的 Minor 版本尾部字段可忽略；未知 Major、未知固定长度或保留位非0时拒绝该包。

最小业务读取集合：启动时读取双方 `DEVICE_INFO`；运行时读取 XO2 的 `MODE_SNAPSHOT` 和 `IO_SNAPSHOT`。`STATUS_SNAPSHOT` 只供旧 96-bit 解析代码迁移，新的第三方程序不应依赖其 legacy 字节布局。`LINK_STATUS`、`ACK/NACK`、`RESPONSE` 只服务链路和命令，不是实验业务数据。

对固定快照，payload 总从 `W13` 开始。例如 `MODE_SNAPSHOT` 的 `experiment_id` 位于 `W17`（W13+P4），`IO_SNAPSHOT` 的 `PO[31:16]`/`PO[15:0]` 位于 `W17/W18`（W13+P4/P5）。读取程序应先完成整包校验再按这些偏移取值，不能边收边更新业务对象。

明确禁止进入公开 payload 的数据：

- XO2/STM32 的 FSM state、FIFO 指针、同步器各级、边沿检测脉冲、原始未消抖按键、ILA/测试点；
- `rx_word0/1/2`、`payload_nibbles`、`num_a/num_c` 内部寄存器等可由公开字段推导或仅供实现使用的数据；
- LCD 控件、字符串、坐标、dirty flag、刷新率、渲染缓存；
- HAL/驱动库原始错误码、调用栈、文件行号、内部错误计数器；
- 固定测试码、递增数据、PRBS、回环专用字段和任何名为 debug/trace/probe/test 的信号。

如未来确需维护诊断，必须另定义不同 Major 或独立维护接口，不能复用保留位把调试信号塞入业务包。

## 18. 实现分层

XO2 建议模块：

```text
fmc16_phy
  → fmc16_rx_word_stream / fmc16_tx_word_stream
  → packet_header_parser + crc16
  → packet_body_buffer + crc32
  → transaction_dispatch
  → status_snapshot_builder / command_executor / event_queue
  → atomic_packet_tx_queue
```

STM32 建议分层：

```text
fmc16_bus.c
fmc16_packet.c
fmc16_crc.c
fmc16_link.c
fmc16_status.c
fmc16_command.c
fmc16_io.c
```

物理层不得解析 mode 业务；协议层不得直接操作 FMC GPIO；应用层不得在 CRC 通过前读取半包字段。

当前 `main.c::MPU_Config()` 已配置 `0x60000000`、64 KiB、non-cacheable 区域，但仍是 bufferable 且允许执行。FMC16实现时必须把该区改成 non-bufferable、execute-never，并确认该 region 没有被其它外设使用；不能因为基地址已经存在就宣称 FMC Bank1 已初始化。

## 19. 验证门禁

### 19.1 协议仿真

- 合法最短/最长包。
- 所有 Message Type 的合法和非法长度。
- SOF0/SOF1 错位、半包终止和重同步。
- Header CRC 与 Packet CRC 的单 bit 错误。
- 未知版本、未知类型、超长 payload。
- 同一 transaction 重试不重复执行。
- 分片缺失、乱序、重复和 object_id 冲突。
- 无 credit 写入、RX 超时、TX 空读。
- `NOE=NWE=0` 时 XO2 必须高阻。
- 13个 mode 的 control byte 和 `BuildFmcuPiValue()` golden vectors。
- M1仅 `PIO[3:0]` 输出、其它 mode 全高阻；远程 PIO 命令默认拒绝。
- PI mirror 必须只在 MCP23017 写成功后推进 generation。
- 对全部公开包运行字段白名单检查，禁止表中的内部/调试字段不得出现在编码器接口或 golden vector 中。
- 第三方参考解码器只依据本规范和 `DEVICE_INFO` 即可解析，不引用 XO2/STM32 源码符号。

### 19.2 工程验证

1. XO2 RTL 仿真 PASS 标记与失败汇总解析。
2. STM32 主机端解析器单元测试与 XO2 共享 golden vectors。
3. Diamond 综合/布局布线无引脚冲突，输出实际资源与时序报告。
4. Keil 构建、MPU 区域和 SDRAM 回归。
5. 上板观测 `NE1/NOE/NWE/DB[15:0]` 时序、总线换向和高阻。
6. 独立确认 XC7A 时钟源，不把 FMC 数据正确与 XC7A 业务有效混为一项结论。
