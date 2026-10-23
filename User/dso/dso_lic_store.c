/**
 * License 持久化实现（AT24C128 @ 0x50，PB7/PB8 软件 I²C）。
 *
 * 安全边界：写的是**签名后的 License 原文**（公开且可验证的凭证），不是私钥；
 * 篡改会被 CRC32 或签名校验拒绝。全程不触碰内部 Flash / ATECC / OTP。
 */
#include "dso_lic_store.h"

#include <string.h>

#include "bsp.h"
#include "bsp_i2c_ui.h"

#define EE_ADDR7        DSO_LIC_STORE_EE_ADDR7
#define EE_PAGE         64u
#define REC_VER_OFF     4u
#define REC_LEN_OFF     6u
#define REC_TOK_OFF     8u
#define REC_CRC_OFF     (REC_TOK_OFF + DSO_LIC_STORE_MAX_TOKEN)   /* 348 */
#define REC_SIZE        (REC_CRC_OFF + 4u)                        /* 352 */

/* 授权路径是串行的（命令通道互斥），静态缓冲足够，避免占任务栈。 */
static uint8_t s_rec[REC_SIZE];
static uint8_t s_rb[REC_SIZE];        /* read-back / load buffer */

static uint32_t crc32_ieee(const uint8_t *d, uint32_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    uint32_t i;
    uint8_t j;

    for (i = 0u; i < n; i++) {
        c ^= (uint32_t)d[i];
        for (j = 0u; j < 8u; j++) {
            if ((c & 1u) != 0u) {
                c = (c >> 1) ^ 0xEDB88320u;
            } else {
                c = c >> 1;
            }
        }
    }
    return ~c;
}

/* 写周期内 EEPROM 不应答（返回 NACK），轮询直到重新 ACK。 */
static int ee_wait_ready(void)
{
    uint32_t i;
    int ack;

    for (i = 0u; i < 50u; i++) {                 /* 最多约 50 ms */
        BSP_I2C_UI_Lock();
        ack = BSP_I2C_UI_Probe(EE_ADDR7);
        BSP_I2C_UI_Unlock();
        if (ack == 0) {
            return 0;
        }
        bsp_DelayUS(1000u);
    }
    return -1;
}

static int ee_write(uint16_t addr, const uint8_t *data, uint16_t n)
{
    uint8_t chunk[2u + EE_PAGE];
    uint16_t off = 0u;
    int rc;

    while (off < n) {
        uint16_t a = (uint16_t)(addr + off);
        uint16_t page_left = (uint16_t)(EE_PAGE - (a & (EE_PAGE - 1u)));
        uint16_t m = (uint16_t)((n - off) < page_left ? (n - off) : page_left);

        chunk[0] = (uint8_t)(a >> 8);
        chunk[1] = (uint8_t)(a & 0xFFu);
        memcpy(&chunk[2], &data[off], m);

        BSP_I2C_UI_Lock();
        rc = BSP_I2C_UI_WriteRaw(EE_ADDR7, chunk, (uint8_t)(2u + m));
        BSP_I2C_UI_Unlock();
        if (rc != 0) {
            return -1;
        }
        if (ee_wait_ready() != 0) {
            return -1;
        }
        off = (uint16_t)(off + m);
    }
    return 0;
}

static int ee_read(uint16_t addr, uint8_t *buf, uint16_t n)
{
    uint16_t off = 0u;
    int rc;

    while (off < n) {
        uint16_t a = (uint16_t)(addr + off);
        uint16_t m = (uint16_t)((n - off) < EE_PAGE ? (n - off) : EE_PAGE);
        uint8_t a2[2];

        a2[0] = (uint8_t)(a >> 8);
        a2[1] = (uint8_t)(a & 0xFFu);
        BSP_I2C_UI_Lock();
        rc = BSP_I2C_UI_WriteRead(EE_ADDR7, a2, 2u, &buf[off], (uint8_t)m);
        BSP_I2C_UI_Unlock();
        if (rc != 0) {
            return -1;
        }
        off = (uint16_t)(off + m);
    }
    return 0;
}

int DSO_LicStore_Save(const char *token, uint16_t len)
{
    uint32_t crc;

    if (token == NULL || len == 0u || len > DSO_LIC_STORE_MAX_TOKEN) {
        return -1;
    }

    (void)memset(s_rec, 0, sizeof(s_rec));
    s_rec[0] = 'L';
    s_rec[1] = 'I';
    s_rec[2] = 'C';
    s_rec[3] = 'S';
    s_rec[REC_VER_OFF] = 1u;
    s_rec[REC_VER_OFF + 1u] = 0u;
    s_rec[REC_LEN_OFF] = (uint8_t)(len & 0xFFu);
    s_rec[REC_LEN_OFF + 1u] = (uint8_t)(len >> 8);
    (void)memcpy(&s_rec[REC_TOK_OFF], token, len);

    crc = crc32_ieee(s_rec, REC_CRC_OFF);
    s_rec[REC_CRC_OFF] = (uint8_t)(crc & 0xFFu);
    s_rec[REC_CRC_OFF + 1u] = (uint8_t)((crc >> 8) & 0xFFu);
    s_rec[REC_CRC_OFF + 2u] = (uint8_t)((crc >> 16) & 0xFFu);
    s_rec[REC_CRC_OFF + 3u] = (uint8_t)((crc >> 24) & 0xFFu);

    if (ee_write(DSO_LIC_STORE_SLOT_ADDR, s_rec, REC_SIZE) != 0) {
        return -1;                       /* bus/ACK failure */
    }
    /* Read back: a "successful" write that did not land would otherwise look
     * like persistence worked until the next reboot. */
    if (ee_read(DSO_LIC_STORE_SLOT_ADDR, s_rb, REC_SIZE) != 0) {
        return -2;                       /* read-back failed */
    }
    if (memcmp(s_rb, s_rec, REC_SIZE) != 0) {
        uint16_t i;
        for (i = 0u; i < REC_SIZE; i++) {
            if (s_rb[i] != s_rec[i]) {
                return (int)(-100 - (int)i);   /* encode first mismatch offset */
            }
        }
        return -3;
    }
    return 0;
}

int DSO_LicStore_Load(char *buf, uint16_t cap, uint16_t *out_len)
{
    uint32_t crc;
    uint16_t len;

    if (buf == NULL || out_len == NULL || cap < 2u) {
        return -1;
    }
    if (ee_read(DSO_LIC_STORE_SLOT_ADDR, s_rec, REC_SIZE) != 0) {
        return -1;                               /* bus / ACK failure */
    }
    if (memcmp(s_rec, "LICS", 4u) != 0) {
        return -2;                               /* empty chip / cleared */
    }
    if (s_rec[REC_VER_OFF] != 1u || s_rec[REC_VER_OFF + 1u] != 0u) {
        return -3;                               /* unknown version */
    }
    len = (uint16_t)((uint16_t)s_rec[REC_LEN_OFF] |
                     ((uint16_t)s_rec[REC_LEN_OFF + 1u] << 8));
    if (len == 0u || len > DSO_LIC_STORE_MAX_TOKEN || len >= cap) {
        return -4;                               /* bad length */
    }
    crc = crc32_ieee(s_rec, REC_CRC_OFF);
    if (s_rec[REC_CRC_OFF] != (uint8_t)(crc & 0xFFu) ||
        s_rec[REC_CRC_OFF + 1u] != (uint8_t)((crc >> 8) & 0xFFu) ||
        s_rec[REC_CRC_OFF + 2u] != (uint8_t)((crc >> 16) & 0xFFu) ||
        s_rec[REC_CRC_OFF + 3u] != (uint8_t)((crc >> 24) & 0xFFu)) {
        return -5;                               /* CRC mismatch (partial write?) */
    }

    (void)memcpy(buf, &s_rec[REC_TOK_OFF], len);
    buf[len] = '\0';
    *out_len = len;
    return 0;
}

int DSO_LicStore_Clear(void)
{
    static const uint8_t zero[4] = { 0u, 0u, 0u, 0u };

    return ee_write(DSO_LIC_STORE_SLOT_ADDR, zero, 4u);
}
