"""Offline fake device: exact 7-frame stream + v0.9 command server.

Used by ``dryrun`` and the unit tests.  It reproduces the firmware's frame
order, shared ``seq`` and ``ts_ms`` cadence, the v0.9 ack shape, the B10
no-op, invalid-index errors and the legacy A2 connect/close behaviour.
"""

from __future__ import annotations

import argparse
import json
import os
import random
import socket
import struct
import threading
import time
from typing import Dict, List, Optional, Tuple

from .config import WATCHDOG

FRAME_ORDER = ("atecc608b", "sht40", "ina226_u10", "ina226_u44",
               "bgm_adc_dac", "watchdog", "pcal6524")

DEVICE_ID = "Eep21025E269FA2E"
UID_HEX = "0123456789abcdef01234567"
PUBKEY_HEX = "AB" * 64
SIGNATURE_HEX = "CD" * 64


class FakeDevice:
    def __init__(self, host: str = "127.0.0.1", port: int = 5005,
                 control_host: str = "127.0.0.1", control_port: int = 15005,
                 interval: float = 1.0,
                 inject: Optional[List[str]] = None,
                 a2_storm: int = 0, a2_interval: float = 0.05,
                 a2_hold: float = 0.0, log=print) -> None:
        self.host = host
        self.port = int(port)
        self.control_host = control_host
        self.control_port = int(control_port)
        self.interval = max(0.01, float(interval))
        self.inject = list(inject or [])
        self.a2_storm = int(a2_storm)
        self.a2_interval = max(0.001, float(a2_interval))
        self.a2_hold = float(a2_hold)
        self.log = log

        self._stop = threading.Event()
        self._ready_tel = threading.Event()
        self._ready_control = threading.Event()
        self._lock = threading.Lock()
        self._tel_sock: Optional[socket.socket] = None
        self._control_listener: Optional[socket.socket] = None
        self._clients: List[socket.socket] = []
        self._threads: List[threading.Thread] = []

        self.seq = 0
        self.ts_ms = 0
        self.passes = 0
        self.last_frames: List[dict] = []
        self.sw_bitmap = 0
        self.buzzer_mute = 0
        self.license: Optional[dict] = None
        self.reboots = 0
        self._pending_reboot = False
        self._faults = {"seq_gap": 0, "ts_reset": False, "reboot": False,
                        "bad_json": False, "oversize": False}
        self._drop_module = ""
        self._parse_inject()

    # ------------------------------------------------------------------ setup
    def _parse_inject(self) -> None:
        for item in self.inject:
            name, _, value = item.partition("=")
            if name == "seq_gap":
                self._faults["seq_gap"] = int(value or 3)
            elif name in ("ts_reset", "reboot", "bad_json", "oversize"):
                self._faults[name] = True
            elif name == "drop_module":
                self._drop_module = value
            else:
                raise ValueError("unknown injection %r" % item)

    def start(self) -> None:
        self._spawn(self._telemetry_loop, "fake-telemetry")
        self._spawn(self._control_loop, "fake-control")
        if self.a2_storm:
            self._spawn(self._storm_loop, "fake-a2-storm")
        if self.a2_hold > 0:
            self._spawn(self._hold_loop, "fake-a2-hold")

    @property
    def control_address(self) -> Tuple[str, int]:
        if self._control_listener is None:
            return (self.control_host, self.control_port)
        addr = self._control_listener.getsockname()
        return (str(addr[0]), int(addr[1]))

    def wait_ready(self, timeout: float = 5.0) -> bool:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self._ready_control.is_set():
                return True
            time.sleep(0.02)
        return False

    def wait_passes(self, count: int, timeout: float = 10.0) -> bool:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.passes >= count:
                return True
            time.sleep(0.02)
        return False

    def stop(self) -> None:
        self._stop.set()
        for sock in [self._tel_sock, self._control_listener] + self._clients:
            if sock is not None:
                try:
                    sock.close()
                except OSError:
                    pass
        self._tel_sock = None
        self._control_listener = None
        for thread in self._threads:
            thread.join(timeout=2.0)
        self._threads = []

    def _spawn(self, target, name: str) -> None:
        thread = threading.Thread(target=target, name=name, daemon=True)
        thread.start()
        self._threads.append(thread)

    # --------------------------------------------------------------- telemetry
    def _telemetry_loop(self) -> None:
        while not self._stop.is_set():
            if self._pending_reboot:
                self._pending_reboot = False
                self._reset_after_reboot()
            try:
                sock = socket.create_connection((self.host, self.port), timeout=2.0)
            except OSError:
                time.sleep(0.2)
                continue
            sock.settimeout(2.0)
            self._tel_sock = sock
            self._ready_tel.set()
            try:
                while not self._stop.is_set():
                    if self._pending_reboot:
                        break
                    self._emit_pass(sock)
                    self.passes += 1
                    deadline = time.monotonic() + self.interval
                    while time.monotonic() < deadline:
                        if self._stop.is_set() or self._pending_reboot:
                            break
                        time.sleep(0.01)
            except OSError:
                pass
            finally:
                try:
                    sock.close()
                except OSError:
                    pass
                self._tel_sock = None
            if self._pending_reboot:
                continue
            time.sleep(0.2)

    def _reset_after_reboot(self) -> None:
        with self._lock:
            self.seq = 0
            self.ts_ms = 0
            self.reboots += 1
        time.sleep(0.2)

    def _next_seq(self) -> int:
        with self._lock:
            self.seq += 1
            # Apply the injected gap after the first pass so the receiver has
            # a previous seq to compare against.
            if self._faults["seq_gap"] and self.seq >= 7:
                self.seq += int(self._faults["seq_gap"])
                self._faults["seq_gap"] = 0
            return self.seq

    def _frame(self, module: str, data: dict, type_: str = "module.data") -> dict:
        return {"ver": "1.0", "type": type_, "module": module,
                "seq": self._next_seq(), "ts_ms": int(self.ts_ms),
                "data": data}

    def _emit_pass(self, sock: socket.socket) -> None:
        self.ts_ms += 1000
        if self._faults["ts_reset"] and self.passes >= 1:
            self._faults["ts_reset"] = False
            self.ts_ms = 1000
        frames: List[dict] = []
        for module in FRAME_ORDER:
            if module == self._drop_module:
                continue
            frames.append(self._frame(module, self._data_for(module)))
        lines = [json.dumps(f, separators=(",", ":"), ensure_ascii=True)
                 for f in frames]
        if self._faults["bad_json"]:
            self._faults["bad_json"] = False
            lines.append("this is not json")
        if self._faults["oversize"]:
            self._faults["oversize"] = False
            lines.append("X" * 5000)
        payload = ("\r\n".join(lines) + "\r\n").encode("ascii")
        sock.sendall(payload)
        self.last_frames = frames
        if self._faults["reboot"] and self.passes >= 1:
            self._faults["reboot"] = False
            self._pending_reboot = True

    def _data_for(self, module: str) -> dict:
        jitter = random.randint(-1, 1)
        if module == "atecc608b":
            return {
                "present": 1, "comm_ok": 1, "device_id": DEVICE_ID,
                "chip_info": {"serial4": "a2c4f16d", "part": "608B",
                              "rev": "00000200", "lock_config": 1,
                              "lock_data": 1},
                "login_state": "AUTHORIZED" if self.license else "NO_LICENSE",
                "fail_cnt": 0,
            }
        if module == "sht40":
            return {"temp_dezi": 245 + jitter, "rh_centi": 4523,
                    "temp_ok": 1, "hi_alarm": 0, "level": 0}
        if module == "ina226_u10":
            return {"cur_ma": 50 + jitter, "over_cur": 0, "valid": 1,
                    "level": 0}
        if module == "ina226_u44":
            return {"cur_ma": 118 + jitter, "over_cur": 0, "valid": 1,
                    "level": 0}
        if module == "bgm_adc_dac":
            return {"freq_valid": 0, "freq_state": 4, "freq_millihz": 0,
                    "gate_us": 0, "edge_count": 0, "ref_ticks": 0,
                    "overflow_count": 0, "last_edge_age_ms": 4000,
                    "overrange": 0, "sample_format": 0,
                    "sample_rate": 100000000, "overrun_count": 0}
        if module == "watchdog":
            return {"feed_en": 1, "kick_count": self.passes, "pfo": 0,
                    "reset_cause": 0, "uptime_ms": int(self.ts_ms)}
        if module == "pcal6524":
            yds = 0x06
            return {"sw": int(self.sw_bitmap), "yds": yds,
                    "eth_link": (yds >> 3) & 1,
                    "usb_active": (yds >> 4) & 1}
        raise KeyError(module)

    # ---------------------------------------------------------------- control
    def _control_loop(self) -> None:
        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            listener.bind((self.control_host, self.control_port))
            listener.listen(4)
        except OSError as exc:
            self.log("[fake] control bind failed: %s" % exc)
            return
        self._control_listener = listener
        self._ready_control.set()
        while not self._stop.is_set():
            try:
                conn, _addr = listener.accept()
            except OSError:
                return
            conn.settimeout(0.5)
            self._clients.append(conn)
            thread = threading.Thread(target=self._client_loop, args=(conn,),
                                      name="fake-control-client", daemon=True)
            thread.start()
            self._threads.append(thread)

    def _client_loop(self, conn: socket.socket) -> None:
        buf = bytearray()
        while not self._stop.is_set():
            try:
                chunk = conn.recv(4096)
            except socket.timeout:
                continue
            except OSError:
                break
            if not chunk:
                break
            buf.extend(chunk)
            while b"\n" in buf:
                idx = buf.find(b"\n")
                raw = bytes(buf[:idx])
                del buf[:idx + 1]
                if raw.endswith(b"\r"):
                    raw = raw[:-1]
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace")
                reply = self._handle_line(line)
                try:
                    conn.sendall((reply + "\r\n").encode("utf-8"))
                except OSError:
                    break
        try:
            conn.close()
        except OSError:
            pass
        if conn in self._clients:
            self._clients.remove(conn)

    def _ack(self, seq: int, cmd: str, result: str = "ok",
             error: str = "", data: Optional[dict] = None) -> str:
        obj = {"ver": "0.9", "type": "ack", "seq": seq, "cmd": cmd,
               "result": result, "error": error}
        if data is not None:
            obj["data"] = data
        return json.dumps(obj, ensure_ascii=True, separators=(",", ":"))

    def _handle_line(self, line: str) -> str:
        try:
            obj = json.loads(line)
        except ValueError:
            return line  # unknown/garbage is echoed, like the firmware
        if not isinstance(obj, dict):
            return line
        cmd = obj.get("cmd")
        if not cmd:
            return line
        seq = int(obj.get("seq", 0) or 0)
        params = obj.get("params") if isinstance(obj.get("params"), dict) else obj
        if cmd == "get_switch":
            data = {"sw": int(self.sw_bitmap), "led": 0x01, "temp": 245,
                    "temp_ok": 1, "eth_link": 1, "usb_active": 0,
                    "buzzer_mute": int(self.buzzer_mute)}
            return self._ack(seq, cmd, data=data)
        if cmd == "set_sw":
            index = params.get("sw")
            on = params.get("on")
            if (not isinstance(index, int) or not isinstance(on, int)
                    or index < 0 or index >= 14 or on not in (0, 1)):
                return self._ack(seq, cmd, "error", "bad_param")
            if index != 9:
                if on:
                    self.sw_bitmap |= (1 << index)
                else:
                    self.sw_bitmap &= ~(1 << index)
            return self._ack(seq, cmd)
        if cmd == "set_buzzer":
            on = params.get("on")
            if not isinstance(on, int) or on not in (0, 1):
                return self._ack(seq, cmd, "error", "bad_param")
            self.buzzer_mute = on
            return self._ack(seq, cmd)
        if cmd == "reboot":
            self._pending_reboot = True
            return self._ack(seq, cmd, data={"mode": "soft"})
        if cmd == "atecc.get_devid":
            return self._ack(seq, cmd, data={
                "device_id": DEVICE_ID, "label8": DEVICE_ID[:8],
                "uid_hex": UID_HEX})
        if cmd == "atecc.get_pubkey":
            return self._ack(seq, cmd, data={
                "pubkey_hex": PUBKEY_HEX, "slot": 0, "device_id": DEVICE_ID})
        if cmd == "atecc.auth_start":
            challenge = str(params.get("challenge_hex", ""))
            if len(challenge) != 64:
                return self._ack(seq, cmd, "error",
                                 "need params.challenge_hex = 64 hex chars "
                                 "(32 bytes)", {})
            return self._ack(seq, cmd, data={
                "device_id": DEVICE_ID, "signature_hex": SIGNATURE_HEX,
                "pubkey_hex": PUBKEY_HEX, "slot": 0, "self_verify": 1})
        if cmd == "atecc.auth_verify":
            token = str(params.get("token", ""))
            if not token:
                return self._ack(seq, cmd, "error", "need params.token", {})
            self.license = {"lic": "L00001", "user": "test",
                            "dev": DEVICE_ID, "nb": 0, "na": 0, "perm": 1}
            return self._ack(seq, cmd, data=dict(self.license))
        if cmd == "atecc.get_license":
            return self._ack(seq, cmd, data={
                "device_id": DEVICE_ID,
                "login_state": "LOGGED_IN" if self.license else "LOGGED_OUT",
                "restored": 0, "license": self.license})
        if cmd == "atecc.license_clear":
            self.license = None
            return self._ack(seq, cmd, data={"cleared": 1})
        return line  # unknown command: echo verbatim

    # --------------------------------------------------------------- A2 storm
    def _storm_loop(self) -> None:
        for _ in range(self.a2_storm):
            if self._stop.is_set():
                return
            try:
                sock = socket.create_connection((self.host, self.port),
                                                timeout=1.0)
                # Board behaviour: connect, see an empty non-blocking recv,
                # close immediately without sending anything (FIN, not RST).
                sock.close()
            except OSError:
                pass
            time.sleep(self.a2_interval)

    def _hold_loop(self) -> None:
        try:
            sock = socket.create_connection((self.host, self.port), timeout=1.0)
        except OSError:
            return
        deadline = time.monotonic() + self.a2_hold
        while time.monotonic() < deadline and not self._stop.is_set():
            time.sleep(0.05)
        try:
            sock.close()
        except OSError:
            pass


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="DSO rack-test fake device")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=5005)
    parser.add_argument("--control-host", default="127.0.0.1")
    parser.add_argument("--control-port", type=int, default=15005)
    parser.add_argument("--interval", type=float, default=1.0)
    parser.add_argument("--inject", action="append", default=[])
    parser.add_argument("--a2-storm", type=int, default=0)
    parser.add_argument("--a2-interval", type=float, default=0.05)
    parser.add_argument("--a2-hold", type=float, default=0.0)
    parser.add_argument("--seconds", type=float, default=0.0,
                        help="run for N seconds then exit (0 = forever)")
    args = parser.parse_args(argv)
    fake = FakeDevice(host=args.host, port=args.port,
                      control_host=args.control_host,
                      control_port=args.control_port,
                      interval=args.interval, inject=args.inject,
                      a2_storm=args.a2_storm, a2_interval=args.a2_interval,
                      a2_hold=args.a2_hold)
    fake.start()
    print("[fake] telemetry -> %s:%d, control on %s:%d"
          % (args.host, args.port, args.control_host, args.control_port))
    try:
        if args.seconds:
            time.sleep(args.seconds)
        else:
            while True:
                time.sleep(1.0)
    except KeyboardInterrupt:
        pass
    finally:
        fake.stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
