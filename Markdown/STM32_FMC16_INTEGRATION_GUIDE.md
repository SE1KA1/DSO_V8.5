# STM32H743 ↔ XO2-4000HC FMC16 从机对接说明

---

## 1. 系统架构

```
                    FMC 16-bit 异步（无地址线、无 NWAIT）
 STM32H743 ───────────────────────────────────────► XO2-4000HC
   (FMC 主端)      NE1 / NOE / NWE / DB[15:0]        (FMC16 从机)
      │                                                  │
      │  命令/状态：写包→从机，读包←从机                   │
      │                                                  │
      └─── MCP23017(I2C) ──► FMCU_PI ──► XC7A PI[15:0]（业务下行，不经 XO2）
```

- XO2 是**纯从机**：只响应 STM32 发起的异步读写周期；
- **每次访问 = 一个 16-bit word**，固定访问同一 FMC 地址（地址线未连接，XO2 不采样地址）；
- 写方向：`NE1` 选中 + `NWE` 低 + `DB` 数据 → XO2 采样，周期结束提交一个 word；
- 读方向：`NE1` 选中 + `NOE` 低 → XO2 驱动 `DB`（TX 队列预取），`NOE` 释放后指针推进；
- 协议包在 word 流之上：启动包、快照、命令、响应、事件全部统一帧格式。

---

## 2. 硬件连接与板级状态

### 2.1 引脚映射

**XO2 侧**（2026-08-11 定案）：

| FMC 信号 | XO2 站点 | 串阻 | 复用边界（必须知道） |
|---|---:|---|---|
| `DB[0..4]` | 2,3,4,5,6 | BR_R12..R16 22Ω | 与 STM32 SPI4/INT0 及 PI 开关 X2_IO 侧重合 |
| `DB[5..10]` | 17,52,65,70,89,117 | BR_R17..R22 22Ω | 进入 PI 开关 |
| `DB[11..12]` | 125,126 | BR_R23..R24 22Ω | 与 XO2 SDA/SCL 重合 |
| `DB[13..15]` | 141,142,143 | BR_R25..R27 22Ω | IO142 与 FPGA_CLK_SEL2 重合 |
| `FMC_CLK` | 91 | BR_R28 0Ω | STM32 PD3/LED5；异步模式不使用 |
| `FMC_NE1` | 50 | BR_R29 22Ω | STM32 侧 **PD7** |
| `FMC_NOE` | X2_TDI | BR_R30 22Ω | **XO2 JTAG TDI**——调试 XO2 时该网络被 JTAG 占用 |
| `FMC_NWE` | X2_TCK | BR_R31 22Ω | **XO2 JTAG TCK**——同上 |
| `FMC_NWAIT` | X2_TMS | BR_R32 22Ω | **禁用**：XO2 高阻，STM32 必须关闭 NWAIT |

**STM32 侧**（FMC 标准复用表，与板级 SDRAM 初始化代码一致；上板前与原理图对照一遍即可）：

| 信号 | STM32 GPIO | AF | 备注 |
|---|---|---:|---|
| FMC_NE1 | PD7 | AF12 | 已由引脚表定案确认 |
| FMC_NOE | PD4 | AF12 | 标准复用 |
| FMC_NWE | PD5 | AF12 | 标准复用 |
| FMC_NWAIT | PD6 | AF12 | 不初始化（保持非 FMC 复用） |
| FMC_D0..D3 | PD14, PD15, PD0, PD1 | AF12 | 与 SDRAM 共用 |
| FMC_D4..D12 | PE7..PE15 | AF12 | 与 SDRAM 共用 |
| FMC_D13..D15 | PD8, PD9, PD10 | AF12 | 与 SDRAM 共用 |
| FMC_A0..A15 / NBL / SDRAM 控制 | PF/PG/PH 等 | AF12 | SDRAM 专用，FMC16 不用，保持现有初始化 |

### 2.2 板级开关/跳线（两阶段）

| 状态 | FPGA_CLK_SEL1/2 | BFPS_SEL_JP | BFPS_nOE_JP | 用途 |
|---|---|---|---|---|
| 首次 FMC 联调 | OPEN | 2-3 | **1-2** | PI 开关全高阻，隔离 FMC_DB |
| FMC 已验证，开启 PI | OPEN | 2-3 | **2-3** | MCP23017/FMCU_PI → XC7A PI |
| X2_IO 侧 | — | **禁止** | — | 与 FMC_DB 冲突，永不选择 |

> ⚠️ 进入 PI 业务只允许选择 MCP23017/FMCU_PI 侧；`PO[31:0]`/`PIO[15:0]` 是独立网络，
> 但只有 XC7A 获得其它合法时钟源时才有有效业务状态。

### 2.3 与板载 SDRAM 共存（同一个 FMC 外设，不同 bank）

| 目标 | FMC bank | 基地址 | 片选 |
|---|---|---|---|
| 板载 SDRAM（现有） | SDRAM bank1 | `0xC000_0000` | FMC_SDNE0（PH5） |
| XO2 FMC16（新增） | NOR/PSRAM bank1 | `0x6000_0000` | FMC_NE1（PD7） |

数据总线共用、控制信号不同，可同时使能；**不要改动现有 SDRAM 初始化**。

---

## 3. STM32 FMC 外设配置

### 3.1 初始化要点

```c
__HAL_RCC_FMC_CLK_ENABLE();
// 数据总线已由 SDRAM 初始化配置为 AF12（PD0/1/8/9/10/14/15、PE7..PE15）。
// FMC16 追加（NE1=PD7 已确认；NOE=PD4、NWE=PD5 按标准复用）：
//   PD7 -> FMC_NE1、PD4 -> FMC_NOE、PD5 -> FMC_NWE，全部 GPIO_AF12_FMC。
```

### 3.2 NOR/PSRAM 控制器

```c
FMC_NORSRAM_InitTypeDef  nor = {0};
FMC_NORSRAM_TimingTypeDef tm  = {0};

nor.Controller         = FMC_NORSRAM_MEM_BANK1;           // NE1
nor.DataAddressMux     = FMC_DATA_ADDRESS_MUX_DISABLE;
nor.MemoryType         = FMC_MEMORY_TYPE_SRAM;
nor.MemoryDataWidth    = FMC_NORSRAM_MEM_BUS_WIDTH_16;
nor.BurstAccessMode    = FMC_BURST_ACCESS_MODE_DISABLE;
nor.WriteOperation     = FMC_WRITE_OPERATION_ENABLE;
nor.WaitSignal         = FMC_WAIT_SIGNAL_DISABLE;         // 无 NWAIT（关键）
nor.ExtendedMode       = FMC_EXTENDED_MODE_DISABLE;
nor.AsynchronousWait   = FMC_ASYNCHRONOUS_WAIT_DISABLE;
nor.WriteBurst         = FMC_WRITE_BURST_DISABLE;
HAL_SRAM_Init(&hsram, &nor, &tm);
```

### 3.3 时序合同与 HAL 换算

XO2 合同 → FMC 字段（寄存器值 = ceil(ns × FMC_CLK_MHz / 1000)）：

| XO2 合同（最小） | FMC 字段 | 推荐起点(ns) |
|---|---:|---:|
| NE1 建立到 NWE/NOE 有效 40ns | `AddressSetupTime` | 40 |
| NWE 低电平 100ns（物理窗口目标） | `AddressSetup + DataSetup` 合计 | ≥100 |
| 写数据建立到 NWE↑ 40ns（硬边界） | `DataSetupTime` | **60** |
| 写数据保持到 NWE↑ 20ns | `DataHoldTime` | 20 |
| 写周期之间高电平 60ns | `AddressHoldTime` | 60 |
| NOE 低电平 100ns | `DataSetupTime`（读） | 60 |
| 读周期之间高电平 60ns | `AddressHoldTime` | 60 |
| 读写换向空档 100ns | `BusTurnAroundDuration` | 100 |

写周期波形（时序关系）：

```text
          <------ AddressSetup 40ns ------><-- DataSetup 60ns --><--20ns-->
NE1  ______|                                                     |________
                                          <------ NWE low 100ns ------>
NWE  _______________|___________________________________________|________
DB   ================        VALID DATA（恒定）                  ==========
                                              ↑
                                 最后一次翻转必须在此前 ≥40ns
```

> **两个数字不要混**：`NWE 低 100ns` 是物理有效窗口目标（= AddressSetup+DataSetup，
> 40+60 恰好 100）；`DataSetupTime=60ns` 只是其中一个 FMC 参数，**不是把合同从 100ns 降成 60ns**。

**时钟换算示例**（FMC kernel clock = HCLK，以 CubeMX 时钟树为准；HCLK=200MHz 时）：

| HAL 字段 | 目标(ns) | 寄存器值(200MHz) |
|---|---:|---:|
| AddressSetupTime | 40 | 8 |
| AddressHoldTime | 60 | 12 |
| DataSetupTime | 60 | 12 |
| DataHoldTime | 20 | 4 |
| BusTurnAroundDuration | 100 | 20 |

> HCLK 每减半，寄存器值同步减半；最终值以实际时钟树配置为准。

### 3.4 最坏相位（对接必读）

XO2 在 NWE 低窗口内用 50MHz（20ns 周期）采样 DB，提交**窗口内最后一次采样**：

- 合同硬边界：**最终数据必须在 NWE 上升前 ≥40ns 完成最后一次翻转**；
- 40ns = 2 个采样周期，保证最后一个采样沿必然落在最终数据稳定之后；
- 若翻转距 NWE 上升 < 40ns，从机可能提交旧值；
- FMC 外设本身是同步时序，只要 `DataSetupTime ≥ 40ns` 就天然满足——**不要用手工 GPIO
  模拟"先写数据再拉低 NWE"的方式**。

### 3.5 MPU 配置（必须）

`0x6000_0000` 区域配置为 **Device（不可缓存、不可写合并）**，否则字节合并/乱序破坏事务原子性：

```c
MPU_Region_InitTypeDef r = {0};
r.Enable           = MPU_REGION_ENABLE;
r.Number           = MPU_REGION_NUMBER4;
r.BaseAddress      = 0x60000000;
r.Size             = MPU_REGION_SIZE_64MB;
r.TypeExtField     = MPU_TEX_LEVEL_0;
r.AccessPermission = MPU_REGION_FULL_ACCESS;
r.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
r.IsShareable      = MPU_ACCESS_SHAREABLE;
r.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;   // 关键
r.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;  // 关键：禁止写合并
HAL_MPU_ConfigRegion(&r);
```

DMA 搬运的包缓冲必须是普通 SRAM，不得直连 FMC 地址。

---

## 4. Word 级访问规则

```c
#define FMC16_BASE   ((volatile uint16_t *)0x60000000UL)

static inline void fmc16_write_word(uint16_t w) { *FMC16_BASE = w; }
static inline uint16_t fmc16_read_word(void)    { return *FMC16_BASE; }
```

- **所有访问用同一地址**（地址线未接）；
- 读返回 `0xFFFF` = TX 队列空（合法值，不是错误）；
- 换向空档由 `BusTurnAroundDuration` 覆盖，无需软件延时。

### 从机行为速查（对拍用）

| 从机规则 | 说明 |
|---|---|
| 读驱动 | `!NE1 && !NOE && NWE` 才驱动 DB；NE1=1 或 NOE=NWE=0 立即高阻 |
| 写采样 | `!NE1 && NOE && !NWE` 内采样；非法组合不接收 |
| 空读 | 返回 0xFFFF，TX 指针不推进 |
| 读推进 | 一个完整合法读周期结束推进一次；毛刺不推进 |
| 写提交 | 一个完整合法写周期结束提交一次；过短/采样未知 → abort 不提交 |
| 非法周期 | NOE=NWE=0：高阻 + abort 一次 + 不推进不提交 |
| 直接换向 | READ↔WRITE 不经 IDLE：abort；读不推进（重试读同 word），写仍可提交 |
| 复位 | 无脉冲、总线高阻；被中断周期不提交不推进 |

---

## 5. 协议包格式

### 5.1 统一包头（13 words + payload + CRC32）

| Word | 字段 | 说明 |
|---:|---|---|
| W0 | SOF0 | 固定 `0xA55A`（不进 CRC） |
| W1 | SOF1 | 固定 `0x5AA5`（不进 CRC） |
| W2 | VERSION_HEADER | **固定 `0x100D`**：Major[15:12]=1、Minor[11:8]=0、**HeaderWords[7:0]=13** |
| W3 | TOTAL_WORDS | 整包字数 = 13 + payload + 2（最短合法包 15） |
| W4 | TYPE_FLAGS | MessageType[15:8]；Flags[7:0] 见下表 |
| W5 | SOURCE_DEST | Source[15:8]、Destination[7:0]（节点 ID 见 §5.2） |
| W6 | CHANNEL | 0x0000 链路 / 0x0001 状态 / 0x0002 命令 / 0x0003 事件 / 0x0004 分片 |
| W7 | PAYLOAD_WORDS | payload 字数（≤256） |
| W8-W9 | SEQUENCE | 32-bit，**高 16 位先传**，Source 自增，自然回绕 |
| W10-W11 | TRANSACTION | 32-bit 事务号，**高 16 位先传** |
| W12 | HEADER_CRC16 | W2..W11 的 CRC-16/CCITT-FALSE |
| W13.. | PAYLOAD | 0~256 words |
| 最后 2 | PACKET_CRC32 | W2..最后 payload word 的 CRC-32/MPEG-2，高 16 位先传 |

接收端必须同时校验：`HeaderWords=13`、`PayloadWords≤256`、`TotalWords=15+PayloadWords`。

**Flags[7:0] 完整定义**：

| Bit | 名称 | 含义 |
|---:|---|---|
| 0 | ACK_REQUIRED | 长命令需先返回 ACK |
| 1 | RESPONSE | 当前包是事务响应 |
| 2 | ERROR | Payload 包含错误状态 |
| 3 | FRAGMENTED | Payload 包含分片前缀 |
| 4 | FIRST_FRAGMENT | 第一片 |
| 5 | LAST_FRAGMENT | 最后一片 |
| 6 | URGENT | 高优先级事件 |
| 7 | **RETRY** | **同一事务的重试包（flags=0x80）** |

> ⚠️ 重试标志是 **bit7（0x80）**，不是 bit1（bit1 是 RESPONSE）。

### 5.2 节点 ID（Source / Destination）

| ID | 节点 |
|---:|---|
| `0x00` | STM32H743 |
| `0x01` | XO2 管理/网关 |
| `0x02` | XC7A 状态源 |
| `0xFE` | Broadcast |
| `0xFF` | 非法/保留 |

```text
STM32 → XO2：SOURCE_DEST = 0x0001   （Source=0x00、Dest=0x01）
XO2 → STM32：SOURCE_DEST = 0x0100   （Source=0x01、Dest=0x00）
```

### 5.3 CRC 与 golden 向量

```text
CRC-16/CCITT-FALSE：poly 0x1021、init 0xFFFF、不反转、xorout 0；输入 W2..W11
CRC-32/MPEG-2：    poly 0x04C11DB7、init 0xFFFFFFFF、不反转、xorout 0；输入 W2..最后 payload word
每个 word 按 bit15→bit0 顺序输入。
```

**ACK golden vector**（XO2→STM32，sequence=1、transaction=1；
场景：STM32 发送 `COMMAND/GET_CAPABILITIES` 后，XO2 返回 `ACK/ACCEPTED`）：

```text
A55A 5AA5 100D 0013 0300 0100 0000 0004 0000 0001 0000 0001 0141
0030 0001 0001 0000 F1E8 829E
```

- W4=`0x0300` → Message Type=`0x03 ACK`（注意：这是 ACK，不是最终的 RESPONSE 0x31）；
- Header CRC16=`0x0141`，Packet CRC32=`0xF1E8829E`；
- 双方实现必须首先通过该向量，再生成各 mode 与 IO_CONTROL 的共享向量。

---

## 6. 启动与建链

```text
STM32                                    XO2
  |                                       |
  | <---------- RESET_NOTICE(0x7F) -------|   上电/复位，seq 从 0 开始
  | <----------- LINK_HELLO(0x01) --------|   含 capabilities、队列容量、reset_reason
  | <----------- DEVICE_INFO(0x05) -------|   设备自描述
  |                                       |
  | ---- GET_CAPABILITIES(0x30) -------->|
  | <----------- ACK(0x03) ---------------|   ACK_REQUIRED 流程（两段式）
  | <----------- RESPONSE(0x31) ----------|   最终应答
  |                                       |
  | ---- SET_PUBLISH_PERIOD ------------>|
  | <----------- RESPONSE OK -------------|
  | ---- SUBSCRIBE_EVENTS -------------->|
  | <----------- RESPONSE OK -------------|
  |                                       |
  | <------- SNAPSHOT(0x10/11/12) --------|   周期发布
  | <------- EVENT(0x20) -----------------|   事件发布
```

STM32 侧要点：
1. 读到 LINK_HELLO 视为建链完成；RESET_NOTICE/LINK_HELLO/DEVICE_INFO 三个启动包依序到达；
2. 发 GET_CAPABILITIES 核对能力位（**注意 ACK 与 RESPONSE 是两个不同包**，都收）；
3. 按 capabilities 决定可用功能；SET_PUBLISH_PERIOD + SUBSCRIBE_EVENTS 开启周期/事件。

**Capabilities（XO2 上报 `0x2BF7`）逐位核对表**：

| bit | 能力 | 上报 |
|---:|---|---|
| 0 | 异步读 | ✅ |
| 1 | 异步写 | ✅ |
| 2 | legacy 96-bit 快照 | ✅ |
| 3 | 分片 | ❌ |
| 4 | 信用流控 | ✅ |
| 5 | 事件发布 | ✅ |
| 6 | 事务去重缓存 | ✅ |
| 7 | 原子快照 | ✅ |
| 8 | IO_CONTROL/IO_SNAPSHOT 1.0 | ✅ |
| 9 | PI 镜像（FMCU_PI） | ✅ |
| 10 | **PIO 逐位输出使能（远程可写）** | ❌ |
| 11 | 本地模式/按键引擎 | ✅ |
| 12 | 远程模式所有权 | ❌ |
| 13 | 可选时钟状态上报 | ✅ |
| 14..31 | 保留 | 0 |

> ⚠️ **bit10=0**：M1 模式 XO2 本地驱动 PIO[3:0] **不等于** 对 STM32 开放 PIO 远程写——
> 远程 `pio_writable_mask=0`，IO_CONTROL 的 PIO mask≠0 一律 READ_ONLY。不要尝试远程 PIO 写。

---

## 7. 命令集（STM32 → XO2）

| Opcode | 命令 | payload | 要点 |
|---:|---|---|---|
| 0x0001 | GET_CAPABILITIES | 4 | 响应含能力位 |
| 0x0002 | GET_LINK_STATUS | 4 | 响应 7 字：link flags/队列余量/uptime |
| 0x0003 | GET_STATUS_SNAPSHOT | 4 | 响应 10 字（含 legacy 96-bit 帧） |
| 0x0010 | SET_MODE | 7 | **首版 UNSUPPORTED**（模式只能 XO2 本地按键循环） |
| 0x0011 | SET_CLK_SELECT | 7 | **首版 UNSUPPORTED** |
| 0x0012 | SET_PUBLISH_PERIOD | 8 | P4=type(0x02/0x10/0x11/0x12)；**P5-P6=period_ms 32-bit（P5 高 16 位先传）**；P7=threshold≠0→BAD_ARGUMENT |
| 0x0013 | SET_CONTROL_OWNER | 7 | **首版 UNSUPPORTED** |
| 0x0020 | SUBSCRIBE_EVENTS | 8 | **P4-P5=event_mask 32-bit（P4 高 16 位先传，one-hot）**；**P6-P7=min_interval_ms 32-bit（P6 高 16 位先传）** |
| 0x0021 | UNSUBSCRIBE_EVENTS | 6 | **P4-P5=mask，按位清除**（不是全清） |
| 0x00F0 | LINK_RESYNC | 7 | P4=reason；P5-P6=last_good_sequence |
| 0x0100 | IO_CONTROL | 13 | 见 §8.3 |

### 响应与错误码（RESPONSE：P0=原 opcode、P1=status、P2=detail、P3=result_words）

| Code | 含义 | Code | 含义 |
|---:|---|---:|
| 0x0000 | OK | 0x0007 | TIMEOUT |
| 0x0001 | ACCEPTED | 0x0008 | CRC_ERROR |
| 0x0002 | BAD_OPCODE | 0x0009 | UNSUPPORTED_VERSION |
| 0x0003 | BAD_LENGTH | 0x000A | INTERNAL_ERROR |
| 0x0004 | BAD_ARGUMENT | 0x000B | READ_ONLY |
| 0x0005 | BUSY | 0x000C | OWNERSHIP_CONFLICT |
| 0x0006 | NO_CREDIT | 0x000D | STALE_GENERATION |

### 事务/重试/去重规则

- 普通命令一次 RESPONSE；设 ACK_REQUIRED（Flags bit0）的长命令：先 ACK/ACCEPTED，后 RESPONSE；
- **重试必须原 tid + 原 opcode + 原参数 + Flags bit7 RETRY（0x80）**——XO2 去重缓存直接重放上次响应，不重复执行；
  **状态（2026-08-13）**：容量裁剪决策 C1 未采用，去重缓存保持 4 项（不裁功能优化计划）
- 收到 NO_CREDIT 或 LINK_STATUS 显示 RX 满：**停止发包**，等信用恢复；
- 多次失败后发 LINK_RESYNC。

---

## 8. 状态、事件与三总线

### 8.1 三总线方向（协议 §8.1）

| 总线 | 方向 | 权限 | 路径 |
|---|---|---|---|
| PI[15:0] | STM32/MCP23017 → XC7A | 可写、可读镜像 | STM32 写 MCP23017(FMCU_PI) → XC7A；XO2 只存镜像，不经 FMC DB |
| PO[31:0] | XC7A → XO2 | **只读** | XO2 两拍一致后上报；写请求 → READ_ONLY |
| PIO[15:0] | XC7A ↔ XO2 | 逐位输入/输出 | XO2 按输出使能三态；**M1 模式 XO2 独占低 4 位** |

### 8.2 HOST_STATUS（STM32→XO2，15 words，channel=0x0001，tid=0）

| P | 字段 | 要点 |
|---|---:|---|
| 0 | schema | `0x0100` |
| 1 | validity | bit0 uptime / bit1 PI / bit2 KEY LED / bit3 应用上下文 / bit4 最后事务；**bit15:5 必须 0** |
| 2-3 | uptime_ms | 高 16 位先传 |
| 4 | pi_requested | 上层请求值 |
| 5 | pi_applied | MCP23017 已应用值 |
| 6-7 | pi_generation | 应用后递增 |
| 8 | key_led_mask | 按键灯状态 |
| 9 | host_state_flags | bit0..3 定义；**bit15:4 必须 0** |
| 10 | fmc_service_flags | bit0..2 定义；**bit15:3 必须 0** |
| 11-12 | last_applied_transaction_id | 高 16 位先传 |
| 13 | application_schema_version | `0x0100` |
| 14 | reserved | **必须 0** |

> XO2 按 validity 逐位门控镜像：bit1=0 时 PI 字段不更新（毒值不污染 generation）。

### 8.3 IO_CONTROL（opcode=0x0100，13 words）

P0=opcode、P1=schema=0x0100、P2=options（bit0 PI/bit1 PIO）、P3=timeout、
P4=pi_update_mask、P5=pi_value、P6=pio_update_mask、P7=pio_output_enable、
P8=pio_drive_value、P9-P10=expected_generation（0xFFFFFFFF=不比较）、P11-P12=new_generation。

规则：PI mask 非零前 **必须已完成 MCP23017 写入**；越权位/旧 generation/非法 option →
整包失败（禁止部分成功）；成功响应返回本地实际 PI mirror、PIO OE/驱动值与 generation。
**首版 PIO 远程写（mask≠0）→ READ_ONLY**（capability bit10=0，见 §6）。

### 8.4 事件（EVENT，0x20，channel=0x0003）

P0=code、P1=source=0x0001（XO2）、P2-P3=timestamp_ms（高 16 位先传）、P4-P5=generation、
P6=schema=0x0100、P7..=数据。

| 事件 | code | mask bit（one-hot） | 数据 |
|---|---:|---:|---|
| MODE_CHANGED | 0x0001 | bit0 | {0,owner=0,mode} |
| CLOCK_CHANGED | 0x0002 | bit1 | {0,0,pll=0,conn=0,full,clk_sel} |
| KEY_CHANGED | 0x0003 | **bit2** | key（消抖后） |
| EXPERIMENT_CHANGED | 0x0004 | bit3 | experiment_id |
| PI_APPLIED | 0x0005 | bit4 | {pi_applied, gen_hi, gen_lo} |
| PO_CHANGED | 0x0010 | **bit15** | {po_hi, po_lo} |
| PIO_CHANGED | 0x0011 | **bit16** | pio_coherent |

> ⚠️ **掩码 = one-hot 位，不是 `mask & code`**：订阅 KEY_CHANGED 必须置 bit2=`0x0004`；
> 按 code 值 0x0003 设置会同时误订阅 MODE(bit0)+CLOCK(bit1)。
> 冷却语义：min_interval 内的事件合并为最近一个、到期补发一次——不要假设每个变化都有独立事件。

### 8.5 M1 模式特殊行为

- `mode==1`：XO2 独占 PIO[3:0]，输出按键 5..8 的翻转计数（num_c5..c8[0]），
  pio_drive_mask=0x000F、pio_drive_value 同步回显；
- 其它模式 PIO 全高阻（观测 XC7A）；
- 高频 PO/PIO 变化默认只更新快照，不产生事件风暴。

---

## 9. 无 NWAIT 流控与信用

- XO2 不拉 NWAIT；STM32 **必须关闭 NWAIT 检测**；
- 信用来自 LINK_STATUS：P0=schema=0x0100、P1=link_state/error_flags、**P2=RX 空闲 word 数**、**P3=TX 占用 word 数**、**P4=RX 空闲包槽**、**P5=TX 待发包数**、P6-P7=uptime_ms（高 16 位先传）——注意 P1 是链路状态标志，不是信用计数，勿与 P2 混淆；
- 发包前检查 `RX 空闲 word 数 ≥ 包长`，否则等下一周期 LINK_STATUS；
- 无信用硬写 → 从机溢出保护 → resync_required → STM32 发 LINK_RESYNC 恢复。

---

## 10. 推荐驱动结构

```text
fmc16_hal.c    FMC + MPU 初始化、word 读写原语
fmc16_link.c   包打包/解析：CRC16/CRC32、seq/tid、重试与去重记录
fmc16_rx.c     读循环：SOF 搜索、整包 CRC、按类型分派
fmc16_tx.c     写队列：信用检查、逐 word 写、ACK_REQUIRED 两段流程
fmc16_cmd.c    命令构造：GET_CAPABILITIES/period/subscribe/IO_CONTROL/HOST_STATUS
fmc16_evt.c    事件/快照回调 → UI/业务层
```

关键设计：

1. **读循环**：主循环轮询——读 1 word，遇 `0xA55A` 收包（读满 TOTAL_WORDS、校验 CRC32）；
   空读（0xFFFF）继续，不视为错误；
2. **写循环**：整包缓冲 → 信用检查 → 逐 word 写；
3. **seq/tid**：seq 每包 +1；事务号每新命令 +1，重试复用原 tid + RETRY(0x80)；
4. **HOST_STATUS**：PI 写入 MCP23017 成功后发（validity.bit1=1），同一时刻仅一个在途；
5. **分派**：0x20 事件→回调；0x10/0x11/0x12 快照→按 schema 解析；0x31 响应→匹配 tid；
   0x02 更新信用；0x01/0x05/0x7F → 建链状态机。

