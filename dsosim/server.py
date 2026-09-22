"""
TCP server wrapper exposing the simulator on port 5005.

Speaks the same line-delimited JSON protocol as the real DSO's TCP
channel: each telemetry frame / control command / ack ends with CRLF.
"""

from __future__ import annotations

import socketserver
import threading

from .device import DSOSimulator
from .protocol import AckFrame, ControlCommand


class _Handler(socketserver.StreamRequestHandler):
    def handle(self):
        dev: DSOSimulator = self.server.dev
        dev.connect_client()
        self.server._client_connected = True
        try:
            while True:
                line = self.rfile.readline()
                if not line:
                    break
                line = line.decode("utf-8", errors="replace").strip()
                if not line:
                    continue
                try:
                    cmd = ControlCommand.from_json(line)
                    ack = dev.send_cmd(cmd.cmd, cmd.params)
                    self.wfile.write((ack.to_json() + "\r\n").encode("utf-8"))
                    self.wfile.flush()
                except Exception as e:  # noqa: BLE001 - keep server alive on bad frame
                    # 坏帧/解析失败：回一条 error ack，保持连接不中断
                    bad = locals().get("cmd")
                    ack = AckFrame(seq=getattr(bad, "seq", 0),
                                   cmd=getattr(bad, "cmd", "?"),
                                   result="error", error=str(e))
                    self.wfile.write((ack.to_json() + "\r\n").encode("utf-8"))
                    self.wfile.flush()
        finally:
            self.server._client_connected = False


class SimulatorServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

    def __init__(self, dev: DSOSimulator, host: str = "0.0.0.0",
                 port: int = 5005):
        super().__init__((host, port), _Handler)
        self.dev = dev
        self._client_connected = False
        self._thread: threading.Thread | None = None

    def start(self) -> None:
        self._thread = threading.Thread(target=self.serve_forever, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self.shutdown()
        self.server_close()
