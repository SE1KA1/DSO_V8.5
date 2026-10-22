/**
 * DEV software authorization backend (Stage 2C) — see dso_auth_soft.h.
 *
 * mbedTLS is used with a MINIMAL config (Middlewares/Third_Party/mbedtls):
 * P-256 + ECDSA(deterministic) + SHA-256, no entropy, no ASM, and a fixed
 * 4 KB arena so the ECP point tables never touch the FreeRTOS heap.
 *
 * Threading: every entry point runs under the command-channel mutex
 * (dso_switch_cmd.c). The shared group and arena are not thread-safe, so do
 * not call these functions from anywhere else or from an ISR.
 */
#include "dso_auth_soft.h"

#include <stdio.h>
#include <string.h>

#include "dso_auth_ca.h"
#include "dso_b64.h"
#include "dso_devkey.h"
#include "dso_atecc608b.h"          /* DSO_Atecc608b_DeviceId16 */
#include "debug_uart_lock.h"

#include "FreeRTOS.h"
#include "task.h"

#include "mbedtls/platform.h"
#include "mbedtls/bignum.h"
#include "mbedtls/ecp.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/md.h"                 /* MBEDTLS_MD_SHA256 */
#include "mbedtls/sha256.h"
#include "mbedtls/memory_buffer_alloc.h"

/* 4 KB fixed arena: ECP point tables live here, never in the 160 KB heap
 * (which already runs close to its limit). */
static unsigned char s_arena[4096];

static mbedtls_ecp_group s_grp;
/* 0 = not initialized yet, 1 = ready, 2 = init failed, 3 = initializing.
 * Initialization is LAZY (first sign/verify, on the caller's task): mbedTLS
 * needs ~4 KB of stack while the boot stack is only 4 KB in total. */
static volatile uint8_t s_init_state = 0u;
static uint8_t s_res_after_sign = 0u;
static uint8_t s_res_after_verify = 0u;
static uint8_t s_boot = 1u;          /* 1 until Init finishes (selftest is not a real call) */
static char s_login[16] = "NO_LICENSE";

/* Lazy init (first use, on the caller's task). */
static int authsoft_ensure(void);

/* Known-answer vector (generated on the PC with the same public test key).
 * Kept independent of the CA so this check survives CA rotation. */
static const uint8_t k_selftest_digest[32] = {
    0x00u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u,
    0x88u, 0x99u, 0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu, 0xFFu,
    0x00u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u,
    0x88u, 0x99u, 0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu, 0xFFu
};
static const uint8_t k_selftest_sig[64] = {
    0xF7u, 0xB5u, 0xFEu, 0x19u, 0x53u, 0xF6u, 0xF8u, 0x61u,
    0x3Eu, 0x8Fu, 0x76u, 0x59u, 0x80u, 0x44u, 0x4Fu, 0x53u,
    0x39u, 0x6Eu, 0xC4u, 0x33u, 0xFAu, 0x40u, 0xFBu, 0x4Bu,
    0x06u, 0x88u, 0x5Fu, 0x21u, 0xEFu, 0x73u, 0xF5u, 0x78u,
    0x29u, 0x1Au, 0xF6u, 0x7Bu, 0xACu, 0x57u, 0x19u, 0x25u,
    0x7Au, 0x90u, 0xE0u, 0x25u, 0x41u, 0x03u, 0xD8u, 0xE8u,
    0xB9u, 0xEAu, 0x2Du, 0xC5u, 0x3Bu, 0xBBu, 0x78u, 0x00u,
    0xE1u, 0xF4u, 0xD6u, 0x89u, 0x58u, 0x2Eu, 0xEEu, 0xD3u
};

/* ===================================================================== */
/* Resource probing (Stage 2C: verify the stack/arena budget once)        */
/* ===================================================================== */

/**
 * One-line resource report for the CURRENT task. Call it right after the
 * deepest operation so the watermark has seen the worst case.
 */
static void report_resources(const char *tag)
{
    size_t max_used = 0u;
    size_t max_blocks = 0u;
    size_t cur_used = 0u;
    size_t cur_blocks = 0u;

    /* uxTaskGetStackHighWaterMark(NULL) dereferences pxCurrentTCB, which is not
     * valid before the scheduler runs. Never let this be called pre-scheduler. */
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return;
    }

    mbedtls_memory_buffer_alloc_max_get(&max_used, &max_blocks);
    mbedtls_memory_buffer_alloc_cur_get(&cur_used, &cur_blocks);

    DebugUartLock();
    printf("[AUTH-SW] %s: stack_free=%u words, heap_free=%u B (min %u B), "
           "arena_max=%u B/%u blk, arena_cur=%u B\r\n",
           tag,
           (unsigned)uxTaskGetStackHighWaterMark(NULL),
           (unsigned)xPortGetFreeHeapSize(),
           (unsigned)xPortGetMinimumEverFreeHeapSize(),
           (unsigned)max_used, (unsigned)max_blocks, (unsigned)cur_used);
    DebugUartUnlock();
}

/* ===================================================================== */
/* Curve operations                                                      */
/* ===================================================================== */

int DSO_AuthSoft_VerifyDigest(const uint8_t pub[64], const uint8_t digest[32],
                              const uint8_t sig[64])
{
    mbedtls_ecp_point q;
    mbedtls_mpi r;
    mbedtls_mpi s;
    unsigned char enc[65];
    int rc = DSO_AUTH_ERR_SIG;

    if (pub == NULL || digest == NULL || sig == NULL) {
        return DSO_AUTH_ERR_PARAM;
    }
    if (authsoft_ensure() != DSO_AUTH_OK) {
        return DSO_AUTH_ERR_INIT;
    }

    mbedtls_ecp_point_init(&q);
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);

    enc[0] = 0x04u;
    memcpy(&enc[1], pub, 64u);

    if (mbedtls_ecp_point_read_binary(&s_grp, &q, enc, sizeof(enc)) == 0 &&
        mbedtls_ecp_check_pubkey(&s_grp, &q) == 0 &&
        mbedtls_mpi_read_binary(&r, sig, 32u) == 0 &&
        mbedtls_mpi_read_binary(&s, &sig[32], 32u) == 0) {
        /* mbedTLS only enforces 1 <= r,s < n, so high-s verifies fine —
         * which is required: the ATECC emits high-s about half the time. */
        if (mbedtls_ecdsa_verify(&s_grp, digest, 32u, &q, &r, &s) == 0) {
            rc = DSO_AUTH_OK;
        }
    }

    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);
    mbedtls_ecp_point_free(&q);
    return rc;
}

int DSO_AuthSoft_SignDigest(const uint8_t digest[32], uint8_t sig[64])
{
    mbedtls_mpi r;
    mbedtls_mpi s;
    mbedtls_mpi priv;
    int rc = DSO_AUTH_ERR_SIG;

    if (digest == NULL || sig == NULL) {
        return DSO_AUTH_ERR_PARAM;
    }
    if (authsoft_ensure() != DSO_AUTH_OK) {
        return DSO_AUTH_ERR_INIT;
    }

    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);
    mbedtls_mpi_init(&priv);

    if (mbedtls_mpi_read_binary(&priv, DSO_DEV_PRIV, 32u) == 0 &&
        mbedtls_ecdsa_sign_det_ext(&s_grp, &r, &s, &priv,
                                   digest, 32u, MBEDTLS_MD_SHA256,
                                   NULL, NULL) == 0 &&
        mbedtls_mpi_write_binary(&r, sig, 32u) == 0 &&
        mbedtls_mpi_write_binary(&s, &sig[32], 32u) == 0) {
        rc = DSO_AUTH_OK;                  /* raw r||s, no DER round trip */
    }

    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);
    mbedtls_mpi_free(&priv);

    /* Worst-case stack of the `auth_start` path, measured on the real task. */
    if (s_boot == 0u && s_res_after_sign == 0u && rc == DSO_AUTH_OK) {
        s_res_after_sign = 1u;
        report_resources("after sign");
    }
    return rc;
}

const uint8_t *DSO_AuthSoft_Pubkey(void)
{
    return DSO_DEV_PUB;
}

const char *DSO_AuthSoft_LoginState(void)
{
    return s_login;
}

/* ===================================================================== */
/* Strict license payload parser                                         */
/* ===================================================================== */

typedef struct {
    const uint8_t *p;
    const uint8_t *end;
} cur_t;

static int cur_peek(const cur_t *c)
{
    return (c->p < c->end) ? (int)*(c->p) : -1;
}

static int expect_lit(cur_t *c, const char *lit)
{
    size_t n = strlen(lit);
    if ((size_t)(c->end - c->p) < n || memcmp(c->p, lit, n) != 0) {
        return -1;
    }
    c->p += n;
    return 0;
}

static int str_char_ok(uint8_t ch)
{
    if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
        (ch >= '0' && ch <= '9')) {
        return 1;
    }
    return (ch == '.' || ch == '_' || ch == ':' || ch == '@' || ch == '-') ? 1 : 0;
}

/** Parse a JSON string with no escapes; enforce length and charset. */
static int parse_str(cur_t *c, char *dst, size_t minlen, size_t maxlen)
{
    size_t n = 0u;

    if (cur_peek(c) != '"') {
        return -1;
    }
    c->p++;
    while (c->p < c->end && *c->p != '"') {
        uint8_t ch = *c->p;
        if (str_char_ok(ch) == 0 || n >= maxlen) {
            return -1;
        }
        dst[n++] = (char)ch;
        c->p++;
    }
    if (cur_peek(c) != '"') {
        return -1;
    }
    c->p++;
    if (n < minlen) {
        return -1;
    }
    dst[n] = '\0';
    return (int)n;
}

/** Parse a canonical unsigned integer: no sign, no leading zeros, uint32 range. */
static int parse_uint(cur_t *c, uint32_t *out)
{
    uint32_t v = 0u;
    int digits = 0;

    while (c->p < c->end && *c->p >= '0' && *c->p <= '9') {
        if (digits > 0 && v == 0u) {
            return -1;                       /* leading zero */
        }
        if (v > (0xFFFFFFFFu - 9u) / 10u) {
            return -1;                       /* overflow */
        }
        v = v * 10u + (uint32_t)(*c->p - '0');
        digits++;
        c->p++;
    }
    if (digits == 0) {
        return -1;
    }
    *out = v;
    return 0;
}

/* Canonical key order (byte-wise sort of the PC profile):
 * dev, jti, kid, lic, na, nb, perm, typ, user, v. */
static int parse_license_payload(const uint8_t *buf, size_t n, DSO_AuthLicense *out)
{
    cur_t c;
    char jti[17];
    char kid[5];
    char typ[5];
    uint32_t v = 0u;

    c.p = buf;
    c.end = buf + n;

    if (expect_lit(&c, "{\"dev\":") != 0) { return -1; }
    if (parse_str(&c, out->dev, 16u, 16u) < 0) { return -1; }
    if (expect_lit(&c, ",\"jti\":") != 0) { return -1; }
    if (parse_str(&c, jti, 1u, 16u) < 0) { return -1; }
    if (expect_lit(&c, ",\"kid\":") != 0) { return -1; }
    if (parse_str(&c, kid, 1u, 4u) < 0) { return -1; }
    if (expect_lit(&c, ",\"lic\":") != 0) { return -1; }
    if (parse_str(&c, out->lic, 1u, 16u) < 0) { return -1; }
    if (expect_lit(&c, ",\"na\":") != 0) { return -1; }
    if (parse_uint(&c, &out->na) != 0) { return -1; }
    if (expect_lit(&c, ",\"nb\":") != 0) { return -1; }
    if (parse_uint(&c, &out->nb) != 0) { return -1; }
    if (expect_lit(&c, ",\"perm\":") != 0) { return -1; }
    if (parse_uint(&c, &out->perm) != 0) { return -1; }
    if (expect_lit(&c, ",\"typ\":") != 0) { return -1; }
    if (parse_str(&c, typ, 3u, 3u) < 0) { return -1; }
    if (expect_lit(&c, ",\"user\":") != 0) { return -1; }
    if (parse_str(&c, out->user, 1u, 32u) < 0) { return -1; }
    if (expect_lit(&c, ",\"v\":") != 0) { return -1; }
    if (parse_uint(&c, &v) != 0) { return -1; }
    if (expect_lit(&c, "}") != 0) { return -1; }
    if (c.p != c.end) { return -1; }          /* trailing bytes */

    if (v != 1u) { return -1; }
    if (strcmp(typ, "lic") != 0) { return -1; }
    if (out->na < out->nb) { return -1; }
    if ((out->perm & ~0x1u) != 0u) { return -1; }   /* only bit0 (FPGA) known */
    return 0;
}

/* ===================================================================== */
/* License verification                                                  */
/* ===================================================================== */

int DSO_AuthSoft_VerifyLicense(const char *token, DSO_AuthLicense *out)
{
    const char *dot;
    const char *sig_txt;
    size_t plen;
    size_t slen;
    int n;
    uint8_t payload[193];
    uint8_t sig[64];
    uint8_t digest[32];
    char did[17];

    if (token == NULL || out == NULL) {
        return DSO_AUTH_ERR_PARAM;
    }
    if (authsoft_ensure() != DSO_AUTH_OK) {
        return DSO_AUTH_ERR_INIT;
    }
    dot = strchr(token, '.');
    if (dot == NULL || strchr(dot + 1, '.') != NULL) {
        return DSO_AUTH_ERR_FORMAT;
    }
    plen = (size_t)(dot - token);
    sig_txt = dot + 1;
    slen = strlen(sig_txt);
    if (plen == 0u || slen == 0u) {
        return DSO_AUTH_ERR_FORMAT;
    }

    n = DSO_B64Url_Decode(token, plen, payload, sizeof(payload));
    if (n <= 0) {
        return DSO_AUTH_ERR_FORMAT;
    }
    if (DSO_B64Url_Decode(sig_txt, slen, sig, sizeof(sig)) != 64) {
        return DSO_AUTH_ERR_FORMAT;
    }
    if (parse_license_payload(payload, (size_t)n, out) != 0) {
        return DSO_AUTH_ERR_FORMAT;
    }

    (void)DSO_Atecc608b_DeviceId16(did);
    if (strcmp(out->dev, did) != 0) {
        return DSO_AUTH_ERR_DEVICE;
    }

    (void)mbedtls_sha256_ret(payload, (size_t)n, digest, 0);
    if (DSO_AuthSoft_VerifyDigest(DSO_AUTH_CA_PUB, digest, sig) != DSO_AUTH_OK) {
        return DSO_AUTH_ERR_SIG;
    }

    /* Time is NOT enforced on-device (no RTC/SNTP): `nb`/`na` are reported up
     * and the server stays authoritative. */
    (void)strcpy(s_login, "AUTHORIZED");
    if (s_res_after_verify == 0u) {
        s_res_after_verify = 1u;
        report_resources("after license verify");
    }
    return DSO_AUTH_OK;
}

/* ===================================================================== */
/* Init + self-test                                                      */
/* ===================================================================== */

static int selftest(void)
{
    uint8_t sig[64];
    int ok_known;
    int ok_sign;

    ok_known = (DSO_AuthSoft_VerifyDigest(DSO_DEV_PUB, k_selftest_digest,
                                          k_selftest_sig) == DSO_AUTH_OK);
    ok_sign = (DSO_AuthSoft_SignDigest(k_selftest_digest, sig) == DSO_AUTH_OK) &&
              (DSO_AuthSoft_VerifyDigest(DSO_DEV_PUB, k_selftest_digest,
                                         sig) == DSO_AUTH_OK);
    if (ok_known && ok_sign) {
        return DSO_AUTH_OK;
    }
    return DSO_AUTH_ERR_INIT;
}

/* Real initialization — runs in a task, NEVER on the boot stack.
 * (ESC: the previous version ran this from main(); mbedTLS sign+verify needs
 * ~4 KB while the MSP is 4 KB total, and it also touched FreeRTOS task APIs
 * before the scheduler started.) */
static int authsoft_do_init(void);   /* forward */

/**
 * Initialize on first use. The callers are command handlers, which run on the
 * ETH_Up task (10 KB stack) — the only place with enough stack for mbedTLS.
 * Never call this from main() or from an ISR.
 * @return DSO_AUTH_OK when ready.
 */
static int authsoft_ensure(void)
{
    if (s_init_state == 1u) {
        return DSO_AUTH_OK;
    }
    if (s_init_state == 2u || s_init_state == 3u) {
        return DSO_AUTH_ERR_INIT;      /* failed, or init already in progress */
    }
    s_init_state = 3u;                 /* in progress (re-entrancy guard) */
    if (authsoft_do_init() != DSO_AUTH_OK) {
        s_init_state = 2u;
        return DSO_AUTH_ERR_INIT;
    }
    return DSO_AUTH_OK;
}

static int authsoft_do_init(void)
{
    int rc;

    /* The arena installs the mbedtls_calloc/free hooks itself; it MUST run
     * before any other mbedTLS call. */
    mbedtls_memory_buffer_alloc_init(s_arena, sizeof(s_arena));

    mbedtls_ecp_group_init(&s_grp);
    rc = mbedtls_ecp_group_load(&s_grp, MBEDTLS_ECP_DP_SECP256R1);
    if (rc != 0) {
        DebugUartLock();
        printf("[AUTH-SW] FAIL: ecp_group_load rc=-0x%04X\r\n", (unsigned)(-rc));
        DebugUartUnlock();
        return DSO_AUTH_ERR_INIT;
    }
    s_init_state = 1u;

    DebugUartLock();
    printf("[AUTH-SW] *** DEV SOFTWARE KEY ENABLED (public test key 00..1F) ***\r\n");
    printf("[AUTH-SW] *** NOT FOR PRODUCTION - ATECC is not used by this path ***\r\n");
    DebugUartUnlock();

    rc = selftest();
    DebugUartLock();
    printf("[AUTH-SW] selftest %s (arena=%u B)\r\n",
           (rc == DSO_AUTH_OK) ? "sign=OK verify=OK" : "FAILED",
           (unsigned)sizeof(s_arena));
    DebugUartUnlock();
    report_resources("after selftest");
    s_boot = 0u;
    return rc;
}

/* No boot-time init and no dedicated task on purpose:
 *   - the boot stack is only 4 KB and mbedTLS needs ~4 KB;
 *   - the FreeRTOS heap is already near its limit, so an 8 KB init task is a
 *     bad trade;
 *   - this runs on the ETH_Up task (10 KB) on first use instead. */
