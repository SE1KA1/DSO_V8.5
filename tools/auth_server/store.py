"""JSON-backed state for the DEV authorization service (stdlib only).

Single-process by design: the firmware accepts one TCP client at a time, so
the service is a short-lived CLI/library, not a daemon. Writes are atomic
(tmp file + os.replace) so a crash cannot leave half a file behind.

Nothing in here is a production user database: passwords use PBKDF2 with a
per-user salt (never bare SHA-256), and no secret is ever logged.
"""

from __future__ import annotations

import hashlib
import json
import os
import time
from pathlib import Path

PBKDF2_ITERATIONS = 600_000
SCHEMA_VERSION = 1


def _default_state() -> dict:
    return {
        "version": SCHEMA_VERSION,
        "seq": 0,
        "users": {},        # user -> {salt, hash, iter}
        "devices": {},      # device_id -> {pubkey, kid, capability, registered_at}
        "bindings": [],     # {user, dev, nb, na, revoked}
        "challenges": {},   # digest_hex -> {dev, user, session, expires}
        "licenses": {},     # jti -> {lic, user, dev, na, revoked}
    }


class Store:
    def __init__(self, path: str | os.PathLike):
        self.path = Path(path)
        self._mtime = 0
        if self.path.exists():
            self.data = json.loads(self.path.read_text(encoding="utf-8"))
            if self.data.get("version") != SCHEMA_VERSION:
                raise RuntimeError("unsupported store version")
            self._mtime = self.path.stat().st_mtime_ns
        else:
            self.data = _default_state()
            self.save()

    def refresh(self) -> None:
        """Reload if another process (e.g. the CLI) rewrote the file.

        The HTTP service keeps a Store in memory while administrators may use
        the CLI to enroll devices. Without this, a change made by the CLI is
        invisible to the running service until it restarts.
        """
        try:
            mtime = self.path.stat().st_mtime_ns
        except OSError:
            return
        if mtime == self._mtime:
            return
        try:
            data = json.loads(self.path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return
        if data.get("version") == SCHEMA_VERSION:
            self.data = data
            self._mtime = mtime

    # ---- persistence -----------------------------------------------------
    def save(self) -> None:
        """Atomic write, with a fallback for Windows file-lock hiccups.

        On Windows `os.replace` can fail with WinError 5 while an antivirus or
        the search indexer holds the destination open for a few milliseconds.
        Retry briefly, then fall back to rewriting the file in place (which
        needs no delete/rename permission).
        """
        self.path.parent.mkdir(parents=True, exist_ok=True)
        payload = json.dumps(self.data, indent=1, sort_keys=True)
        tmp = self.path.with_name("%s.%d.tmp" % (self.path.name, os.getpid()))
        tmp.write_text(payload, encoding="utf-8")

        last: OSError | None = None
        for attempt in range(8):
            try:
                os.replace(tmp, self.path)
                self._mark_saved()
                return
            except OSError as exc:          # WinError 5 / 32: transient lock
                last = exc
                time.sleep(0.05 * (1 + attempt))

        try:
            with open(self.path, "w", encoding="utf-8") as fh:
                fh.write(payload)
            try:
                os.unlink(tmp)
            except OSError:
                pass
            self._mark_saved()
            return
        except OSError:
            pass
        raise last if last is not None else RuntimeError("cannot save store")

    def _mark_saved(self) -> None:
        try:
            self._mtime = self.path.stat().st_mtime_ns
        except OSError:
            self._mtime = 0

    def next_seq(self) -> int:
        self.data["seq"] += 1
        return self.data["seq"]

    # ---- users -----------------------------------------------------------
    def add_user(self, user: str, password: str) -> None:
        salt = os.urandom(16)
        digest = hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"),
                                     salt, PBKDF2_ITERATIONS)
        self.data["users"][user] = {
            "salt": salt.hex(),
            "hash": digest.hex(),
            "iter": PBKDF2_ITERATIONS,
        }
        self.save()

    def check_password(self, user: str, password: str) -> bool:
        rec = self.data["users"].get(user)
        if not rec:
            return False
        digest = hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"),
                                     bytes.fromhex(rec["salt"]), rec["iter"])
        return _const_eq(digest.hex(), rec["hash"])

    def has_user(self, user: str) -> bool:
        return user in self.data["users"]

    def list_users(self) -> list[str]:
        return sorted(self.data["users"].keys())

    # ---- devices ---------------------------------------------------------
    def register_device(self, dev: str, pubkey_hex: str, kid: str = "k1",
                        capability: str = "provisioned",
                        host: str | None = None, port: int = 5005,
                        overwrite: bool = True) -> None:
        """Out-of-band/admin enrollment: never call this from an online path.

        ``host``/``port`` are the device's TCP command endpoint. They are set
        HERE (by an administrator), never taken from a request body — otherwise
        the API would become a LAN scanner.
        """
        if not overwrite and dev in self.data["devices"]:
            raise ValueError("device already registered")
        rec = self.data["devices"].get(dev, {})
        rec.update({
            "pubkey": pubkey_hex.lower(),
            "kid": kid,
            "capability": capability,
            "registered_at": int(time.time()),
        })
        if host is not None:
            rec["host"] = host
            rec["port"] = int(port)
        self.data["devices"][dev] = rec
        self.save()

    def get_device(self, dev: str) -> dict | None:
        return self.data["devices"].get(dev)

    def list_devices(self) -> dict:
        return dict(self.data["devices"])

    def list_bindings(self) -> list:
        return list(self.data["bindings"])

    def set_capability(self, dev: str, capability: str) -> None:
        rec = self.data["devices"].get(dev)
        if rec is None:
            raise RuntimeError("unknown device")
        rec["capability"] = capability
        self.save()

    # ---- bindings (user <-> device) --------------------------------------
    def add_binding(self, user: str, dev: str, nb: int, na: int,
                    replace: bool = False) -> None:
        """Add a binding; refuses to duplicate an active one unless replace=True.

        An "active" binding is one that is not revoked and whose end is still in
        the future relative to the new binding's start. Expired ones never block.
        """
        active = [b for b in self.data["bindings"]
                  if b["user"] == user and b["dev"] == dev
                  and not b["revoked"] and b["na"] >= nb]
        if active and not replace:
            until = max(b["na"] for b in active)
            raise ValueError("binding already exists until %d" % until)
        for b in active:
            b["revoked"] = True
        self.data["bindings"].append(
            {"user": user, "dev": dev, "nb": nb, "na": na, "revoked": False})
        self.save()

    def find_binding(self, user: str, dev: str, now_off: int) -> dict | None:
        for b in self.data["bindings"]:
            if (b["user"] == user and b["dev"] == dev and not b["revoked"]
                    and b["nb"] <= now_off <= b["na"]):
                return b
        return None

    def revoke_bindings(self, user: str, dev: str) -> None:
        for b in self.data["bindings"]:
            if b["user"] == user and b["dev"] == dev:
                b["revoked"] = True
        self.save()

    # ---- challenges ------------------------------------------------------
    def put_challenge(self, digest_hex: str, dev: str, user: str,
                      session: str, expires: int) -> None:
        self.data["challenges"][digest_hex] = {
            "dev": dev, "user": user, "session": session, "expires": expires}
        self.save()

    def count_open(self, dev: str, now_off: int) -> int:
        return sum(1 for c in self.data["challenges"].values()
                   if c["dev"] == dev and c["expires"] >= now_off)

    def take_challenge(self, digest_hex: str) -> dict | None:
        """Consume a challenge — it is single-use even if the proof fails."""
        entry = self.data["challenges"].pop(digest_hex, None)
        if entry is not None:
            self.save()
        return entry

    def expire_challenges(self, now_off: int) -> None:
        stale = [k for k, c in self.data["challenges"].items()
                 if c["expires"] < now_off]
        if stale:
            for k in stale:
                del self.data["challenges"][k]
            self.save()

    # ---- licenses --------------------------------------------------------
    def record_license(self, jti: str, lic: str, user: str, dev: str, na: int,
                       nb: int | None = None) -> None:
        if jti in self.data["licenses"]:
            raise RuntimeError("duplicate jti")
        self.data["licenses"][jti] = {
            "nb": nb,
            "lic": lic, "user": user, "dev": dev, "na": na, "revoked": False}
        self.save()

    def get_license(self, jti: str) -> dict | None:
        return self.data["licenses"].get(jti)

    def list_licenses(self, user: str | None = None) -> list[dict]:
        """Issued licenses, newest first, with their jti included (optional user filter)."""
        out = [dict(rec, jti=jti) for jti, rec in self.data["licenses"].items()
               if user is None or rec.get("user") == user]
        out.sort(key=lambda r: r.get("lic", ""), reverse=True)
        return out

    def list_bound_devices(self, user: str, now_off: int) -> list[str]:
        """Devices this user is allowed to use right now (active bindings only)."""
        seen: list[str] = []
        for b in self.data["bindings"]:
            if (b["user"] == user and not b["revoked"]
                    and b["nb"] <= now_off <= b["na"] and b["dev"] not in seen):
                seen.append(b["dev"])
        return seen

    def revoke_by_lic(self, lic: str) -> str:
        """Revoke by human-facing license number; returns the jti."""
        for jti, rec in self.data["licenses"].items():
            if rec.get("lic") == lic:
                if rec["revoked"]:
                    raise ValueError("license already revoked")
                rec["revoked"] = True
                self.save()
                return jti
        raise KeyError(lic)

    def revoke_license(self, jti: str) -> None:
        rec = self.data["licenses"].get(jti)
        if rec is None:
            raise RuntimeError("unknown jti")
        rec["revoked"] = True
        self.save()

    def revoke_device(self, dev: str) -> int:
        """Revoke every binding and every issued license for one device.

        The device itself cannot learn about this while offline (no clock, no
        push channel) — enforcement is server-side: no new license will be
        issued, so the cached one simply expires.
        @return number of records changed.
        """
        n = 0
        for b in self.data["bindings"]:
            if b["dev"] == dev and not b["revoked"]:
                b["revoked"] = True
                n += 1
        for rec in self.data["licenses"].values():
            if rec["dev"] == dev and not rec["revoked"]:
                rec["revoked"] = True
                n += 1
        if n:
            self.save()
        return n

    def is_revoked(self, jti: str) -> bool:
        rec = self.data["licenses"].get(jti)
        return bool(rec and rec["revoked"])


def _const_eq(a: str, b: str) -> bool:
    if len(a) != len(b):
        return False
    diff = 0
    for x, y in zip(a.encode(), b.encode()):
        diff |= x ^ y
    return diff == 0
