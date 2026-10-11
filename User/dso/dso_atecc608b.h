/**
 * @file dso_atecc608b.h
 * @brief ATECC608B-SSHDA-T 密码芯片驱动（软件 I2C，PB7=SDA/PB8=SCL，7-bit 地址 0x60）。
 *
 * 唤醒规范（ATECC608 全系）：上电后 ≥20ms 才可动 I2C；必须先 Wake —— 以 ≤100kHz 发
 * 0x00 使 SDA 连续低 ≥60µs（tWLO），释放后上升沿唤醒；Wake 后等 ≥1.5ms（tWHI）再
 * 访问 7-bit 地址（SSHDA 0x60：总线写字节 0xC0 / 读 0xC1；TNGTLS 定制版为 0x35）。
 * 未满足 Wake 时芯片处于 Sleep，对地址一律 NACK（看起来像总线上没有器件）。
 * 两条唤醒实现：GPIO 脉冲法（BSP_I2C_UI_WakePulseMeasUs，**本驱动主用**，不产生地址
 * 字节/不广播）与 0x00 字节法（BSP_I2C_UI_WakeZeroBytes；其第一个 0x00 即 I2C 通用
 * 呼叫地址，本总线有器件会应答它，故其返回值**不能**当唤醒判据）。
 * 另：唤醒与寻址必须在**同一持锁窗口**内完成（中途只等 tWHI），否则同总线任务的
 * 1s 轮询会插进来、探针也会落到唤醒窗口之外 —— 曾因此恒判 NACK。
 *
 * 覆盖：I2C 命令/执行状态机 + 响应 CRC16 校验 + Info/Config 读。
 * 登录（Nonce/Sign/Verify/AES）等上层流程定稿后再封装，本轮只留 opcode 宏。
 * 数据面 schema 见 module_telemetry/packet_specs/01_atecc608b（密码芯片）.md。
 */
#ifndef DSO_ATECC608B_H
#define DSO_ATECC608B_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * I2C **7 位**地址（PB7/PB8 软件 I2C 总线，与 PCAL6524/SHT40/INA226 共用）。
 * **0x60** —— 2026-10-10 换上新的 ATECC608B-SSHDA-T 后**实机确认**的地址
 * （总线写字节 = (0x60<<1)|0 = 0xC0，读字节 = 0xC1）。
 * 逻辑分析仪对照：同一份固件下旧片在 0x60 全部 NACK、新片全部 ACK，故 0x35/0x36
 * 两个候选（TNGTLS 定制版）**已实测排除**。
 * BSP 原语一律传 7 位地址（内部左移），禁止传左移后的 0xC0 之类。
 */
#define DSO_ATECC608B_I2C_ADDR     0x60u

/* ---- 唤醒/时序参数（TNGTLS 规范） ---- */
/** 上电后首次动 I2C 前的等待（规范 ≥20ms，取 5× 余量） */
#define DSO_ATECC_POR_WAIT_MS      100u
/** Wake 后最小等待 tWHI（≥1.5ms） */
#define DSO_ATECC_TWHI_MS          2u

/* ---- 规格合规基线（官方规格书；Arm I 的判定与打印同源于此） ---- */
/** tWLO：唤醒低电平最小宽度（µs） */
#define DSO_ATECC_TWLO_MIN_US      60u
/** tWHI：唤醒释放后到首次通信的最小等待（µs） */
#define DSO_ATECC_TWHI_MIN_US      1500u
/** 未就绪时的保守等待（自检使能 tWHIST ≥20ms） */
#define DSO_ATECC_TWHI_STD_MS      25u
/**
 * 识别阶段位延迟（半周期 µs）——**整轮判定实验的固定速率档**：
 *   5µs ≈ 93kHz（≈100kHz 标准模式，规范要求的"Wake 用 ≤100kHz"上限值）
 *  10µs ≈ 48kHz
 *  30µs ≈ 16kHz
 *  60µs ≈ 8kHz
 * 500µs ≈ 2kHz（**当前固定值**）：示波器已确认芯片在"第 9 个时钟 SCL 高电平期"
 * 拉低 SDA（= 确实 ACK），而现行单点采样漏读 → 把高电平期拉长到 500µs，给
 * 多点采样（BSP_I2C_UI_SetAckMultiSample）留足窗口覆盖 ACK 的真实时刻。
 * Arm D 仍会把前四档逐一探一遍做对照；臂 E 的全地址扫描也跑在本档上。
 */
#define DSO_ATECC_BIT_DELAY_US     500u
/** Arm G 信标 / 多采样复探的背靠背探测次数（示波器要一段干净可判定的 0xC0 串） */
#define DSO_ATECC_BEACON_N         32u
/**
 * ATECC **会话档**位延迟（半周期 µs）：Info/Read 等正式命令期间用。
 * 当前取 **20µs ≈25kHz**（2026-10-11 由 500µs 下调）。
 *
 * 历史：
 *   2026-10-10 曾改成 30µs，随后 Info 恒报 CRC 错（rc=-3）。真因**不是速率本身**，
 *     而是 `i2c_wait_ack()` 的 ACK 多点采样把 ACK 时隙的 SCL 高电平压到几十 ns →
 *     从机没采到第 9 个时钟 → 响应帧位计数错位（已在 bsp_i2c_ui.c 修好），
 *     当时为求稳把档位退回 500µs。
 *   2026-10-11 实测复现（同一颗片、同一份代码，**只改本档位**）：
 *     Exec 两段式读 Config 32B @500µs → rc=-1(ERR_IO)；@20µs → rc=0 ✓
 *     单次事务读 36B @20µs → count=0x23（完整 32 字节包）✓
 *     ⇒ 500µs 档对本片不可靠（长事务尤其明显），20µs 已验证 Info/Read 均通。
 * 注：2026-09-22 那颗正品在 500µs 下也通，说明"低速不稳"是本批器件的特性。
 * 改本宏后回归两条：`[ATECC] Info rc=0` 与配置区 dump 完整。
 */
#define DSO_ATECC_SESSION_BIT_US   20u
/** 臂 E 追加的慢速扫描档（60µs 半周期 ≈ 8kHz）：慢速下的**全地址**覆盖 */
#define DSO_ATECC_SCAN_SLOW_US     60u
/**
 * 臂 E / 冷态基线用的**快档**（5µs 半周期 ≈ 93kHz）：单个地址探测 ~0.15ms，
 * 使"唤醒+一批地址"稳稳落在唤醒窗口内（500µs 档下一个地址要 ~10ms，一批 8 个
 * 就到 ~80ms，芯片若只在唤醒后短时可访问就会被整批漏掉）。
 */
#define DSO_ATECC_SCAN_FAST_US     5u
/** Arm M（逻辑分析仪判定夹具）用的解码友好档：20µs 半周期 ≈ 25kHz */
#define DSO_ATECC_LA_SLOW_US       20u
/** Wake 用 0x00 字节数（100kHz 下每字节 SDA 低 ≈86µs） */
#define DSO_ATECC_WAKE_ZB_N        3u
/** 每帧 Exec 前是否先 Wake（识别阶段 =1；将来周期上报可调 0/加窗口判断） */
#define ATECC_WAKE_BEFORE_EXEC     1

/* ---- 分层判定实验（只读诊断；2026-10-10 芯片已确认工作，默认关闭） ---- */
/** 上电探测任务是否跑判定电池（1=跑）。置 0 时整块诊断代码不参与编译，
 *  probe_task 只做"唤醒 + 只读 Info + Config dump"，开机干净、快。 */
#define DSO_ATECC_DIAG_BATTERY     0
/** 扩展臂 Arm F：唤醒脉冲宽度扫描 + 总线恢复（1=跑） */
#define DSO_ATECC_DIAG_ARM_F       1
/** 判定电池每臂的重试轮数（Arm A/B） */
#define DSO_ATECC_DIAG_ROUNDS      3u
/**
 * 判定电池启动前的额外静默等待（ms）：等 ETH/WAVE/WDOG 等启动期打印输出完，
 * 否则判定报告的中间几行会被别的模块的打印插花打散（实测 Arm C 被插花）。
 * 与 POR 要求无关（Arm 0 仍会打印真实的上电毫秒数）。
 */
#define DSO_ATECC_DIAG_SETTLE_MS   1200u

/* ---- CRC 约定：标准 vs **位序镜像**（2026-10-11） ----
 * 本批器件（仿制件）的 CRC 是标准 CRC-16/ARC 的位序镜像。证据来自它**自己发出的**
 * 两条帧（两个互相独立的 16 位匹配，偶然概率 ≈ 2^-32）：
 *   裸读覆盖 {04,11}：我们的 ARC = 0xCCC2，rev16 = 0x4333，它实测尾字节 33 43  ✔
 *   命令覆盖 {04,FF}：我们的 ARC = 0x8042，rev16 = 0x4201，它实测尾字节 01 42  ✔
 * 所以我们的帧（含正确 CRC）在它眼里"CRC 错" → 回 0xFF；而它的帧我们也验不过。
 * 正品 ATECC 用的是标准 CRC（2026-09-22 那颗片用同一份代码 Info rc=0 即为证）。 */
/** 1 = 允许诊断臂确认后**自动**切到镜像 CRC；0 = 永远用标准 CRC（正品行为）。 */
#define DSO_ATECC_CRC_MIRROR       1
/** 1 = 跑一次性 CRC 约定判定臂（<2 s，4+1 帧）；只在 Info 失败时触发。 */
#define DSO_ATECC_DIAG_CRC_CONV    1

/** I2C word address：0x03 = Command（ATECC608 系列唯一合法 word 地址） */
#define DSO_ATECC608B_WORD_CMD     0x03u

/** 命令组上限（含 Count 与 CRC，Microchip 规格 155） */
#define DSO_ATECC608B_GROUP_MAX    155u

/** 响应等待超时 ms（crypto 大命令如 Sign 后续按需放宽） */
#define DSO_ATECC608B_RESP_MS      300u

/* 常用 opcode（Sign/Verify/Nonce/AES 供上层登录流程使用，本轮未封装） */
#define ATECC_OP_READ      0x02u
#define ATECC_OP_INFO      0x30u
#define ATECC_OP_NONCE     0x16u
#define ATECC_OP_SIGN      0x41u
#define ATECC_OP_VERIFY    0x45u
#define ATECC_OP_AES       0x51u

/* Read 命令（**32 字节读**）的两个参数 —— 2026-10-11 实测修正：
 *   Param1 = 0x80：bit7=1 表示 32 字节读，bits6:4=zone(0=Config,2=Data)，**bits3:0 必须为 0**。
 *     （旧宏 `ATECC_READ32_CONFIG_BLOCK(blk)` 把块号塞进低 4 位 → 芯片对 blk≥1 回
 *       **0x0F ExecutionError**；blk=0 时 0x80|0 恰好等于正确值，所以只有第 0 块能读通。）
 *   Param2 = 地址：**以 4 字节字计**，Config 第 blk 块（32B）→ blk*8。
 *     （同款约定：数据区读槽 N 也是 N*8。） */
#define ATECC_READ32_P1            0x80u
#define ATECC_READ32_ADDR(blk)     ((uint16_t)((uint16_t)(blk) * 8u))

/* 错误码（<0）；最近一次执行状态码见 DSO_Atecc608b_LastStatus() */
#define DSO_ATECC_ERR_PARAM    (-5)   /* 长度/参数非法 */
#define DSO_ATECC_ERR_EXEC     (-4)   /* 芯片返回非 0 状态（读 LastStatus） */
#define DSO_ATECC_ERR_CRC      (-3)
#define DSO_ATECC_ERR_TIMEOUT  (-2)
#define DSO_ATECC_ERR_IO       (-1)

/**
 * 通用命令：组包发送 → 轮询执行完成 → 读取并 CRC 校验响应。
 * @return ≥0：输出字节数（0=成功无输出）；<0：DSO_ATECC_ERR_*。
 */
int DSO_Atecc608b_Exec(uint8_t opcode, uint8_t p1, uint16_t p2,
                       const uint8_t *in, uint16_t in_len,
                       uint8_t *out, uint16_t out_max);

/** 最近一次 Exec 返回的芯片状态字节（0x00=成功）。 */
uint8_t DSO_Atecc608b_LastStatus(void);

/**
 * CRC 约定开关：1 = 用**位序镜像** CRC（rev16），0 = 标准 CRC-16/ARC。
 * 由 CRC 约定判定臂在确认后自动置位；也可由上层显式设置（换片后需复位）。
 * 默认 0（标准），故对正品器件行为无任何影响。
 */
void DSO_Atecc608b_SetCrcMirror(uint8_t on);

/** 当前 CRC 约定：1 = 镜像，0 = 标准（供日志/遥测标注）。 */
uint8_t DSO_Atecc608b_CrcMirror(void);

/** Info(Revision)：返回 4 字节版本。@return 0 成功；<0 错误。 */
int DSO_Atecc608b_InfoRevision(uint8_t rev[4]);

/** Read：读 Config zone 第 block 个 32 字节块。@return 0 成功；<0 错误。 */
int DSO_Atecc608b_ReadConfig32(uint8_t block, uint8_t out[32]);

/**
 * 唤醒并按规范等待就绪（Sleep → 可访问）。
 * 顺序：≤100kHz 发 N 个 0x00（SDA 低 ≥60µs）→ 等 ≥tWHI → 以地址探针确认就绪。
 * @param zero_acked 非 NULL 时输出"0x00 是否被应答"（1=总线有器件回应通用呼叫）
 * @param ready_ms   非 NULL 时输出实测就绪耗时 ms（0xFFFF=超时未就绪）
 * @return 0 就绪；DSO_ATECC_ERR_TIMEOUT 未就绪。
 */
int DSO_Atecc608b_Wake(uint8_t *zero_acked, uint16_t *ready_ms);

/** 探测（Wake + Info 重试 ≤3 次）。@return 0 在位；<0 不在/错误。 */
int DSO_Atecc608b_Probe(void);

/** 创建一次性上电探测任务（打印 chip 识别/序列号/锁状态后自删）。 */
void DSO_Atecc608b_Init(void);

/* ==================== 授权/设备身份（Stage 1，**非破坏**） ====================
 * 依据 Markdown/STM32H743 + ATECC608B-SSHDA-T + LCMXO2 FPGA 远程学生设备授权体系.md。
 * 本阶段红线：**不写配置区、不写数据槽、不 Lock、不碰 OTP**（芯片保持可反复改）。
 * 只读 + 片内签名/验签（全部在芯片内部完成，密钥不出片）。 */

/* ---- 命令 opcode 与模式（ATECC608 定义；Mode/Param2 字节序按 CryptoAuthLib 惯例） ---- */
#define ATECC_OP_WRITE     0x12u   /* 预留：本阶段**不封装不调用**（写入属 Stage 2） */
#define ATECC_OP_GENKEY    0x40u   /* Mode 0x04=在槽内新建 ECC 私钥，返回 64B 公钥 */
#define ATECC_OP_SHA       0x47u   /* 预留（SHA/HMAC，Mode 区分） */
#define ATECC_OP_ECDH      0x43u   /* 预留 */
#define ATECC_NONCE_MODE_PASSTHRU   0x03u  /* 32B 输入直接作为 TempKey（"挑战即摘要"） */
#define ATECC_SIGN_MODE_DIGEST      0x80u  /* 对 TempKey 里的摘要用槽内私钥签名 */
#define ATECC_VERIFY_MODE_STORED    0x00u  /* 用槽内公钥验签 */
#define ATECC_GENKEY_MODE_NEW_ECC   0x04u  /* 新建 ECC 私钥（Mode 0x00=只取公钥） */

/** 设备证明用的私钥槽（文档：Slot0 = 设备 ECC P256 根私钥，Sign-only） */
#define DSO_ATECC_AUTH_SLOT         0u

/* ---- DeviceID：16 个十六进制字符 = 标签前 8 字符 + 8 字符随机段 ----
 * 随机段本阶段取自 **STM32 96-bit UID 的 CRC32**（跨重启稳定，是服务器 Device 表主键的前提；
 * 文档第 13 行本就允许用 STM32 UID 作复合身份）。**Stage 2 升级路径**：随机段改由 ATECC TRNG
 * 生成并写入 Slot1 持久化（格式不变，后端无感）。 */
#define DSO_DEVICE_LABEL            "Eep210251A08891"  /* 开发板物理标签；量产按台改 */
#define DSO_DEVICE_ID_LEN           16u                /* 字符数（不含结尾 NUL） */

/** 设备身份/芯片状态的只读快照（周期上报用；由探测任务与每次访问刷新） */
typedef struct {
    uint8_t present;      /* 1=地址应答且 Info 成功 */
    uint8_t comm_ok;      /* 1=最近一次访问成功 */
    uint8_t fail_cnt;     /* 连续失败次数 */
    uint8_t rev[4];       /* Info(Revision) 4 字节 */
    uint8_t serial4[4];   /* SN 低 4 字节（spec 的 serial4） */
    uint8_t lock_config;  /* 1=配置区已锁（cfg[0x57]==0） */
    uint8_t lock_data;    /* 1=数据区已锁（cfg[0x56]==0 或 SlotLocked 非 0） */
} DSO_Atecc608b_Status;

/** 读取只读状态快照（线程安全：内部短暂持总线锁）。 */
void DSO_Atecc608b_GetStatus(DSO_Atecc608b_Status *st);

/** DeviceID 16 字符（大写十六进制风格，标签段原样）写入 out[17]（含 NUL）。@return 0 成功 */
int DSO_Atecc608b_DeviceId16(char out[DSO_DEVICE_ID_LEN + 1u]);

/** STM32 96-bit UID 的 24 位十六进制串写入 out[25]（供后端对账）。@return 0 成功 */
int DSO_Atecc608b_UidHex(char out[25]);

/** 读取槽内公钥 64 字节（X||Y）。只读：Data 区两次 32B Read。
 *  @return 0 成功；<0 失败（私钥槽常被配成 ReadConfig=Never，此时须在 GenKey 时取公钥） */
int DSO_Atecc608b_GetPubKey64(uint8_t slot, uint8_t out[64]);

/** 片内挑战签名自测：Nonce(Mode 0x03 挑战即摘要) → Sign(Slot0) → Verify(Stored, Slot0)。
 *  全程密钥不出片、无任何写入。@param allow_genkey 1=仅当签名因"槽内无密钥"失败时，
 *  允许 GenKey(Mode 0x04) 新建 Slot0 私钥后重试（**会消耗一次空槽→有密钥的转变**，
 *  默认 0 即纯只读）。@return 0 成功；<0 错误码；verify_ok 输出片内验签结果（1=通过） */
int DSO_Atecc608b_AuthSelfTest(const uint8_t challenge[32], uint8_t sig_out[64],
                               uint8_t *verify_ok, uint8_t allow_genkey);

/** 只读：逐槽打印 SlotConfig（原始字节 + 按 ATECC608 惯例解读），供"先读后写"决策。
 *  @param cfg128 调用方已读到的 128 字节配置区；传 NULL 则函数自己读（多花 ~4 次读） */
void DSO_Atecc608b_DumpSlotConfig(const uint8_t *cfg128);

/** 上电是否跑一次片内挑战签名自测（bring-up 期 =1；稳定后可设 0）。
 *  用固定挑战（仅自检用），正式流程由服务器下发随机挑战。 */
#define DSO_ATECC_AUTH_SELFTEST_AT_BOOT   1

/** 连续访问失败达到该次数判为 tamper（遥测事件阈值；0 时不可为 0） */
#define DSO_ATECC_TAMPER_FAIL             10u

/** 字节数组 → 大写十六进制串（out 需 2*n+1 字节）。命令应答里格式化签名/公钥用。 */
void DSO_Atecc608b_HexUpper(const uint8_t *b, uint16_t n, char *out);

#ifdef __cplusplus
}
#endif

#endif /* DSO_ATECC608B_H */
