# 模块 1：ATECC608B-SSHDA-T（密码芯片，身份登录/防篡改）

## 1. 硬件与用途

| 项 | 值 |
|---|---|
| 器件 | Microchip ATECC608B-SSHDA-T（I2C 加密认证芯片，ECDSA/AES/SHA） |
| I2C 7 位地址 | **0x60**（SDA=PB7, SCL=PB8，与 PCAL6524/SHT40/INA226 共用软件 I2C 总线） |
| 用途 | ①用户安全身份识别登录（挑战-签名认证）；②恶意防篡改（芯片锁状态/通信异常/校验失败事件上报） |

> 驱动层（I2C 命令/状态机）尚在开发：本轮交付数据包格式与参考实现；`present/comm_ok` 等为设备侧真实采集结果，联调时填充。

## 2. 周期上报 `type: module.data`（建议 1s 周期，或状态变化即时）

```json
"data": {
  "present": 1,              // int 0/1  芯片在位（地址应答）
  "comm_ok": 1,              // int 0/1  最近读取成功
  "chip_info": {             // 芯片信息（首次成功采集后携带，失败可省略）
    "serial4": "a2c4f16d",   // string   9 字节序列号前 4B 的小写 hex（余下 5B 含可配置区，联调定）
    "part": "608B",          // string   器件型号短名（读 Config zone ID）
    "rev": 0,                // int      版本/修正
    "lock_config": 1,        // int 0/1  Config zone 锁定状态（1=已锁，防篡改关键）
    "lock_data": 1           // int 0/1  Data zone 锁定状态（1=已锁）
  },
  "login_state": "LOGGED_OUT", // string  LOGGED_OUT | CHALLENGE | LOGGED_IN
  "fail_cnt": 0              // int      连续通信失败计数（复位后清零）
}
```

## 3. 事件 `type: module.event`

```json
"data": {
  "event": "tamper",        // string  tamper | login
  "level": "warning",       // string  info | warning | error
  "detail": "i2c read fail x5"  // string  ≤200 字符，人读描述
}
```

| event | 触发 |
|---|---|
| `tamper` | I2C 读失败超阈值 / 芯片锁状态异常（本应锁定却未锁）/ 签名或校验失败——疑似篡改/替换 |
| `login` | 登录认证成功（info）或失败（warning/error，附失败原因） |

## 4. 下行命令骨架（`command.execute`，TBD 标注）

设备侧预留以下命令，**完整密码学握手流程待与后端定稿**后实现：

| 命令 | 参数 | 说明 |
|---|---|---|
| `atecc.get_chip_info` | `{}` | 返回 chip_info（见 §2） |
| `atecc.auth_start` | `{challenge_hex}` | 后端下发随机挑战（hex）→ 设备内 ATECC 签名 → `command.result` 返回 `{signature_hex, pubkey_slot}`（**TBD**：签名方案/公钥登记方式未定稿） |
| `atecc.auth_verify` | `{...}` | 后端回执/会话建立（**TBD**） |

> 后端同事如对登录语义有建议，可在联调前提出；数据面（状态/事件/心跳）不受影响。

## 5. 示例完整帧

周期状态：
```json
{"ver":"1.0","type":"module.data","module":"atecc608b","seq":42,"ts_ms":1234567,"data":{"present":1,"comm_ok":1,"chip_info":{"serial4":"a2c4f16d","part":"608B","rev":0,"lock_config":1,"lock_data":1},"login_state":"LOGGED_OUT","fail_cnt":0}}
```

防篡改事件：
```json
{"ver":"1.0","type":"module.event","module":"atecc608b","seq":88,"ts_ms":1290300,"data":{"event":"tamper","level":"warning","detail":"i2c read fail x5"}}
```

登录失败事件：
```json
{"ver":"1.0","type":"module.event","module":"atecc608b","seq":89,"ts_ms":1291000,"data":{"event":"login","level":"warning","detail":"signature verify failed"}}
```

## 6. 后端解析要点
- 用 `module=="atecc608b"` 分派到本模块解析器；`data` schema 校验可用 `python_ref/module_atecc608b.py` 的 `validate_*`。
- `serial4` 为序列号指纹（防误换芯片）；`lock_config/lock_data` 任一为 0 且产品应锁定 → 视为异常，可转人工/事件。
- `fail_cnt` 与 `tamper` 事件配合：后端可设告警阈值（如连续 3 次 tamper）。
