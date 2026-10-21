# 设备授权接口（Stage 1，设备侧）

> 依据 `Markdown/STM32H743 + ATECC608B-SSHDA-T + LCMXO2 FPGA 远程学生设备授权体系.md`。
> **本阶段边界（非破坏）**：不写 ATECC 配置区、不写数据槽、不 Lock、不碰 OTP。设备侧只做
> **只读识别 + 片内签名/验签**（私钥永不出片）。服务器（CA / License 签发 / 用户库）由后端实现。
>
> ⚠️ **2026-10-21 状态**：服务器侧的协议与实现见 **`Markdown/设备授权协议_v0.md`**
> 与 `tools/auth_server/`（Stage 2A，已完成）。器件仍处于**出厂未锁定态**，
> `auth_start` 回 `rc=-4 st=0x0F`，能力被判定为 `identity_only` —— 详见
> `Markdown/ATECC608B_结论_配置区未锁定.md`。
> 芯片地址：ATECC608B-SSHDA-T，**7-bit `0x60`**（线上写 `0xC0` / 读 `0xC1`），PB7=SDA / PB8=SCL。

---

## 1. DeviceID（16 个十六进制字符）

```
DeviceID = 标签前 8 字符 + 8 个十六进制字符（随机段）
例：      "Eep21025" + "A1B2C3D4"  ->  "Eep21025A1B2C3D4"
```

| 段 | 来源 | 说明 |
|---|---|---|
| 前 8 字符 | 物理标签串 `Eep210251A08891` 的前 8 字符 | 固件宏 `DSO_DEVICE_LABEL`；**量产按台配置** |
| 后 8 字符 | **STM32H743 96-bit UID 的 CRC32**（大写十六进制） | 固件 `DSO_Atecc608b_DeviceId16()`；**跨重启稳定**（DeviceID 是服务器 Device 表主键） |

**为什么随机段不用 ATECC TRNG（现在还不用）**：TRNG 每次上电取值不同，必须**持久化**才有意义，
而持久化 = 写 Slot1（属 Stage 2）。文档第 13 行本就允许用 STM32 UID 作复合身份。
**Stage 2 升级路径**：随机段改由 ATECC TRNG 生成并写入 Slot1，**格式不变、后端无感**。

---

## 2. 命令（v0.9 通道：串口 + TCP 5005 设备侧服务端）

请求/应答均为**单行 JSON**，与 `set_sw`/`get_switch` 同一方言。
（v1.0 `command.execute`/`command.result` 通道留同名映射，本阶段未实现。）

### 2.1 `atecc.get_devid`

```json
-> {"cmd":"atecc.get_devid","params":{},"seq":1}
<- {"ver":"0.9","type":"ack","seq":1,"cmd":"atecc.get_devid","result":"ok","error":"",
    "data":{"device_id":"Eep21025A1B2C3D4","label8":"Eep21025","uid_hex":"<24 hex>"}}
```

`uid_hex` = STM32 96-bit UID 原值（供后端与设备台账对账）。

### 2.2 `atecc.get_pubkey`

```json
-> {"cmd":"atecc.get_pubkey","params":{},"seq":2}
<- {"ver":"0.9","type":"ack","seq":2,"cmd":"atecc.get_pubkey","result":"ok","error":"",
    "data":{"pubkey_hex":"<128 hex>","slot":0,"device_id":"Eep21025A1B2C3D4"}}
```

`pubkey_hex` = 64 字节 **X||Y**（不含未压缩前缀 `0x04`）。
实现为 ATECC `GenKey(Mode 0x00)`：芯片用槽内私钥**重算**公钥后输出，**私钥不参与传输也不改变**
（与 CryptoAuthLib `atcab_get_pubkey` 同路径）。**后端登记后请勿再让设备侧改该槽密钥。**

### 2.3 `atecc.auth_start`（设备证明）

```json
-> {"cmd":"atecc.auth_start","params":{"challenge_hex":"<64 hex = 32 bytes>"},"seq":3}
<- {"ver":"0.9","type":"ack","seq":3,"cmd":"atecc.auth_start","result":"ok","error":"",
    "data":{"device_id":"Eep21025A1B2C3D4","signature_hex":"<128 hex>",
            "pubkey_hex":"<128 hex>","slot":0,"self_verify":1}}
```

失败时 `result:"error"`，`error` 里带 `rc` 与芯片状态字节（例：Slot0 里没有可用 ECC 私钥）。

### 2.4 `atecc.auth_verify`（**占位，未实现**）

校验服务器 CA 签名的 License 需要 **Slot2 预置 CA 公钥**（要写数据槽 / 可能改配置区）
→ 属 **Stage 2**。

---

## 3. 密码学语义（后端验签按此实现）

设备侧调用序列（**全部在芯片内部完成，私钥不出片**）：

| 步 | ATECC 命令 | 语义 |
|---|---|---|
| 1 | `Nonce(0x16, Mode 0x03)` | **Pass-through**：把服务器给的 **32 字节挑战直接作为 TempKey**（即"挑战就是待签摘要"） |
| 2 | `Sign(0x41, Mode 0x80, Slot0)` | 用 Slot0 私钥对 TempKey 签名 → **64 字节 `r‖s`**（各 32 字节大端） |
| 3 | `Verify(0x45, Mode 0x00, Slot0)` | 片内用 Slot0 公钥验签（响应 1 字节，`0x00`=通过）→ 即应答里的 `self_verify` |

**算法**：ECDSA **P-256**（secp256r1）+ **SHA-256**。
**关键点**：Mode 0x03 让挑战**充当消息摘要**，因此后端验签时**不要对挑战再做一次哈希**，
直接把它当作 `sha256(message)` 的结果用于 ECDSA 验证。

后端验签（Python，`cryptography`）——文档内自测依据：

```python
from cryptography.hazmat.primitives.asymmetric import ec, utils
from cryptography.hazmat.primitives.asymmetric.utils import encode_dss_signature
from cryptography.hazmat.primitives import hashes
from cryptography.exceptions import InvalidSignature

pub_xy = bytes.fromhex(pubkey_hex)          # 64 字节 X||Y
challenge = bytes.fromhex(challenge_hex)    # 32 字节，即"摘要"
sig = bytes.fromhex(signature_hex)          # 64 字节 r||s
r, s = int.from_bytes(sig[:32], "big"), int.from_bytes(sig[32:], "big")

pub = ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(), b"\x04" + pub_xy)
try:
    pub.verify(encode_dss_signature(r, s), challenge,
               ec.ECDSA(utils.Prehashed(hashes.SHA256())))   # 挑战已是摘要 → Prehashed
    print("device proof OK")
except InvalidSignature:
    print("device proof FAILED")
```

> 用 `Prehashed` 是关键：ATECC 签的是**摘要本身**，不是再由 OpenSSL/cryptography 去哈希一次。
> 若后端希望"签原始报文"，可让服务器把 `SHA256(报文)` 的结果当挑战下发（设备侧无需改动）。

---

## 4. 遥测帧（`module.data` / `module.event`，1s 周期）

```json
{"ver":"1.0","type":"module.data","module":"atecc608b","seq":21,"ts_ms":52025,
 "data":{"present":1,"comm_ok":1,"device_id":"Eep21025A1B2C3D4",
         "chip_info":{"serial4":"0123abcd","part":"608B","rev":"00006003",
                      "lock_config":0,"lock_data":0},
         "login_state":"LOGGED_OUT","fail_cnt":0}}
```

- `device_id`（本阶段新增）、`present/comm_ok/chip_info{serial4,part,rev,lock_config,lock_data}`、
  `login_state`（Stage 1 恒 `LOGGED_OUT`）、`fail_cnt`（连续访问失败次数）
- 事件：`tamper`（连续失败达阈值）；`login` 事件随 Stage 2 的授权流程接入
- 字段校验沿用 `module_telemetry/python_ref/module_atecc608b.py`

---

## 5. 阶段划分与依赖

| 阶段 | 内容 | 状态 |
|---|---|---|
| **Stage 1** | DeviceID 派生上报；片内挑战签名自测（Nonce/Sign/Verify）；SlotConfig 只读解读；遥测帧；3 条命令 | **本轮已实现** |
| **Stage 2A** | **协议 v0 + PC 侧授权服务 + 模拟设备闭环**（挑战/证明/License 签发与校验/能力协商），见 `Markdown/设备授权协议_v0.md`、`tools/auth_server/` | **2026-10-21 完成**（零硬件风险） |
| **Stage 2B** | 设备侧预置：改配置区 → `Lock(Config)` → 写 Slot0（GenKey）/ CA 公钥到 **slot 8–15** → `atecc.auth_verify` | **阻塞**：2026-10-20 实测确认不 Lock 数据区不可访问；需用户明确批准不可逆操作 |
| **Stage 2C** | 备选：设备侧软件密钥过渡（micro-ecc/mbedTLS），把整链在真机跑通、芯片不参与 | 未做 |
| **Stage 3（量产，不可逆）** | `Lock(Config)`、`Lock(Data+OTP)`、OTP 写入、ROP/PCROP、关闭 SWD、HSM 侧 CA 私钥 | 需 HSM/CA 配合 + 逐项单独批准 |

**已知依赖/缺口（需要其它侧配合）**

| 项 | 说明 |
|---|---|
| FPGA 随机源 / FPGA 唯一 ID | **现有 FMC16 命令集没有"读 FPGA 随机值/唯一 ID"的 opcode**，需 XO2 侧新增 RTL + 协议；且全板上板未验。文档第 16/54 行的"FPGA 参与复合身份"因此暂不能实现 |
| TLS 1.3（文档第 57 行） | 工程内**无 mbedTLS/wolfSSL**，LwIP `altcp/TLS` 未编入；需先引入密码库并把签名运算交给 ATECC |
| RTC 时间同步 / License 到期 | `bsp_cpu_rtc.c` 明确"当前 DSO 主链路未启用"，无 SNTP；License 时效判定目前无时间基准 |
| Nonce 防重放 / 吊销黑名单 | 属服务器侧（缓存已用挑战、心跳吊销）；设备侧现成只有 1s `device.status` 心跳 |
