"""Append-only audit log (one JSON object per line).

Deliberately independent from `Store`:
  * its own file and lock, so audit I/O never serializes authorization;
  * append-only, one complete line per `os.write`-equivalent call;
  * never receives passwords, tokens, keys or raw request bodies.

Event names are stable; see Markdown/设备授权协议_v0.md §12.
"""

from __future__ import annotations

import json
import sys
import threading
import time
from pathlib import Path

MAX_FIELD = 512
MAX_TAIL = 500


def _clean(value):
    if isinstance(value, str):
        # strip control characters and bound the size
        value = "".join(ch for ch in value if ch >= " " or ch == "\t")
        return value[:MAX_FIELD]
    return value


class AuditLog:
    def __init__(self, path: str | Path):
        self.path = Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self._lock = threading.Lock()

    def write(self, event: str, **fields) -> None:
        record = {"ts": int(time.time()), "event": event}
        for key, value in fields.items():
            if value is None:
                continue
            record[key] = _clean(value)
        line = json.dumps(record, ensure_ascii=False, separators=(",", ":")) + "\n"
        try:
            with self._lock:
                with open(self.path, "a", encoding="utf-8", buffering=1) as fh:
                    fh.write(line)
        except OSError as exc:               # never let audit break the request
            print("[audit] write failed: %s" % exc, file=sys.stderr)

    def tail(self, limit: int = 100) -> list[dict]:
        try:
            limit = max(0, min(int(limit), MAX_TAIL))
        except (TypeError, ValueError):
            limit = 100
        if limit == 0:
            return []
        try:
            with open(self.path, "r", encoding="utf-8", errors="replace") as fh:
                lines = fh.readlines()[-limit:]
        except OSError:
            return []
        out: list[dict] = []
        for line in lines:
            line = line.strip()
            if not line:
                continue
            try:
                out.append(json.loads(line))
            except ValueError:
                continue
        return out
