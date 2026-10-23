"""Append-only audit log (one JSON object per line).

Deliberately independent from `Store`:
  * its own file and lock, so audit I/O never serializes authorization;
  * append-only, one complete line per `os.write`-equivalent call;
  * never receives passwords, tokens, keys or raw request bodies.

Event names are stable; see Markdown/设备授权协议_v0.md §12.
"""

from __future__ import annotations

import json
import os
import sys
import threading
import time
from pathlib import Path

MAX_FIELD = 512
MAX_TAIL = 500
DEFAULT_MAX_BYTES = 2_000_000      # rotate to <name>.1 above this size


def _clean(value):
    if isinstance(value, str):
        # strip control characters and bound the size
        value = "".join(ch for ch in value if ch >= " " or ch == "\t")
        return value[:MAX_FIELD]
    return value


class AuditLog:
    def __init__(self, path: str | Path, max_bytes: int = DEFAULT_MAX_BYTES):
        self.path = Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.max_bytes = max_bytes
        self._lock = threading.Lock()

    def _rotate_if_needed(self) -> None:
        """Keep one previous generation: audit.jsonl -> audit.jsonl.1."""
        if not self.max_bytes:
            return
        try:
            if self.path.stat().st_size < self.max_bytes:
                return
        except OSError:
            return
        backup = self.path.with_name(self.path.name + ".1")
        try:
            if backup.exists():
                os.unlink(backup)
            os.replace(self.path, backup)
        except OSError as exc:
            print("[audit] rotate failed: %s" % exc, file=sys.stderr)

    def write(self, event: str, **fields) -> None:
        record = {"ts": int(time.time()), "event": event}
        for key, value in fields.items():
            if value is None:
                continue
            record[key] = _clean(value)
        line = json.dumps(record, ensure_ascii=False, separators=(",", ":")) + "\n"
        try:
            with self._lock:
                self._rotate_if_needed()
                with open(self.path, "a", encoding="utf-8", buffering=1) as fh:
                    fh.write(line)
        except OSError as exc:               # never let audit break the request
            print("[audit] write failed: %s" % exc, file=sys.stderr)

    def tail(self, limit: int = 100, event: str | None = None,
             device: str | None = None) -> list[dict]:
        """Newest-first scan, then chronological order; optional server-side filters."""
        try:
            limit = max(0, min(int(limit), MAX_TAIL))
        except (TypeError, ValueError):
            limit = 100
        if limit == 0:
            return []
        try:
            with open(self.path, "r", encoding="utf-8", errors="replace") as fh:
                lines = fh.readlines()
        except OSError:
            return []
        out: list[dict] = []
        for line in reversed(lines):
            line = line.strip()
            if not line:
                continue
            try:
                rec = json.loads(line)
            except ValueError:
                continue
            if event and rec.get("event") != event:
                continue
            if device and rec.get("device") != device:
                continue
            out.append(rec)
            if len(out) >= limit:
                break
        out.reverse()
        return out
