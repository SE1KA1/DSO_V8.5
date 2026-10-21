"""Local DEV CA key (P-256) — sign/verify raw 32-byte digests.

DEV ONLY: the private key lives in a local PEM file (gitignored). Production
must keep the CA key in an HSM (see the architecture doc §8-3).

All ECDSA operations use ``Prehashed(SHA256())`` because the ATECC signs the
digest itself; hashing again here would silently break interoperability with
``tools/atecc_verify_signature.py``.
"""

from __future__ import annotations

import os
from pathlib import Path

try:
    from cryptography.exceptions import InvalidSignature
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.hazmat.primitives.asymmetric.utils import (
        decode_dss_signature,
        encode_dss_signature,
    )
except ImportError as _exc:                          # pragma: no cover
    raise ImportError(
        "the DEV auth service needs the `cryptography` package. The repo's "
        ".venv does NOT include it -- run with the system Python (the one that "
        "works for tools/atecc_verify_signature.py), or `pip install cryptography`."
    ) from _exc

CURVE = ec.SECP256R1()

# Order n of the P-256 group; used for low-s normalization.
# Authoritative value (openssl ecparam -name prime256v1 -param_enc explicit):
#   FFFFFFFF 00000000 FFFFFFFF FFFFFFFF BCE6FAAD A7179E84 F3B9CAC2 FC632551
# Note the F3B9CAC2 nibble — B3B9CAC2 (a common typo) silently corrupts every
# signature that needs normalization.
P256_ORDER = 0xFFFF_FFFF_0000_0000_FFFF_FFFF_FFFF_FFFF_BCE6_FAAD_A717_9E84_F3B9_CAC2_FC63_2551


class KeyFormatError(ValueError):
    """Bad key material or bad signature encoding."""


def raw_to_der(sig: bytes) -> bytes:
    """64-byte r||s -> DER."""
    if len(sig) != 64:
        raise KeyFormatError("signature must be 64 bytes")
    r = int.from_bytes(sig[:32], "big")
    s = int.from_bytes(sig[32:], "big")
    if not (0 < r < P256_ORDER) or not (0 < s < P256_ORDER):
        raise KeyFormatError("r/s out of range")
    return encode_dss_signature(r, s)


def der_to_raw(der: bytes) -> bytes:
    """DER -> 64-byte r||s."""
    r, s = decode_dss_signature(der)
    return r.to_bytes(32, "big") + s.to_bytes(32, "big")


def is_low_s(sig: bytes) -> bool:
    if len(sig) != 64:
        raise KeyFormatError("signature must be 64 bytes")
    return int.from_bytes(sig[32:], "big") <= P256_ORDER // 2


def normalize_low_s(sig: bytes) -> bytes:
    """Return the canonical (low-s) form so one license has one encoding."""
    if len(sig) != 64:
        raise KeyFormatError("signature must be 64 bytes")
    s = int.from_bytes(sig[32:], "big")
    if s > P256_ORDER // 2:
        s = P256_ORDER - s
    return sig[:32] + s.to_bytes(32, "big")


class CAKey:
    """A P-256 signing key plus its raw X||Y public form."""

    def __init__(self, private_key, kid: str = "k1"):
        self._key = private_key
        self.kid = kid

    # ---- construction ----------------------------------------------------
    @classmethod
    def generate(cls, kid: str = "k1") -> "CAKey":
        return cls(ec.generate_private_key(CURVE), kid)

    @classmethod
    def load_or_create(cls, path: str | os.PathLike, kid: str = "k1") -> "CAKey":
        path = Path(path)
        if path.exists():
            data = path.read_bytes()
            return cls(serialization.load_pem_private_key(data, password=None), kid)
        key = cls.generate(kid)
        path.parent.mkdir(parents=True, exist_ok=True)
        pem = key._key.private_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PrivateFormat.PKCS8,
            encryption_algorithm=serialization.NoEncryption(),
        )
        path.write_bytes(pem)
        try:
            os.chmod(path, 0o600)
        except OSError:
            pass                                   # Windows: best effort
        return key

    # ---- public material -------------------------------------------------
    def public_raw(self) -> bytes:
        """64-byte X||Y (no 0x04 prefix), the form the ATECC expects."""
        nums = self._key.public_key().public_numbers()
        return (nums.x.to_bytes(32, "big") + nums.y.to_bytes(32, "big"))

    def public_hex(self) -> str:
        return self.public_raw().hex()

    # ---- operations ------------------------------------------------------
    def sign_digest(self, digest: bytes) -> bytes:
        """Sign a 32-byte digest; returns low-s r||s (64 bytes)."""
        if len(digest) != 32:
            raise KeyFormatError("digest must be 32 bytes")
        der = self._key.sign(digest, _prehashed())
        return normalize_low_s(der_to_raw(der))


def _prehashed() -> ec.ECDSA:
    from cryptography.hazmat.primitives.asymmetric.utils import Prehashed
    return ec.ECDSA(Prehashed(hashes.SHA256()))


def verify_digest(pubkey_raw: bytes, digest: bytes, sig: bytes,
                  require_low_s: bool = True) -> bool:
    """Verify a raw r||s signature over a 32-byte digest.

    ``require_low_s=False`` must be used for ATECC device proofs: the chip's
    Sign command emits high-s roughly half the time.
    """
    try:
        if len(pubkey_raw) != 64 or len(digest) != 32:
            return False
        if require_low_s:
            if not is_low_s(sig):
                return False
            use = sig
        else:
            # OpenSSL rejects high-s signatures outright, and the ATECC emits
            # them roughly half the time. (r, n-s) is the same signature, so
            # normalize instead of rejecting.
            use = normalize_low_s(sig)
        der = raw_to_der(use)
    except KeyFormatError:
        return False
    try:
        pub = ec.EllipticCurvePublicKey.from_encoded_point(CURVE, b"\x04" + pubkey_raw)
        pub.verify(der, digest, _prehashed())
        return True
    except (InvalidSignature, ValueError):
        return False


def sig_from_hex(sig_hex: str) -> bytes:
    try:
        sig = bytes.fromhex(sig_hex)
    except ValueError:
        raise KeyFormatError("signature is not hex") from None
    if len(sig) != 64:
        raise KeyFormatError("signature must be 64 bytes")
    return sig


def pub_from_hex(pub_hex: str) -> bytes:
    try:
        pub = bytes.fromhex(pub_hex)
    except ValueError:
        raise KeyFormatError("public key is not hex") from None
    if len(pub) != 64:
        raise KeyFormatError("public key must be 64 bytes")
    return pub
