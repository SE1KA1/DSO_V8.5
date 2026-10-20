# 更正通知：撤回 2026-10-13 技术反馈中的两项主张

| 项目 | 内容 |
|---|---|
| 致 | 立创商城 / 供应商技术支持（如需转原厂，见第五节） |
| 原反馈日期 | 2026-10-13（《ATECC608B-SSHDA-T 技术反馈：I²C 帧 CRC 校验行为与规格不符》） |
| 更正日期 | **2026-10-20** |
| 型号 / 地址 | ATECC608B-SSHDA-T，7-bit `0x60`，`RevNum = 00 00 60 03` |
| 原始记录 | `ATECC608B-CRC异常_给供应商反馈_原始2026-10-13.md`（保留作为取证存档，勿再作为诉求依据） |

---

## 一、结论：原反馈中的两项“缺陷”**均系我方误判，现予撤回**

| # | 原主张 | 更正 |
|---|---|---|
| 1 | “该器件 CRC 采用 16 位反转约定，与数据手册不符” | **撤回。** 我方驱动当时的 CRC 变体**算错了**。改用与 Microchip CryptoAuthLib `atCRC()` 相同的算法后，器件对标准 CRC **5/5 全部接受**（`std 5/5 | rev16 0/5 | swap 0/5 | word3 0/5`）。**器件的 CRC 行为完全符合规格。** |
| 2 | “随机数发生器返回固定图案，属第二项独立硬缺陷” | **撤回。** `FF FF 00 00 …` 是 **ATECC608B 在“配置区未锁定（出厂测试态）”下的数据手册规定测试图案**，用于产线自检；配置区锁定后才输出真随机数。本机读回 `LockValue@0x56 = 0x55`、`LockConfig@0x57 = 0x55`，确认处于未锁定态。 |

因此：

- **该 `0x60` 器件在通信、CRC、身份、配置区层面均无异常**；
- 原反馈中关于“批次问题 / 退换货”的请求**一并撤回**；
- 对由此给贵司技术支持带来的工作量，我们表示歉意。

### 支撑数据（2026-10-20 复测，原始日志）

```
VERDICT: std 5/5 accepted | rev16 0/5 accepted | swap 0/5 accepted | word3 0/5 accepted
[ATECC] Info rc=0 st=0x00 rev=00006003 crc=spec <ATECC608B>
[ATECC] lockval@56=55 lockcfg@57=55 slotlock@58=FFFF
[ATECC-CFG] VERDICT: CONFIG ZONE UNLOCKED (0x55/0x55)
             -> Random FF FF 00 00... is the DOCUMENTED pre-lock test pattern
```

---

## 二、仍然需要贵司协助的一件事（与原两项主张无关）

在**另一块板**上还有一颗同期采购的同型号器件，长期“在 `0x60` 无应答”。复查后的事实：

| 项目 | 该器件实测 | ATECC608B 应为 |
|---|---|---|
| 实际 7-bit 地址 | **`0x64`**（在 0x60 无应答） | `0x60` |
| `RevNum` | **`00 02 00 09`** | `00 00 60 03` |
| I²C 地址字段 | `C8`（= `0x64 << 1`） | `C0`（= `0x60 << 1`） |
| `Sign` / `GenKey` | **`0x03`（Parse error，命令不被解析）** | 应能解析（ECC 器件） |
| 结论 | 表现为 **ATSHA204A 级器件**（SHA 家族，无 ECC） | — |

**请求**（仅此一条）：

1. 请确认该器件（地址 `0x64`、`RevNum 00 02 00 09`）的**实际型号与供货批次**；
2. 若确为 ATSHA204A 系列，请说明为何以 ATECC608B-SSHDA-T 的名义交付，并给出处置方案。

> 说明：本节只针对那颗 `0x64` 的器件。**`0x60` 那颗器件不再有任何投诉。**

---

## 三、我们已自行完成的工作（供贵司核对）

1. 用 Microchip 官方 `atCRC()` 的输出逐向量校准 CRC（`{04,11}→0x4333`、`{04,FF}→0x4201`、
   `{07,30,00,00,00}→0x5D03`），并在硬件上用 5 条不同命令 × 4 个变体验证；
2. 读回完整 128 字节配置区，确认 `SlotConfig`/`KeyConfig`/锁字节均为出厂默认；
3. 依据数据手册确认“未锁定 ⇒ Random 输出测试图案”的前置条件，并修正了固件内的诊断判据。

---

## 四、后续

- 该器件将在完成配置规划后由我方自行决定是否锁定（不可逆操作，与贵司无关）；
- 若贵司希望获取完整复测日志或原文更正记录，可随时索取。

---

## 五、英文摘要（如需转原厂）

> **Correction — withdrawal of two claims made on 2026-10-13**
>
> We previously reported (1) that the ATECC608B-SSHDA-T did not accept the datasheet CRC and
> (2) that its RNG returned a fixed pattern. **Both claims are withdrawn.**
>
> - The CRC mismatch was **our** implementation error: our driver used CRC-16/ARC while the
>   device (like CryptoAuthLib's `atCRC()`) uses poly `0x8005`, init 0, LSB-first input.
>   After correcting it, the device accepts the standard CRC **5/5** on five different commands.
> - The `FF FF 00 00 …` pattern is the **documented pre-lock test output** of the RNG when the
>   Configuration zone is unlocked. We read back `LockValue@0x56 = 0x55` and
>   `LockConfig@0x57 = 0x55`, confirming that state.
>
> The part at address `0x60` is therefore fully conformant; no RMA is requested for it.
>
> The only remaining question concerns a **different** part from the same purchase which answers
> at 7-bit address `0x64` with `RevNum 00 02 00 09`, no ECC support (`Sign` → parse error `0x03`),
> i.e. ATSHA204A-class behaviour. We ask the supplier to confirm its actual model and batch.
>
> We apologise for the incorrect report and thank the support team for their time.
