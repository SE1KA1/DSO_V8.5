# ATECC608B-SSHDA-T 随机数发生器返回固定值 —— 排查报告与复现请求

| 项目 | 内容 |
|---|---|
| 报告日期 | 2026-10-20 |
| 器件型号 | ATECC608B-SSHDA-T（订购型号，订购渠道见供货单据） |
| 器件 7-bit 地址 | `0x60` |
| 器件身份（Info 读回） | `RevNum = 00 00 60 03`，配置区内容见附录 A |
| 主机 | STM32H743 + 软件（GPIO 位翻转）I²C |
| 报告性质 | **单项技术现象报告**，请求判定与复现，不作来源推断 |

---

> ## ⚠️ 最终结论（2026-10-20 追加，本文档已结案）
>
> 本报告发出的**唯一主张**（“`Random` 返回固定值 ⇒ 器件异常”）**已撤回**。
>
> 第三方复核（`ATECC608B_TRNG随机源异常_分析和解决方案.md`）指出并被本机实测确认：
> **`FF FF 00 00 …` 是 ATECC608B 在“配置区未锁定（出厂测试态）”下的数据手册规定
> 测试图案**，锁定配置区后才输出真随机数。本机读回
> `LockValue@0x56 = 0x55`、`LockConfig@0x57 = 0x55` ⇒ 确实处于未锁定态。
>
> **权威结论见 → [ATECC608B_结论_配置区未锁定.md](ATECC608B_结论_配置区未锁定.md)**。
>
> 本文档以下内容保留作为**工程取证记录**（通信/CRC/时序的验证过程仍然有效、
> 仍可作为复现步骤与自检向量），但**所有“缺陷”结论作废**。

---

## 0. 一页摘要

**现象**：向该器件发送 `Random`（`0x1B`）命令并连读两次，每次读满 32 字节数据，
**两次应答的 34 个字节（count + 32 数据 + 2 CRC）逐字节完全相同**；
且返回的数据本身是**固定重复图案**：前 8 字节为

```
FF FF 00 00 FF FF 00 00
```

即 32 字节 = 4 字节图案重复 8 次。

**前置条件已闭环**（§2 给出逐项证据）：

- 命令帧的 CRC 已用 Microchip CryptoAuthLib 官方 `atCRC()` 的**同一算法**计算，
  并经多命令对照测试在**硬件上 5/5 被接受**（响应状态 `0x00`，不是 `0xFF`）；
- 器件**认可并执行**该命令：应答 `count = 0x23`（1 + 32 + 2，正是 Random 的应答长度）；
- `Info`（`0x30`）能正常返回合法 `RevNum = 00 00 60 03` 与配置区内容，证明同一总线、
  同一时序、同一地址下**读写链路是通的**；
- 两次读取各在**独立唤醒、独立命令**下完成，且读缓冲在每次读取前已清零。

**请求**：

1. 请确认：**正品 ATECC608B 的 `Random` 连续两次调用，是否必然返回不同的 32 字节？**
   （我方理解数据手册规定 `Random` 使用片内 TRNG，每次输出新值。）
2. 若确认必然不同，请用**正品/同批次**器件按 §6 复测，并给出判定与处置意见
   （参考件对比 / RMA / 批次失效分析）。
3. 若我方对 `Random` 语义或 Mode 的理解有误，请直接指出——这正是本报告希望得到的答案。

**本报告的主张范围**：**只主张"`Random` 返回固定值"这一项**。
此前沟通中关于 CRC 约定、数据区访问、`GenKey` 的主张已**撤回或降级**，
原因见 **§7 更正声明**——请以本报告为准。

---

## 1. 被测对象与环境

| 项目 | 实测/配置 |
|---|---|
| 器件 | ATECC608B-SSHDA-T，空白（TrustCUSTOM）型，未预置密钥 |
| I²C 地址 | 7-bit `0x60`（写地址字节 `0xC0`，读 `0xC1`）；该总线扫描确认存在 |
| 总线 | 软件（GPIO 位翻转）I²C，`PB7 = SDA`、`PB8 = SCL`，开漏 + 上拉 |
| 速率 | 半周期 **20 µs**（≈25 kHz，远低于器件 1 MHz 上限）；历史复测覆盖 5–500 µs 各档，现象一致 |
| 唤醒 | SCL 保持高时把 SDA 拉低 **100 µs**（规范要求 ≥ `tWLO` 60 µs），释放后等 **2 ms**（规范标准值 1.5 ms） |
| 应答读取 | 命令写入后立即读 34 字节；失败则每 1 ms 重试，最多 20 次 |
| 同总线其它器件 | 地址扫描在 `0x01–0x7F` 共找到 9 个应答地址（含本器件），其余 8 个为板上传感器/外设，均工作正常 ⇒ 总线电气与时序本身无问题 |
| 供电 | 板载 3.3 V，与 MCU 同域 |

---

## 2. 通信正确性证据（为什么这不是"通信没通就下结论"）

这一节的目的：排除"器件根本没收到/没执行命令"的所有常见原因。

### 2.1 CRC 约定：已用官方算法校准，并在硬件上被接受

我方使用的 CRC 参数（与 Microchip CryptoAuthLib `atCRC()` **逐帧等值**）：

| 参数 | 值 |
|---|---|
| 多项式 | `0x8005`（非反射形式）/ `0xA001`（反射形式），取决于写法 |
| 初值 | `0x0000` |
| 输入位序 | **每个字节先处理最低位（LSB first）** |
| 最终异或 | 无 |
| 校验值 | `CRC("123456789") = 0xBCDD`（与官方 `atCRC()` 一致） |
| 覆盖范围 | `Count + 全部命令数据字节`（**不含** I²C 的 word address `0x03`） |
| 发送顺序 | 低字节先发 |

官方库实测向量（直接调用 `cryptoauthlib` 导出的 `atCRC()` 得到）：

```
atCRC(04 11)             = 0x4333
atCRC(04 FF)             = 0x4201
atCRC(07 30 00 00 00)    = 0x5D03
```

硬件对照测试（5 条不同命令 × 4 种 CRC 变体，除末尾 2 字节外其余字节完全一致）：

| 命令 | 帧长 | 我方标准 CRC | 器件是否接受 |
|---|---|---|---|
| `Info` `0x30` | 8 B | `03 5D` | ✅ 接受 |
| `Random` `0x1B` | 8 B | `24 CD` | ✅ 接受 |
| `Read` 配置区 32 B `0x02 P1=0x80` | 8 B | `09 AD` | ✅ 接受 |
| `Read` 配置区 4 B `0x02 P1=0x00` | 8 B | `1E 2D` | ✅ 接受 |
| `Nonce` 32 B `0x16 P1=0x03` | 40 B | 见附录 C | ✅ 接受 |

汇总：**标准 CRC 5/5 被接受**；16 位整体反转 0/5；两字节交换 0/5；含 word 地址覆盖 0/5。
（此结果在**两颗不同器件**上各复现一次，一致。）

> 若 CRC 有误，器件会回 `count=04`、状态 `0xFF`。本报告中所有应答状态均为 `0x00`。

### 2.2 身份与配置区可读（证明读写链路正确）

```
Info (0x30):  rc=0  status=0x00  RevNum=00 00 60 03      <- 608B 家族的合法版本号
Config[0x00..0x1F]:
  01 23 29 16 00 00 60 03 34 41 2B 64 EE 61 49 00
  C0 00 00 00 83 20 87 20 8F 20 C4 8F 8F 8F 8F 8F
```

（`SN`、`RevNum`、`I2C_Enable`/地址字段等均落在手册规定位置，内容自洽。）

### 2.3 状态码判据

| 状态 | 含义 | 本报告中的出现情况 |
|---|---|---|
| `0x00` | Success | **本报告两条 Random 命令的应答状态** |
| `0x03` | Parse error（命令/参数不认识） | 未出现 |
| `0x0F` | Execution error（认识但拒绝执行） | 未出现 |
| `0x11` | 唤醒后未收到有效命令 | 未出现 |
| `0xFF` | CRC / 通信错误 | 未出现（说明帧被正确接收） |

---

## 3. 现象

### 3.1 发出的命令帧（8 字节，含 CRC）

```
03 07 1B 00 00 00 24 CD
│  │  │  │  └──┴─ P2 = 0x0000（未使用）
│  │  │  └─────── P1 = 0x00  （Random 的 Mode：不带种子）
│  │  └────────── Opcode 0x1B = Random
│  └───────────── Count = 0x07 = 7 + 0 个数据字节
└──────────────── Word address 0x03 = Command
                 CRC 覆盖 07 1B 00 00 00 -> 0xCD24 -> 低字节先发 24 CD
```

### 3.2 应答结构

```
[count = 0x23] [32 字节随机数] [CRC_lo] [CRC_hi]        （0x23 = 1 + 32 + 2）
```

### 3.3 实测结果（两次独立唤醒 / 两条独立命令）

| | count | 数据前 8 字节 | 说明 |
|---|---|---|---|
| 第 1 次 | `0x23` | `FF FF 00 00 FF FF 00 00` | 合法应答 |
| 第 2 次 | `0x23` | `FF FF 00 00 FF FF 00 00` | 合法应答 |

固件对**完整 34 个应答字节**（count + 32 数据 + 2 CRC）做了逐字节比较，
结果 **完全相同**；控制台只打印前 8 字节，完整 32 字节可在需要时提供（见 §9）。

> 说明：两条命令之间重新执行了唤醒流程，且每次读取前把接收缓冲**清零**，
> 因此"读到的是上一次的残留"这一解释不成立。

### 3.4 图案本身的异常之处（独立于"两次相同"）

即使暂时不谈"两次完全相同"，返回的 32 字节数据本身是**周期为 4 的重复图案**
（`FF FF 00 00` 重复 8 次）。任何正常的随机源——包括伪随机数发生器——
输出都不会呈现这种结构。这一条**无法用"缓冲未刷新""旧值残留"来解释**：
残留只会重复**上一次的数据**，不会是周期性交替的 `FF/00` 图案。

也就是说，即使不考虑重复性，**这 32 字节也不可能是随机数**。

---

## 4. 排除主机侧原因

| 可能原因 | 排除依据 |
|---|---|
| 命令未被接收（CRC 错） | 应答状态 `0x00`；CRC 已在硬件上 5/5 被接受（§2.1） |
| 命令被接收但未执行 | 若未执行，`count` 不会等于 Random 特有的 `0x23` |
| 读到的是上一次应答的残留 | 每次读前清零缓冲；两次之间重新唤醒；且无法解释周期性图案（§3.4） |
| 读错了器件/地址 | 同一地址能正常返回合法 `RevNum` 与配置区（§2.2） |
| 总线速率过快 | 半周期 20 µs（≈25 kHz / 有效约 12.5 kbit/s），远低于 1 MHz；5–500 µs 各档结果一致 |
| 电源、上拉、接触不良 | 同一总线另外 8 个器件工作正常；同一夹具重复读取结果稳定 |
| `Mode = 0x00` 是某种"固定输出"模式 | 我方理解 `Mode = 0x00` 表示"不向 TRNG 写入种子"，输出仍应是新随机数；`Mode = 0x01` 才是"写种子"。**此处如有误解，请指出（这是本报告要问的关键之一）** |
| 是"确定性伪随机（PRNG）" | 确定性 PRNG 的输出不会呈全 1/全 0 交替的固定图案 |
| 器件处于睡眠/未唤醒 | 唤醒后 `Info` 立即正常应答；读应答时器件处于唤醒态 |

---

## 5. 判据与影响

**判据（本报告当时的理解 —— 已被更正）**：数据手册规定 `Random` 命令使用片内 **TRNG**
产生 32 字节随机数。两次调用得到相同 32 字节的概率约为 **2⁻²⁵⁶**；而实际观测到的不只是
"相同"，还是一个**固定图案**——当时据此判断为功能缺失。

> **更正**：该推理漏掉了数据手册的**前置条件**——配置区未锁定时，RNG 被规定输出这一
> 测试图案。补上前置条件后，观测结果与器件行为**完全一致**，不构成异常。

**影响（按更正后的结论）**：

1. 器件**可以**正常使用；但在未锁定（出厂）状态下随机源处于测试模式，不能用于密钥生成；
2. 这是一个**状态问题**，不是器件问题 —— 解决路径是完成配置并锁定配置区（不可逆，需规划）；
3. 在完成锁定之前，该器件只能用于链路联调与只读验证。

---

## 6. 复现方法（三种，任选其一）

### 方法 A —— 官方 CryptoAuthLib（推荐第三方使用）

```c
#include "cryptoauthlib.h"

ATCAIfaceCfg cfg = {
    .iface_type            = ATCA_I2C_IFACE,
    .devtype               = ATECC608B,
    .atcai2c.slave_address = 0xC0,     /* 7-bit 0x60 << 1 */
    .atcai2c.bus           = 1,
    .atcai2c.baud          = 100000,
    .wake_delay            = 1500,     /* us */
    .rx_retries            = 20,
};

uint8_t a[32], b[32];

atcab_init(&cfg);
atcab_random(a);        /* 第 1 次 */
atcab_random(b);        /* 第 2 次 */
/* 打印 a、b 全部 32 字节并比较 */
```

**判读**：

| 结果 | 含义 |
|---|---|
| 两次不同 | 正常 ⇒ 请把此结果告知我方（说明问题在我方板级/线缆，我们继续排查） |
| 两次相同 | 异常 ⇒ 与本报告一致 |
| 两次相同且为 `FF FF 00 00` 重复图案 | 与本报告完全一致 |

### 方法 B —— 裸 I²C（无任何库，任意主机）

1. 唤醒：保持 SCL 为高时把 SDA 拉低 ≥ 60 µs，随后释放；等待 ≥ 1.5 ms；
2. 发送命令（8 字节）：

```
03 07 1B 00 00 00 24 CD
```

3. 立即读 **34** 字节（`count` + 32 数据 + 2 CRC）；
4. 间隔 ≥ 2 ms，重复步骤 1–3，得到第二次结果；
5. 比较两次的 34 字节，并记下数据段（32 字节）。

### 方法 C —— 我方固件（如对方愿用现成镜像）

固件中该项检测由 `DSO_ATECC_EVIDENCE_MATRIX` 宏控制，控制台输出形如：

```
-- Random x2 (full 32 data bytes): count=0x23 IDENTICAL -> TRNG IS A STUB (security defect!)
     #1 first8=FF FF 00 00 FF FF 00 00
     #2 first8=FF FF 00 00 FF FF 00 00
```

（判据实现上要求"两次都读到合法应答且 `count = 0x23`"，否则打印 `INCONCLUSIVE`，
以避免"器件无应答导致两次缓冲都为零"被误判——该防护已在代码中。）

### 请第三方提供的结果

1. 两次应答的**完整 34 字节**；
2. 器件 `Info` 读回的 `RevNum`；
3. 所用器件来源（正品/同批/其它）与测试日期。

---

## 7. 更正声明（必须与旧材料一起阅读）

为避免误导，我方主动说明此前沟通过程中的两处修正：

### 7.1 CRC 约定——**撤回旧主张**

- 早期我方驱动程序使用的 CRC 变体**有误**（当时误判为"标准算法"，实则与器件不符），
  由此得出了"该器件 CRC 不标准 / 疑似镜像 CRC 仿制件"的结论。
- 2026-10-20 我方直接调用 Microchip CryptoAuthLib 导出的 `atCRC()` 交叉验证，
  确认**器件行为与官方算法完全一致**；修正驱动后，标准 CRC 在硬件上 **5/5 被接受**。
- 因此：**"CRC 不标准"这一主张完全撤回**，该器件在 CRC 层面**没有任何异常**。
  该修正已在两颗器件上分别验证。

### 7.2 数据区访问与 GenKey——**不作为缺陷主张**

- 读数据槽（`Read 0x02`）与 `GenKey` 均返回 `0x0F`（执行错，即"命令被认识但拒绝执行"）。
- 我方后续分析认为，这**可能由器件自身的 `SlotConfig` 读写策略正常解释**
  （该槽被配置为不可写 / 读取受限），属于"严格执行配置"的表现，而非功能缺陷。
- 因此在**本报告及后续沟通中，这两项均不作为缺陷主张**，除非第三方能给出相反依据。
  如需我方提供该配置位段的完整读出，请告知。

### 7.3 结论

**本报告唯一保留的主张是：`Random`（`0x1B`）返回固定值。**

> **2026-10-20 追加：该主张亦已撤回。** 固定图案是**数据手册规定的“配置区未锁定”
> 测试输出**，本机读回 `LockConfig@0x57 = 0x55` 证实处于该状态。
> 结论以 [ATECC608B_结论_配置区未锁定.md](ATECC608B_结论_配置区未锁定.md) 为准。

---

## 8. 希望第三方回答的问题

> **2026-10-20 状态：全部已答复。** 关键答复是 Q1/Q2——**未锁定时确实会返回固定测试值**；
> Q4——**属普遍现象，无需 RMA**。详见第三方分析文件与本项目结论文件。

| # | 问题 |
|---|---|
| Q1 | 正品 ATECC608B 的 `Random`（`0x1B`，`Mode=0x00`）连续两次调用，是否**必然**返回不同的 32 字节？ |
| Q2 | 是否存在**任何**模式、配置位或状态，会使 `Random` 返回固定/可重复的值？ |
| Q3 | 我方对该命令 `Mode` 字段的理解（`0x00` = 不写种子、仍输出新随机数；`0x01` = 写种子）是否正确？ |
| Q4 | 请按 §6 用正品或同批次器件复测，并给出结论：**同批普遍现象**还是**单件不良**？ |
| Q5 | 若确认为异常，处置建议是什么（参考件对比 / 退换 / 批次失效分析 / 需要我方提供哪些补充数据）？ |
| Q6 | 是否需要更多材料（完整 32 字节 × 多次读取、逻辑分析仪波形、完整配置区读出）？如需，请说明格式。 |

---

## 9. 附录

### 附录 A：配置区前 32 字节（实测）

```
cfg[0x00..0x1F]:
  01 23 29 16 00 00 60 03 34 41 2B 64 EE 61 49 00
  C0 00 00 00 83 20 87 20 8F 20 C4 8F 8F 8F 8F 8F
```

说明：`RevNum`（偏移 `0x04..0x07`）= `00 00 60 03`，符合 ATECC608B 家族；
`I2C_Enable` 等字段与 `0x60` 地址相符；其余为出厂序列与访问策略。

### 附录 B：日志原样（检测块相关输出）

```
[ATECC] Info rc=0 st=0x00 rev=00006003 crc=spec <ATECC608B>
cfg00: 01 23 29 16 00 00 60 03 34 41 2B 64 EE 61 49 00 C0 00 00 00 83 20 87 20 8F 20 C4 8F 8F 8F 8F 8F
-- Random x2 (full 32 data bytes): count=0x23 IDENTICAL -> TRNG IS A STUB (security defect!)
     #1 first8=FF FF 00 00 FF FF 00 00
     #2 first8=FF FF 00 00 FF FF 00 00
```

### 附录 C：CRC 自检（任何人都可以复核）

按 §2.1 的参数计算以下向量，结果应完全一致：

| 输入 | CRC | 发送（低字节先） |
|---|---|---|
| ASCII `"123456789"` | `0xBCDD` | — |
| `04 11`（唤醒状态读） | `0x4333` | `33 43` |
| `04 FF`（状态读） | `0x4201` | `01 42` |
| `07 30 00 00 00`（Info） | `0x5D03` | `03 5D` |
| `07 1B 00 00 00`（Random） | `0xCD24` | `24 CD` |
| `07 02 80 00 00`（Read 32B 配置区） | `0xAD09` | `09 AD` |
| `07 02 00 00 00`（Read 4B） | `0x2D1E` | `1E 2D` |

### 附录 D：状态码速查

| 状态 | 含义 |
|---|---|
| `0x00` | Success |
| `0x01` | Misc |
| `0x03` | Parse error |
| `0x0F` | Execution error |
| `0x11` | After wake, no command received |
| `0xEE` | Watchdog about to expire |
| `0xFF` | CRC or communication error |

### 附录 E：可提供的补充数据

如需下列任一项，请告知：

1. **完整 32 字节**随机数（我方固件当前只打印前 8 字节，改动一行即可输出全部 32 字节）；
2. **多次（>2）连续读取**结果；
3. **逻辑分析仪**的 SDA/SCL 波形（含命令帧与应答帧逐字节解码）；
4. **完整配置区**（`0x00–0x7F`）读出。

---

## English summary (for forwarding to the manufacturer / distributor)

**Device:** ATECC608B-SSHDA-T, 7-bit address `0x60`, `RevNum = 00 00 60 03`.

**Observation:** The `Random` command (`0x1B`) was issued twice, each after a fresh wake,
and the full 32-byte payload was read both times. The two 34-byte responses
(count + 32 data + 2 CRC) are **byte-identical**, and the payload itself is a **fixed
repeating pattern**:

```
03 07 1B 00 00 00 24 CD          <- command frame (CRC verified against atCRC())
read #1 : count=0x23, first 8 bytes = FF FF 00 00 FF FF 00 00
read #2 : count=0x23, first 8 bytes = FF FF 00 00 FF FF 00 00
```

**Preconditions verified:** The command CRC is computed with exactly the algorithm
returned by CryptoAuthLib's `atCRC()` and is **accepted by the device in 5/5 controlled
tests** (status `0x00`, never `0xFF`); the `Random` response length (`0x23`) is correct;
`Info` returns a valid `RevNum` and configuration block on the same bus/timing;
the receive buffer is cleared before each read and the device is re-woken between reads.

**Impact:** A crypto device whose TRNG returns a constant value cannot be used for key
generation or ECDSA signing (predictable nonce ⇒ private-key disclosure).

**Requests:**

1. Please confirm that a genuine ATECC608B never returns the same 32 bytes from two
   consecutive `Random` calls.
2. Please reproduce §6 with a genuine / same-batch part and advise whether this is a
   batch-level or single-unit defect, and what the next step is (reference-part
   comparison, RMA, failure analysis).
3. Please correct us if our understanding of `Random Mode = 0x00` is wrong.

**Scope note:** This report asserts **only** the TRNG anomaly. Earlier claims about a
non-standard CRC and about data-zone / `GenKey` refusals have been **withdrawn or
downgraded** (see §7): after calibration, the device accepts the standard CRC 5/5, and
the `0x0F` refusals are now considered an expected consequence of the device's own
`SlotConfig` policy.
