"""
Byte-stream demultiplexer: separates DSOW binary frames from text lines
so a mixed UART stream never corrupts either side.

Text lines are decoded UTF-8 with errors="replace" (encoding anomalies
never raise). Binary DSOW frames are recognised by their magic and
delivered as raw bytes — partial frame bytes are buffered until the
frame is complete, so a text line that happens to contain 'DSOW' is
still cleanly split.
"""

from __future__ import annotations

import json
import time
from dataclasses import dataclass, field

from ..frame import HEADER_SIZE, TRAILER_SIZE, MAGIC

DSOW_MAGIC = MAGIC  # b"DSOW"


@dataclass
class TextLine:
    ts_ms: int
    text: str


@dataclass
class BinaryFrame:
    ts_ms: int
    data: bytes


class StreamDemux:
    """Feed raw bytes; collect text lines and binary frames separately."""

    def __init__(self):
        self._buf = bytearray()
        self.text_lines: list[TextLine] = []
        self.binary_frames: list[BinaryFrame] = []
        self._clock = None

    def _now_ms(self) -> int:
        # monotonic-ish ms; allows deterministic tests via set_clock
        if self._clock is not None:
            return self._clock()
        return int(time.time() * 1000)

    def set_clock(self, fn) -> None:
        """Test hook: deterministic timestamp source."""
        self._clock = fn

    # ---- feeding ----------------------------------------------------------

    def feed(self, data: bytes) -> None:
        if not data:
            return
        self._buf.extend(data)
        self._drain()

    def _drain(self) -> None:
        while True:
            idx = self._buf.find(DSOW_MAGIC)
            if idx < 0:
                # no magic: emit only complete \r\n-terminated text lines
                self._emit_complete_text()
                return
            # a CRLF BEFORE the magic: emit that text line, continue
            nl = self._buf.find(b"\r\n", 0, idx)
            if nl >= 0:
                self._emit_text(bytes(self._buf[:nl]))
                del self._buf[:nl + 2]
                continue
            # a CRLF inside the would-be header ⇒ magic was text
            nl2 = self._buf.find(b"\r\n", idx, min(len(self._buf), idx + HEADER_SIZE))
            if nl2 >= 0:
                self._emit_text(bytes(self._buf[:nl2]))
                del self._buf[:nl2 + 2]
                continue
            # need the full header to validate
            if len(self._buf) < idx + HEADER_SIZE:
                return  # wait
            header = bytes(self._buf[idx:idx + HEADER_SIZE])
            version = header[4]
            ftype = header[5]
            payload_size = int.from_bytes(header[30:34], "little")
            valid = (version == 1 and ftype in (0, 1, 2)
                     and payload_size <= 1 << 22)  # sanity cap
            if not valid:
                # invalid header ⇒ treat magic as plain text
                self._emit_text(bytes(self._buf[:idx + len(DSOW_MAGIC)]))
                del self._buf[:idx + len(DSOW_MAGIC)]
                continue
            total = HEADER_SIZE + payload_size + TRAILER_SIZE
            if len(self._buf) < idx + total:
                return  # wait for full frame
            frame = bytes(self._buf[idx:idx + total])
            self.binary_frames.append(BinaryFrame(self._now_ms(), frame))
            del self._buf[:idx + total]

    def _emit_complete_text(self) -> None:
        end = 0
        while True:
            i = self._buf.find(b"\r\n", end)
            if i < 0:
                break
            line = bytes(self._buf[end:i])
            if line:
                self.text_lines.append(
                    TextLine(self._now_ms(),
                             line.decode("utf-8", errors="replace")))
            end = i + 2
        del self._buf[:end]

    def _emit_text(self, raw: bytes) -> None:
        if not raw:
            return
        text = raw.decode("utf-8", errors="replace")
        for line in text.split("\r"):
            if line:
                self.text_lines.append(TextLine(self._now_ms(), line.strip("\n")))

    # ---- accessors ----------------------------------------------------------

    def pop_text(self) -> list[TextLine]:
        out = self.text_lines
        self.text_lines = []
        return out

    def pop_frames(self) -> list[BinaryFrame]:
        out = self.binary_frames
        self.binary_frames = []
        return out

    def pending_bytes(self) -> int:
        return len(self._buf)


# ---------------------------------------------------------------------------
# Reset reason parsing
# ---------------------------------------------------------------------------

RESET_REASONS = (
    "POWER_ON",        # clean boot from power-up / reset button
    "HARD_FAULT",      # HardFault_Handler entered
    "ERROR_HANDLER",   # Error_Handler(__FILE__, __LINE__) called
    "WATCHDOG",        # watchdog-triggered reboot (if log hints)
    "UNKNOWN",
)

_RESET_PATTERNS = (
    # (reason, pattern, confidence)
    ("HARD_FAULT", "Enter HardFault_Handler", 1.0),
    ("HARD_FAULT", "HardFault_Handler, System Halt", 1.0),
    ("ERROR_HANDLER", "[ERR] Error_Handler", 0.95),
    ("ERROR_HANDLER", "Error_Handler file=", 0.9),
    ("WATCHDOG", "watchdog", 0.7),
    ("POWER_ON", "[BOOT] UART1 direct OK", 0.8),
    ("POWER_ON", "[BOOT] SDRAM OK", 0.7),
    ("POWER_ON", "LwIP init", 0.6),
)


def parse_reset_reason(lines: list[str]) -> dict:
    """
    Infer the reset reason from boot-log lines.

    Returns {'reason': str, 'confidence': float, 'matched': str|None}.
    The strongest matching pattern wins; POWER_ON is only reported when
    a clean-boot marker is seen and no fault pattern precedes it.
    """
    best = {"reason": "UNKNOWN", "confidence": 0.0, "matched": None}
    for line in lines:
        low = line.lower()
        for reason, pat, conf in _RESET_PATTERNS:
            if pat.lower() in low and conf > best["confidence"]:
                best = {"reason": reason, "confidence": conf, "matched": pat}
    return best


# ---------------------------------------------------------------------------
# Log writer (JSONL)
# ---------------------------------------------------------------------------

class SerialLogger:
    """Appends timestamped text/binary events to a JSONL file."""

    def __init__(self, path: str, open_mode: str = "a"):
        self.path = path
        self._fp = open(path, open_mode, encoding="utf-8", buffering=1)
        self._count = 0

    def log_text(self, text: str, ts_ms: int | None = None) -> None:
        rec = {"t": ts_ms if ts_ms is not None else int(time.time() * 1000),
               "k": "text", "d": text}
        self._fp.write(json.dumps(rec, ensure_ascii=False) + "\n")
        self._count += 1

    def log_binary(self, data: bytes, ts_ms: int | None = None) -> None:
        rec = {"t": ts_ms if ts_ms is not None else int(time.time() * 1000),
               "k": "bin", "len": len(data), "d": data.hex()}
        self._fp.write(json.dumps(rec) + "\n")
        self._count += 1

    def close(self) -> None:
        if self._fp:
            self._fp.close()
            self._fp = None

    @property
    def count(self) -> int:
        return self._count
