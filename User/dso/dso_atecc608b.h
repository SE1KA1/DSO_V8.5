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
 * **当前测试值 0x35**（TNGTLS 家族默认地址之一；总线写字节 = (0x35<<1)|0 = 0x6A，
 * 读字节 = 0x6B）—— 用户要求以 0x35 为主地址再试一轮。
 * 板上实物为 ATECC608B-SSHDA-T（用户确认），其标准版默认地址是 **0x60**
 * （写 0xC0 / 读 0xC1）；另一变体 0x36（写 0x6C / 读 0x6D）。
 * 三者都在探测任务里并列探测，改主地址只需改本宏。
 * BSP 原语一律传 7 位地址（内部左移），禁止传左移后的 0x6A 之类。
 */
#define DSO_ATECC608B_I2C_ADDR     0x35u

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
 * ATECC **会话档**位延迟（半周期 µs）：Info/Read 等正式命令期间用。取 30µs ≈16kHz，
 * 与 BSP 的 ACK 多点采样（32 点、步长 = 半周期/32）正好铺满整个 SCL 高电平期；
 * 数据相位也有 30µs 的建立/保持余量（默认 2µs 对这颗片可能偏快）。
 */
#define DSO_ATECC_SESSION_BIT_US   30u
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

/* ---- 分层判定实验（只读诊断；定论后可整块关掉，不影响正式路径） ---- */
/** 上电探测任务是否跑 [ATECC-0..F] 判定电池（1=跑） */
#define DSO_ATECC_DIAG_BATTERY     1
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

/* Read 命令 Param1：bit7=32 字节读；bits6:4=zone(0=Config,2=Data)；bits3:0=block */
#define ATECC_READ32_CONFIG_BLOCK(blk)  ((uint8_t)(0x80u | ((blk) & 0x0Fu)))

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

#ifdef __cplusplus
}
#endif

#endif /* DSO_ATECC608B_H */
