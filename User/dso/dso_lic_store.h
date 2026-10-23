/**
 * License 持久化（AT24C128）—— 只写外部 EEPROM，不碰内部 Flash / ATECC / OTP。
 *
 * 器件：AT24C128C-SSHM，7-bit 地址 0x50（A2A1A0=000），2 字节地址，页 64 B，
 *       与 ATECC/SHT40/INA226/PCAL6524 共用 PB7(SDA)/PB8(SCL) 软件 I²C（有总线锁）。
 *
 * 记录布局（0x0000 起，512 B 槽位）：
 *   0    4   magic "LICS"
 *   4    2   version = 1
 *   6    2   token 长度（1..340，小端）
 *   8  340   License token 原文（ASCII，签名后的凭证，非秘密）
 * 348    4   CRC32（覆盖 0..347，小端）
 */
#ifndef DSO_LIC_STORE_H
#define DSO_LIC_STORE_H

#include <stdint.h>

#define DSO_LIC_STORE_EE_ADDR7   0x50u
#define DSO_LIC_STORE_MAX_TOKEN  340u
#define DSO_LIC_STORE_SLOT_ADDR  0x0000u

/** 写入一条 License（覆盖旧记录）。@return 0 成功；<0 失败（EEPROM 不在/写保护）。 */
int DSO_LicStore_Save(const char *token, uint16_t len);

/** 读回并校验 magic/version/CRC；成功时把 token 写进 buf 并以 NUL 结尾。
 *  @return 0 成功；<0 无记录 / 校验失败。 */
int DSO_LicStore_Load(char *buf, uint16_t cap, uint16_t *out_len);

/** 让记录失效（只清 magic）。 */
int DSO_LicStore_Clear(void);

#endif /* DSO_LIC_STORE_H */
