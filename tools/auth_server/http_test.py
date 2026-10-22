"""HTTP API tests (stdlib only: http.client / socket / unittest).

Run from the repo root:
    python -m unittest tools.auth_server.http_test -v
"""

from __future__ import annotations

import http.client
import json
import os
import socket
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__)))))

from dsosim.atecc import AteccSimulator
from dsosim.device import DSOSimulator
from dsosim.server import SimulatorServer
from tools.auth_server import proto
from tools.auth_server.audit import AuditLog
from tools.auth_server.device_client import DeviceTimeout
from tools.auth_server.http_api import ServerContext, create_server
from tools.auth_server.keystore import CAKey
from tools.auth_server.store import Store

DEV_ID = "Eep21025E269FA2E"
USER = "stu001"
PWD = "correct horse battery"
ADMIN = "0123456789abcdef0123456789abcdef"


class FakeDevice:
    """Minimal device stand-in for the cases a real simulator cannot produce."""

    def __init__(self, dev_id=DEV_ID, delay=0.0, timeout=False, ident=None):
        self.dev_id = dev_id
        self.delay = delay
        self.timeout = timeout
        self.ident = ident if ident is not None else dev_id

    def __enter__(self):
        return self

    def __exit__(self, *_exc):
        return False

    def _maybe_sleep(self):
        if self.delay:
            time.sleep(self.delay)

    def get_devid(self):
        if self.timeout:
            raise DeviceTimeout("fake timeout")
        return {"result": "ok", "data": {"device_id": self.ident}}

    def auth_start(self, challenge_hex):
        self._maybe_sleep()
        if self.timeout:
            raise DeviceTimeout("fake timeout")
        return {"result": "error", "error": "not implemented in fake"}

    def auth_verify(self, token, device_id=None):
        return {"result": "ok", "data": {}}

    def get_license(self):
        return {"result": "ok", "data": {"device_id": self.ident,
                                         "login_state": "NO_LICENSE",
                                         "license": None}}


class Base(unittest.TestCase):
    client_factory = None          # None -> real DeviceClient (dsosim)

    def setUp(self) -> None:
        import tools.auth_server.store as store_mod
        old = store_mod.PBKDF2_ITERATIONS
        store_mod.PBKDF2_ITERATIONS = 1_000
        self.addCleanup(setattr, store_mod, "PBKDF2_ITERATIONS", old)

        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.state = Path(self.tmp.name)

        # CA first: the simulator must know it in order to verify licenses.
        self.ca = CAKey.generate("k1")

        # dsosim device (real TCP endpoint) unless the test injects a fake
        self.sim_pub = None
        self.dev_port = 1
        sim = AteccSimulator(DEV_ID, provisioned=True,
                             ca_pub_hex=self.ca.public_hex())
        if self.client_factory is None:
            dev = DSOSimulator(seed=1)
            dev.atecc = sim
            srv = SimulatorServer(dev, host="127.0.0.1", port=0)
            srv.start()
            self.addCleanup(srv.stop)
            self.dev_port = srv.server_address[1]
            self.sim_pub = sim.pubkey_hex()

        self.store = Store(self.state / "store.json")
        if self.sim_pub is None:
            self.sim_pub = sim.pubkey_hex()
        self.store.add_user(USER, PWD)
        self.store.register_device(DEV_ID, self.sim_pub, "k1", "provisioned",
                                   host="127.0.0.1", port=self.dev_port)
        now = proto.now_offset()
        self.store.add_binding(USER, DEV_ID, now - 60, now + 3600)

        self.audit = AuditLog(self.state / "audit.jsonl")
        self.ctx = ServerContext(self.store, self.ca, self.audit, ADMIN,
                                 client_factory=self.client_factory)
        self.httpd = create_server("127.0.0.1", 0, self.ctx)
        self.thread = threading.Thread(target=self.httpd.serve_forever, daemon=True)
        self.thread.start()
        self.addCleanup(self._stop_http)
        self.port = self.httpd.server_address[1]

    def _stop_http(self):
        self.httpd.shutdown()
        self.httpd.server_close()
        self.thread.join(timeout=5)

    # ---- HTTP helpers ----------------------------------------------------
    def req(self, method, path, body=None, headers=None):
        conn = http.client.HTTPConnection("127.0.0.1", self.port, timeout=30)
        hdrs = dict(headers or {})
        data = None
        if body is not None:
            data = body if isinstance(body, (bytes, bytearray)) else json.dumps(body).encode()
            hdrs.setdefault("Content-Type", "application/json")
            hdrs.setdefault("Content-Length", str(len(data)))
        conn.request(method, path, body=data, headers=hdrs)
        resp = conn.getresponse()
        raw = resp.read()
        conn.close()
        try:
            payload = json.loads(raw.decode("utf-8"))
        except ValueError:
            payload = {"_raw": raw.decode("utf-8", "replace")}
        return resp.status, payload, dict(resp.getheaders())

    def post(self, path, body, headers=None):
        return self.req("POST", path, body, headers)

    def get(self, path, headers=None):
        return self.req("GET", path, None, headers)

    def raw_http(self, request: bytes):
        s = socket.create_connection(("127.0.0.1", self.port), timeout=10)
        try:
            s.sendall(request)
            data = s.recv(4096)
        finally:
            s.close()
        line = data.split(b"\r\n", 1)[0].decode("latin-1")
        try:
            return int(line.split()[1])
        except (IndexError, ValueError):
            return 0

    def login(self) -> str:
        status, body, _ = self.post("/api/session", {"user": USER, "password": PWD})
        self.assertEqual(status, 200, body)
        return body["sid"]


# ---------------------------------------------------------------------------
# Health + parsing guards
# ---------------------------------------------------------------------------

class TestBasics(Base):
    def test_health(self) -> None:
        status, body, headers = self.get("/api/health")
        self.assertEqual(status, 200)
        self.assertTrue(body["ok"])
        self.assertEqual(headers.get("Cache-Control"), "no-store")
        self.assertIn("charset=utf-8", headers.get("Content-Type", ""))

    def test_login_ok(self) -> None:
        status, body, _ = self.post("/api/session", {"user": USER, "password": PWD})
        self.assertEqual(status, 200)
        self.assertEqual(body["user"], USER)
        self.assertTrue(body["sid"])

    def test_bad_credentials_same_shape(self) -> None:
        s1, b1, _ = self.post("/api/session", {"user": USER, "password": "nope"})
        s2, b2, _ = self.post("/api/session", {"user": "ghost", "password": "nope"})
        self.assertEqual(s1, 401)
        self.assertEqual(s2, 401)
        self.assertEqual(b1.get("code"), b2.get("code"))

    def test_login_lockout(self) -> None:
        for _ in range(5):
            self.post("/api/session", {"user": USER, "password": "nope"})
        status, body, _ = self.post("/api/session", {"user": USER, "password": PWD})
        self.assertEqual(status, 429)
        self.assertEqual(body["code"], "locked")

    def test_bad_json(self) -> None:
        conn = http.client.HTTPConnection("127.0.0.1", self.port, timeout=10)
        conn.request("POST", "/api/session", body=b"{not json",
                     headers={"Content-Type": "application/json",
                              "Content-Length": "9"})
        self.assertEqual(conn.getresponse().status, 400)
        conn.close()

    def test_missing_content_length(self) -> None:
        status = self.raw_http(b"POST /api/session HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                               b"Connection: close\r\n\r\n")
        self.assertEqual(status, 411)

    def test_chunked_rejected(self) -> None:
        status = self.raw_http(b"POST /api/session HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                               b"Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n")
        self.assertEqual(status, 400)

    def test_oversized_body(self) -> None:
        status = self.raw_http(b"POST /api/session HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                               b"Content-Type: application/json\r\n"
                               b"Content-Length: 9000\r\nConnection: close\r\n\r\n")
        self.assertEqual(status, 413)

    def test_bad_content_type(self) -> None:
        status, body, _ = self.req("POST", "/api/session", b"x",
                                   {"Content-Type": "text/plain", "Content-Length": "1"})
        self.assertEqual(status, 415)

    def test_unknown_route_and_method(self) -> None:
        status, _, _ = self.get("/api/nope")
        self.assertEqual(status, 404)
        status, body, headers = self.get("/api/session")
        self.assertEqual(status, 405)
        self.assertEqual(headers.get("Allow"), "POST")

    def test_bad_host_header(self) -> None:
        status = self.raw_http(b"GET /api/health HTTP/1.1\r\nHost: evil.example\r\n"
                               b"Connection: close\r\n\r\n")
        self.assertEqual(status, 421)

    def test_picks_up_external_store_change(self) -> None:
        """A CLI `add-user` while the service runs must be visible immediately."""
        other = Store(self.state / "store.json")
        other.add_user("stu003", "third password")
        status, body, _ = self.post("/api/session",
                                    {"user": "stu003", "password": "third password"})
        self.assertEqual(status, 200, body)

    def test_device_without_address(self) -> None:
        other = Store(self.state / "store.json")
        rec = other.get_device(DEV_ID)
        rec.pop("host", None)
        other.save()
        sid = self.login()
        status, body, _ = self.post("/api/authorize", {"sid": sid, "device": DEV_ID})
        self.assertEqual(status, 409)
        self.assertEqual(body["code"], "device_no_address")


# ---------------------------------------------------------------------------
# Authorization
# ---------------------------------------------------------------------------

class TestAuthorize(Base):
    def test_full_flow(self) -> None:
        sid = self.login()
        status, body, _ = self.post("/api/authorize", {"sid": sid, "device": DEV_ID})
        self.assertEqual(status, 200, body)
        self.assertEqual(body["dev"], DEV_ID)
        self.assertEqual(body["user"], USER)
        # the device really holds it now
        status, lic, _ = self.get("/api/device/%s/license" % DEV_ID,
                                  {"X-Session": sid})
        self.assertEqual(status, 200)
        self.assertEqual(lic["login_state"], "AUTHORIZED")
        self.assertEqual(lic["license"]["lic"], body["lic"])

    def test_unknown_session(self) -> None:
        status, body, _ = self.post("/api/authorize",
                                    {"sid": "deadbeefdeadbeef", "device": DEV_ID})
        self.assertEqual(status, 401)
        self.assertEqual(body["code"], "session_expired")

    def test_expired_session(self) -> None:
        sid = self.login()
        self.ctx.svc._sessions[sid]["expires"] = 0
        status, body, _ = self.post("/api/authorize", {"sid": sid, "device": DEV_ID})
        self.assertEqual(status, 401)

    def test_no_binding(self) -> None:
        sid = self.login()
        self.store.revoke_device(DEV_ID)
        status, body, _ = self.post("/api/authorize", {"sid": sid, "device": DEV_ID})
        self.assertEqual(status, 403)
        self.assertEqual(body["code"], "no_binding")

    def test_unknown_device(self) -> None:
        sid = self.login()
        status, body, _ = self.post("/api/authorize",
                                    {"sid": sid, "device": "Eep9999999999999"})
        self.assertEqual(status, 404)
        self.assertEqual(body["code"], "device_unknown")

    def test_bad_ttl(self) -> None:
        sid = self.login()
        status, _, _ = self.post("/api/authorize",
                                 {"sid": sid, "device": DEV_ID, "ttl": 5})
        self.assertEqual(status, 400)

    def test_license_requires_auth(self) -> None:
        status, _, _ = self.get("/api/device/%s/license" % DEV_ID)
        self.assertEqual(status, 401)


class TestFakeDevice(Base):
    """Cases a healthy simulator cannot produce."""

    def client_factory(self, host, port):        # noqa: N802 - test hook
        return FakeDevice(delay=self._delay, ident=self._ident)

    def setUp(self) -> None:
        self._delay = 0.0
        self._ident = DEV_ID
        super().setUp()

    def test_device_mismatch(self) -> None:
        self._ident = "Eep1111111111111"
        sid = self.login()
        status, body, _ = self.post("/api/authorize", {"sid": sid, "device": DEV_ID})
        self.assertEqual(status, 409)
        self.assertEqual(body["code"], "device_mismatch")

    def test_device_timeout(self) -> None:
        self.ctx.client_factory = lambda host, port: FakeDevice(timeout=True)
        sid = self.login()
        status, body, _ = self.post("/api/authorize", {"sid": sid, "device": DEV_ID})
        self.assertEqual(status, 504)

    def test_device_busy(self) -> None:
        self.ctx.client_factory = lambda host, port: FakeDevice(delay=2.0,
                                                                ident=DEV_ID)
        sid = self.login()
        results = []

        def run():
            results.append(self.post("/api/authorize",
                                     {"sid": sid, "device": DEV_ID})[0])

        t = threading.Thread(target=run)
        t.start()
        time.sleep(0.3)
        status, body, _ = self.post("/api/authorize", {"sid": sid, "device": DEV_ID})
        t.join()
        self.assertEqual(status, 409)
        self.assertEqual(body["code"], "device_busy")


class TestHealthDuringDeviceWork(Base):
    """Regression: a slow device call must not block unrelated requests."""

    def client_factory(self, host, port):        # noqa: N802
        return FakeDevice(delay=2.0)

    def test_health_while_authorizing(self) -> None:
        sid = self.login()
        t = threading.Thread(target=lambda: self.post(
            "/api/authorize", {"sid": sid, "device": DEV_ID}))
        t.start()
        time.sleep(0.3)
        started = time.monotonic()
        status, _, _ = self.get("/api/health")
        elapsed = time.monotonic() - started
        t.join()
        self.assertEqual(status, 200)
        self.assertLess(elapsed, 0.5, "health was blocked by device I/O")


# ---------------------------------------------------------------------------
# Static UI + device list + Origin policy
# ---------------------------------------------------------------------------

class TestStatic(Base):
    def test_index_and_admin_pages(self) -> None:
        for path in ("/", "/admin"):
            status, body, headers = self.get(path)
            self.assertEqual(status, 200, path)
            self.assertIn("text/html; charset=utf-8", headers.get("Content-Type", ""))
            self.assertIn("<title>", body.get("_raw", ""))
            self.assertTrue(headers.get("Content-Security-Policy"))

    def test_assets(self) -> None:
        status, _, headers = self.get("/static/app.css")
        self.assertEqual(status, 200)
        self.assertIn("text/css", headers.get("Content-Type", ""))
        status, _, headers = self.get("/static/app.js")
        self.assertEqual(status, 200)
        self.assertIn("javascript", headers.get("Content-Type", ""))

    def test_traversal_is_refused(self) -> None:
        for path in ("/static/../http_api.py",
                     "/static/%2e%2e/http_api.py",
                     "/static/.state/store.json"):
            status, body, _ = self.get(path)
            self.assertIn(status, (400, 404), path)
            raw = body.get("_raw", "")
            self.assertNotIn("def main", raw)
            self.assertNotIn("users", raw)

    def test_origin_same_origin_allowed(self) -> None:
        status, _, _ = self.get("/api/health",
                                {"Origin": "http://127.0.0.1:%d" % self.port})
        self.assertEqual(status, 200)

    def test_origin_foreign_refused(self) -> None:
        status, body, _ = self.get("/api/health",
                                   {"Origin": "http://evil.example"})
        self.assertEqual(status, 403)
        self.assertEqual(body.get("code"), "cross_origin")

    def test_device_list_requires_auth_and_hides_details(self) -> None:
        status, _, _ = self.get("/api/devices")
        self.assertEqual(status, 401)
        sid = self.login()
        status, body, _ = self.get("/api/devices", {"X-Session": sid})
        self.assertEqual(status, 200)
        self.assertEqual(body["devices"][0]["device"], DEV_ID)
        text = json.dumps(body)
        self.assertNotIn("pubkey", text)
        self.assertNotIn("host", text)


# ---------------------------------------------------------------------------
# Admin + audit
# ---------------------------------------------------------------------------

class TestAdmin(Base):
    ADMIN_H = {"X-Admin-Token": ADMIN}

    def test_admin_required(self) -> None:
        status, _, _ = self.post("/api/admin/users",
                                 {"user": "x", "password": "12345678"})
        self.assertEqual(status, 401)
        status, _, _ = self.post("/api/admin/users",
                                 {"user": "x", "password": "12345678"},
                                 {"X-Admin-Token": "wrong"})
        self.assertEqual(status, 401)

    def test_add_user_and_login(self) -> None:
        status, _, _ = self.post("/api/admin/users",
                                 {"user": "stu002", "password": "second pass"},
                                 self.ADMIN_H)
        self.assertEqual(status, 201)
        status, body, _ = self.post("/api/session",
                                    {"user": "stu002", "password": "second pass"})
        self.assertEqual(status, 200)
        # duplicate -> 409
        status, body, _ = self.post("/api/admin/users",
                                    {"user": "stu002", "password": "second pass"},
                                    self.ADMIN_H)
        self.assertEqual(status, 409)

    def test_register_device_duplicate(self) -> None:
        body = {"device": DEV_ID, "pubkey": "ab" * 64, "host": "127.0.0.1",
                "port": 5005}
        status, _, _ = self.post("/api/admin/devices", body, self.ADMIN_H)
        self.assertEqual(status, 409)

    def test_revoke_then_authorize_fails(self) -> None:
        sid = self.login()
        status, _, _ = self.post("/api/admin/revoke", {"device": DEV_ID},
                                 self.ADMIN_H)
        self.assertEqual(status, 200)
        status, body, _ = self.post("/api/authorize", {"sid": sid, "device": DEV_ID})
        self.assertEqual(status, 403)

    def test_revoke_unknown_jti(self) -> None:
        status, body, _ = self.post("/api/admin/revoke", {"jti": "nope"},
                                    self.ADMIN_H)
        self.assertEqual(status, 404)

    def test_revoke_by_license_number(self) -> None:
        sid = self.login()
        status, body, _ = self.post("/api/authorize", {"sid": sid, "device": DEV_ID})
        self.assertEqual(status, 200, body)
        lic = body["lic"]

        status, listing, _ = self.get("/api/admin/licenses", self.ADMIN_H)
        self.assertEqual(status, 200)
        entry = next(x for x in listing["licenses"] if x["lic"] == lic)
        self.assertFalse(entry["revoked"])
        self.assertTrue(entry.get("jti"))

        status, body, _ = self.post("/api/admin/revoke", {"lic": lic}, self.ADMIN_H)
        self.assertEqual(status, 200, body)
        self.assertEqual(body["lic"], lic)

        status, listing, _ = self.get("/api/admin/licenses", self.ADMIN_H)
        entry = next(x for x in listing["licenses"] if x["lic"] == lic)
        self.assertTrue(entry["revoked"])

        # second attempt is a conflict, unknown numbers are 404
        status, _, _ = self.post("/api/admin/revoke", {"lic": lic}, self.ADMIN_H)
        self.assertEqual(status, 409)
        status, _, _ = self.post("/api/admin/revoke", {"lic": "L99999"}, self.ADMIN_H)
        self.assertEqual(status, 404)

    def test_duplicate_binding_refused(self) -> None:
        body = {"user": USER, "device": DEV_ID, "days": 30}
        status, resp, _ = self.post("/api/admin/bindings", body, self.ADMIN_H)
        self.assertEqual(status, 409)
        self.assertEqual(resp["code"], "binding_exists")

        body["replace"] = True
        status, _, _ = self.post("/api/admin/bindings", body, self.ADMIN_H)
        self.assertEqual(status, 201)

        # exactly one active binding remains
        status, listing, _ = self.get("/api/admin/devices", self.ADMIN_H)
        self.assertEqual(status, 200)
        self.assertEqual(listing["devices"][0]["bindings"], 1)

    def test_licenses_requires_admin(self) -> None:
        status, _, _ = self.get("/api/admin/licenses")
        self.assertEqual(status, 401)
        status, _, _ = self.get("/api/admin/licenses", {"X-Admin-Token": "no"})
        self.assertEqual(status, 401)

    def test_devices_listing(self) -> None:
        status, body, _ = self.get("/api/admin/devices", self.ADMIN_H)
        self.assertEqual(status, 200)
        self.assertEqual(body["devices"][0]["device"], DEV_ID)

    def test_audit_tail_and_no_secrets(self) -> None:
        wrong = "wrong-password-xyz"
        self.login()
        self.post("/api/session", {"user": USER, "password": wrong})
        self.get("/api/route-that-does-not-exist")
        status, body, _ = self.get("/api/admin/audit?limit=50", self.ADMIN_H)
        self.assertEqual(status, 200)
        events = [e["event"] for e in body["events"]]
        self.assertIn("login_ok", events)
        self.assertIn("login_failed", events)
        self.assertIn("request_denied", events)

        raw = (self.state / "audit.jsonl").read_text(encoding="utf-8")
        self.assertNotIn(PWD, raw)
        self.assertNotIn(wrong, raw)          # passwords never reach the log
        self.assertNotIn(ADMIN, raw)          # nor the admin token
        sid = self.login()
        self.assertNotIn(sid, raw)            # nor raw session ids


if __name__ == "__main__":
    unittest.main(verbosity=2)
