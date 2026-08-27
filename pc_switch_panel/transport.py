"""TCP transport for the switch-panel GUI.

Sends JSON control commands and receives CRLF-delimited reply lines, aligned
with dsosim protocol v0.9. A background reader thread splits the socket stream
on ``\\n`` and queues decoded lines — works against both the real DSO (port
5005) and the ``python -m dsosim.server`` simulator.
"""

from __future__ import annotations

import json
import queue
import socket
import threading
import time

CRLF = b"\r\n"


class SwitchTransport:
    """Thin socket wrapper: send_command + line reader thread + get_switch poll."""

    def __init__(self) -> None:
        self._sock: socket.socket | None = None
        self._rx: queue.Queue = queue.Queue(maxsize=256)
        self._stop = threading.Event()
        self._reader: threading.Thread | None = None

    # ---- lifecycle ----------------------------------------------------------

    @property
    def is_open(self) -> bool:
        return self._sock is not None

    def connect(self, host: str, port: int, timeout: float = 3.0) -> bool:
        self.close()
        try:
            sock = socket.create_connection((host, port), timeout=timeout)
        except OSError:
            return False
        sock.settimeout(None)  # reader thread blocks; close() unblocks it
        self._sock = sock
        self._stop.clear()
        self._reader = threading.Thread(target=self._read_loop, daemon=True,
                                        name="sw-reader")
        self._reader.start()
        return True

    def close(self) -> None:
        self._stop.set()
        sock = self._sock
        self._sock = None
        if sock is not None:
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            try:
                sock.close()
            except OSError:
                pass
        if self._reader is not None:
            self._reader.join(timeout=1.0)
            self._reader = None

    # ---- reader thread -------------------------------------------------------

    def _read_loop(self) -> None:
        assert self._sock is not None
        buf = bytearray()
        while not self._stop.is_set():
            try:
                data = self._sock.recv(4096)
            except OSError:
                break
            if not data:
                break
            buf.extend(data)
            while True:
                idx = buf.find(b"\n")
                if idx < 0:
                    break
                line = bytes(buf[:idx])
                del buf[:idx + 1]
                if line.endswith(b"\r"):
                    line = line[:-1]
                if line:
                    self._rx_put(line.decode("utf-8", errors="replace"))
        self._rx_put(None)  # disconnect sentinel

    def _rx_put(self, line: str | None) -> None:
        try:
            self._rx.put(line)
        except queue.Full:
            pass

    # ---- sending --------------------------------------------------------------

    def send_cmd(self, cmd: str, params: dict | None = None, seq: int = 0) -> int:
        """Send a JSON control command; returns bytes written (0 if not open)."""
        if self._sock is None:
            return 0
        payload = json.dumps({"cmd": cmd, "params": params or {}, "seq": seq})
        try:
            self._sock.sendall(payload.encode("utf-8") + CRLF)
        except OSError:
            return 0
        return len(payload) + 2

    # ---- receiving ------------------------------------------------------------

    def next_line(self, timeout: float = 0.5) -> str | None:
        """Next decoded text line from the device, or None on timeout/disconnect."""
        try:
            item = self._rx.get(timeout=timeout)
        except queue.Empty:
            return None
        if item is None:  # disconnect sentinel
            return None
        return item

    def query_state(self, timeout: float = 1.0,
                    on_line=None) -> tuple[int, int] | None:
        """Send get_switch and parse the ack's data → (sw, led). None on no reply.

        Skips any interleaved lines that are not the get_switch ack (e.g. stale
        set_sw acks), so the caller can poll freely. Every line read is passed to
        ``on_line(line)`` if provided (for a raw serial log view).
        """
        self.send_cmd("get_switch")
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return None
            line = self.next_line(timeout=max(0.01, remaining))
            if line is None:
                return None
            if on_line is not None:
                on_line(line)
            try:
                obj = json.loads(line)
            except json.JSONDecodeError:
                continue
            if (obj.get("type") == "ack"
                    and obj.get("cmd") == "get_switch"
                    and obj.get("result") == "ok"):
                data = obj.get("data") or {}
                try:
                    sw = int(data.get("sw", 0))
                    led = int(data.get("led", 0))
                except (TypeError, ValueError):
                    continue
                return sw, led
            # not our ack — keep reading
