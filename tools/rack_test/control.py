"""Wrapper around the v0.9 device command channel.

The board listens on a single-client TCP port (``192.168.100.50:5005``).  This
class owns exactly one connection for the harness; every command is serialized
and retried after a transport failure.  GUI / auth-service clients must not
connect at the same time.
"""

from __future__ import annotations

import subprocess
import sys
import threading
import time
from typing import Any, Dict, Optional, Tuple

from tools.auth_server.device_client import (
    DeviceClient, DeviceError, DeviceTimeout,
)

from .config import REPO_ROOT, RackConfig
from .sink import RotatingJsonl


def _short_params(params: Optional[dict]) -> dict:
    out = {}
    for key, value in (params or {}).items():
        text = str(value)
        out[key] = text if len(text) <= 40 else text[:37] + "..."
    return out


class RackControl:
    def __init__(self, config: RackConfig,
                 event_sink: Optional[RotatingJsonl] = None,
                 log=print) -> None:
        self.config = config
        self.event_sink = event_sink
        self.log = log
        self._client = DeviceClient(config.device_host,
                                    int(config.device_port),
                                    timeout=float(config.control_timeout_s))
        self._lock = threading.Lock()
        self._connected_once = False
        self.last_error: str = ""

    # ------------------------------------------------------------- lifecycle
    def connect(self) -> None:
        with self._lock:
            self._client.connect()
            self._connected_once = True

    def close(self) -> None:
        with self._lock:
            self._client.close()

    def __enter__(self) -> "RackControl":
        self.connect()
        return self

    def __exit__(self, *_exc) -> None:
        self.close()

    # --------------------------------------------------------------- commands
    def send(self, cmd: str, params: Optional[dict] = None,
             retries: Optional[int] = None) -> dict:
        attempts = int(self.config.control_retries if retries is None else retries)
        attempts = max(1, attempts)
        last_exc: Optional[Exception] = None
        for attempt in range(1, attempts + 1):
            started = time.monotonic()
            with self._lock:
                try:
                    ack = self._client.send_cmd(cmd, params or {})
                except (DeviceError, DeviceTimeout) as exc:
                    last_exc = exc
                    self.last_error = str(exc)
                    self._client.close()
                    ack = None
            if ack is not None:
                latency_ms = int((time.monotonic() - started) * 1000)
                self._event({
                    "kind": "command", "cmd": cmd,
                    "params": _short_params(params),
                    "result": ack.get("result"),
                    "error": ack.get("error", ""),
                    "attempt": attempt,
                    "latency_ms": latency_ms,
                })
                return ack
            self._event({"kind": "command_error", "cmd": cmd,
                         "attempt": attempt, "error": str(last_exc)})
            if attempt < attempts:
                time.sleep(float(self.config.control_retry_delay_s))
        raise DeviceError(str(last_exc) if last_exc else "command failed")

    # ------------------------------------------------------------ convenience
    def probe(self, timeout_s: float = 5.0) -> Dict[str, Any]:
        """Short, throwaway get_switch used by the preflight.

        A separate client with a short timeout distinguishes:
          * refused  -> no listener / accept queue full;
          * timeout  -> TCP handshake ok but the listener task did not reply.
        """
        client = DeviceClient(self.config.device_host,
                              int(self.config.device_port),
                              timeout=float(timeout_s))
        try:
            client.connect()
            ack = client.send_cmd("get_switch")
            if ack.get("result") != "ok":
                raise DeviceError("get_switch failed: %s"
                                  % ack.get("error", ""))
            data = ack.get("data")
            return data if isinstance(data, dict) else {}
        finally:
            client.close()

    def get_switch(self) -> Dict[str, Any]:
        ack = self.send("get_switch")
        if ack.get("result") != "ok":
            raise DeviceError("get_switch failed: %s" % ack.get("error", ""))
        data = ack.get("data")
        return data if isinstance(data, dict) else {}

    def set_sw(self, index: int, on: int) -> dict:
        return self.send("set_sw", {"sw": int(index), "on": int(on)})

    def set_buzzer(self, on: int) -> dict:
        return self.send("set_buzzer", {"on": int(on)})

    def reboot(self) -> dict:
        return self.send("reboot", {"mode": "soft"})

    def atecc_get_devid(self) -> dict:
        return self.send("atecc.get_devid")

    def atecc_get_pubkey(self) -> dict:
        return self.send("atecc.get_pubkey")

    def atecc_auth_start(self, challenge_hex: str) -> dict:
        return self.send("atecc.auth_start", {"challenge_hex": challenge_hex})

    def atecc_auth_verify(self, token: str, device_id: str = "") -> dict:
        params = {"token": token}
        if device_id:
            params["device_id"] = device_id
        return self.send("atecc.auth_verify", params)

    def atecc_get_license(self) -> dict:
        return self.send("atecc.get_license")

    def atecc_license_clear(self) -> dict:
        return self.send("atecc.license_clear")

    # ------------------------------------------------------------ auth flow
    def run_auth_flow(self, user: str, password: str,
                      timeout_s: float = 180.0) -> Tuple[bool, str, str]:
        """Run the full authorize->license->push chain via the auth-service CLI.

        The board listener is single-client, so the harness control connection
        is closed first.  The password is fed on stdin (never argv).
        Returns ``(ok, stdout, stderr)``.
        """
        self.close()
        cmd = [
            sys.executable, "-m", "tools.auth_server.service", "flow",
            "--host", self.config.device_host,
            "--port", str(int(self.config.device_port)),
            "--user", user,
            "--ttl", str(int(self.config.auth_ttl_s)),
        ]
        self._event({"kind": "auth_flow_start", "user": user,
                     "host": self.config.device_host,
                     "port": int(self.config.device_port)})
        try:
            proc = subprocess.run(
                cmd, cwd=str(REPO_ROOT),
                input=(password + "\n"), text=True,
                capture_output=True, timeout=timeout_s,
            )
        except (OSError, subprocess.SubprocessError) as exc:
            self._event({"kind": "auth_flow_error", "error": str(exc)})
            return (False, "", str(exc))
        ok = proc.returncode == 0
        self._event({"kind": "auth_flow_result", "ok": ok,
                     "returncode": proc.returncode,
                     "stdout": proc.stdout[-2000:],
                     "stderr": proc.stderr[-2000:]})
        return (ok, proc.stdout, proc.stderr)

    # --------------------------------------------------------------- internal
    def _event(self, record: dict) -> None:
        if self.event_sink is None:
            return
        rec = dict(record)
        rec.setdefault("kind", "command")
        try:
            self.event_sink.write(rec)
        except OSError:
            pass
