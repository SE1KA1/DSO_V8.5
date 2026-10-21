/**
 * Strict base64url (RFC 4648 §5) decoder — no padding, canonical only.
 *
 * Used by the DEV authorization backend to decode license tokens. Rejects
 * anything that could make two different strings decode to the same bytes
 * (padding, '+' '/', non-zero trailing bits), so a device-side parser cannot
 * be fed a "second encoding" of an already-signed payload.
 */
#ifndef DSO_B64_H
#define DSO_B64_H

#include <stddef.h>
#include <stdint.h>

/**
 * Decode `in_len` characters of base64url into `out`.
 * @return number of decoded bytes (>= 0), or -1 on any format error /
 *         insufficient output space.
 */
int DSO_B64Url_Decode(const char *in, size_t in_len,
                      uint8_t *out, size_t out_max);

#endif /* DSO_B64_H */
