/**
 * Strict base64url decoder (see dso_b64.h).
 */
#include "dso_b64.h"

/* A-Z a-z 0-9 - _ ; everything else (including '=' '+ '/' and whitespace) is
 * rejected on purpose. */
static int b64_val(char c)
{
    if (c >= 'A' && c <= 'Z') { return (int)(c - 'A'); }
    if (c >= 'a' && c <= 'z') { return (int)(c - 'a') + 26; }
    if (c >= '0' && c <= '9') { return (int)(c - '0') + 52; }
    if (c == '-') { return 62; }
    if (c == '_') { return 63; }
    return -1;
}

int DSO_B64Url_Decode(const char *in, size_t in_len,
                      uint8_t *out, size_t out_max)
{
    size_t i;
    size_t o = 0u;
    size_t rem;
    int a, b, c, d;

    if (in == NULL || out == NULL || in_len == 0u) {
        return -1;
    }
    if ((in_len & 3u) == 1u) {          /* impossible length for base64 */
        return -1;
    }

    for (i = 0u; i + 4u <= in_len; i += 4u) {
        a = b64_val(in[i]);
        b = b64_val(in[i + 1u]);
        c = b64_val(in[i + 2u]);
        d = b64_val(in[i + 3u]);
        if (a < 0 || b < 0 || c < 0 || d < 0) {
            return -1;
        }
        if (o + 3u > out_max) {
            return -1;
        }
        out[o++] = (uint8_t)(((unsigned)a << 2) | ((unsigned)b >> 4));
        out[o++] = (uint8_t)(((unsigned)b << 4) | ((unsigned)c >> 2));
        out[o++] = (uint8_t)(((unsigned)c << 6) | (unsigned)d);
    }

    rem = in_len - i;
    if (rem == 2u) {
        a = b64_val(in[i]);
        b = b64_val(in[i + 1u]);
        if (a < 0 || b < 0) {
            return -1;
        }
        if (((unsigned)b & 0x0Fu) != 0u) {   /* non-zero trailing bits */
            return -1;
        }
        if (o + 1u > out_max) {
            return -1;
        }
        out[o++] = (uint8_t)(((unsigned)a << 2) | ((unsigned)b >> 4));
    } else if (rem == 3u) {
        a = b64_val(in[i]);
        b = b64_val(in[i + 1u]);
        c = b64_val(in[i + 2u]);
        if (a < 0 || b < 0 || c < 0) {
            return -1;
        }
        if (((unsigned)c & 0x03u) != 0u) {   /* non-zero trailing bits */
            return -1;
        }
        if (o + 2u > out_max) {
            return -1;
        }
        out[o++] = (uint8_t)(((unsigned)a << 2) | ((unsigned)b >> 4));
        out[o++] = (uint8_t)(((unsigned)b << 4) | ((unsigned)c >> 2));
    }

    return (int)o;
}
