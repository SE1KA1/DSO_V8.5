"""ATECC608B simulation for the DSO simulator (DEV ONLY).

Models a **provisioned** part (software P-256 key + CA public key in memory) so
the authorization protocol can be exercised end-to-end without hardware.

``cryptography`` is imported lazily: dsosim stays stdlib-only unless an
``atecc.*`` command is actually used.

When ``provisioned=False`` the model reproduces the real firmware's error
shape (``rc=-4 st=0x0F``), which is what an **unlocked** ATECC returns today
(see Markdown/ATECC608B_结论_配置区未锁定.md).
"""

from __future__ import annotations

import hashlib

# Real firmware error text (User/dso/dso_switch_cmd.c): "rc=-4 st=0x0F (...)"".
ERR_NO_KEY = "rc=-4 st=0x0F (no usable ECC key / config zone unlocked)"


class AteccSimulator:
    def __init__(self, device_id: str = "Eep21025E269FA2E",
                 provisioned: bool = True, ca_pub_hex: str | None = None):
        self.device_id = device_id
        self.provisioned = provisioned
        self.ca_pub_hex = ca_pub_hex
        self._key = None
        if provisioned:
            self._key = self._generate_key()

    # ---- crypto (lazy) ---------------------------------------------------
    @staticmethod
    def _generate_key():
        from cryptography.hazmat.primitives.asymmetric import ec
        return ec.generate_private_key(ec.SECP256R1())

    def pubkey_hex(self) -> str:
        nums = self._key.public_key().public_numbers()
        return (nums.x.to_bytes(32, "big") + nums.y.to_bytes(32, "big")).hex()

    def _sign_digest(self, digest: bytes) -> str:
        """Sign a 32-byte digest exactly like ATECC Sign (no low-s normalization)."""
        from cryptography.hazmat.primitives import hashes
        from cryptography.hazmat.primitives.asymmetric import ec, utils
        der = self._key.sign(digest, ec.ECDSA(utils.Prehashed(hashes.SHA256())))
        r, s = utils.decode_dss_signature(der)
        return (r.to_bytes(32, "big") + s.to_bytes(32, "big")).hex()

    # ---- command dispatch ------------------------------------------------
    def handle(self, cmd: str, params: dict) -> tuple[str, str, dict | None]:
        """Return (result, error, data) for the v0.9 ack."""
        if cmd == "atecc.get_devid":
            uid = hashlib.sha256(self.device_id.encode("ascii")).hexdigest()[:24]
            return "ok", "", {"device_id": self.device_id,
                              "label8": self.device_id[:8],
                              "uid_hex": uid.upper()}
        if cmd == "atecc.get_pubkey":
            if not self.provisioned:
                return "error", ERR_NO_KEY, {"slot": 0}
            return "ok", "", {"pubkey_hex": self.pubkey_hex(), "slot": 0,
                              "device_id": self.device_id}
        if cmd == "atecc.auth_start":
            challenge_hex = (params or {}).get("challenge_hex", "")
            if not isinstance(challenge_hex, str) or len(challenge_hex) != 64:
                return "error", "challenge_hex must be 32 bytes of hex", \
                       {"device_id": self.device_id}
            if not self.provisioned:
                return "error", ERR_NO_KEY, {"device_id": self.device_id}
            try:
                digest = bytes.fromhex(challenge_hex)
            except ValueError:
                return "error", "challenge_hex is not hex", {"device_id": self.device_id}
            return "ok", "", {
                "device_id": self.device_id,
                "signature_hex": self._sign_digest(digest),
                "pubkey_hex": self.pubkey_hex(),
                "slot": 0,
                "self_verify": 1,
            }
        if cmd == "atecc.auth_verify":
            return self._auth_verify(params or {})
        return "error", "unknown cmd %s" % cmd, None

    def _auth_verify(self, params: dict) -> tuple[str, str, dict | None]:
        """Mirror the firmware's reply shapes exactly, including data.code."""
        token = params.get("token", "")
        from tools.auth_server import proto
        from tools.auth_server.keystore import verify_digest
        try:
            payload, sig = proto.split_token(token)
            fields = proto.parse_payload(payload)
        except proto.TokenError as exc:
            return ("error", "license rejected rc=-2 (malformed token: %s)" % exc,
                    {"code": -2})
        if fields["dev"] != self.device_id:
            return ("error",
                    "license rejected rc=-3 (token bound to another device)",
                    {"code": -3})
        if not self.ca_pub_hex:
            return ("error",
                    "license rejected rc=-4 (no CA public key provisioned)",
                    {"code": -4})
        try:
            ca_pub = bytes.fromhex(self.ca_pub_hex)
        except ValueError:
            return ("error", "license rejected rc=-4 (CA public key is not hex)",
                    {"code": -4})
        digest = hashlib.sha256(payload).digest()
        if not verify_digest(ca_pub, digest, sig, require_low_s=True):
            return ("error", "license rejected rc=-4 (signature invalid)",
                    {"code": -4})
        return "ok", "", {"lic": fields["lic"], "user": fields["user"],
                          "dev": fields["dev"], "na": fields["na"],
                          "perm": fields["perm"]}
