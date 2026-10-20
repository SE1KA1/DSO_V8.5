# ATECC608B 镜像 CRC 判定与 Stage 1 结论（2026-10-11）

> ## ⚠️ 本文结论已被推翻（2026-10-20 更正）
>
> - **“镜像 CRC 仿制件”判定撤回**：根因是我方驱动用了 CRC-16/ARC（poly `0xA001` 反射），
>   而器件要求 **poly `0x8005`、init 0、输入位 LSB-first**。改用官方 `atCRC()` 的算法后，
>   器件标准 CRC **5/5 被接受**，镜像/交换/含 word 三种变体全部 0/5。**器件 CRC 完全合规。**
> - **“TRNG 是桩”判定撤回**：`FF FF 00 00 …` 是**配置区未锁定**时的出厂测试图案。
> - 本文中引用“Slot0=`0x8F9F`”等配置结论还叠加了**偏移 bug**（`SlotConfig` 实际从 `0x14` 起，
>   `0x20` 是 slot6）——`0x8F9F` 其实是 **slot6**。
>
> 权威结论见 `ATECC608B_结论_配置区未锁定.md`。本文保留作为排查过程与工具方法记录。

> 一句话结论：手上这批 7-bit 地址 `0x60` 的片子是**CRC 引擎按位序镜像**工作的**仿制件**；
> 固件侧（总线、分帧、位序、标准 CRC-16/ARC）**全部正确**。已加默认关闭的镜像约定开关，
> 只读链路完全打通，Stage 1（设备侧授权接口）验收通过。
> ⚠️ **该器件不可用于正式安全用途**（密钥可信度/认证背书均无保证）。

## 1. 判定方法（可复用）

不要只扫 CRC 变体——**拿器件自己发出的帧反推它的约定**。取两条已经拿到的响应：

| 帧 | 覆盖字节 | 我们的 CRC-16/ARC | rev16 | 器件实测尾字节 | |
|---|---|---|---|---|---|
| 裸读（无命令） | `{04,11}` | `0xCCC2` | `0x4333` | `33 43` | ✔ |
| 命令后（被拒） | `{04,FF}` | `0x8042` | `0x4201` | `01 42` | ✔ |

两个互相独立的 16 位匹配（偶然概率 ≈ 2⁻³²）⇒ 它的 CRC = **标准 CRC 的位序镜像**。
这也解释了为什么此前 **13 个标准 CRC 家族 × 8 种分帧/覆盖**全扫不命中：镜像不在任何标准家族里
（已验：BUYPASS/0x8005 给 0x9865/0x9A01，不是）。

**验证**（只发只读命令）：给 Info 帧换上镜像尾字节

```
标准  03 07 30 00 00 00 BA C0  → 04 FF 01 42（仍被拒）
镜像  03 07 30 00 00 00 03 5D  → 07 00 00 60 03 …  ✅ 被接受（count=07、RevNum=00006003）
备选  03 07 30 00 00 00 5D 03  → 仍被拒（它读 CRC 的字节序与自己的发送次序一致）
反证  03 07 30 00 00 00 C0 BA  → 仍被拒（只换字节序不够，必须整体 rev16）
```

## 2. 实现（均在 `User/dso/dso_atecc608b.c/.h`，默认关闭，对正品零影响）

- `atecc_rev16()` + `atecc_crc_wire()`；在 `atecc_exec_body()` 的**发送**与**两处响应校验**共 3 点统一变换。
- `s_crc_mirror` / `s_crc_swap` 两个静态开关；对外 `DSO_Atecc608b_SetCrcMirror()` / `CrcMirror()`。
- 宏 `DSO_ATECC_CRC_MIRROR`（默认 1 = 允许自动启用）、`DSO_ATECC_DIAG_CRC_CONV`。
- **开机快路径**：标准 CRC 失败 → 镜像 CRC 重试 → **复验通过才切档**（否则回退标准），
  然后走正常启动流程（Config dump / SlotConfig / device_id / 自测），不掉进诊断臂。
- 诊断臂 `atecc_diag_crc_conv()`（4 候选约定判定）与 `atecc_diag_read_cfg()`（Read 三臂分解）保留作后备。

## 3. 过程中查出并修掉的三个**真缺陷**（对正品同样成立）

1. **会话档 500µs 不可靠** → 降到 **20µs**。实测：同一颗片、同一份代码，只改 `DSO_ATECC_SESSION_BIT_US`，
   Read 配置区 @500µs 得 `rc=-1`(ERR_IO)、@20µs 得 `rc=0`。500µs 是 2026-10-10 为绕开已修复的
   ACK 采样 bug 而设的保守档。
   ⚠️ 更正：早期"2026-09-22 那颗正品在 500µs 也通"的说法经取证否定——同期日志（9-06/9-18/9-20/
   9-22/9-23）与 9.22 提交正文都记录旧片三地址全 NACK；`rc=0 / 00006003 / 配置区 dump` 首次进入
   git 是 2026-10-11 的 `32fdfec`，来源是那颗**镜像 CRC 仿制片**。故"低速不稳是正品特性"不成立，
   20µs 是当前唯一验证过能读回响应的档位。
2. **Read(32B) 地址编码错误**：旧宏 `ATECC_READ32_CONFIG_BLOCK(blk)=0x80|blk` 把块号塞进 Param1 低 4 位。
   规格要求 bit7=1 时**低 4 位必须为 0**，块地址放 **Param2 且以 4 字节字计 → `blk*8`**。
   症状：`blk≥1` 回 **0x0F ExecutionError**；`blk=0` 因 `0x80|0` 恰好正确而侥幸能读。
   已换成 `ATECC_READ32_P1` + `atecc_rd32_addr()`，并加一次性单位自判（备选 `blk*32`/`blk*1`；
   非法候选会被 0x0F 拒掉，故"哪次成功"自证地址正确）。
3. **`comm_ok`/`fail_cnt` 语义混淆**：`atecc_note()` 原把**任何负 rc** 当通信故障。
   而 `st=0x0F`（ExecutionError）是芯片**已答复**、只是拒绝执行。旧逻辑下"槽里没密钥"会把
   遥测打成 `comm_ok:0`，且试满 `DSO_ATECC_TAMPER_FAIL`(=10) 次就**误报 tamper 事件**。
   现按规格（`packet_specs/01_atecc608b（密码芯片）.md`：`fail_cnt` = **连续通信失败**计数）分开：
   仅 `ERR_IO/-TIMEOUT/-CRC` 置 `comm_ok=0` 并累计，`ERR_EXEC/-ERR_PARAM` 视为通信正常。

## 4. 器件实测状态（Stage 2 规划依据）

配置区 128B 全读出（交叉验证：配置区里的 Rev 与 Info 的 RevNum 一致）：

| 项 | 值 | 含义 |
|---|---|---|
| `LockValue@0x56` / `LockConfig@0x57` | `55 55` | **未锁**（0x55=未锁、0x00=已锁） |
| `SlotLocked@0x58` | `FFFF` | 无任何槽被锁 |
| `SlotConfig` | slot0=`8F9F`、slot1=`8FAF`、slot9=`8FAF`，其余 `0000`/`FFFF` | slot0/1/9 标为 `IsSecret=1` 的密钥槽 |
| `KeyConfig@0x60..0x7F` | `33 00 / 1C 00 / 3C 00 …` | 标准 KeyConfig 表 |
| `0x00-0x01` | `01 23` | ATECC 固定标记 |
| `Sign`/`GenKey` 读公钥 | `rc=-4 st=0x0F` | **Slot0 内没有可用 ECC 私钥** |

## 5. Stage 1（设备侧）验收（NetAssist 实测）

```
{"cmd":"atecc.get_devid","seq":1}
  → result:"ok"  device_id:"Eep21025E269FA2E" label8:"Eep21025" uid_hex:"0038001F3130510B36343832"
{"cmd":"atecc.get_pubkey","seq":2}
  → result:"error" "pubkey read failed rc=-4 st=0x0F (Slot0 has no ECC key?)"
{"cmd":"atecc.auth_start","params":{"challenge_hex":"<64 hex>"},"seq":3}
  → result:"error" "sign/verify failed rc=-4 st=0x0F …"（回带 device_id）
```

遥测（`module.data`）：
```json
{"present":1,"comm_ok":1,"device_id":"Eep21025E269FA2E",
 "chip_info":{"serial4":"01232916","part":"608B","rev":"00006003","lock_config":0,"lock_data":0},
 "login_state":"LOGGED_OUT","fail_cnt":0}
```
`login_state:LOGGED_OUT` 正确（无密钥必然无法进入已登录态）。

## 6. 红线与后续

- **调试期红线未触碰**：全程只发 `Info(0x30)` / `Read(0x02)`；未发 `Write(0x12)`/`Lock(0x17)`/`GenKey(0x40)`；
  I²C word 地址恒 `0x03`（不用 `0x00/0x01/0x02` Reset/Sleep/Idle）；**不碰 OTP**；唤醒只用 GPIO 脉冲。
  配置区实测 `55 55` ⇒ 芯片仍完全可改。
- **Stage 2（写槽/建密钥/锁定）未做**：需要单独批准。**建议不做**——仿制件密钥没有可信度；正式用途应换正品。
- **换回正品时**：把 `DSO_ATECC_CRC_MIRROR` 置 0（或什么都不用做——快路径会自动回退标准 CRC）；
  `blk*8` 的地址编码与 20µs 会话档对正品同样正确。
- **待办（不影响功能）**：① 配置区**偏移标签**按真实布局重排（实测 Rev 在 `0x04`，驱动的
  `ATECC_CFG_REV_OFF`=0x08 会把 `rev@08` 标错；`serial4` 取自 `cfg[0..3]`= `01 23 29 16` 并非真序列号）；
  ② 把若干模块在 `DebugUartLock` 之外的 printf 纳入锁，消除日志插花。
