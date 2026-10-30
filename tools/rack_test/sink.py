"""Append-only JSONL sinks with size rotation, and atomic JSON snapshots."""

from __future__ import annotations

import json
import os
import threading
from pathlib import Path
from typing import Any, Optional


class RotatingJsonl:
    """One JSON object per line; rotate to ``.1``, ``.2`` ... when full."""

    def __init__(self, path: Path, max_bytes: int = 32 * 1024 * 1024,
                 backups: int = 3) -> None:
        self.path = Path(path)
        self.max_bytes = max(1, int(max_bytes))
        self.backups = max(0, int(backups))
        self._lock = threading.Lock()
        self._fh = None
        self._size = 0
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self._open()

    def _open(self) -> None:
        if self.path.exists():
            self._size = self.path.stat().st_size
        else:
            self._size = 0
        self._fh = open(self.path, "a", encoding="utf-8", newline="\n")

    def _rotate(self) -> None:
        assert self._fh is not None
        self._fh.close()
        self._fh = None
        if self.backups <= 0:
            try:
                self.path.unlink()
            except OSError:
                pass
        else:
            oldest = self.path.with_name(self.path.name + ".%d" % self.backups)
            try:
                oldest.unlink()
            except OSError:
                pass
            for index in range(self.backups - 1, 0, -1):
                src = self.path.with_name(self.path.name + ".%d" % index)
                dst = self.path.with_name(self.path.name + ".%d" % (index + 1))
                if src.exists():
                    try:
                        os.replace(str(src), str(dst))
                    except OSError:
                        pass
            try:
                os.replace(str(self.path), str(self.path) + ".1")
            except OSError:
                pass
        self._size = 0
        self._open()

    def write(self, record: dict) -> None:
        line = json.dumps(record, ensure_ascii=False, separators=(",", ":"))
        payload = line + "\n"
        with self._lock:
            if self._fh is None:
                return
            if self._size > 0 and self._size + len(payload) > self.max_bytes:
                self._rotate()
            assert self._fh is not None
            self._fh.write(payload)
            self._fh.flush()
            self._size += len(payload)

    def close(self) -> None:
        with self._lock:
            if self._fh is not None:
                self._fh.close()
                self._fh = None

    @property
    def size(self) -> int:
        return self._size


def write_atomic_json(path: Path, obj: Any) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + ".tmp")
    with open(tmp, "w", encoding="utf-8") as fh:
        json.dump(obj, fh, ensure_ascii=False, indent=1)
        fh.write("\n")
    os.replace(str(tmp), str(path))


def read_json(path: Path, default: Optional[Any] = None) -> Any:
    try:
        with open(path, "r", encoding="utf-8") as fh:
            return json.load(fh)
    except (OSError, ValueError):
        return default
