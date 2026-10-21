/**
 * Minimal mbedTLS 2.28.3 configuration — DSO Stage 2C (DEV software P-256).
 *
 * This replaces the stock mbedTLS config. It enables ONLY what the device-side
 * authorization backend needs: P-256 ECDSA with RFC6979 deterministic signing,
 * SHA-256, and a fixed static arena for the ECP point tables.
 *
 * Deliberately NOT defined (each one is an AC5 or dependency trap):
 *   MBEDTLS_HAVE_ASM      ARMCC 5 cannot assemble GNU inline asm
 *   MBEDTLS_HAVE_INT64    128-bit attribute paths; 32-bit limbs are fine here
 *   MBEDTLS_HAVE_TIME(_DATE), TIMING_C, FS_IO, NET_C, ENTROPY_C, CTR_DRBG_C,
 *   SELF_TEST, THREADING_C, X509_*, PK_*, SSL_*, AES_C, CIPHER_C, RSA_C,
 *   ECDH_C, OID_C, PEM_*, BASE64_C, PSA_CRYPTO_C
 *
 * The private key is a fixed public test key and signing is deterministic
 * (RFC6979), so no entropy source and no RNG are needed anywhere.
 */
#ifndef MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_H

/* ---- platform / memory: one fixed arena, never the FreeRTOS heap ---- */
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_MEMORY_BUFFER_ALLOC_C

/* ---- bignum / curve (P-256 only) ---- */
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_MAX_BITS        256
#define MBEDTLS_ECP_WINDOW_SIZE     2     /* T_size 4: ~1.5-2 KB arena peak */
#define MBEDTLS_ECP_FIXED_POINT_OPTIM 1
#define MBEDTLS_ECP_NO_INTERNAL_RNG      /* verify/sign need no DRBG blinding */

/* ---- ECDSA (deterministic / RFC6979) ---- */
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECDSA_DETERMINISTIC

/* ---- ASN.1: check_config.h requires these whenever ECDSA_C is on ---- */
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C

/* ---- hash / DRBG ---- */
#define MBEDTLS_MD_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_HMAC_DRBG_C
#define MBEDTLS_NO_PLATFORM_ENTROPY

#include "mbedtls/check_config.h"

#endif /* MBEDTLS_CONFIG_H */
