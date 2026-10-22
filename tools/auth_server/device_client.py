"""Minimal client for the device's v0.9 JSON channel (TCP 5005 / UART).

Firmware quirks this client handles:
  * replies are matched by ``type=="ack"`` + ``seq`` + ``cmd``;
  * the same connection also carries periodic telemetry frames -> filter by type;
  * an **unknown** command is echoed back verbatim instead of being acked
    (User/dso/dso_eth_upload.c) -> detect the echo and report "unsupported";
  * the command line buffer is 512 bytes and truncates silently
    -> refuse oversized requests locally instead of sending a corrupt frame.
"""

from __future__ import annotations

import json
import socket
import time

from .proto import MAX_REQUEST

DEFAULT_PORT = 5005


class DeviceError(RuntimeError):
    """Transport- or protocol-level failure."""


class DeviceTimeout(DeviceError):
    """No matching ack before the deadline."""


class DeviceClient:
    def __init__(self, host: str, port: int = DEFAULT_PORT,
                 timeout: float = 20.0):
        # ACK timeout. The FIRST atecc.auth_start on a freshly booted device also
        # performs the one-time mbedTLS init + self-test (several ECDSA ops), so
        # a few seconds is normal there.
        self.host = host
        self.port = port
        self.timeout = timeout
        self._sock: socket.socket | None = None
        self._buf = b""
        self._seq = 0

    # ---- lifecycle -------------------------------------------------------
    def __enter__(self) -> "DeviceClient":
        self.connect()
        return self

    def __exit__(self, *_exc) -> None:
        self.close()

    def connect(self) -> None:
        if self._sock is not None:
            return
        try:
            # Connect fast (5 s), then wait patiently for acks.
            self._sock = socket.create_connection((self.host, self.port),
                                                  timeout=min(self.timeout, 5.0))
        except OSError as exc:
            raise DeviceError("cannot connect %s:%d: %s"
                              % (self.host, self.port, exc)) from None
        self._sock.settimeout(self.timeout)
        self._buf = b""

    def close(self) -> None:
        if self._sock is not None:
            try:
                self._sock.close()
            except OSError:
                pass
            self._sock = None

    # ---- raw I/O ---------------------------------------------------------
    def _readline(self) -> str:
        assert self._sock is not None
        deadline = time.monotonic() + self.timeout
        while b"\n" not in self._buf:
            if time.monotonic() > deadline:
                raise DeviceTimeout("no reply within %.1fs" % self.timeout)
            try:
                chunk = self._sock.recv(4096)
            except socket.timeout:
                raise DeviceTimeout("no reply within %.1fs" % self.timeout) from None
            except OSError as exc:
                raise DeviceError("socket error: %s" % exc) from None
            if not chunk:
                raise DeviceError("device closed the connection")
            self._buf += chunk
        line, self._buf = self._buf.split(b"\n", 1)
        return line.decode("utf-8", errors="replace").strip()

    def send_cmd(self, cmd: str, params: dict | None = None) -> dict:
        """Send one command and return its ack object."""
        self._seq += 1
        seq = self._seq
        line = json.dumps({"cmd": cmd, "params": params or {}, "seq": seq},
                          separators=(",", ":"), ensure_ascii=True)
        raw = (line + "\r\n").encode("ascii")
        # Refuse locally BEFORE touching the network: the firmware's 512-byte
        # line buffer truncates silently, which would corrupt the frame.
        if len(raw) > MAX_REQUEST:
            raise DeviceError("request is %d bytes; the firmware line buffer is "
                              "512 and truncates silently (limit %d)"
                              % (len(raw), MAX_REQUEST))
        self.connect()
        assert self._sock is not None
        try:
            self._sock.sendall(raw)
        except OSError as exc:
            raise DeviceError("send failed: %s" % exc) from None

        deadline = time.monotonic() + self.timeout
        while True:
            if time.monotonic() > deadline:
                raise DeviceTimeout("no ack for %s" % cmd)
            text = self._readline()
            if not text:
                continue
            try:
                obj = json.loads(text)
            except ValueError:
                continue                       # not JSON: ignore
            if not isinstance(obj, dict):
                continue
            if obj.get("type") != "ack":
                # our own command echoed back => the firmware does not know it
                if obj.get("cmd") == cmd and obj.get("seq") == seq:
                    raise DeviceError("firmware does not support %s" % cmd)
                continue                       # telemetry / unrelated frame
            if obj.get("seq") != seq or obj.get("cmd") != cmd:
                continue
            return obj

    # ---- command wrappers ------------------------------------------------
    def get_devid(self) -> dict:
        return self.send_cmd("atecc.get_devid")

    def get_pubkey(self) -> dict:
        return self.send_cmd("atecc.get_pubkey")

    def auth_start(self, challenge_hex: str) -> dict:
        return self.send_cmd("atecc.auth_start",
                             {"challenge_hex": challenge_hex})

    def auth_verify(self, token: str, device_id: str | None = None) -> dict:
        params: dict = {"token": token}
        if device_id:
            params["device_id"] = device_id
        return self.send_cmd("atecc.auth_verify", params)

    def get_license(self) -> dict:
        return self.send_cmd("atecc.get_license")
