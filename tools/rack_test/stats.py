"""Thread-safe in-memory counters for the receiver and the scenario checks."""

from __future__ import annotations

import threading
import time
from collections import deque
from typing import Any, Deque, Dict, Optional

from .config import MODULES, WATCHDOG, RackConfig


def _wall_ms() -> int:
    return int(time.time() * 1000)


class RunStats:
    def __init__(self, config: RackConfig) -> None:
        self.config = config
        self._lock = threading.Lock()
        self.started_mono = time.monotonic()
        self.started_wall_ms = _wall_ms()

        self.modules: Dict[str, dict] = {
            name: {
                "count": 0,
                "events": 0,
                "bad_frames": 0,
                "first_wall_ms": None,
                "last_wall_ms": None,
                "last_ts_ms": None,
                "max_gap_ms": 0,
                "fields": {},
                "last_data": {},
            }
            for name in MODULES
        }
        self.watchdog_frames = 0
        self.bad_frames = 0
        self.invalid_lines = 0
        self.legacy_lines = 0
        self.oversize_lines = 0
        self.partial_lines = 0

        self.accepted = 0
        self.active = 0
        self.max_concurrent = 0
        self.rejected = 0
        self.closed = 0
        self.rst_closes = 0
        self.fin_closes = 0
        self.a2_accepts = 0
        self.a2_rst = 0
        self.telemetry_connections = 0
        self.legacy_connections = 0
        self.invalid_connections = 0
        self.reconnects = 0

        self.events: Dict[str, Dict[str, int]] = {}

        # rolling frame timestamps for a live frame rate
        self._frame_times: Deque[float] = deque(maxlen=4096)

        # storm window
        self._storm_last_accepts = 0
        self._storm_last_mono = time.monotonic()

    # ------------------------------------------------------------------ feed
    def observe_frame(self, frame: dict, wall_ms: int, peer: str) -> None:
        module = frame.get("module")
        wall_s = wall_ms / 1000.0
        with self._lock:
            if module == WATCHDOG:
                self.watchdog_frames += 1
                self._frame_times.append(wall_s)
                return
            slot = self.modules.get(module)
            if slot is None:
                return
            self._frame_times.append(wall_s)
            if slot["last_wall_ms"] is not None:
                gap = wall_ms - int(slot["last_wall_ms"])
                if gap > slot["max_gap_ms"]:
                    slot["max_gap_ms"] = gap
            else:
                slot["first_wall_ms"] = wall_ms
            slot["count"] += 1
            slot["last_wall_ms"] = wall_ms
            slot["last_ts_ms"] = frame.get("ts_ms")
            data = frame.get("data")
            if isinstance(data, dict):
                slot["last_data"] = dict(data)
                fields = slot["fields"]
                for key, value in data.items():
                    if isinstance(value, bool):
                        value = int(value)
                    if not isinstance(value, int):
                        continue
                    agg = fields.get(key)
                    if agg is None:
                        fields[key] = {
                            "min": value, "max": value, "sum": value,
                            "count": 1, "last": value,
                        }
                    else:
                        if value < agg["min"]:
                            agg["min"] = value
                        if value > agg["max"]:
                            agg["max"] = value
                        agg["sum"] += value
                        agg["count"] += 1
                        agg["last"] = value

    def observe_event(self, frame: dict, wall_ms: int) -> None:
        module = frame.get("module")
        data = frame.get("data") or {}
        name = str(data.get("event", "unknown"))
        with self._lock:
            self._frame_times.append(wall_ms / 1000.0)
            slot = self.modules.get(module)
            if slot is not None:
                slot["events"] += 1
            per_module = self.events.setdefault(module, {})
            per_module[name] = per_module.get(name, 0) + 1

    def note_bad_frame(self, module: Optional[str], error: str) -> None:
        with self._lock:
            self.bad_frames += 1
            if module and module in self.modules:
                self.modules[module]["bad_frames"] += 1

    # ---------------------------------------------------------- connections
    def note_accept(self, peer: str) -> None:
        with self._lock:
            self.accepted += 1
            self.active += 1
            if self.active > self.max_concurrent:
                self.max_concurrent = self.active

    def note_classified(self, channel: str) -> None:
        with self._lock:
            if channel == "telemetry":
                self.telemetry_connections += 1
            elif channel == "legacy":
                self.legacy_connections += 1
            else:
                self.invalid_connections += 1

    def note_close(self, channel: str, rst: bool, had_bytes: bool) -> None:
        with self._lock:
            self.closed += 1
            if self.active > 0:
                self.active -= 1
            if rst:
                self.rst_closes += 1
            else:
                self.fin_closes += 1
            if channel in ("legacy", "unknown") and not had_bytes:
                # The legacy (FMC telemetry) client connects and closes without
                # sending a line; it is never classified by content.
                self.a2_accepts += 1
                if rst:
                    self.a2_rst += 1

    def note_rejected(self) -> None:
        with self._lock:
            self.rejected += 1

    # --------------------------------------------------------------- errors
    def note_invalid(self) -> None:
        with self._lock:
            self.invalid_lines += 1

    def note_legacy(self) -> None:
        with self._lock:
            self.legacy_lines += 1

    def note_oversize(self) -> None:
        with self._lock:
            self.oversize_lines += 1

    def note_partial(self) -> None:
        with self._lock:
            self.partial_lines += 1

    # ------------------------------------------------------------- snapshot
    def frame_rate(self) -> float:
        times = list(self._frame_times)
        if len(times) < 2:
            return 0.0
        span = times[-1] - times[0]
        if span <= 0:
            return 0.0
        return (len(times) - 1) / span

    def snapshot(self) -> dict:
        with self._lock:
            modules = {}
            for name, slot in self.modules.items():
                modules[name] = {
                    "count": slot["count"],
                    "events": slot["events"],
                    "bad_frames": slot["bad_frames"],
                    "first_wall_ms": slot["first_wall_ms"],
                    "last_wall_ms": slot["last_wall_ms"],
                    "last_ts_ms": slot["last_ts_ms"],
                    "max_gap_ms": slot["max_gap_ms"],
                    "fields": {k: dict(v) for k, v in slot["fields"].items()},
                    "last_data": dict(slot["last_data"]),
                }
            return {
                "schema": "dso.rack_test.stats/1",
                "started_wall_ms": self.started_wall_ms,
                "uptime_s": round(time.monotonic() - self.started_mono, 3),
                "frame_rate": round(self.frame_rate(), 3),
                "watchdog_frames": self.watchdog_frames,
                "bad_frames": self.bad_frames,
                "invalid_lines": self.invalid_lines,
                "legacy_lines": self.legacy_lines,
                "oversize_lines": self.oversize_lines,
                "partial_lines": self.partial_lines,
                "connections": {
                    "accepted": self.accepted,
                    "active": self.active,
                    "max_concurrent": self.max_concurrent,
                    "rejected": self.rejected,
                    "closed": self.closed,
                    "rst_closes": self.rst_closes,
                    "fin_closes": self.fin_closes,
                    "telemetry": self.telemetry_connections,
                    "legacy": self.legacy_connections,
                    "invalid": self.invalid_connections,
                    "reconnects": max(0, self.telemetry_connections - 1),
                    "a2_accepts": self.a2_accepts,
                    "a2_rst": self.a2_rst,
                },
                "events": {m: dict(v) for m, v in self.events.items()},
                "modules": modules,
            }

    def storm_line(self) -> Optional[str]:
        """Return one aggregate line per storm window, or None if quiet."""
        now = time.monotonic()
        with self._lock:
            elapsed = now - self._storm_last_mono
            if elapsed < self.config.storm_interval_s:
                return None
            accepts = self.a2_accepts
            delta = accepts - self._storm_last_accepts
            self._storm_last_accepts = accepts
            self._storm_last_mono = now
            if delta <= 0:
                return None
            rate = delta / elapsed
            return ("[rack] a2_window=%.0fs accepts=%d rate=%.1f/s rst=%d"
                    % (elapsed, delta, rate, self.a2_rst))

    def summary_line(self, integrity: Optional[dict] = None) -> str:
        snap = self.snapshot()
        parts = []
        for name in MODULES:
            parts.append("%s:%d" % (name, snap["modules"][name]["count"]))
        integrity = integrity or {}
        return ("[rack] up=%ds frames=%.1f/s a2=%d(%.1f/s) active=%d "
                "seq_gap=%d reboot=%d bad=%d mods=%s"
                % (int(snap["uptime_s"]), snap["frame_rate"],
                   snap["connections"]["a2_accepts"],
                   snap["connections"]["a2_accepts"] / max(1.0, snap["uptime_s"]),
                   snap["connections"]["active"],
                   int(integrity.get("seq_gaps", 0)),
                   int(integrity.get("reboots", 0)),
                   snap["bad_frames"],
                   ",".join(parts)))
