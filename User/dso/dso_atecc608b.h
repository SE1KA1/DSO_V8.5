/**
 * @file dso_atecc608b.h
 * @brief ATECC608B-SSHDA-T 密码芯片驱动（软件 I2C，PB7=SDA/PB8=SCL，7-bit 地址 0x60）。
 *
 * 唤醒规范（ATECC608 全系）：上电后 ≥20ms 才可动 I2C；必须先 Wake —— 以 ≤100kHz 发
 * 0x00 使 SDA 连续低 ≥60µs（tWLO），释放后上升沿唤醒；Wake 后等 ≥1.5ms（tWHI）再
 * 访问 7-bit 地址（SSHDA 0x60：总线写字节 0xC0 / 读 0xC1；TNGTLS 定制版为 0x35）。
 * 未满足 Wake 时芯片处于 Sleep，对地址一律 NACK（看起来像总线上没有器件）。
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
 * 当前测试值 **0x36**（ATECC608 Trust 定制版 TNGTLS 家族的默认地址之一；
 * 总线上写字节 = (0x36<<1)|0 = 0x6C，读字节 = 0x6D）。
 * 其他可能值：SSHDA-T 标准版 0x60（写 0xC0/读 0xC1）、TNGTLS 另一变体 0x35
 * （写 0x6A/读 0x6B）——三者都在探测任务中并列打印，改地址只需改本宏。
 * BSP 原语一律传 7 位地址（内部左移），禁止传左移后的 0x6C 之类。
 */
#define DSO_ATECC608B_I2C_ADDR     0x36u

/* ---- 唤醒/时序参数（TNGTLS 规范） ---- */
/** 上电后首次动 I2C 前的等待（规范 ≥20ms，取 5× 余量） */
#define DSO_ATECC_POR_WAIT_MS      100u
/** Wake 后最小等待 tWHI（≥1.5ms） */
#define DSO_ATECC_TWHI_MS          2u
/** 未就绪时的保守等待（自检使能 tWHIST ≥20ms） */
#define DSO_ATECC_TWHI_STD_MS      25u
/** 识别阶段位延迟（5µs 半周期 ≈ 93kHz ≈ 100kHz 标准模式） */
#define DSO_ATECC_BIT_DELAY_US     5u
/** Wake 用 0x00 字节数（100kHz 下每字节 SDA 低 ≈86µs） */
#define DSO_ATECC_WAKE_ZB_N        3u
/** 每帧 Exec 前是否先 Wake（识别阶段 =1；将来周期上报可调 0/加窗口判断） */
#define ATECC_WAKE_BEFORE_EXEC     1

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
