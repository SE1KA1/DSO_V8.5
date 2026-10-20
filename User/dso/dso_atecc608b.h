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
 * 注：**"500µs 对正品也可靠"这一点本项目没有证据**。早期"2026-09-22 那颗正品在 500µs 下也通"
 * 的说法经取证（git log -S + 全库检索）否定：那天的同期日志与提交正文都写着三地址全 NACK，
 * 而 `Info rc=0 / 00006003` 首次进入 git 是 2026-10-11（"镜像 CRC 仿制片"这一判定已于
 * 2026-10-20 撤回：是我方 CRC 变体算错，器件本身合规）。
 * 20µs 是目前**唯一**验证过能读回响应的档位。
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

/* ---- 分层判定实验（只读诊断；正式路径已能识别本批器件，故默认关闭） ---- */
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

/* ---- CRC（2026-10-20 校准：此前"镜像 CRC ⇒ 仿制件"的判定是**我们的算法错了**） ----
 * ATECC 的 CRC 是：poly 0x8005、init 0、**输入位按 LSB-first 处理**（等价于 BUYPASS 参数 +
 * 每字节位反转）。驱动里的 atecc_crc16() 已按此实现，并用 Microchip 官方库实测校准：
 *   atCRC("123456789") = 0xBCDD；atCRC({04,11}) = 0x4333；atCRC(Info 组) = 0x5D03
 * 旧实现用的是 CRC-16/ARC（反射多项式 0xA001，满足公开向量 0xBB3D）——**不是** ATECC 的算法，
 * 两者只差一次 16 位反转，因此曾误判"器件只接受镜像 CRC"。**该结论已撤回**：官方库与器件
 * 逐帧一致，器件在 CRC 上完全合规。勿再引入"镜像约定"这类运行时切换。 */

/**
 * 1 = 开机地址扫描时对**每个未知地址**做"命令型判定"：先唤醒再探地址 →
 * 地址 ACK 且连续两次读 NACK（命令型）→ 用**标准与镜像两种 CRC 各试一次 Info**；
 * 若读回的字节构成**经 CRC 验证的 ATECC 帧**（count=0x04+文档状态码 或 0x07，且随后两字节等于
 * {count..} 的 CRC——标准或镜像之一），也按 ATECC 处理。命中即把 `s_addr` 切过去，
 * 后续启动流程（配置区 dump 等）自动接管。
 * 用途：查清"不应答的片子是否活着、只是在别的地址"（2026-10-13 由此定案：旧片在 0x64）。
 * 只发 Info(0x30)（只读）；对 EEPROM 区间（0x50-0x57）与已知器件**绝不发命令**。
 */
#define DSO_ATECC_DIAG_ADDR_VERDICT 1

/**
 * 1 = 开机跑一次**单命令取证臂**（Info × 4 个 CRC 候选尾字节，整块打印）。
 * 已被下面的多命令矩阵取代，默认关闭（留作后备）。
 */
#define DSO_ATECC_EVIDENCE_ONESHOT 0

/**
 * 1 = 开机跑**多命令 CRC 对照矩阵**（取证用）：5 条合法只读命令（Info/Random/Read×2/Nonce，
 * 帧长与 CRC 覆盖各不相同）× 4 个变体（**只改最后两个 CRC 字节**：标准 / 16 位整体反转 /
 * 两字节交换 / 含 word 地址覆盖），另附"Random 连读两次 + 锁字节"判读。
 * 2026-10-20 实测结论：`std 5/5 | rev16 0/5 | swap 0/5 | word3 0/5`（CRC 已校准，器件合规）；
 * Random 的固定图案 `FF FF 00 00…` 是**配置区未锁定时的出厂测试图案**（数据手册规定），非缺陷。
 * 取证完成后可置 0 去掉约 2 s 开机开销。
 */
#define DSO_ATECC_EVIDENCE_MATRIX  1

/* ---- Stage 2 最小步骤：Slot0 预置私钥（**开发用，默认关闭**）----
 * 目的：把设备私钥写进 Slot0，使 `atecc.auth_start`（Nonce→Sign→Verify）真正跑通，
 *       从而把整条授权链路在真机上打通（将来换正品流程原样可用）。
 * ⚠️ 定位：**2026-10-20 更正** —— "镜像 CRC 仿制件 / 随机源是桩"两项判定均已撤回。器件当前是
 *    **出厂未锁定态**（LockValue/LockConfig = 0x55/0x55），此状态下数据区写与 GenKey 都会按
 *    手册规定被拒（0x0F）⇒ 本流程在当前状态**预期失败**，失败不代表器件有问题。
 *    写入的是一把**公开的一次性测试密钥**（私钥明文在固件里，仅用于打通链路）。
 * 红线：**不 Lock、不碰 OTP、不写配置区**（除非另行批准）。 */
/** 1 = 开机跑一次性预置任务（先写备用槽验证写路径，再试写 Slot0）。**默认 0**。
 *  2026-10-15 预置实测已做完（未锁状态下三条路线均被拒，属预期）⇒ 复位为 0；
 *  待"是否需要锁定配置区"决策后再置 1（预置本身幂等）。 */
#define DSO_ATECC_PROVISION_ONESHOT 0u
/**
 * Write 命令（0x12）写**数据区 32 字节**时 Param1(Mode) 的取值。
 * 候选 `0x82` = bit7=1(32B) + bits1:0=10(数据区)；另一说法 `0xA0` = bits6:4 编码 zone。
 * ⚠️ **必须由预置任务第 1 步的只读判定实测确认**（打印 `[ATECC-P] data-zone param1 = …`）：
 *    该值用错会把 32 字节写进**配置区**（SN/I2C 地址所在块）—— 本流程最大的风险点。
 */
#define ATECC_WRITE_MODE_DATA32    0x82u
/** 第 2 步"写路径验证"用的**备用槽**。注意（2026-10-20 实测）：本片 slot5 的 SlotConfig 是
 *  **0x8F8F**（IsSecret=1、wrCfg=8=Never），并非 0x0000 ⇒ 直接写会被策略拒绝；若要做写路径
 *  实验，需先把 slot5 的 SlotConfig 改成 0x0000（改配置区，可逆，需另行批准）。 */
#define ATECC_WRITE_TEST_SLOT      5u
/**
 * 1 = 允许"退到**片上 GenKey（Mode 0x04）**"路线（即计划的**选项 2**）。
 * 背景（2026-10-15 实测 + 2026-10-20 更正）：数据槽读/写回 `0x0F`、GenKey 回 `0x0F`，
 * 其原因是**配置区未锁定**（出厂测试态；同时 SlotConfig 也限制访问）⇒ **不是器件缺陷**。
 * 未锁状态下 GenKey 预期仍被拒；它只在"写完配置区并锁定"之后才有意义。
 * **默认 0：不经你明确同意，绝不往 Slot0 写任何东西。**
 */
#define DSO_ATECC_PROV_GENKEY      0u

/* ---- 可逆实验：SlotConfig[5] 0x8F8F -> 0x0000（**唯一一次的配置区写**，DEV ONLY）----
 * 目的：分离"数据区 0x0F 是槽策略导致"还是"配置区未锁定导致"。步骤：
 *   1) 只读验址：Read(0x02, P1=0x00, P2=0x0007) 必须回 8F 8F 8F 8F（证明 P2 是**字号**）；
 *   2) 写配置区 **word 7**（字节 0x1C-0x1F）为 8F 8F 00 00 —— 只动 0x1E/0x1F；
 *   3) 整块回读逐字节比对（其它字节必须不变）；
 *   4) 跑现有写路径测试（槽5 写图案再读回）判定数据区在未锁状态下是否可写。
 * 红线：**不发送 Lock**、**不碰 OTP**、不改其它任何配置字节、失败即停不重试。
 * ⚠️ Mode=0x00 是"4 字节 + Config zone"；Param2=7 是**字号**（不是字节偏移 0x1C，
 *    后者会解码成 block3/word4 从而写到 0x70-0x73，且可能不报错）。 */
#define DSO_ATECC_CFG5_ONESHOT     1u
#define ATECC_WRITE_MODE_CFG4      0x00u
#define ATECC_CFG_SLOT5_WORD       0x0007u
/* 1 = 本次运行执行**恢复**：把 word7 写回出厂值 `8F 8F 8F 8F`（即 SlotConfig[5]=0x8F8F），
 * 不跑数据区测试；已是出厂值则跳过（幂等，可放心反复上电）。0 = 执行原实验（改成 0x0000）。 */
#define DSO_ATECC_CFG5_REVERT      1u

#if (DSO_ATECC_CFG5_ONESHOT != 0u) && (DSO_ATECC_PROVISION_ONESHOT != 0u)
#error "enable only ONE ATECC one-shot writer (CFG5 vs PROVISION)"
#endif

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
#define ATECC_OP_RANDOM    0x1Bu   /* Random（CRC 对照矩阵用；只读） */
#define ATECC_OP_SIGN      0x41u
#define ATECC_OP_VERIFY    0x45u
#define ATECC_OP_AES       0x51u

/* Read 命令（**32 字节读**）的两个参数 —— 2026-10-11 实测修正：
 *   Param1 = 0x80：bit7=1 表示 32 字节读，bits6:4=zone(0=Config,2=Data)，**bits3:0 必须为 0**。
 *     （旧宏 `ATECC_READ32_CONFIG_BLOCK(blk)` 把块号塞进低 4 位 → 芯片对 blk≥1 回
 *       **0x0F ExecutionError**；blk=0 时 0x80|0 恰好等于正确值，所以只有第 0 块能读通。）
 *   Param2 = 地址：**以 4 字节字计**，Config 第 blk 块（32B）→ blk*8。
 *     （同款约定：数据区读槽 N 也是 N*8。）
 *   ⚠️ zone 位：数据手册说在 **bits1:0**（00=Config / 10=Data ⇒ 数据区 32B 读写 = 0x82）；
 *     我们此前按 bits6:4 理解。两种说法都能解释我们唯一的实测点（0x80 = Config）⇒ **未定**，
 *     由 `DSO_ATECC_PROVISION_ONESHOT` 第 1 步做**只读判定**坐实（见 ATECC_WRITE_MODE_DATA32）。 */
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
#define ATECC_OP_WRITE     0x12u   /* 只由 Stage 2 预置路径使用（DSO_Atecc608b_ProvisionSlot0）*/
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

/* ---- Stage 2 最小步骤：Slot0 预置（**开发用，非破坏**）---- */
/** 公钥不吻合（写入没生效 / 该片的私钥→公钥与标准 P-256 不一致） */
#define DSO_ATECC_ERR_VERIFY   (-6)

/**
 * 把 priv（32 字节 P-256 私钥，**大端**）写进 Slot0，再读回公钥与 pub_expect 比对。
 * 前置（函数内部自查）：`s_addr == 0x60` 且 Info 复验 `rev[2]==0x60`、芯片未锁、
 * Slot0 的 WriteConfig 允许写入。
 * 注意：Slot0 的 IsSecret 已为 1 ⇒ 写进去的私钥**读不出来**，只能用公钥比对验证 ✓。
 * @param priv       32 字节私钥（大端）
 * @param pub_expect 期望的 64 字节公钥 X||Y（大端），用于写后比对
 * @return 0 成功（被接受且公钥吻合）；DSO_ATECC_ERR_EXEC 被芯片拒绝（状态见 LastStatus）；
 *         DSO_ATECC_ERR_VERIFY 写入被接受但公钥不吻合；其余为传输层错误码。
 */
int DSO_Atecc608b_ProvisionSlot0(const uint8_t priv[32], const uint8_t pub_expect[64]);

/** 创建一次性预置任务（**1024 字栈**；受 DSO_ATECC_PROVISION_ONESHOT 控制，默认不创建）。 */
void DSO_Atecc608b_Provision_Init(void);

/** 创建"SlotConfig[5] -> 0x0000"可逆实验任务（**1024 字栈**；受 DSO_ATECC_CFG5_ONESHOT 控制）。
 *  只写配置区 word 7（字节 0x1C-0x1F）里的 0x1E/0x1F 两个字节；不 Lock、不碰 OTP。 */
void DSO_Atecc608b_Cfg5Exp_Init(void);

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
