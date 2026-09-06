/**
 * @file dso_atecc608b.h
 * @brief ATECC608B 密码芯片驱动（软件 I2C，PB7=SDA/PB8=SCL，地址 0x60）。
 *
 * 覆盖：I2C 命令/执行状态机 + 响应 CRC16 校验 + Info/Config 读。
 * 登录（Nonce/Sign/Verify/AES）等上层流程定稿后再封装，本轮只留 opcode 宏。
 * 数据面 schema 见 module_telemetry/packet_specs/01_atecc608b.md。
 */
#ifndef DSO_ATECC608B_H
#define DSO_ATECC608B_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** I2C 7 位地址（PB7/PB8 软件 I2C 总线，与 PCAL6524/SHT40/INA226 共用） */
#define DSO_ATECC608B_I2C_ADDR     0x60u

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

/** 探测（Info 重试 ≤3 次）。@return 0 在位；<0 不在/错误。 */
int DSO_Atecc608b_Probe(void);

/** 创建一次性上电探测任务（打印 chip 识别/序列号/锁状态后自删）。 */
void DSO_Atecc608b_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* DSO_ATECC608B_H */
