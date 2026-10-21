/**
 * Public throwaway P-256 test keypair, shared by:
 *   - the ATECC provisioning path (Slot0 test key)
 *   - the DEV software backend (Stage 2C)
 *
 * ⚠️ This is a PUBLIC key pair: the private key is 00 01 02 … 1F and is
 *    published in this repository on purpose. It must never protect anything
 *    real. Having one definition prevents the two backends from drifting.
 */
#ifndef DSO_DEVKEY_H
#define DSO_DEVKEY_H

#include <stdint.h>

/** 32-byte private key: 00 01 02 … 1F. */
extern const uint8_t DSO_DEV_PRIV[32];

/** 64-byte public key, X || Y (big-endian), matching DSO_DEV_PRIV. */
extern const uint8_t DSO_DEV_PUB[64];

#endif /* DSO_DEVKEY_H */
