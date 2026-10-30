"""seq / ts_ms continuity, 32-bit wrap and reboot (epoch) detection.

The device increments one shared ``seq`` for every frame, including the
watchdog frame, so *all* envelope-valid frames must be fed here even though
the watchdog module itself is out of test scope.
"""

from __future__ import annotations

import threading
from typing import Any, Dict, List

SEQ_MOD = 1 << 32
TS_MOD = 1 << 32
_WRAP_MARGIN = 1 << 16


class IntegrityTracker:
    def __init__(self) -> None:
        self._lock = threading.Lock()
        self.last_seq = None
        self.last_ts = None
        self.epoch = 0
        self.frames = 0
        self.seq_gaps = 0
        self.max_seq_gap = 0
        self.duplicates = 0
        self.seq_resets = 0
        self.ts_resets = 0
        self.reboots = 0
        self.ts_wraps = 0
        self.seq_wraps = 0
        self.gaps: List[dict] = []
        self.reboot_events: List[dict] = []

    # ------------------------------------------------------------------ feed
    def feed(self, frame: dict, wall_ms: int) -> List[dict]:
        """Feed one envelope-valid frame; return the events it produced."""
        seq = frame.get("seq")
        ts = frame.get("ts_ms")
        events: List[dict] = []
        if not isinstance(seq, int) or not isinstance(ts, int):
            return events
        seq &= SEQ_MOD - 1
        ts &= TS_MOD - 1
        with self._lock:
            self.frames += 1
            if self.last_seq is None:
                self.last_seq = seq
                self.last_ts = ts
                return events

            prev_seq = int(self.last_seq)
            prev_ts = int(self.last_ts)

            seq_wrapped = prev_seq > SEQ_MOD - _WRAP_MARGIN and seq < _WRAP_MARGIN
            ts_wrapped = prev_ts > TS_MOD - _WRAP_MARGIN and ts < _WRAP_MARGIN
            if seq_wrapped:
                self.seq_wraps += 1
                events.append({"kind": "seq_wrap", "wall_ms": wall_ms,
                               "prev": prev_seq, "now": seq})
            if ts_wrapped:
                self.ts_wraps += 1
                events.append({"kind": "ts_wrap", "wall_ms": wall_ms,
                               "prev": prev_ts, "now": ts})

            reboot = False
            if not ts_wrapped and ts + 1000 < prev_ts:
                reboot = True
            elif not seq_wrapped and seq < prev_seq and (
                    prev_seq - seq > 1000 or seq <= 2):
                # s_seq resets to 0 on boot; a tiny backwards step without a
                # ts reset is treated as a duplicate/out-of-order frame.
                if ts <= prev_ts:
                    reboot = True

            if reboot:
                self.epoch += 1
                self.reboots += 1
                self.ts_resets += 1
                self.seq_resets += 1
                event = {"kind": "reboot", "wall_ms": wall_ms,
                         "epoch": self.epoch, "prev_seq": prev_seq,
                         "seq": seq, "prev_ts_ms": prev_ts, "ts_ms": ts}
                self.reboot_events.append(event)
                if len(self.reboot_events) > 100:
                    del self.reboot_events[:-100]
                events.append(event)
            else:
                if not seq_wrapped:
                    if seq == prev_seq:
                        self.duplicates += 1
                        events.append({"kind": "duplicate", "wall_ms": wall_ms,
                                       "seq": seq})
                    else:
                        expected = (prev_seq + 1) % SEQ_MOD
                        if seq != expected:
                            delta = (seq - expected) % SEQ_MOD
                            self.seq_gaps += 1
                            if delta > self.max_seq_gap:
                                self.max_seq_gap = delta
                            gap = {"kind": "seq_gap", "wall_ms": wall_ms,
                                   "expected": expected, "got": seq,
                                   "delta": delta}
                            self.gaps.append(gap)
                            if len(self.gaps) > 200:
                                del self.gaps[:-200]
                            events.append(gap)
                if ts < prev_ts and not ts_wrapped:
                    # Small ts regression that did not look like a reboot.
                    self.ts_resets += 1
                    events.append({"kind": "ts_reset", "wall_ms": wall_ms,
                                   "prev": prev_ts, "now": ts})

            self.last_seq = seq
            self.last_ts = ts
            return events

    # -------------------------------------------------------------- snapshot
    def snapshot(self) -> Dict[str, Any]:
        with self._lock:
            return {
                "frames": self.frames,
                "epoch": self.epoch,
                "last_seq": self.last_seq,
                "last_ts_ms": self.last_ts,
                "seq_gaps": self.seq_gaps,
                "max_seq_gap": self.max_seq_gap,
                "duplicates": self.duplicates,
                "seq_resets": self.seq_resets,
                "ts_resets": self.ts_resets,
                "reboots": self.reboots,
                "ts_wraps": self.ts_wraps,
                "seq_wraps": self.seq_wraps,
                "gaps": [dict(g) for g in self.gaps[-50:]],
                "reboot_events": [dict(e) for e in self.reboot_events[-20:]],
            }
