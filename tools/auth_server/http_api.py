"""HTTP/JSON API for the DEV authorization service (stdlib only).

Run:
    python -m tools.auth_server.http_api --port 8080

Design notes (see Markdown/设备授权协议_v0.md §12):
  * plaintext HTTP, loopback by default; `--unsafe-lan` is an explicit opt-in;
  * admin token in `X-Admin-Token`, never in the query string;
  * device host/port come from the enrollment record, NEVER from a request;
  * a per-device lock serializes device round trips (a first call can take
    seconds); a short global lock only wraps service/store calls;
  * every rejection is audited; secrets never are.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import os
import re
import secrets
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote_plus, urlsplit

from . import proto
from .audit import AuditLog
from .device_client import DeviceClient, DeviceError, DeviceTimeout
from .flow import FlowError, authorize
from .keystore import CAKey
from .service import AuthError, AuthService
from .store import Store

VERSION = "0.2"
STATE_DIR = Path(__file__).resolve().parent / ".state"
WEB_DIR = Path(__file__).resolve().parent / "web"

CSP = ("default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; "
       "connect-src 'self'; img-src 'self' data:")
STATIC_TYPES = {
    ".html": "text/html; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".js": "application/javascript; charset=utf-8",
    ".svg": "image/svg+xml",
    ".ico": "image/x-icon",
}

MAX_BODY = 8192
MAX_CONCURRENT = 16
ADMIN_FAIL_WINDOW_S = 60
ADMIN_FAIL_MAX = 10
DEVICE_RE = re.compile(r"^[A-Za-z0-9]{16}$")
USER_RE = re.compile(r"^[A-Za-z0-9._:@-]{1,32}$")
HEX64_RE = re.compile(r"^[0-9A-Fa-f]{128}$")
LOOPBACK = {"127.0.0.1", "localhost", "::1"}


class ApiError(Exception):
    def __init__(self, status: int, code: str, message: str = "", detail: str = ""):
        super().__init__(message or code)
        self.status = status
        self.code = code
        self.message = message or code
        self.detail = detail


def _sha8(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()[:8]


def _pairs_hook(pairs):
    seen = set()
    for key, _ in pairs:
        if key in seen:
            raise ValueError("duplicate key: %s" % key)
        seen.add(key)
    return dict(pairs)


def _reject_number(value):
    raise ValueError("non-integer number not allowed: %s" % value)


class ServerContext:
    def __init__(self, store: Store, ca: CAKey, audit: AuditLog,
                 admin_token: str, client_factory=None):
        self.store = store
        self.ca = ca
        self.audit = audit
        self.svc = AuthService(store, ca)
        self.svc_lock = threading.RLock()
        self.admin_token = admin_token
        self.client_factory = client_factory or (lambda host, port: DeviceClient(host, port))
        self.sem = threading.BoundedSemaphore(MAX_CONCURRENT)
        self._locks: dict[str, threading.Lock] = {}
        self._locks_guard = threading.Lock()
        self._admin_fails: dict[str, list[float]] = {}
        self._admin_lock = threading.Lock()

    # ---- device plumbing -------------------------------------------------
    def device_lock(self, dev: str) -> threading.Lock:
        with self._locks_guard:
            lock = self._locks.get(dev)
            if lock is None:
                lock = threading.Lock()
                self._locks[dev] = lock
            return lock

    def client_for(self, dev: str):
        rec = self.store.get_device(dev)
        if rec is None:
            raise ApiError(404, "device_unknown", "device is not registered")
        host = rec.get("host")
        if not host:
            raise ApiError(409, "device_no_address",
                           "device has no host/port; re-enroll it with --host")
        return self.client_factory(host, int(rec.get("port", 5005) or 5005))

    # ---- admin auth ------------------------------------------------------
    def require_admin(self, peer: str, token: str | None) -> None:
        now = time.time()
        with self._admin_lock:
            fails = [t for t in self._admin_fails.get(peer, [])
                     if now - t < ADMIN_FAIL_WINDOW_S]
            if len(fails) >= ADMIN_FAIL_MAX:
                self._admin_fails[peer] = fails
                raise ApiError(429, "admin_rate_limited",
                               "too many failed admin attempts")
            ok = bool(token) and secrets.compare_digest(str(token), self.admin_token or "")
            if not ok:
                fails.append(now)
                self._admin_fails[peer] = fails
                raise ApiError(401, "admin_denied", "valid X-Admin-Token required")
            self._admin_fails[peer] = []


class AuthHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = "DSOAuth/" + VERSION
    sys_version = ""

    @property
    def ctx(self) -> ServerContext:
        return self.server.ctx                      # type: ignore[attr-defined]

    def log_message(self, fmt, *args):              # never log headers/bodies
        sys.stderr.write("[http] %s %s\n" % (self.address_string(), fmt % args))

    # ---- verbs -----------------------------------------------------------
    def do_GET(self):
        self._dispatch("GET")

    def do_POST(self):
        self._dispatch("POST")

    def _dispatch(self, method: str):
        self._sent = False
        path = urlsplit(self.path).path
        try:
            if not self.ctx.sem.acquire(timeout=5):
                raise ApiError(503, "busy", "server busy")
            try:
                self._check_host()
                if path.startswith("/api/") and path != "/api/health":
                    # Pick up changes made by the CLI while we are running.
                    with self.ctx.svc_lock:
                        self.ctx.store.refresh()
                self._route(method)
            finally:
                self.ctx.sem.release()
        except ApiError as exc:
            self._send_json(exc.status, {"ok": False, "code": exc.code,
                                         "error": exc.message,
                                         "detail": exc.detail or None})
            self.ctx.audit.write("request_denied", method=method, path=path,
                                 status=exc.status, code=exc.code,
                                 peer=self.client_address[0])
        except (BrokenPipeError, ConnectionResetError):
            return
        except Exception as exc:                     # noqa: BLE001 - never leak a stack
            self._send_json(500, {"ok": False, "code": "internal",
                                  "error": "internal error"})
            self.ctx.audit.write("internal_error", method=method, path=path,
                                 detail=repr(exc), peer=self.client_address[0])

    # ---- guards ----------------------------------------------------------
    def _check_host(self) -> None:
        host = (self.headers.get("Host") or "").split(":")[0].strip("[]").lower()
        if host and host not in LOOPBACK:
            raise ApiError(421, "bad_host", "unexpected Host header")
        origin = self.headers.get("Origin")
        if origin:
            # Browsers attach Origin to same-origin POSTs too, so the UI needs
            # an allowlist rather than an outright rejection.
            port = self.server.server_address[1]
            allowed = {"http://127.0.0.1:%d" % port,
                       "http://localhost:%d" % port,
                       "http://[::1]:%d" % port}
            if origin not in allowed:
                raise ApiError(403, "cross_origin",
                               "cross-origin requests are not allowed")

    def _json_body(self) -> dict:
        if self.headers.get("Transfer-Encoding"):
            raise ApiError(400, "bad_request", "chunked encoding is not supported")
        if self.headers.get("Content-Encoding"):
            raise ApiError(400, "bad_request", "content-encoding is not supported")
        ctype = (self.headers.get("Content-Type") or "").split(";")[0].strip().lower()
        if ctype and ctype != "application/json":
            raise ApiError(415, "bad_content_type", "Content-Type must be application/json")
        raw_len = self.headers.get("Content-Length")
        if raw_len is None:
            raise ApiError(411, "length_required", "Content-Length is required")
        try:
            n = int(raw_len)
        except ValueError:
            raise ApiError(400, "bad_request", "bad Content-Length") from None
        if n < 0 or n > MAX_BODY:
            raise ApiError(413, "too_large", "body exceeds %d bytes" % MAX_BODY)
        data = self.rfile.read(n)
        if len(data) != n:
            raise ApiError(400, "bad_request", "short body")
        try:
            text = data.decode("utf-8")
        except UnicodeDecodeError:
            raise ApiError(400, "bad_request", "body is not UTF-8") from None
        try:
            obj = json.loads(text, object_pairs_hook=_pairs_hook,
                             parse_float=_reject_number,
                             parse_constant=_reject_number)
        except ValueError as exc:
            raise ApiError(400, "bad_json", str(exc)) from None
        if not isinstance(obj, dict):
            raise ApiError(400, "bad_json", "top-level value must be an object")
        return obj

    def _session_id(self, body: dict | None = None) -> str:
        sid = (body or {}).get("sid") or self.headers.get("X-Session")
        if not sid:
            raise ApiError(401, "auth_required", "session required")
        return str(sid)

    def _validate_session(self, sid: str) -> str:
        try:
            with self.ctx.svc_lock:
                return self.ctx.svc.validate_session(sid)
        except AuthError as exc:
            raise ApiError(401, exc.code, str(exc)) from None

    # ---- routing ---------------------------------------------------------
    def _route(self, method: str) -> None:
        path = urlsplit(self.path).path
        if ".." in path or "\\" in path or "%2e" in path.lower() or "\x00" in path:
            raise ApiError(400, "bad_path", "invalid path")

        if path == "/api/health":
            self._need(method, "GET")
            self._send_json(200, {"ok": True, "version": VERSION})
            return
        if path == "/api/session":
            self._need(method, "POST")
            self._post_session()
            return
        if path == "/api/session/logout":
            self._need(method, "POST")
            self._post_logout()
            return
        if path == "/api/authorize":
            self._need(method, "POST")
            self._post_authorize()
            return
        if path == "/api/admin/users":
            self._need(method, "POST")
            self._post_admin_user()
            return
        if path == "/api/admin/devices":
            if method == "GET":
                self._get_admin_devices()
            else:
                self._need(method, "POST")
                self._post_admin_device()
            return
        if path == "/api/admin/bindings":
            self._need(method, "POST")
            self._post_admin_binding()
            return
        if path == "/api/admin/revoke":
            self._need(method, "POST")
            self._post_admin_revoke()
            return
        if path == "/api/admin/licenses":
            self._need(method, "GET")
            self._get_admin_licenses()
            return
        if path == "/api/admin/audit":
            self._need(method, "GET")
            self._get_admin_audit()
            return
        if path == "/api/admin/audit/export":
            self._need(method, "GET")
            self._get_admin_audit_export()
            return
        if path == "/api/devices":
            self._need(method, "GET")
            self._get_devices()
            return
        if path == "/api/history":
            self._need(method, "GET")
            self._get_history()
            return

        m = re.fullmatch(r"/api/device/([^/]+)/license", path)
        if m:
            self._need(method, "GET")
            self._get_device_license(m.group(1))
            return
        m = re.fullmatch(r"/api/admin/device/([^/]+)/clear-license", path)
        if m:
            self._need(method, "POST")
            self._post_admin_clear_license(m.group(1))
            return

        if self._serve_static(path):
            return
        raise ApiError(404, "not_found", "no such endpoint")

    def _need(self, method: str, expected: str) -> None:
        if method != expected:
            raise ApiError(405, "method_not_allowed",
                           "use %s" % expected, detail=expected)

    # ---- session ---------------------------------------------------------
    def _post_session(self) -> None:
        body = self._json_body()
        user = body.get("user")
        password = body.get("password")
        if not isinstance(user, str) or not USER_RE.match(user):
            raise ApiError(400, "bad_user", "bad user name")
        if not isinstance(password, str) or not (1 <= len(password) <= 128):
            raise ApiError(400, "bad_password", "password must be 1..128 chars")
        ctx = self.ctx
        try:
            with ctx.svc_lock:
                sid = ctx.svc.login(user, password)
                expires = ctx.svc.session_expires(sid)
        except AuthError as exc:
            ctx.audit.write("login_failed", user=user, code=exc.code,
                            peer=self.client_address[0])
            status = 429 if exc.code == "locked" else 401
            raise ApiError(status, exc.code, str(exc)) from None
        ctx.audit.write("login_ok", user=user, sid=_sha8(sid),
                        peer=self.client_address[0])
        self._send_json(200, {"ok": True, "sid": sid, "user": user,
                              "expires_at": expires})

    def _post_logout(self) -> None:
        body = self._json_body()
        sid = self._session_id(body)
        with self.ctx.svc_lock:
            self.ctx.svc.logout(sid)
        self.ctx.audit.write("logout", sid=_sha8(sid), peer=self.client_address[0])
        self._send_json(200, {"ok": True})

    # ---- authorize -------------------------------------------------------
    def _post_authorize(self) -> None:
        body = self._json_body()
        sid = self._session_id(body)
        dev = body.get("device")
        ttl = body.get("ttl", 900)
        if not isinstance(dev, str) or not DEVICE_RE.match(dev):
            raise ApiError(400, "bad_device", "device must be 16 alphanumerics")
        if isinstance(ttl, bool) or not isinstance(ttl, int) or not (60 <= ttl <= 86400):
            raise ApiError(400, "bad_ttl", "ttl must be 60..86400 seconds")

        user = self._validate_session(sid)
        ctx = self.ctx
        lock = ctx.device_lock(dev)
        if not lock.acquire(timeout=1.0):
            raise ApiError(409, "device_busy", "another request is using this device")
        try:
            client = ctx.client_for(dev)
            try:
                with client:
                    ack = client.get_devid()
                    got = (ack.get("data") or {}).get("device_id")
                    if ack.get("result") != "ok" or got != dev:
                        raise ApiError(409, "device_mismatch",
                                       "device did not identify as %s" % dev)
                    res = authorize(ctx.svc, client, sid, dev, ttl_s=ttl,
                                    lock=ctx.svc_lock)
            except DeviceTimeout as exc:
                raise ApiError(504, "device_timeout",
                               "no reply from device; outcome unknown - query the "
                               "device before retrying") from None
            except DeviceError as exc:
                raise ApiError(502, "device_error", str(exc)) from None
            except FlowError as exc:
                status = {"device_unknown": 404, "push_failed": 502,
                          "device_proof_failed": 502}.get(exc.code, 403)
                ctx.audit.write("authorize_failed", user=user, device=dev,
                                code=exc.code, detail=exc.detail,
                                peer=self.client_address[0])
                raise ApiError(status, exc.code, str(exc), exc.detail) from None
        finally:
            lock.release()

        fields = res["fields"]
        ctx.audit.write("authorize_ok", user=user, device=dev, lic=fields["lic"],
                        jti=fields["jti"], na=fields["na"], perm=fields["perm"],
                        peer=self.client_address[0])
        self._send_json(200, {"ok": True, "lic": fields["lic"],
                              "user": fields["user"], "dev": fields["dev"],
                              "nb": fields["nb"], "na": fields["na"],
                              "perm": fields["perm"],
                              "device_ack": res["ack"]})

    # ---- device query ----------------------------------------------------
    def _get_device_license(self, dev: str) -> None:
        if not DEVICE_RE.match(dev):
            raise ApiError(400, "bad_device", "device must be 16 alphanumerics")
        self._require_session_or_admin()
        ctx = self.ctx
        lock = ctx.device_lock(dev)
        if not lock.acquire(timeout=1.0):
            raise ApiError(409, "device_busy", "another request is using this device")
        try:
            client = ctx.client_for(dev)
            try:
                with client:
                    ack = client.get_license()
            except DeviceTimeout:
                raise ApiError(504, "device_timeout", "no reply from device") from None
            except DeviceError as exc:
                raise ApiError(502, "device_error", str(exc)) from None
        finally:
            lock.release()
        if ack.get("result") != "ok":
            raise ApiError(502, "device_error", str(ack.get("error", "")))
        data = ack.get("data") or {}
        self._send_json(200, {"ok": True,
                              "device_id": data.get("device_id"),
                              "login_state": data.get("login_state"),
                              "license": data.get("license")})

    # ---- admin: erase the license stored on the device -------------------
    def _post_admin_clear_license(self, dev: str) -> None:
        self._admin()
        if not DEVICE_RE.match(dev):
            raise ApiError(400, "bad_device", "device must be 16 alphanumerics")
        ctx = self.ctx
        lock = ctx.device_lock(dev)
        if not lock.acquire(timeout=1.0):
            raise ApiError(409, "device_busy", "another request is using this device")
        try:
            client = ctx.client_for(dev)
            try:
                with client:
                    ack = client.clear_license()
            except DeviceTimeout:
                raise ApiError(504, "device_timeout", "no reply from device") from None
            except DeviceError as exc:
                raise ApiError(502, "device_error", str(exc)) from None
        finally:
            lock.release()
        if ack.get("result") != "ok":
            raise ApiError(502, "device_error", str(ack.get("error", "")))
        ctx.audit.write("admin_license_cleared", device=dev,
                        peer=self.client_address[0])
        self._send_json(200, {"ok": True, "device": dev})

    def _require_session_or_admin(self) -> str | None:
        """@return the session's user name, or None for an admin-token caller."""
        token = self.headers.get("X-Admin-Token")
        if token is not None:
            self.ctx.require_admin(self.client_address[0], token)
            return None
        sid = self.headers.get("X-Session")
        if not sid:
            raise ApiError(401, "auth_required", "X-Session or X-Admin-Token required")
        return self._validate_session(sid)

    # ---- admin: users / devices / bindings / revoke ----------------------
    def _admin(self) -> None:
        self.ctx.require_admin(self.client_address[0],
                               self.headers.get("X-Admin-Token"))

    def _post_admin_user(self) -> None:
        self._admin()
        body = self._json_body()
        user = body.get("user")
        password = body.get("password")
        if not isinstance(user, str) or not USER_RE.match(user):
            raise ApiError(400, "bad_user", "bad user name")
        if not isinstance(password, str) or not (8 <= len(password) <= 128):
            raise ApiError(400, "bad_password", "password must be 8..128 chars")
        ctx = self.ctx
        with ctx.svc_lock:
            if ctx.store.has_user(user):
                raise ApiError(409, "user_exists", "user already exists")
            ctx.store.add_user(user, password)
        ctx.audit.write("admin_user_added", user=user, peer=self.client_address[0])
        self._send_json(201, {"ok": True, "user": user})

    def _post_admin_device(self) -> None:
        self._admin()
        body = self._json_body()
        dev = body.get("device")
        pubkey = body.get("pubkey")
        host = body.get("host")
        port = body.get("port", 5005)
        capability = body.get("capability", "provisioned")
        if not isinstance(dev, str) or not DEVICE_RE.match(dev):
            raise ApiError(400, "bad_device", "device must be 16 alphanumerics")
        if not isinstance(pubkey, str) or not HEX64_RE.match(pubkey):
            raise ApiError(400, "bad_pubkey", "pubkey must be 128 hex chars")
        if not isinstance(host, str) or not (1 <= len(host) <= 64):
            raise ApiError(400, "bad_host", "host is required")
        if isinstance(port, bool) or not isinstance(port, int) or not (1 <= port <= 65535):
            raise ApiError(400, "bad_port", "port must be 1..65535")
        if capability not in ("provisioned", "identity_only"):
            raise ApiError(400, "bad_capability", "capability must be provisioned|identity_only")
        ctx = self.ctx
        with ctx.svc_lock:
            try:
                ctx.store.register_device(dev, pubkey, "k1", capability,
                                          host=host, port=port, overwrite=False)
            except ValueError:
                raise ApiError(409, "device_exists", "device already registered") from None
        ctx.audit.write("admin_device_added", device=dev, host=host, port=port,
                        capability=capability, peer=self.client_address[0])
        self._send_json(201, {"ok": True, "device": dev})

    def _post_admin_binding(self) -> None:
        self._admin()
        body = self._json_body()
        user = body.get("user")
        dev = body.get("device")
        days = body.get("days", 30)
        if not isinstance(user, str) or not USER_RE.match(user):
            raise ApiError(400, "bad_user", "bad user name")
        if not isinstance(dev, str) or not DEVICE_RE.match(dev):
            raise ApiError(400, "bad_device", "device must be 16 alphanumerics")
        if isinstance(days, bool) or not isinstance(days, int) or not (1 <= days <= 3650):
            raise ApiError(400, "bad_days", "days must be 1..3650")
        replace = body.get("replace", False)
        if not isinstance(replace, bool):
            raise ApiError(400, "bad_request", "replace must be a boolean")
        ctx = self.ctx
        with ctx.svc_lock:
            if not ctx.store.has_user(user):
                raise ApiError(404, "user_unknown", "unknown user")
            if ctx.store.get_device(dev) is None:
                raise ApiError(404, "device_unknown", "unknown device")
            now = ctx.svc._now_off()
            try:
                ctx.store.add_binding(user, dev, now, now + days * 86400,
                                      replace=replace)
            except ValueError as exc:
                raise ApiError(409, "binding_exists", str(exc)) from None
        ctx.audit.write("admin_binding_added", user=user, device=dev, days=days,
                        replaced=replace, peer=self.client_address[0])
        self._send_json(201, {"ok": True, "replaced": replace})

    def _post_admin_revoke(self) -> None:
        self._admin()
        body = self._json_body()
        jti = body.get("jti")
        dev = body.get("device")
        ctx = self.ctx
        if jti is not None:
            if not isinstance(jti, str) or not (1 <= len(jti) <= 16):
                raise ApiError(400, "bad_jti", "bad jti")
            with ctx.svc_lock:
                if ctx.store.get_license(jti) is None:
                    raise ApiError(404, "license_unknown", "unknown jti")
                ctx.store.revoke_license(jti)
            ctx.audit.write("admin_revoke_license", jti=jti, peer=self.client_address[0])
            self._send_json(200, {"ok": True, "jti": jti})
            return
        lic = body.get("lic")
        if lic is not None:
            # The operator sees "L00005" in the UI, not the jti — accept both.
            if not isinstance(lic, str) or not (1 <= len(lic) <= 16):
                raise ApiError(400, "bad_lic", "bad license number")
            with ctx.svc_lock:
                try:
                    jti = ctx.store.revoke_by_lic(lic)
                except KeyError:
                    raise ApiError(404, "license_unknown",
                                   "unknown license number: %s" % lic) from None
                except ValueError as exc:
                    raise ApiError(409, "already_revoked", str(exc)) from None
            ctx.audit.write("admin_revoke_license", jti=jti, lic=lic,
                            peer=self.client_address[0])
            self._send_json(200, {"ok": True, "lic": lic, "jti": jti})
            return
        if dev is not None:
            if not isinstance(dev, str) or not DEVICE_RE.match(dev):
                raise ApiError(400, "bad_device", "device must be 16 alphanumerics")
            with ctx.svc_lock:
                n = ctx.store.revoke_device(dev)
            ctx.audit.write("admin_revoke_device", device=dev, records=n,
                            peer=self.client_address[0])
            self._send_json(200, {"ok": True, "device": dev, "records": n})
            return
        raise ApiError(400, "bad_request", "give jti or device")

    def _get_admin_licenses(self) -> None:
        self._admin()
        with self.ctx.svc_lock:
            items = self.ctx.store.list_licenses()
        self._send_json(200, {"ok": True, "licenses": items})

    def _get_admin_devices(self) -> None:
        self._admin()
        with self.ctx.svc_lock:
            devices = self.ctx.store.list_devices()
            bindings = self.ctx.store.list_bindings()
            licenses = self.ctx.store.list_licenses()
        out = []
        for dev, rec in sorted(devices.items()):
            out.append({"device": dev,
                        "pubkey": rec.get("pubkey"),
                        "capability": rec.get("capability"),
                        "host": rec.get("host"),
                        "port": rec.get("port"),
                        "bindings": sum(1 for b in bindings
                                        if b["dev"] == dev and not b["revoked"]),
                        "licenses": sum(1 for l in licenses
                                        if l.get("dev") == dev)})
        self._send_json(200, {"ok": True, "users": self.ctx.store.list_users(),
                              "devices": out})

    def _query_pairs(self) -> dict:
        pairs = {}
        query = urlsplit(self.path).query
        for part in query.split("&") if query else []:
            key, _, value = part.partition("=")
            if key:
                pairs[key] = unquote_plus(value)
        return pairs

    def _audit_filters(self) -> tuple[str | None, str | None]:
        pairs = self._query_pairs()
        event = pairs.get("event") or None
        device = pairs.get("device") or None
        if event is not None and not re.fullmatch(r"[a-z0-9_]{1,32}", event):
            raise ApiError(400, "bad_event", "bad event filter")
        if device is not None and not DEVICE_RE.match(device):
            raise ApiError(400, "bad_device", "device must be 16 alphanumerics")
        return event, device

    def _get_admin_audit(self) -> None:
        self._admin()
        pairs = self._query_pairs()
        limit = 100
        if "limit" in pairs:
            try:
                limit = int(pairs["limit"])
            except ValueError:
                raise ApiError(400, "bad_limit", "limit must be an integer") from None
        if limit < 0 or limit > 500:
            raise ApiError(400, "bad_limit", "limit must be 0..500")
        event, device = self._audit_filters()
        events = self.ctx.audit.tail(limit, event=event, device=device)
        self._send_json(200, {"ok": True, "events": events,
                              "filter": {"event": event, "device": device}})

    def _get_admin_audit_export(self) -> None:
        self._admin()
        event, device = self._audit_filters()
        rows = self.ctx.audit.tail(500, event=event, device=device)
        buf = io.StringIO()
        writer = csv.writer(buf)
        writer.writerow(["ts", "event", "user", "device", "lic", "jti", "peer", "detail"])
        for rec in rows:
            writer.writerow([rec.get("ts", ""), rec.get("event", ""),
                             rec.get("user", ""), rec.get("device", ""),
                             rec.get("lic", ""), rec.get("jti", ""),
                             rec.get("peer", ""), rec.get("detail", "")])
        body = buf.getvalue().encode("utf-8")
        self._send_bytes(200, body, "text/csv; charset=utf-8",
                         extra={"Content-Disposition":
                                'attachment; filename="audit.csv"'})

    # ---- device list for the student page --------------------------------
    def _get_devices(self) -> None:
        """Device IDs and capability only — and only what this user may use.

        Deliberately omits pubkey/host/port (the command endpoint is nobody's
        business) and, for a student session, hides devices they are not bound
        to. An admin token still sees everything.
        """
        user = self._require_session_or_admin()
        with self.ctx.svc_lock:
            devices = self.ctx.store.list_devices()
            if user is not None:
                allowed = set(self.ctx.store.list_bound_devices(
                    user, proto.now_offset()))
                devices = {d: r for d, r in devices.items() if d in allowed}
        out = [{"device": dev, "capability": rec.get("capability")}
               for dev, rec in sorted(devices.items())]
        self._send_json(200, {"ok": True, "devices": out})

    # ---- the student's own history ---------------------------------------
    def _get_history(self) -> None:
        user = self._require_session_or_admin()
        pairs = self._query_pairs()
        limit = 50
        if "limit" in pairs:
            try:
                limit = int(pairs["limit"])
            except ValueError:
                raise ApiError(400, "bad_limit", "limit must be an integer") from None
        if limit < 1 or limit > 100:
            raise ApiError(400, "bad_limit", "limit must be 1..100")
        with self.ctx.svc_lock:
            items = self.ctx.store.list_licenses(user=user)[:limit]
        history = [{"lic": x.get("lic"), "dev": x.get("dev"),
                    "nb": x.get("nb"), "na": x.get("na"),
                    "revoked": bool(x.get("revoked"))}
                   for x in items]
        self._send_json(200, {"ok": True, "user": user, "history": history})

    # ---- static UI -------------------------------------------------------
    def _serve_static(self, path: str) -> bool:
        if path == "/":
            name = "index.html"
        elif path == "/admin":
            name = "admin.html"
        elif path.startswith("/static/"):
            name = path[len("/static/"):]
        else:
            return False
        if (not name or "/" in name or "\\" in name or name.startswith(".")
                or "\x00" in name or ".." in name):
            raise ApiError(404, "not_found", "no such file")
        target = WEB_DIR / name
        try:
            if not target.is_file():
                raise ApiError(404, "not_found", "no such file")
            body = target.read_bytes()
        except OSError:
            raise ApiError(404, "not_found", "no such file") from None
        ctype = STATIC_TYPES.get(Path(name).suffix.lower(),
                                 "application/octet-stream")
        self._send_bytes(200, body, ctype)
        return True

    # ---- response --------------------------------------------------------
    def _send_bytes(self, status: int, body: bytes, content_type: str,
                    extra: dict | None = None) -> None:
        if getattr(self, "_sent", False):
            return
        try:
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("Content-Security-Policy", CSP)
            for key, value in (extra or {}).items():
                self.send_header(key, value)
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            self._sent = True
            self.close_connection = True

    def _send_json(self, status: int, obj: dict) -> None:
        if getattr(self, "_sent", False):
            return
        body = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        try:
            self.send_response(status)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            if status == 405 and obj.get("detail"):
                self.send_header("Allow", str(obj["detail"]))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            self._sent = True
            self.close_connection = True


# ---------------------------------------------------------------------------
# server assembly
# ---------------------------------------------------------------------------

def create_server(host: str, port: int, ctx: ServerContext) -> ThreadingHTTPServer:
    httpd = ThreadingHTTPServer((host, port), AuthHandler)
    httpd.daemon_threads = True
    httpd.ctx = ctx                                    # type: ignore[attr-defined]
    return httpd


def _pid_alive(pid: int) -> bool:
    """Portable-ish liveness probe (os.kill(pid, 0) is not usable on Windows)."""
    if pid <= 0:
        return False
    if os.name == "nt":
        import ctypes
        SYNCHRONIZE = 0x0010_0000
        handle = ctypes.windll.kernel32.OpenProcess(SYNCHRONIZE, False, pid)
        if not handle:
            return False
        ctypes.windll.kernel32.CloseHandle(handle)
        return True
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        return False


def acquire_server_lock(lock_path: Path) -> bool:
    """Take the single-instance lock; steals it if the owner process is gone."""
    for _ in range(2):
        try:
            fd = os.open(lock_path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
            with os.fdopen(fd, "w", encoding="ascii") as fh:
                fh.write(str(os.getpid()))
            return True
        except FileExistsError:
            try:
                pid = int(lock_path.read_text(encoding="ascii").strip() or "0")
            except (OSError, ValueError):
                pid = 0
            if _pid_alive(pid):
                return False
            try:
                os.unlink(lock_path)            # stale: owner is gone
            except OSError:
                return False
    return False


def load_or_create_admin_token(path: Path) -> str:
    try:
        token = path.read_text(encoding="utf-8").strip()
        if token:
            return token
    except FileNotFoundError:
        pass
    token = secrets.token_hex(16)
    fd = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as fh:
        fh.write(token + "\n")
    return token


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="DSO DEV authorization HTTP API")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--state", default=None, help="state dir (default: .state)")
    ap.add_argument("--unsafe-lan", action="store_true",
                    help="allow binding to a non-loopback address (plaintext HTTP!)")
    args = ap.parse_args(argv)

    if args.host not in LOOPBACK and not args.unsafe_lan:
        print("refusing to bind %s: plaintext HTTP on the LAN exposes sessions."
              % args.host)
        print("pass --unsafe-lan if that is really what you want.")
        return 2

    state = Path(args.state) if args.state else STATE_DIR
    state.mkdir(parents=True, exist_ok=True)
    lock_path = state / "server.lock"
    if not acquire_server_lock(lock_path):
        print("another instance appears to be running (%s)." % lock_path)
        print("if it crashed, delete that file and retry.")
        return 3

    try:
        store = Store(state / "store.json")
        ca = CAKey.load_or_create(state / "ca_key.pem")
        token = load_or_create_admin_token(state / "admin_token")
        audit = AuditLog(state / "audit.jsonl")
        ctx = ServerContext(store, ca, audit, token)
        httpd = create_server(args.host, args.port, ctx)
        host, port = httpd.server_address[:2]
        audit.write("server_start", host=host, port=port, version=VERSION)
        print("auth API listening on http://%s:%d" % (host, port), flush=True)
        print("admin token file: %s" % (state / "admin_token"), flush=True)
        print("audit log       : %s" % (state / "audit.jsonl"), flush=True)
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            httpd.server_close()
            audit.write("server_stop", host=host, port=port)
    finally:
        try:
            os.unlink(lock_path)
        except OSError:
            pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
