"""License token format v0 — strict, byte-exact, stdlib only.

Design rules (see Markdown/设备授权协议_v0.md):
  * the CA signs the **serialized payload bytes**; the verifier hashes exactly
    the bytes it received (never a re-serialization);
  * payload is a flat ASCII JSON object, keys sorted byte-wise, no whitespace,
    integers only, no escapes / floats / booleans / null / nesting;
  * token = b64url(payload) "." b64url(r||s 64 bytes), no padding on the wire.

Anything that does not match this profile raises TokenError.
"""

from __future__ import annotations

import base64
import hashlib
import json
import re

TOKEN_VERSION = 1
TOKEN_TYPE = "lic"

# nb/na are uint32 seconds since 2020-01-01T00:00:00Z (valid until ~2156),
# deliberately avoiding the 2038 problem and the firmware's 32-bit `int` parser.
EPOCH_BASE = 1_577_836_800

# Hard budget: the firmware's command line buffer is 512 bytes and truncates
# silently (User/dso/dso_eth_upload.c). Keep everything well under it.
MAX_PAYLOAD = 192
MAX_TOKEN = 340
MAX_REQUEST = 480

PERM_FPGA = 0x1          # bit0 = may open the FPGA experiment channel
PERM_KNOWN = 0x1         # unknown bits are rejected

PROOF_TAG = b"DSO-PROOF-V1"

_STR_CHARS = re.compile(r"^[A-Za-z0-9._:@-]+$")
_B64_CHARS = re.compile(r"^[A-Za-z0-9_-]+$")

# name -> (kind, lo, hi); for "str" lo/hi are lengths, for "int" they are values
FIELD_SPEC: dict[str, tuple[str, int, int]] = {
    "dev": ("str", 16, 16),          # must equal the device's own DeviceID
    "jti": ("str", 1, 16),           # 128-bit random, revocation key
    "kid": ("str", 1, 4),            # CA key id (audit / rotation)
    "lic": ("str", 1, 16),           # license number
    "na": ("int", 0, 0xFFFF_FFFF),   # not-after  (epoch base offset)
    "nb": ("int", 0, 0xFFFF_FFFF),   # not-before
    "perm": ("int", 0, 0xFFFF_FFFF), # permission bitmap
    "typ": ("str", 3, 3),            # domain separation: always "lic"
    "user": ("str", 1, 32),          # student account
    "v": ("int", 1, 1),              # format version
}


class TokenError(ValueError):
    """Raised for any deviation from the strict token profile."""


def _validate_fields(obj: dict) -> None:
    if not isinstance(obj, dict):
        raise TokenError("payload must be an object")
    got = set(obj.keys())
    want = set(FIELD_SPEC.keys())
    if got != want:
        raise TokenError("bad field set (unknown=%s missing=%s)"
                         % (sorted(got - want), sorted(want - got)))
    for name, (kind, lo, hi) in FIELD_SPEC.items():
        v = obj[name]
        if isinstance(v, bool):                       # bool is an int subclass
            raise TokenError("%s: boolean not allowed" % name)
        if kind == "int":
            if not isinstance(v, int):
                raise TokenError("%s: must be an integer" % name)
            if not (lo <= v <= hi):
                raise TokenError("%s: out of range" % name)
        else:
            if not isinstance(v, str):
                raise TokenError("%s: must be a string" % name)
            if not (lo <= len(v) <= hi):
                raise TokenError("%s: bad length" % name)
            if not _STR_CHARS.match(v):
                raise TokenError("%s: bad charset" % name)
    if obj["v"] != TOKEN_VERSION:
        raise TokenError("unsupported version")
    if obj["typ"] != TOKEN_TYPE:
        raise TokenError("wrong token type")
    if obj["na"] < obj["nb"]:
        raise TokenError("na < nb")
    if obj["perm"] & ~PERM_KNOWN:
        raise TokenError("unknown permission bit")


def canonical_payload(fields: dict) -> bytes:
    """Serialize a license payload in the one accepted canonical form."""
    _validate_fields(fields)
    text = json.dumps(fields, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=True, allow_nan=False)
    raw = text.encode("ascii")
    if len(raw) > MAX_PAYLOAD:
        raise TokenError("payload too long (%d > %d)" % (len(raw), MAX_PAYLOAD))
    return raw


def _reject_constant(value):                          # NaN / Infinity
    raise TokenError("floats not allowed: %s" % value)


def _pairs_hook(pairs):
    seen = set()
    for key, _ in pairs:
        if key in seen:
            raise TokenError("duplicate key: %s" % key)
        seen.add(key)
    return dict(pairs)


def parse_payload(raw: bytes) -> dict:
    """Parse + fully validate payload bytes (exact bytes, canonical form)."""
    if not isinstance(raw, (bytes, bytearray)):
        raise TokenError("payload must be bytes")
    if len(raw) > MAX_PAYLOAD:
        raise TokenError("payload too long")
    try:
        text = bytes(raw).decode("ascii")
    except UnicodeDecodeError:
        raise TokenError("payload is not ASCII") from None
    try:
        obj = json.loads(text, object_pairs_hook=_pairs_hook,
                         parse_float=_reject_constant,
                         parse_constant=_reject_constant)
    except TokenError:
        raise
    except Exception as exc:                          # noqa: BLE001
        raise TokenError("malformed json: %s" % exc) from None
    _validate_fields(obj)
    if canonical_payload(obj) != bytes(raw):
        # catches whitespace / key order / escapes / leading zeros etc.
        raise TokenError("payload is not in canonical form")
    return obj


def b64u_encode(raw: bytes) -> str:
    return base64.urlsafe_b64encode(raw).decode("ascii").rstrip("=")


def b64u_decode(text: str) -> bytes:
    if not isinstance(text, str) or not _B64_CHARS.match(text):
        raise TokenError("bad base64url alphabet")
    pad = "=" * (-len(text) % 4)
    try:
        raw = base64.urlsafe_b64decode(text + pad)
    except Exception:                                 # noqa: BLE001
        raise TokenError("bad base64url") from None
    if b64u_encode(raw) != text:                       # non-canonical tail bits
        raise TokenError("non-canonical base64url")
    return raw


def encode_token(payload: bytes, sig: bytes) -> str:
    if len(sig) != 64:
        raise TokenError("signature must be 64 bytes")
    token = b64u_encode(payload) + "." + b64u_encode(sig)
    if len(token) > MAX_TOKEN:
        raise TokenError("token too long (%d > %d)" % (len(token), MAX_TOKEN))
    return token


def split_token(token: str) -> tuple[bytes, bytes]:
    """Split a token into (payload_bytes, sig64) without trusting either part."""
    if not isinstance(token, str):
        raise TokenError("token must be a string")
    if len(token) > MAX_TOKEN:
        raise TokenError("token too long")
    parts = token.split(".")
    if len(parts) != 2:
        raise TokenError("token must have exactly one '.'")
    if not parts[0] or not parts[1]:
        raise TokenError("empty token part")
    payload = b64u_decode(parts[0])
    sig = b64u_decode(parts[1])
    if len(sig) != 64:
        raise TokenError("signature must be 64 bytes")
    return payload, sig


def make_challenge(device_id: str, nonce: bytes, ts: int) -> bytes:
    """Challenge = SHA256(tag || device_id || nonce32 || ts_be64).

    The device signs this 32-byte value directly (ATECC Nonce Mode 0x03 turns
    the challenge into the digest), so the signature is bound to a device id
    and a server timestamp even though the firmware only sees 32 opaque bytes.
    """
    if not isinstance(device_id, str) or not device_id:
        raise ValueError("device_id required")
    if len(nonce) != 32:
        raise ValueError("nonce must be 32 bytes")
    h = hashlib.sha256()
    h.update(PROOF_TAG)
    h.update(device_id.encode("ascii"))
    h.update(nonce)
    h.update(int(ts).to_bytes(8, "big"))
    return h.digest()


def now_offset(now: float | None = None) -> int:
    """Current time as uint32 seconds since EPOCH_BASE."""
    import time as _time
    return int((now if now is not None else _time.time()) - EPOCH_BASE)
