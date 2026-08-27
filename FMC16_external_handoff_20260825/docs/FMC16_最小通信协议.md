# FMC16 最小通信协议

## 总线

| 项目 | 配置 |
|---|---|
| 模式 | 异步 NOR/SRAM |
| 数据宽度 | 16 bit |
| 地址/数据复用 | 关闭 |
| 访问地址 | `0x60000000` |
| 地址线 | 不使用，所有 word 访问同一地址 |
| 读 | `NE1=0, NOE=0, NWE=1` |
| 写 | `NE1=0, NWE=0, NOE=1` |
| NWAIT | 关闭 |
| FMC_CLK | 不使用 |
| 空读 | `0xFFFF` |
| 最大 payload | 256 words |

XO2 只在有效读周期驱动数据总线。`NOE` 和 `NWE` 不得同时为低。

## 字序

- 每次访问传输一个 16-bit word。
- 32-bit 数值先传高 16 位，再传低 16 位。
- CRC 按每个 word 的 bit15 到 bit0 计算。
- 解析时逐字段组合数值，不要把缓冲区强转为 packed struct。

## 数据包

| Word | 字段 | 内容 |
|---:|---|---|
| W0 | SOF0 | `0xA55A` |
| W1 | SOF1 | `0x5AA5` |
| W2 | VERSION_HEADER | `0x100D` |
| W3 | TOTAL_WORDS | 整包 word 数 |
| W4 | TYPE_FLAGS | type[15:8]，flags[7:0] |
| W5 | SOURCE_DEST | source[15:8]，destination[7:0] |
| W6 | CHANNEL | 通道号 |
| W7 | PAYLOAD_WORDS | payload word 数 |
| W8-W9 | SEQUENCE | 高字在前 |
| W10-W11 | TRANSACTION_ID | 高字在前 |
| W12 | HEADER_CRC16 | W2-W11 的 CRC16 |
| W13... | PAYLOAD | 消息内容 |
| 最后两字 | PACKET_CRC32 | 高字在前 |

长度关系：

```text
TOTAL_WORDS = 15 + PAYLOAD_WORDS
```

合法长度为 15～271 words。收到完整数据包并校验 Packet CRC 后，才能执行命令。

## CRC

Header CRC：

```text
CRC-16/CCITT-FALSE
poly    = 0x1021
init    = 0xFFFF
refin   = false
refout  = false
xorout  = 0x0000
range   = W2-W11
```

Packet CRC：

```text
CRC-32/MPEG-2
poly    = 0x04C11DB7
init    = 0xFFFFFFFF
refin   = false
refout  = false
xorout  = 0x00000000
range   = W2 到最后一个 payload word，包含 W12
```

黄金向量：

```text
A55A 5AA5 100D 0013 0300 0100 0000 0004 0000 0001 0000 0001 0141
0030 0001 0001 0000 F1E8 829E
```

计算结果：

```text
Header CRC16 = 0x0141
Packet CRC32 = 0xF1E8829E
```

## 节点和通道

| 数值 | 含义 |
|---:|---|
| node `0x00` | STM32H743 |
| node `0x01` | XO2 |
| channel `0x0000` | 链路管理 |
| channel `0x0002` | 控制命令 |

## 消息类型

| Type | 消息 |
|---:|---|
| `0x01` | LINK_HELLO |
| `0x03` | ACK |
| `0x04` | NACK |
| `0x05` | DEVICE_INFO |
| `0x30` | COMMAND |
| `0x31` | RESPONSE |
| `0x7F` | RESET_NOTICE |

flags bit0 是 `ACK_REQUIRED`，bit7 是 `RETRY`。

## 启动包

XO2 复位后依次发送：

| 消息 | Payload | 总长 | Sequence |
|---|---:|---:|---:|
| RESET_NOTICE | 8 | 23 | 0 |
| LINK_HELLO | 11 | 26 | 1 |
| DEVICE_INFO | 16 | 31 | 2 |

LINK_HELLO 的设备类型为 `0x584F`。能力字段位于 P4-P5，高 16 位在前。

基础通信要求 bit0 和 bit1 同时为 1：

| Bit | 含义 |
|---:|---|
| 0 | 异步读 |
| 1 | 异步写 |

已知端点：

| 端点 | 能力值 |
|---|---:|
| 最小通信端 | `0x00000003` |
| 完整 XO2 FMC 端 | `0x00002BF7` |

判断时使用 `(capabilities & 0x00000003) == 0x00000003`。不要按精确值判断，否则完整端会被误拒绝。

## GET_CAPABILITIES

COMMAND 使用 type `0x30`、channel `0x0002`、source `0x00`、destination `0x01`。

payload：

| Word | 内容 |
|---:|---|
| P0 | opcode=`0x0001` |
| P1 | schema=`0x0100` |
| P2 | options=`0x0000` |
| P3 | timeout=`0x0000` |

设置 `ACK_REQUIRED` 后，XO2 先返回 ACK，再返回 RESPONSE。两个包都回显原 transaction ID。

RESPONSE 中：

```text
original opcode = 0x0001
status          = 0x0000
result words    = 6
```

六个结果字依次为：

```text
capabilities[31:16]
capabilities[15:0]
max_payload_words
max_outstanding
rx_queue_words
tx_queue_words
```

RESPONSE 的能力字应与 LINK_HELLO 一致，并且包含异步读写能力。其余能力位由上层功能决定是否使用。

未知 opcode 返回 NACK，状态为 `BAD_OPCODE=0x0002`。

重试时复用原 transaction ID、opcode 和参数，并设置 `RETRY`。一个数据包应连续写完，中间不要插入其它 FMC 访问。

本文件只定义已经跑通的最小链路。完整板的 PI、PO、PIO、快照和事件处理见 `XO2FMC完整板数据对接.md`，两部分不要混作同一验收结果。
