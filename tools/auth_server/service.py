"""Authorization service (library + CLI) — DEV ONLY.

Flow implemented here follows Markdown/设备授权协议_v0.md:

    login  ->  issue_challenge  ->  (device signs)  ->  verify_proof
                                                       |
                                          issue_license (CA-signed token)
                                                       |
                                          device: atecc.auth_verify

Security boundary (deliberate, documented):
  * this service protects *policy and audit* — who may use which device;
  * it does **not** make an unprovisioned device trustworthy. DeviceID is
    derived in firmware from a label macro plus CRC32(STM32 UID), and the
    unlocked ATECC cannot sign or verify anything.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import secrets
import sys
import time
from pathlib import Path

from . import keystore, proto
from .keystore import CAKey, KeyFormatError, sig_from_hex
from .store import Store

CHALLENGE_TTL_S = 120
MAX_OPEN_CHALLENGES = 4
PROOF_ATTEMPTS = 3
PROOF_DEFINITIVE_FAILURES = 2       # 2 of 3 st=0x0F => identity_only
LOGIN_MAX_FAILS = 5                 # brute-force guard (per account)
LOGIN_LOCK_S = 300


class AuthError(RuntimeError):
    """Policy or protocol violation."""


class AuthService:
    def __init__(self, store: Store, ca: CAKey, now=time.time):
        self.store = store
        self.ca = ca
        self._now = now
        self._sessions: dict[str, dict] = {}
        self._login_fails: dict[str, int] = {}
        self._login_lock_until: dict[str, float] = {}

    # ---- helpers ---------------------------------------------------------
    def _now_off(self) -> int:
        return proto.now_offset(self._now())

    def _session(self, sid: str) -> dict:
        sess = self._sessions.get(sid)
        if sess is None:
            raise AuthError("unknown or expired session")
        return sess

    # ---- 1. login --------------------------------------------------------
    def login(self, user: str, password: str) -> str:
        now = self._now()
        if self._login_lock_until.get(user, 0.0) > now:
            raise AuthError("too many failed attempts; account temporarily locked")
        if not self.store.check_password(user, password):
            fails = self._login_fails.get(user, 0) + 1
            self._login_fails[user] = fails
            if fails >= LOGIN_MAX_FAILS:
                self._login_lock_until[user] = now + LOGIN_LOCK_S
                self._login_fails[user] = 0
            raise AuthError("bad credentials")
        self._login_fails.pop(user, None)
        sid = secrets.token_hex(8)
        self._sessions[sid] = {"user": user, "proofs": set()}
        return sid

    # ---- 2. challenge ----------------------------------------------------
    def issue_challenge(self, sid: str, dev: str) -> str:
        sess = self._session(sid)
        if self.store.get_device(dev) is None:
            raise AuthError("device is not registered")
        now_off = self._now_off()
        self.store.expire_challenges(now_off)
        if self.store.count_open(dev, now_off) >= MAX_OPEN_CHALLENGES:
            raise AuthError("too many open challenges for this device")
        nonce = os.urandom(32)
        digest = proto.make_challenge(dev, nonce, int(self._now()))
        self.store.put_challenge(digest.hex(), dev, sess["user"], sid,
                                 now_off + CHALLENGE_TTL_S)
        return digest.hex()

    # ---- 3. device proof -------------------------------------------------
    def verify_proof(self, sid: str, dev: str, challenge_hex: str,
                     sig_hex: str, pubkey_hex: str | None = None,
                     self_verify: int = 1) -> bool:
        sess = self._session(sid)
        entry = self.store.take_challenge(challenge_hex.lower())  # single use
        if entry is None:
            raise AuthError("unknown or already used challenge")
        if entry["expires"] < self._now_off():
            raise AuthError("challenge expired")
        if entry["session"] != sid or entry["dev"] != dev:
            raise AuthError("challenge was issued for another session/device")
        rec = self.store.get_device(dev)
        if rec is None:
            raise AuthError("device is not registered")
        if pubkey_hex is not None and pubkey_hex.lower() != rec["pubkey"]:
            raise AuthError("device public key does not match enrollment")
        if int(self_verify) != 1:
            raise AuthError("device self-verification failed")
        try:
            sig = sig_from_hex(sig_hex)
            pub = keystore.pub_from_hex(rec["pubkey"])
        except KeyFormatError as exc:
            raise AuthError(str(exc)) from None
        digest = bytes.fromhex(challenge_hex)
        # ATECC Sign emits high-s about half the time -> do NOT require low-s.
        if not keystore.verify_digest(pub, digest, sig, require_low_s=False):
            raise AuthError("device proof signature is invalid")
        sess["proofs"].add(dev)
        return True

    # ---- 4. license ------------------------------------------------------
    def issue_license(self, sid: str, dev: str, perms: int = proto.PERM_FPGA,
                      ttl_s: int = 900) -> str:
        sess = self._session(sid)
        if dev not in sess["proofs"]:
            raise AuthError("no completed device proof in this session")
        rec = self.store.get_device(dev)
        if rec is None:
            raise AuthError("device is not registered")
        if rec.get("capability") != "provisioned" and (perms & proto.PERM_FPGA):
            raise AuthError("device is identity_only: FPGA permission refused")
        now_off = self._now_off()
        if self.store.find_binding(sess["user"], dev, now_off) is None:
            raise AuthError("no active user<->device binding")
        jti = secrets.token_hex(8)          # 64-bit + store uniqueness check
        lic = "L%05d" % self.store.next_seq()
        fields = {
            "v": proto.TOKEN_VERSION,
            "typ": proto.TOKEN_TYPE,
            "lic": lic,
            "user": sess["user"],
            "dev": dev,
            "nb": now_off,
            "na": now_off + max(1, int(ttl_s)),
            "perm": perms,
            "jti": jti,
            "kid": self.ca.kid,
        }
        payload = proto.canonical_payload(fields)
        sig = self.ca.sign_digest(hashlib.sha256(payload).digest())
        token = proto.encode_token(payload, sig)
        self.store.record_license(jti, lic, sess["user"], dev, fields["na"])
        return token

    def verify_license(self, token: str, expect_dev: str | None = None) -> dict:
        payload, sig = proto.split_token(token)
        fields = proto.parse_payload(payload)
        if fields["kid"] != self.ca.kid:
            raise AuthError("unknown CA key id")
        digest = hashlib.sha256(payload).digest()
        if not keystore.verify_digest(self.ca.public_raw(), digest, sig,
                                      require_low_s=True):
            raise AuthError("license signature is invalid")
        if expect_dev is not None and fields["dev"] != expect_dev:
            raise AuthError("license is bound to another device")
        rec = self.store.get_license(fields["jti"])
        if rec is None:
            raise AuthError("unknown license id (not issued by this service)")
        if rec["revoked"] or self.store.is_revoked(fields["jti"]):
            raise AuthError("license revoked")
        now_off = self._now_off()
        if now_off < fields["nb"]:
            raise AuthError("license not yet valid")
        if now_off > fields["na"]:
            raise AuthError("license expired")
        return fields

    def revoke(self, jti: str) -> None:
        self.store.revoke_license(jti)


# ---------------------------------------------------------------------------
# Device probing: what can this device actually do right now?
# ---------------------------------------------------------------------------

def classify_error(error_text: str) -> str:
    """Map a firmware error string to 'no-key' / 'transient' / 'other'."""
    text = (error_text or "").lower()
    if "st=0x0f" in text:
        return "no-key"        # execution error: no key / unlocked config zone
    if "st=0xee" in text or "rc=-1" in text or "timeout" in text:
        return "transient"     # watchdog / I/O: says nothing about provisioning
    return "other"


def probe_device(host: str, port: int = 5005) -> dict:
    """Read-only probe: identity + device-proof capability."""
    from .device_client import DeviceClient, DeviceError

    out: dict = {"host": host, "port": port, "device_id": None,
                 "pubkey_hex": None, "capability": "unknown", "notes": []}
    with DeviceClient(host, port) as dev:
        ack = dev.get_devid()
        if ack.get("result") != "ok":
            out["notes"].append("get_devid failed: %s" % ack.get("error"))
            out["capability"] = "no-identity"
            return out
        out["device_id"] = ack.get("data", {}).get("device_id")
        if not out["device_id"]:
            out["capability"] = "no-identity"
            return out
        ack = dev.get_pubkey()
        if ack.get("result") == "ok":
            out["pubkey_hex"] = ack.get("data", {}).get("pubkey_hex")
        else:
            out["notes"].append("get_pubkey: %s" % ack.get("error"))

        definitive = 0
        for _ in range(PROOF_ATTEMPTS):
            challenge = os.urandom(32).hex()
            try:
                ack = dev.auth_start(challenge)
            except DeviceError as exc:
                out["notes"].append("auth_start transport: %s" % exc)
                continue
            if ack.get("result") == "ok":
                out["capability"] = "provisioned"
                out["pubkey_hex"] = ack.get("data", {}).get("pubkey_hex",
                                                            out["pubkey_hex"])
                return out
            kind = classify_error(ack.get("error", ""))
            out["notes"].append("auth_start: %s -> %s" % (ack.get("error"), kind))
            if kind == "no-key":
                definitive += 1
        out["capability"] = ("identity_only"
                             if definitive >= PROOF_DEFINITIVE_FAILURES
                             else "unknown")
    return out


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

STATE_DIR = Path(__file__).resolve().parent / ".state"


def _service() -> tuple[AuthService, Store, CAKey]:
    state = STATE_DIR
    store = Store(state / "store.json")
    ca = CAKey.load_or_create(state / "ca_key.pem")
    return AuthService(store, ca), store, ca


def _cmd_init(_args) -> int:
    _service()
    print("state dir:", STATE_DIR)
    print("CA key    :", STATE_DIR / "ca_key.pem")
    print("store     :", STATE_DIR / "store.json")
    return 0


def _cmd_add_user(args) -> int:
    _svc, store, _ca = _service()
    pwd = args.password or input("password for %s: " % args.user)
    store.add_user(args.user, pwd)
    print("user added:", args.user)
    return 0


def _cmd_register(args) -> int:
    _svc, store, _ca = _service()
    store.register_device(args.device, args.pubkey, args.kid, args.capability)
    print("device registered:", args.device, args.capability)
    return 0


def _cmd_bind(args) -> int:
    _svc, store, _ca = _service()
    now_off = proto.now_offset()
    store.add_binding(args.user, args.device, now_off, now_off + args.days * 86400)
    print("binding added:", args.user, "<->", args.device, "for", args.days, "days")
    return 0


def _cmd_probe(args) -> int:
    out = probe_device(args.host, args.port)
    for key in ("device_id", "pubkey_hex", "capability"):
        print("%-12s %s" % (key + ":", out[key]))
    for note in out["notes"]:
        print("  note:", note)
    if out["capability"] == "identity_only":
        print("=> identity is readable, but the device cannot prove itself")
        print("   (config zone unlocked: no key in Slot0). See")
        print("   Markdown/ATECC608B_结论_配置区未锁定.md")
    return 0


def _cmd_verify(args) -> int:
    svc, _store, _ca = _service()
    fields = svc.verify_license(args.token, args.device)
    for k in sorted(fields):
        print("%-6s %s" % (k + ":", fields[k]))
    return 0


def _cmd_flow(args) -> int:
    """Run the whole chain against a live device: proof -> license -> verify."""
    from .device_client import DeviceClient, DeviceError

    svc, store, _ca = _service()
    pwd = args.password or input("password for %s: " % args.user)
    try:
        with DeviceClient(args.host, args.port) as client:
            ack = client.get_devid()
            if ack.get("result") != "ok":
                print("get_devid failed:", ack.get("error"))
                return 2
            dev = ack["data"]["device_id"]
            print("device_id   :", dev)
            if store.get_device(dev) is None:
                print("device is NOT registered. Enroll its public key first:")
                print("  python -m tools.auth_server.service register %s <128-hex pubkey>" % dev)
                return 2

            sid = svc.login(args.user, pwd)
            challenge = svc.issue_challenge(sid, dev)
            print("challenge   :", challenge)
            ack = client.auth_start(challenge)
            if ack.get("result") != "ok":
                print("auth_start FAILED:", ack.get("error"))
                print("  (an unlocked ATECC answers st=0x0F here; enable")
                print("   DSO_AUTH_DEV_SOFTKEY in the firmware for the DEV path)")
                return 3
            print("signature   :", ack["data"]["signature_hex"][:32], "...")
            svc.verify_proof(sid, dev, challenge,
                             ack["data"]["signature_hex"],
                             ack["data"]["pubkey_hex"],
                             ack["data"]["self_verify"])
            print("device proof: OK")

            token = svc.issue_license(sid, dev, ttl_s=args.ttl)
            print("license     :", token[:40], "... (%d bytes)" % len(token))
            ack = client.auth_verify(token, dev)
            if ack.get("result") != "ok":
                print("auth_verify FAILED:", ack.get("error"))
                return 4
            data = ack.get("data", {})
            print("verify      : OK  lic=%s user=%s na=%s perm=%s"
                  % (data.get("lic"), data.get("user"), data.get("na"),
                     data.get("perm")))
            print("=> full authorization chain works on this device")
            return 0
    except DeviceError as exc:
        print("device error:", exc)
        return 5


def _cmd_show_ca(args) -> int:
    """Emit the CA public key as a C header (firmware build-time constant)."""
    _svc, _store, ca = _service()
    raw = ca.public_raw()
    if args.c:
        print("/* Generated by `python -m tools.auth_server.service show-ca --c`.")
        print(" * CA public key (X||Y, 64 bytes) used by the DEV firmware to verify")
        print(" * license tokens. Regenerate this header whenever the CA key changes. */")
        print("#ifndef DSO_AUTH_CA_H")
        print("#define DSO_AUTH_CA_H")
        print("")
        print("#include <stdint.h>")
        print("")
        print("static const uint8_t DSO_AUTH_CA_PUB[64] = {")
        for off in range(0, 64, 8):
            chunk = ", ".join("0x%02Xu" % b for b in raw[off:off + 8])
            print("    %s%s" % (chunk, "," if off < 56 else ""))
        print("};")
        print("")
        print("#endif /* DSO_AUTH_CA_H */")
    else:
        print(raw.hex())
    return 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="DSO DEV authorization service")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("init", help="create state dir + CA key")
    p.set_defaults(func=_cmd_init)

    p = sub.add_parser("add-user", help="add a student account")
    p.add_argument("user")
    p.add_argument("--password", default=None)
    p.set_defaults(func=_cmd_add_user)

    p = sub.add_parser("register", help="enroll a device (out-of-band only)")
    p.add_argument("device")
    p.add_argument("pubkey", help="64-byte X||Y as hex")
    p.add_argument("--kid", default="k1")
    p.add_argument("--capability", default="provisioned",
                   choices=["provisioned", "identity_only"])
    p.set_defaults(func=_cmd_register)

    p = sub.add_parser("bind", help="bind a user to a device")
    p.add_argument("user")
    p.add_argument("device")
    p.add_argument("--days", type=int, default=30)
    p.set_defaults(func=_cmd_bind)

    p = sub.add_parser("probe", help="read-only capability probe over TCP")
    p.add_argument("--host", required=True)
    p.add_argument("--port", type=int, default=5005)
    p.set_defaults(func=_cmd_probe)

    p = sub.add_parser("verify-license", help="verify a license token")
    p.add_argument("token")
    p.add_argument("--device", default=None)
    p.set_defaults(func=_cmd_verify)

    p = sub.add_parser("show-ca",
                       help="print the CA public key (hex, or --c for a C header)")
    p.add_argument("--c", action="store_true",
                   help="emit User/dso/dso_auth_ca.h content")
    p.set_defaults(func=_cmd_show_ca)

    p = sub.add_parser("flow",
                       help="run the whole chain against a live device")
    p.add_argument("--host", required=True)
    p.add_argument("--port", type=int, default=5005)
    p.add_argument("--user", required=True)
    p.add_argument("--password", default=None)
    p.add_argument("--ttl", type=int, default=900, help="license TTL seconds")
    p.set_defaults(func=_cmd_flow)

    args = ap.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
