"""
CH340 serial adapter — PC-side bridge to the DSO debug UART.

Features:
  - discovery (VID/PID 0x1A86:0x7523 + description match)
  - configurable baud rate (DSO UART1 default 1152000)
  - background reader thread with text/binary demux
  - continuous RX/TX with a write mutex
  - boot-log capture + reset-reason inference
  - plug/unplug monitoring
  - JSONL log saving (text + binary kept separate)
"""

from __future__ import annotations

import queue
import threading
import time

import serial
import serial.tools.list_ports as list_ports

from .demux import StreamDemux, TextLine, BinaryFrame, parse_reset_reason, SerialLogger

CH340_VID = 0x1A86
CH340_PID = 0x7523
CH340_DESC_MARKERS = ("CH340", "USB-SERIAL", "USB Serial", "WCH")

DEFAULT_BAUD = 1152000  # DSO UART1 debug baud


def discover_ch340() -> list[dict]:
    """Return [{port, vid, pid, description}] for attached CH340 adapters."""
    out = []
    for p in list_ports.comports():
        is_ch340 = False
        if p.vid is not None and p.pid is not None:
            is_ch340 = (p.vid == CH340_VID and p.pid in (CH340_PID, 0x55D4))
        if not is_ch340 and p.description:
            is_ch340 = any(m.lower() in p.description.lower()
                           for m in CH340_DESC_MARKERS)
        if is_ch340:
            out.append({"port": p.device, "vid": p.vid, "pid": p.pid,
                        "description": p.description})
    return out


class CH340Adapter:
    """Thin, safe wrapper around a CH340 serial port."""

    def __init__(self, port: str, baudrate: int = DEFAULT_BAUD,
                 read_timeout: float = 0.1, write_timeout: float = 1.0):
        self.port = port
        self.baudrate = baudrate
        self._read_timeout = read_timeout
        self._write_timeout = write_timeout
        self._ser: serial.Serial | None = None
        self._demux = StreamDemux()
        self._rx_queue: queue.Queue = queue.Queue(maxsize=4096)
        self._write_lock = threading.Lock()
        self._reader: threading.Thread | None = None
        self._stop = threading.Event()
        self._logger: SerialLogger | None = None
        self._plug_monitor: threading.Thread | None = None
        self._on_unplug = None
        self._last_unplug_warned = False

    # ---- lifecycle ----------------------------------------------------------

    def open(self) -> bool:
        if self._ser is not None:
            return True
        if "://" in self.port:  # URL schemes (loop://, hwgrep://, ...)
            self._ser = serial.serial_for_url(
                self.port, baudrate=self.baudrate,
                timeout=self._read_timeout, write_timeout=self._write_timeout)
        else:
            self._ser = serial.Serial(port=self.port, baudrate=self.baudrate,
                                      timeout=self._read_timeout,
                                      write_timeout=self._write_timeout)
        self._stop.clear()
        self._reader = threading.Thread(target=self._reader_loop,
                                        name="ch340-reader", daemon=True)
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
        if self._logger is not None:
            self._logger.close()
            self._logger = None

    @property
    def is_open(self) -> bool:
        return self._ser is not None and self._ser.is_open

    # ---- background reader ---------------------------------------------------

    def _reader_loop(self) -> None:
        assert self._ser is not None
        while not self._stop.is_set():
            try:
                data = self._ser.read(1024)
            except (serial.SerialException, OSError):
                self._on_serial_error()
                return
            if not data:
                continue
            self._demux.feed(data)
            for line in self._demux.pop_text():
                if self._logger is not None:
                    self._logger.log_text(line.text, line.ts_ms)
                self._rx_queue.put(("text", line))
            for frame in self._demux.pop_frames():
                if self._logger is not None:
                    self._logger.log_binary(frame.data, frame.ts_ms)
                self._rx_queue.put(("bin", frame))

    def _on_serial_error(self) -> None:
        if not self._last_unplug_warned:
            self._rx_queue.put(("event", "unplug"))
            self._last_unplug_warned = True

    # ---- receiving -----------------------------------------------------------

    def read_line(self, timeout: float = 1.0) -> TextLine | None:
        """Block until a text line arrives or timeout. None on timeout."""
        return self._read_kind("text", timeout)

    def read_binary_frame(self, timeout: float = 5.0) -> BinaryFrame | None:
        """Block until a DSOW binary frame arrives or timeout."""
        return self._read_kind("bin", timeout)

    def _read_kind(self, kind: str, timeout: float):
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return None
            try:
                item = self._rx_queue.get(timeout=remaining)
            except queue.Empty:
                return None
            if item[0] == kind:
                return item[1]
            # other kind or event: keep waiting for the requested kind

    # ---- sending ---------------------------------------------------------------

    def write_text(self, line: str, newline: bool = True) -> int:
        payload = line.encode("utf-8", errors="replace")
        if newline:
            payload += b"\r\n"
        return self.write_bytes(payload)

    def write_bytes(self, data: bytes) -> int:
        if self._ser is None:
            raise RuntimeError("port not open")
        with self._write_lock:
            return self._ser.write(data)

    def send_command(self, cmd: str) -> int:
        """Send a textual command and return bytes written."""
        return self.write_text(cmd)

    # ---- boot log / reset reason ------------------------------------------------

    def capture_boot_log(self, duration: float = 2.0,
                         idle_timeout: float = 0.5) -> list[str]:
        """Collect lines after opening; stop after `duration` or idle gap."""
        lines: list[str] = []
        deadline = time.monotonic() + duration
        last = time.monotonic()
        while time.monotonic() < deadline:
            line = self.read_line(timeout=0.2)
            if line is not None:
                lines.append(line.text)
                last = time.monotonic()
            elif time.monotonic() - last > idle_timeout and lines:
                break
        return lines

    def reset_reason(self, lines: list[str] | None = None) -> dict:
        if lines is None:
            lines = self.capture_boot_log()
        return parse_reset_reason(lines)

    # ---- logging ---------------------------------------------------------------

    def start_logging(self, path: str) -> None:
        self._logger = SerialLogger(path)

    def stop_logging(self) -> None:
        if self._logger is not None:
            self._logger.close()
            self._logger = None

    # ---- plug/unplug monitoring --------------------------------------------------

    def start_plug_monitor(self, interval: float = 1.0,
                           on_unplug=None, on_plug=None) -> None:
        """Poll list_ports; call callbacks on CH340 state changes."""
        self._on_unplug = on_unplug
        self._plug_monitor = threading.Thread(
            target=self._plug_loop, args=(interval, on_plug),
            name="ch340-plug", daemon=True)
        self._plug_monitor.start()

    def _plug_loop(self, interval: float, on_plug) -> None:
        seen = {p["port"] for p in discover_ch340()}
        while not self._stop.is_set():
            time.sleep(interval)
            now = {p["port"] for p in discover_ch340()}
            for gone in seen - now:
                if self._on_unplug:
                    self._on_unplug(gone)
            for added in now - seen:
                if on_plug:
                    on_plug(added)
            seen = now
