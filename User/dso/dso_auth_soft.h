/**
 * DEV software authorization backend (Stage 2C) — P-256 ECDSA on the STM32.
 *
 * This exists because the ATECC608B on this board is factory-unlocked: its data
 * zone is inaccessible, so it can neither sign nor verify (see
 * Markdown/ATECC608B_结论_配置区未锁定.md). The backend lets the whole
 * authorization chain run on the real device WITHOUT touching the chip.
 *
 * ⚠️ DEV ONLY:
 *   - the private key is the PUBLIC test key 00 01 … 1F (dso_devkey.c);
 *   - signing is deterministic (RFC6979), no RNG;
 *   - it must be compiled in only behind DSO_AUTH_DEV_SOFTKEY and never shipped.
 *
 * It does not gate anything: it reports license state only.
 */
#ifndef DSO_AUTH_SOFT_H
#define DSO_AUTH_SOFT_H

#include <stdint.h>

/** Parsed license fields (fixed-size, matching the PC token profile). */
typedef struct {
    char     lic[17];
    char     user[33];
    char     dev[17];
    uint32_t nb;
    uint32_t na;
    uint32_t perm;
} DSO_AuthLicense;

/** Error codes (negative = failure). */
#define DSO_AUTH_OK           0
#define DSO_AUTH_ERR_INIT    (-1)
#define DSO_AUTH_ERR_FORMAT  (-2)   /* token shape / JSON not canonical */
#define DSO_AUTH_ERR_DEVICE  (-3)   /* token bound to another device */
#define DSO_AUTH_ERR_SIG     (-4)   /* signature does not verify */
#define DSO_AUTH_ERR_PARAM   (-5)

/** Initialize the arena, load the P-256 group and run the self-test.
 *  @return 0 on success; <0 if mbedTLS init or the self-test failed. */
int DSO_AuthSoft_Init(void);

/** Sign a 32-byte digest with the DEV key (RFC6979). */
int DSO_AuthSoft_SignDigest(const uint8_t digest[32], uint8_t sig[64]);

/** Verify r||s over a 32-byte digest. Accepts high-s (the ATECC emits it too). */
int DSO_AuthSoft_VerifyDigest(const uint8_t pub[64], const uint8_t digest[32],
                              const uint8_t sig[64]);

/** Parse + verify a license token (bound to this device, signed by the CA). */
int DSO_AuthSoft_VerifyLicense(const char *token, DSO_AuthLicense *out);

/** DEV public key (64 bytes X||Y) used for `atecc.get_pubkey` / `auth_start`. */
const uint8_t *DSO_AuthSoft_Pubkey(void);

/** Reported license state: "NO_LICENSE" / "AUTHORIZED". */
const char *DSO_AuthSoft_LoginState(void);

#endif /* DSO_AUTH_SOFT_H */
