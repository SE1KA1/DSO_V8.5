"""End-to-end + adversarial tests for the DEV authorization protocol.

Run from the repo root:

    python -m unittest tools.auth_server.e2e_test -v

The suite covers the positive flow, replay/freshness, every token-shape
rejection, signature rules (low-s for licenses, high-s allowed for device
proofs), policy refusals, the firmware line-length limit, and one full TCP
round trip against the dsosim simulator.
"""

from __future__ import annotations

import os
import sys

# Allow running this file directly from any working directory:
#   python tools/auth_server/e2e_test.py
# (running `python -m tools.auth_server.e2e_test` still requires the repo root
#  as the current directory).
if __package__ in (None, ""):
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__)))))

import hashlib
import json
import os
import re
import tempfile
import time
import unittest
from pathlib import Path

from dsosim.atecc import AteccSimulator
from dsosim.device import DSOSimulator
from dsosim.server import SimulatorServer
from tools.auth_server import keystore, proto
from tools.auth_server.device_client import DeviceClient, DeviceError
from tools.auth_server.keystore import CAKey, P256_ORDER, is_low_s
from tools.auth_server.service import (AuthError, AuthService, classify_error,
                                       probe_device)
from tools.auth_server.store import Store

DEV_ID = "Eep21025E269FA2E"
USER = "stu001"
PWD = "correct horse battery"


class Clock:
    def __init__(self, t: float | None = None):
        self.t = time.time() if t is None else t

    def __call__(self) -> float:
        return self.t

    def advance(self, seconds: float) -> None:
        self.t += seconds


class Harness(unittest.TestCase):
    def setUp(self) -> None:
        # 600k PBKDF2 iterations per user is right for production but would
        # dominate the test runtime; shrink it for the suite only.
        import tools.auth_server.store as store_mod
        old_iter = store_mod.PBKDF2_ITERATIONS
        store_mod.PBKDF2_ITERATIONS = 1_000
        self.addCleanup(setattr, store_mod, "PBKDF2_ITERATIONS", old_iter)

        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.store = Store(Path(self.tmp.name) / "store.json")
        self.ca = CAKey.generate("k1")
        self.clock = Clock()
        self.svc = AuthService(self.store, self.ca, now=self.clock)
        self.store.add_user(USER, PWD)
        self.sim = AteccSimulator(DEV_ID, provisioned=True,
                                  ca_pub_hex=self.ca.public_hex())
        self.store.register_device(DEV_ID, self.sim.pubkey_hex(), "k1")
        now = proto.now_offset(self.clock.t)
        self.store.add_binding(USER, DEV_ID, now - 60, now + 3600)

    # -- helpers -----------------------------------------------------------
    def login(self, user: str = USER, pwd: str = PWD) -> str:
        return self.svc.login(user, pwd)

    def prove(self, sid: str | None = None) -> str:
        sid = sid or self.login()
        challenge = self.svc.issue_challenge(sid, DEV_ID)
        sig = self.sim._sign_digest(bytes.fromhex(challenge))
        self.svc.verify_proof(sid, DEV_ID, challenge, sig,
                              self.sim.pubkey_hex(), 1)
        return sid

    def license(self, sid: str | None = None, **kw) -> str:
        sid = sid or self.prove()
        return self.svc.issue_license(sid, DEV_ID, **kw)


# ---------------------------------------------------------------------------
# Positive path
# ---------------------------------------------------------------------------

class TestPositive(Harness):
    def test_full_flow(self) -> None:
        sid = self.prove()
        token = self.svc.issue_license(sid, DEV_ID)
        self.assertLessEqual(len(token), proto.MAX_TOKEN)
        fields = self.svc.verify_license(token, expect_dev=DEV_ID)
        self.assertEqual(fields["dev"], DEV_ID)
        self.assertEqual(fields["user"], USER)
        self.assertEqual(fields["perm"], proto.PERM_FPGA)
        self.assertEqual(fields["v"], 1)
        self.assertEqual(fields["typ"], "lic")

    def test_simulated_device_accepts_license(self) -> None:
        token = self.license()
        result, error, data = self.sim.handle("atecc.auth_verify",
                                              {"token": token})
        self.assertEqual(result, "ok", error)
        self.assertEqual(data["dev"], DEV_ID)
        self.assertEqual(data["user"], USER)

    def test_known_answer_cross_check(self) -> None:
        """Our verifier and tools/atecc_verify_signature.py must agree."""
        from tools.atecc_verify_signature import _verify_raw
        challenge = os.urandom(32)
        sig_hex = self.sim._sign_digest(challenge)
        pub_hex = self.sim.pubkey_hex()
        self.assertTrue(_verify_raw(pub_hex, challenge.hex(), sig_hex))
        self.assertTrue(keystore.verify_digest(
            bytes.fromhex(pub_hex), challenge, bytes.fromhex(sig_hex),
            require_low_s=False))

    def test_token_fits_firmware_line(self) -> None:
        token = self.license()
        line = json.dumps({"cmd": "atecc.auth_verify",
                           "params": {"token": token}, "seq": 1},
                          separators=(",", ":"))
        self.assertLess(len((line + "\r\n").encode()), proto.MAX_REQUEST)


# ---------------------------------------------------------------------------
# Replay / freshness
# ---------------------------------------------------------------------------

class TestReplayAndFreshness(Harness):
    def test_same_proof_twice_rejected(self) -> None:
        sid = self.login()
        challenge = self.svc.issue_challenge(sid, DEV_ID)
        sig = self.sim._sign_digest(bytes.fromhex(challenge))
        self.svc.verify_proof(sid, DEV_ID, challenge, sig,
                              self.sim.pubkey_hex(), 1)
        with self.assertRaises(AuthError):
            self.svc.verify_proof(sid, DEV_ID, challenge, sig,
                                  self.sim.pubkey_hex(), 1)

    def test_challenge_expires(self) -> None:
        sid = self.login()
        challenge = self.svc.issue_challenge(sid, DEV_ID)
        sig = self.sim._sign_digest(bytes.fromhex(challenge))
        self.clock.advance(200)                          # > 120 s TTL
        with self.assertRaises(AuthError):
            self.svc.verify_proof(sid, DEV_ID, challenge, sig,
                                  self.sim.pubkey_hex(), 1)

    def test_challenge_is_session_bound(self) -> None:
        sid_a = self.login()
        challenge = self.svc.issue_challenge(sid_a, DEV_ID)
        sid_b = self.login()
        sig = self.sim._sign_digest(bytes.fromhex(challenge))
        with self.assertRaises(AuthError):
            self.svc.verify_proof(sid_b, DEV_ID, challenge, sig,
                                  self.sim.pubkey_hex(), 1)

    def test_concurrent_proofs_only_one_wins(self) -> None:
        sid = self.login()
        challenge = self.svc.issue_challenge(sid, DEV_ID)
        sig = self.sim._sign_digest(bytes.fromhex(challenge))
        first = self.svc.verify_proof(sid, DEV_ID, challenge, sig,
                                      self.sim.pubkey_hex(), 1)
        self.assertTrue(first)
        with self.assertRaises(AuthError):
            self.svc.verify_proof(sid, DEV_ID, challenge, sig,
                                  self.sim.pubkey_hex(), 1)

    def test_open_challenge_cap(self) -> None:
        sid = self.login()
        for _ in range(4):
            self.svc.issue_challenge(sid, DEV_ID)
        with self.assertRaises(AuthError):
            self.svc.issue_challenge(sid, DEV_ID)


# ---------------------------------------------------------------------------
# Token shape (pure format tests)
# ---------------------------------------------------------------------------

class TestTokenShape(Harness):
    def good_fields(self) -> dict:
        token = self.license()
        payload, _sig = proto.split_token(token)
        return proto.parse_payload(payload)

    def test_no_dot(self) -> None:
        with self.assertRaises(proto.TokenError):
            proto.split_token("abcdef")

    def test_three_parts(self) -> None:
        with self.assertRaises(proto.TokenError):
            proto.split_token("a.b.c")

    def test_empty_part(self) -> None:
        with self.assertRaises(proto.TokenError):
            proto.split_token(".abcd")

    def test_padding_chars(self) -> None:
        with self.assertRaises(proto.TokenError):
            proto.b64u_decode("YWI=")

    def test_standard_base64_chars(self) -> None:
        with self.assertRaises(proto.TokenError):
            proto.b64u_decode("ab+/cd")

    def test_non_canonical_tail_bits(self) -> None:
        # b'ab' encodes canonically to 'YWI'; 'YWJ' decodes to the same bytes
        # with non-zero unused bits and must be rejected.
        self.assertEqual(proto.b64u_encode(b"ab"), "YWI")
        with self.assertRaises(proto.TokenError):
            proto.b64u_decode("YWJ")

    def test_oversized_token(self) -> None:
        with self.assertRaises(proto.TokenError):
            proto.split_token("A" * 400 + "." + "B" * 86)

    def test_signature_length(self) -> None:
        payload = proto.canonical_payload(self.good_fields())
        with self.assertRaises(proto.TokenError):
            proto.encode_token(payload, b"\x00" * 63)

    def test_payload_not_ascii(self) -> None:
        with self.assertRaises(proto.TokenError):
            proto.parse_payload(b'{"v":1,"\xff":2}')

    def test_duplicate_key(self) -> None:
        fields = self.good_fields()
        raw = json.dumps(fields, sort_keys=True,
                         separators=(",", ":")).encode()
        raw = raw[:-1] + b',"v":1}'
        with self.assertRaises(proto.TokenError):
            proto.parse_payload(raw)

    def test_float(self) -> None:
        fields = self.good_fields()
        raw = json.dumps(fields, sort_keys=True, separators=(",", ":"))
        raw = re.sub(r'"nb":\d+', '"nb":1.5', raw, count=1)
        with self.assertRaises(proto.TokenError):
            proto.parse_payload(raw.encode())

    def test_whitespace_is_not_canonical(self) -> None:
        fields = self.good_fields()
        raw = json.dumps(fields, sort_keys=True, separators=(", ", ": "))
        with self.assertRaises(proto.TokenError):
            proto.parse_payload(raw.encode())

    def test_unknown_key(self) -> None:
        fields = dict(self.good_fields(), zz=1)
        with self.assertRaises(proto.TokenError):
            proto.canonical_payload(fields)

    def test_unknown_version(self) -> None:
        fields = dict(self.good_fields(), v=2)
        with self.assertRaises(proto.TokenError):
            proto.canonical_payload(fields)

    def test_unknown_permission_bit(self) -> None:
        fields = dict(self.good_fields(), perm=2)
        with self.assertRaises(proto.TokenError):
            proto.canonical_payload(fields)

    def test_na_before_nb(self) -> None:
        fields = self.good_fields()
        fields["na"] = fields["nb"] - 1
        with self.assertRaises(proto.TokenError):
            proto.canonical_payload(fields)

    def test_bool_rejected(self) -> None:
        fields = dict(self.good_fields(), v=True)
        with self.assertRaises(proto.TokenError):
            proto.canonical_payload(fields)


# ---------------------------------------------------------------------------
# Signature rules
# ---------------------------------------------------------------------------

class TestSignatureRules(Harness):
    def test_tampered_payload(self) -> None:
        token = self.license()
        payload, sig = proto.split_token(token)
        bad = bytearray(payload)
        bad[5] ^= 0x01
        with self.assertRaises(proto.TokenError):
            # even before signature checking, the bytes are no longer canonical
            proto.parse_payload(bytes(bad))
        rebuilt = proto.encode_token(bytes(bad), sig)
        with self.assertRaises((AuthError, proto.TokenError)):
            self.svc.verify_license(rebuilt)

    def test_tampered_signature(self) -> None:
        token = self.license()
        payload, sig = proto.split_token(token)
        bad = bytearray(sig)
        bad[10] ^= 0x01
        with self.assertRaises(AuthError):
            self.svc.verify_license(proto.encode_token(payload, bytes(bad)))

    def test_signature_from_other_bytes(self) -> None:
        token = self.license()
        payload, _sig = proto.split_token(token)
        other = hashlib.sha256(b"not the payload").digest()
        sig = self.ca.sign_digest(other)
        with self.assertRaises(AuthError):
            self.svc.verify_license(proto.encode_token(payload, sig))

    def test_high_s_license_rejected(self) -> None:
        token = self.license()
        payload, sig = proto.split_token(token)
        s = int.from_bytes(sig[32:], "big")
        high = sig[:32] + (P256_ORDER - s).to_bytes(32, "big")
        self.assertFalse(is_low_s(high))
        with self.assertRaises(AuthError):
            self.svc.verify_license(proto.encode_token(payload, high))

    def test_high_s_device_proof_accepted(self) -> None:
        sid = self.login()
        challenge = self.svc.issue_challenge(sid, DEV_ID)
        sig = bytes.fromhex(self.sim._sign_digest(bytes.fromhex(challenge)))
        if is_low_s(sig):
            s = int.from_bytes(sig[32:], "big")
            sig = sig[:32] + (P256_ORDER - s).to_bytes(32, "big")
        self.assertFalse(is_low_s(sig))
        self.assertTrue(self.svc.verify_proof(sid, DEV_ID, challenge,
                                              sig.hex(), self.sim.pubkey_hex(), 1))

    def test_zero_and_out_of_range_r_s(self) -> None:
        payload, _sig = proto.split_token(self.license())
        for bad in (b"\x00" * 64,
                    (P256_ORDER).to_bytes(32, "big") + (1).to_bytes(32, "big"),
                    (1).to_bytes(32, "big") + b"\x00" * 32):
            with self.assertRaises(AuthError):
                self.svc.verify_license(proto.encode_token(payload, bad))

    def test_other_ca_key(self) -> None:
        other = CAKey.generate("k1")
        payload = proto.canonical_payload({
            "v": 1, "typ": "lic", "lic": "L00001", "user": USER, "dev": DEV_ID,
            "nb": proto.now_offset(self.clock.t),
            "na": proto.now_offset(self.clock.t) + 600,
            "perm": 1, "jti": "0011223344556677", "kid": "k1"})
        sig = other.sign_digest(hashlib.sha256(payload).digest())
        with self.assertRaises(AuthError):
            self.svc.verify_license(proto.encode_token(payload, sig))

    def test_unknown_kid(self) -> None:
        payload = proto.canonical_payload({
            "v": 1, "typ": "lic", "lic": "L00001", "user": USER, "dev": DEV_ID,
            "nb": proto.now_offset(self.clock.t),
            "na": proto.now_offset(self.clock.t) + 600,
            "perm": 1, "jti": "0011223344556677", "kid": "k9"})
        sig = self.ca.sign_digest(hashlib.sha256(payload).digest())
        with self.assertRaises(AuthError):
            self.svc.verify_license(proto.encode_token(payload, sig))


# ---------------------------------------------------------------------------
# Policy
# ---------------------------------------------------------------------------

class TestPolicy(Harness):
    def test_license_requires_proof(self) -> None:
        sid = self.login()
        with self.assertRaises(AuthError):
            self.svc.issue_license(sid, DEV_ID)

    def test_license_requires_binding(self) -> None:
        sid = self.prove()
        self.store.revoke_bindings(USER, DEV_ID)
        with self.assertRaises(AuthError):
            self.svc.issue_license(sid, DEV_ID)

    def test_identity_only_cannot_get_fpga(self) -> None:
        sid = self.prove()
        self.store.set_capability(DEV_ID, "identity_only")
        with self.assertRaises(AuthError):
            self.svc.issue_license(sid, DEV_ID, perms=proto.PERM_FPGA)
        read_only = self.svc.issue_license(sid, DEV_ID, perms=0)
        self.assertEqual(self.svc.verify_license(read_only)["perm"], 0)

    def test_self_verify_zero_rejected(self) -> None:
        sid = self.login()
        challenge = self.svc.issue_challenge(sid, DEV_ID)
        sig = self.sim._sign_digest(bytes.fromhex(challenge))
        with self.assertRaises(AuthError):
            self.svc.verify_proof(sid, DEV_ID, challenge, sig,
                                  self.sim.pubkey_hex(), 0)

    def test_pubkey_mismatch_rejected(self) -> None:
        sid = self.login()
        challenge = self.svc.issue_challenge(sid, DEV_ID)
        sig = self.sim._sign_digest(bytes.fromhex(challenge))
        with self.assertRaises(AuthError):
            self.svc.verify_proof(sid, DEV_ID, challenge, sig,
                                  "00" * 64, 1)

    def test_unregistered_device(self) -> None:
        sid = self.login()
        with self.assertRaises(AuthError):
            self.svc.issue_challenge(sid, "Eep99999999999999")

    def test_revoked_license(self) -> None:
        token = self.license()
        jti = self.svc.verify_license(token)["jti"]
        self.svc.revoke(jti)
        with self.assertRaises(AuthError):
            self.svc.verify_license(token)

    def test_license_expires(self) -> None:
        token = self.license(ttl_s=10)
        self.clock.advance(11)
        with self.assertRaises(AuthError):
            self.svc.verify_license(token)

    def test_foreign_jti_rejected(self) -> None:
        payload = proto.canonical_payload({
            "v": 1, "typ": "lic", "lic": "L00001", "user": USER, "dev": DEV_ID,
            "nb": proto.now_offset(self.clock.t),
            "na": proto.now_offset(self.clock.t) + 600,
            "perm": 1, "jti": "deadbeefdeadbeef", "kid": "k1"})
        sig = self.ca.sign_digest(hashlib.sha256(payload).digest())
        with self.assertRaises(AuthError):
            self.svc.verify_license(proto.encode_token(payload, sig))

    def test_bad_password(self) -> None:
        with self.assertRaises(AuthError):
            self.svc.login(USER, "wrong")

    def test_login_rate_limit(self) -> None:
        for _ in range(5):
            with self.assertRaises(AuthError):
                self.svc.login(USER, "wrong")
        # even the right password is refused while the account is locked
        with self.assertRaises(AuthError):
            self.svc.login(USER, PWD)
        self.clock.advance(301)
        self.assertTrue(self.svc.login(USER, PWD))


# ---------------------------------------------------------------------------
# Device client limits
# ---------------------------------------------------------------------------

class TestClientLimits(unittest.TestCase):
    def test_oversized_request_refused_locally(self) -> None:
        client = DeviceClient("127.0.0.1", 1)     # never connected
        with self.assertRaises(DeviceError) as ctx:
            client.send_cmd("atecc.auth_verify", {"token": "A" * 600})
        self.assertIn("512", str(ctx.exception))


# ---------------------------------------------------------------------------
# TCP round trip against dsosim
# ---------------------------------------------------------------------------

class TestTcp(Harness):
    def _serve(self, provisioned: bool) -> int:
        dev = DSOSimulator(seed=1)
        dev.atecc = AteccSimulator(DEV_ID, provisioned=provisioned,
                                   ca_pub_hex=self.ca.public_hex())
        # enroll the key the *server-side* simulator actually holds
        if provisioned:
            self.store.register_device(DEV_ID, dev.atecc.pubkey_hex(), "k1")
        srv = SimulatorServer(dev, host="127.0.0.1", port=0)
        srv.start()
        self.addCleanup(srv.stop)
        return srv.server_address[1]

    def test_provisioned_device_full_loop(self) -> None:
        port = self._serve(provisioned=True)
        info = probe_device("127.0.0.1", port)
        self.assertEqual(info["device_id"], DEV_ID)
        self.assertEqual(info["capability"], "provisioned")

        sid = self.login()
        with DeviceClient("127.0.0.1", port) as client:
            challenge = self.svc.issue_challenge(sid, DEV_ID)
            ack = client.auth_start(challenge)
            self.assertEqual(ack["result"], "ok", ack.get("error"))
            self.svc.verify_proof(sid, DEV_ID, challenge,
                                  ack["data"]["signature_hex"],
                                  ack["data"]["pubkey_hex"],
                                  ack["data"]["self_verify"])
            token = self.svc.issue_license(sid, DEV_ID)
            ack = client.auth_verify(token, DEV_ID)
            self.assertEqual(ack["result"], "ok", ack.get("error"))
            self.assertEqual(ack["data"]["user"], USER)

    def test_unprovisioned_device_is_identity_only(self) -> None:
        port = self._serve(provisioned=False)
        info = probe_device("127.0.0.1", port)
        self.assertEqual(info["device_id"], DEV_ID)
        self.assertEqual(info["capability"], "identity_only")
        self.assertTrue(any("no-key" in n for n in info["notes"]))

    def test_unknown_command_is_reported(self) -> None:
        port = self._serve(provisioned=True)
        with DeviceClient("127.0.0.1", port) as client:
            ack = client.send_cmd("atecc.nonexistent")
            self.assertEqual(ack["result"], "error")


class TestErrorClassification(unittest.TestCase):
    def test_classify(self) -> None:
        self.assertEqual(classify_error("rc=-4 st=0x0F (...)"), "no-key")
        self.assertEqual(classify_error("rc=-4 st=0xEE (...)"), "transient")
        self.assertEqual(classify_error("rc=-1 timeout"), "transient")
        self.assertEqual(classify_error(""), "other")


class TestFirmwareDevKey(unittest.TestCase):
    """The firmware's shared test key must match the PC's derivation."""

    def test_devkey_matches(self) -> None:
        src = (Path(__file__).resolve().parents[2] / "User" / "dso"
               / "dso_devkey.c")
        if not src.exists():
            self.skipTest("firmware tree not present")
        text = src.read_text(encoding="utf-8", errors="replace")

        def array(name: str) -> bytes:
            m = re.search(name + r"\s*\[[^\]]*\]\s*=\s*\{(.*?)\};", text, re.S)
            self.assertIsNotNone(m, "missing " + name)
            return bytes(int(x, 16)
                         for x in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1)))

        priv = array("DSO_DEV_PRIV")
        pub = array("DSO_DEV_PUB")
        self.assertEqual(priv, bytes(range(32)))
        from tools.atecc_verify_signature import _pub_from_priv
        self.assertEqual(pub, _pub_from_priv(priv))


if __name__ == "__main__":
    unittest.main(verbosity=2)
