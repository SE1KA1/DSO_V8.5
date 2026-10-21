mbedTLS 2.28.3 — TRIMMED SUBSET vendored for DSO Stage 2C
=========================================================

Origin
------
  Source package : mbedtls_2.28.3.orig.tar.gz
  Upstream       : Mbed TLS 2.28.3
  Fetched from   : https://mirrors.ustc.edu.cn/debian/pool/main/m/mbedtls/
  License        : Apache-2.0 (see LICENSE in this directory)

Why it is here
--------------
The DSO device-authorization chain needs on-device P-256 ECDSA when the ATECC
part cannot be provisioned (config zone unlocked; see
Markdown/ATECC608B_结论_配置区未锁定.md). This is a DEV-only transition
("Stage 2C"); it does not touch the ATECC at all.

What was kept
-------------
  library/  bignum.c ecp.c ecp_curves.c ecdsa.c asn1parse.c asn1write.c
            md.c sha256.c hmac_drbg.c platform.c platform_util.c
            memory_buffer_alloc.c constant_time.c error.c
  include/  the complete mbedtls header set (headers cost no flash)

What was changed
----------------
  include/mbedtls/config.h   REPLACED with a minimal configuration:
                             P-256 + ECDSA + deterministic signing + SHA-256,
                             a fixed memory arena, no ASM, no entropy,
                             no TLS/X509/PK/AES/RSA, no time/filesystem/network.

Do not "restore" the stock config.h: it enables GNU inline assembly and 64-bit
limb paths that ARMCC 5 (AC5) cannot compile, and pulls in modules we do not
link.

Maintenance
-----------
To update, fetch a newer 2.28.x orig tarball from the same mirror, re-copy the
same file list, and re-apply the minimal config.h above. Keep this file in sync.

Security note
-------------
This copy is used with a FIXED, PUBLIC test key (00 01 .. 1F) for protocol
bring-up only. It must never be shipped in a production image.
