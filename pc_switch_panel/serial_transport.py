"""CH340 serial transport for the switch-panel GUI.

Mirrors SwitchTransport's interface (send_cmd / next_line / query_state /
close / is_open) over pyserial, so main.py can switch between TCP and serial
with the same polling code. The DSO debug UART1 is 1152000 baud; its TX mixes
debug logs with our JSON ack replies, so query_state() skips every line that is
not a get_switch ack.
"""

from __future__ import annotations

import json
import queue
import threading
import time

import serial
import serial.tools.list_ports as list_ports

CH340_VID = 0x1A86
CH340_PID = 0x7523
CH340_DESC_MARKERS = ("CH340", "USB-SERIAL", "USB Serial", "WCH")
DEFAULT_BAUD = 1152000
CRLF = b"\r\n"


def discover_ch340() -> list[dict]:
    """Return [{port, description}] for attached CH340/CH34x adapters."""
    out = []
    for p in list_ports.comports():
        is_ch340 = False
        if p.vid is not None and p.pid is not None:
            is_ch340 = (p.vid == CH340_VID and p.pid in (CH340_PID, 0x55D4))
        if not is_ch340 and p.description:
            is_ch340 = any(m.lower() in p.description.lower()
                           for m in CH340_DESC_MARKERS)
        if is_ch340:
            out.append({"port": p.device, "description": p.description})
    return out


class SerialTransport:
    """pyserial wrapper with a background line-splitting reader thread."""

    def __init__(self) -> None:
        self._ser: serial.Serial | None = None
        self._rx: queue.Queue = queue.Queue(maxsize=256)
        self._stop = threading.Event()
        self._reader: threading.Thread | None = None

    # ---- lifecycle ----------------------------------------------------------

    @property
    def is_open(self) -> bool:
        return self._ser is not None and self._ser.is_open

    def connect(self, port: str, baud: int = DEFAULT_BAUD) -> bool:
        self.close()
        try:
            ser = serial.Serial(port=port, baudrate=baud, timeout=0.1,
                                write_timeout=1.0)
        except (serial.SerialException, OSError, ValueError):
            return False
        self._ser = ser
        self._stop.clear()
        self._reader = threading.Thread(target=self._read_loop, daemon=True,
                                        name="ser-reader")
        self._reader.start()
        return True

    def close(self) -> None:
        self._stop.set()
        if self._reader is not None:
            self._reader.join(timeout=1.0)
            self._reader = None
        if self._ser is not None:
            try:
                self._ser.close()
            except Exception:  # noqa: BLE001
                pass
            self._ser = None

    # ---- reader thread --------------------------------------------------------

    def _read_loop(self) -> None:
        assert self._ser is not None
        buf = bytearray()
        while not self._stop.is_set():
            try:
                data = self._ser.read(1024)
            except (serial.SerialException, OSError):
                break
            if not data:
                continue
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
                    self._put(line.decode("utf-8", errors="replace"))
        self._put(None)  # disconnect sentinel

    def _put(self, line: str | None) -> None:
        try:
            self._rx.put(line)
        except queue.Full:
            pass

    # ---- sending --------------------------------------------------------------

    def send_cmd(self, cmd: str, params: dict | None = None, seq: int = 0) -> int:
        if not self.is_open:
            return 0
        payload = json.dumps({"cmd": cmd, "params": params or {}, "seq": seq})
        try:
            return self._ser.write(payload.encode("utf-8") + CRLF)
        except (serial.SerialException, OSError):
            return 0

    # ---- receiving ------------------------------------------------------------

    def next_line(self, timeout: float = 0.5) -> str | None:
        try:
            item = self._rx.get(timeout=timeout)
        except queue.Empty:
            return None
        return item  # None sentinel on disconnect

    def query_state(self, timeout: float = 1.0,
                    on_line=None) -> tuple[int, int, int | None, int] | None:
        """Send get_switch; return (sw, led, temp_dezi, temp_ok). None on no reply.

        ``temp_dezi`` is the SHT40 temperature in tenths of a degree (245 = 24.5°C),
        or None when the device/firmware omits it. Every line read is passed to
        ``on_line(line)`` if provided (raw log view).
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
                    raw = data.get("temp")
                    temp_dezi = int(raw) if raw is not None else None
                    temp_ok = int(data.get("temp_ok", 0))
                except (TypeError, ValueError):
                    continue
                return sw, led, temp_dezi, temp_ok
            # not our ack (debug log, set_sw ack) — keep reading
