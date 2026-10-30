"""TCP receiver for the board's module telemetry.

Two board-originated connections arrive on the same port:

* the telemetry client (``dso_module_upload``) which sends 7 JSON lines per
  second and never reads;
* the legacy FMC telemetry client (``dso_fmc_telemetry``) which - because of a
  firmware recv bug - connects and closes again at a ~50 ms cadence without
  sending a line.

Empty connections are closed with RST so the board's small LwIP pool does not
fill with TIME_WAIT entries.  A connection that has sent any byte is always
closed with FIN.
"""

from __future__ import annotations

import os
import selectors
import socket
import struct
import threading
import time
from pathlib import Path
from typing import Dict, Optional

from . import decoders
from .config import RackConfig
from .integrity import IntegrityTracker
from .sink import RotatingJsonl, write_atomic_json
from .stats import RunStats


def _wall_ms() -> int:
    return int(time.time() * 1000)


def _iso_now() -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime()) + ".%03dZ" % (
        int(time.time() * 1000) % 1000)


class _Conn:
    __slots__ = ("sock", "peer", "fileno", "buf", "bytes_in", "channel",
                 "opened", "last_data", "discard", "invalid_logged",
                 "oversize_logged")

    def __init__(self, sock: socket.socket, peer: str) -> None:
        self.sock = sock
        self.peer = peer
        self.fileno = sock.fileno()
        self.buf = bytearray()
        self.bytes_in = 0
        self.channel = "unknown"
        self.opened = time.monotonic()
        self.last_data = self.opened
        self.discard = False
        self.invalid_logged = False
        self.oversize_logged = False


class RackReceiver:
    def __init__(self, config: RackConfig, stats: RunStats,
                 integrity: IntegrityTracker, run_dir: Path,
                 raw_sink: Optional[RotatingJsonl] = None,
                 event_sink: Optional[RotatingJsonl] = None,
                 log=print) -> None:
        self.config = config
        self.stats = stats
        self.integrity = integrity
        self.run_dir = Path(run_dir)
        self.raw_sink = raw_sink
        self.event_sink = event_sink
        self.log = log

        self._listener: Optional[socket.socket] = None
        self._selector: Optional[selectors.DefaultSelector] = None
        self._conns: Dict[int, _Conn] = {}
        self._thread: Optional[threading.Thread] = None
        self._stop = threading.Event()
        self._ready = threading.Event()
        self._raw_counter = 0
        self._last_status = 0.0
        self._last_summary = 0.0
        self._last_sweep = 0.0
        self.error: Optional[str] = None

    # --------------------------------------------------------------- lifecycle
    def bind(self) -> None:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind((self.config.bind_host, int(self.config.bind_port)))
        sock.listen(64)
        sock.setblocking(False)
        self._listener = sock
        self._selector = selectors.DefaultSelector()
        self._selector.register(sock, selectors.EVENT_READ, None)

    @property
    def local_port(self) -> int:
        if self._listener is None:
            return int(self.config.bind_port)
        return int(self._listener.getsockname()[1])

    def start(self) -> None:
        if self._listener is None:
            self.bind()
        self._thread = threading.Thread(target=self.serve_forever,
                                        name="rack-receiver", daemon=True)
        self._thread.start()

    def wait_ready(self, timeout: float = 5.0) -> bool:
        return self._ready.wait(timeout)

    def stop(self) -> None:
        self._stop.set()
        if self._thread is not None and self._thread.is_alive():
            self._thread.join(timeout=3.0)

    def serve_forever(self) -> None:
        if self._listener is None:
            self.bind()
        assert self._selector is not None and self._listener is not None
        self._ready.set()
        self.log("[rack] receiver listening on %s:%d"
                 % (self.config.bind_host, self.local_port))
        try:
            while not self._stop.is_set():
                try:
                    events = self._selector.select(timeout=0.2)
                except (OSError, ValueError):
                    break
                for key, _mask in events:
                    try:
                        if key.data is None:
                            self._accept_all()
                        else:
                            self._read(key.data)
                    except Exception as exc:
                        self._event({"kind": "loop_error",
                                     "error": str(exc)})
                self._timers()
        finally:
            self._shutdown()

    # ------------------------------------------------------------------ accept
    def _accept_all(self) -> None:
        assert self._listener is not None and self._selector is not None
        while True:
            try:
                sock, addr = self._listener.accept()
            except BlockingIOError:
                return
            except OSError:
                return
            peer = "%s:%d" % (addr[0], addr[1])
            if len(self._conns) >= int(self.config.max_conns):
                self.stats.note_rejected()
                self._close_socket(sock, rst=True)
                continue
            sock.setblocking(False)
            conn = _Conn(sock, peer)
            self._conns[conn.fileno] = conn
            self._selector.register(sock, selectors.EVENT_READ, conn)
            self.stats.note_accept(peer)

    # ------------------------------------------------------------------- read
    def _read(self, conn: _Conn) -> None:
        try:
            data = conn.sock.recv(4096)
        except BlockingIOError:
            return
        except OSError:
            self._drop(conn)
            return
        if not data:
            self._drop(conn, eof=True)
            return
        conn.bytes_in += len(data)
        conn.last_data = time.monotonic()
        conn.buf.extend(data)
        self._drain(conn)

    def _drain(self, conn: _Conn) -> None:
        max_line = int(self.config.max_line_bytes)
        while True:
            if conn.discard:
                idx = conn.buf.find(b"\n")
                if idx < 0:
                    conn.buf.clear()
                    return
                del conn.buf[:idx + 1]
                conn.discard = False
                continue
            idx = conn.buf.find(b"\n")
            if idx < 0:
                if len(conn.buf) > max_line:
                    if not conn.oversize_logged:
                        self.stats.note_oversize()
                        self._event({"kind": "oversize", "peer": conn.peer,
                                     "bytes": len(conn.buf)})
                        conn.oversize_logged = True
                    conn.buf.clear()
                    conn.discard = True
                return
            raw = bytes(conn.buf[:idx])
            del conn.buf[:idx + 1]
            if raw.endswith(b"\r"):
                raw = raw[:-1]
            if not raw:
                continue
            if len(raw) > max_line:
                if not conn.oversize_logged:
                    self.stats.note_oversize()
                    self._event({"kind": "oversize", "peer": conn.peer,
                                 "bytes": len(raw)})
                    conn.oversize_logged = True
                continue
            line = raw.decode("utf-8", errors="replace")
            try:
                self._handle_line(conn, line, _wall_ms())
            except Exception as exc:  # one bad line must not kill the receiver
                self.stats.note_bad_frame(None, str(exc))
                self._event({"kind": "handler_error", "peer": conn.peer,
                             "error": str(exc), "line": line[:512]})

    # ------------------------------------------------------------------ lines
    def _handle_line(self, conn: _Conn, line: str, wall_ms: int) -> None:
        if conn.channel == "unknown":
            try:
                kind, obj, reason = decoders.classify_line(line)
            except Exception as exc:  # codec load failure must not kill the loop
                conn.channel = "invalid"
                self.stats.note_classified("invalid")
                self.stats.note_invalid()
                if not conn.invalid_logged:
                    conn.invalid_logged = True
                    self._event({"kind": "invalid_line", "peer": conn.peer,
                                 "reason": "decoder error: %s" % exc,
                                 "line": line[:512]})
                return
            if kind == "telemetry":
                conn.channel = "telemetry"
                self.stats.note_classified("telemetry")
                self._event({"kind": "classify", "peer": conn.peer,
                             "channel": "telemetry"})
                self._telemetry(conn, obj, wall_ms, line)
                return
            if kind == "legacy":
                conn.channel = "legacy"
                self.stats.note_classified("legacy")
                self.stats.note_legacy()
                self._event({"kind": "classify", "peer": conn.peer,
                             "channel": "legacy",
                             "messageType": obj.get("messageType")})
                return
            conn.channel = "invalid"
            self.stats.note_classified("invalid")
            self.stats.note_invalid()
            if not conn.invalid_logged:
                conn.invalid_logged = True
                self._event({"kind": "invalid_line", "peer": conn.peer,
                             "reason": reason, "line": line[:512]})
            return

        if conn.channel == "telemetry":
            # A second line on the same connection: parse it again (cheap) so
            # a mid-stream corruption cannot slip through.
            kind, obj, reason = decoders.classify_line(line)
            if kind != "telemetry":
                self.stats.note_invalid()
                self.stats.note_bad_frame(None, reason)
                self._event({"kind": "bad_frame", "peer": conn.peer,
                             "reason": reason, "line": line[:512]})
                return
            self._telemetry(conn, obj, wall_ms, line)
            return

        if conn.channel == "legacy":
            self.stats.note_legacy()
            return

        self.stats.note_invalid()

    def _telemetry(self, conn: _Conn, frame: dict, wall_ms: int,
                   raw_line: str) -> None:
        module = frame.get("module")
        # Feed continuity for every envelope-valid frame, including watchdog.
        for event in self.integrity.feed(frame, wall_ms):
            event = dict(event)
            event["peer"] = conn.peer
            self._event(event)

        errors = decoders.decode_module(frame)
        if errors:
            msg = "; ".join(errors)
            self.stats.note_bad_frame(module, msg)
            self._event({"kind": "bad_frame", "peer": conn.peer,
                         "module": module, "seq": frame.get("seq"),
                         "error": msg, "line": raw_line[:512]})
            return

        if frame.get("type") == "module.event":
            self.stats.observe_event(frame, wall_ms)
        else:
            self.stats.observe_frame(frame, wall_ms, conn.peer)

        if self.raw_sink is not None and int(self.config.raw_sample) > 0:
            self._raw_counter += 1
            if self._raw_counter % int(self.config.raw_sample) == 0:
                self.raw_sink.write({
                    "schema": "dso.rack_test.telemetry/1",
                    "wall_ms": wall_ms,
                    "recv_iso": _iso_now(),
                    "peer": conn.peer,
                    "channel": "telemetry",
                    "ver": frame.get("ver"),
                    "type": frame.get("type"),
                    "module": module,
                    "seq": frame.get("seq"),
                    "ts_ms": frame.get("ts_ms"),
                    "data": frame.get("data"),
                })

    # ------------------------------------------------------------------ close
    def _drop(self, conn: _Conn, eof: bool = False) -> None:
        if conn.fileno in self._conns:
            del self._conns[conn.fileno]
        if self._selector is not None:
            try:
                self._selector.unregister(conn.sock)
            except (KeyError, ValueError, OSError):
                pass
        had_bytes = conn.bytes_in > 0
        if conn.buf:
            self.stats.note_partial()
        rst = not had_bytes
        self._close_socket(conn.sock, rst=rst)
        self.stats.note_close(conn.channel, rst, had_bytes)
        if had_bytes:
            self._event({"kind": "close", "peer": conn.peer,
                         "channel": conn.channel, "bytes_in": conn.bytes_in,
                         "rst": rst})

    @staticmethod
    def _close_socket(sock: socket.socket, rst: bool) -> None:
        if rst:
            try:
                if os.name == "nt":
                    linger = struct.pack("HH", 1, 0)
                else:
                    linger = struct.pack("ii", 1, 0)
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, linger)
            except OSError:
                pass
        try:
            sock.close()
        except OSError:
            pass

    # ----------------------------------------------------------------- timers
    def _timers(self) -> None:
        now = time.monotonic()
        cfg = self.config

        if now - self._last_status >= float(cfg.status_interval_s):
            self._last_status = now
            try:
                write_atomic_json(self.run_dir / "status.json", self.status())
            except OSError:
                pass

        if now - self._last_summary >= float(cfg.summary_interval_s):
            self._last_summary = now
            try:
                self.log(self.stats.summary_line(self.integrity.snapshot()))
            except Exception:
                pass
            storm = self.stats.storm_line()
            if storm:
                self.log(storm)

        if now - self._last_sweep >= 1.0:
            self._last_sweep = now
            timeout = float(cfg.unknown_idle_timeout_s)
            stale = [c for c in self._conns.values()
                     if c.channel == "unknown" and c.bytes_in == 0
                     and now - c.opened > timeout]
            for conn in stale:
                self._event({"kind": "idle_rst", "peer": conn.peer})
                self._drop(conn)

    # ---------------------------------------------------------------- helpers
    def _event(self, record: dict) -> None:
        if self.event_sink is None:
            return
        rec = dict(record)
        rec.setdefault("schema", "dso.rack_test.event/1")
        rec.setdefault("wall_ms", _wall_ms())
        rec.setdefault("recv_iso", _iso_now())
        try:
            self.event_sink.write(rec)
        except OSError:
            pass

    def status(self) -> dict:
        snap = self.stats.snapshot()
        integrity = self.integrity.snapshot()
        a2_rate = 0.0
        up = snap.get("uptime_s") or 0
        if up > 0:
            a2_rate = snap["connections"]["a2_accepts"] / up
        return {
            "schema": "dso.rack_test.status/1",
            "updated_at": _iso_now(),
            "uptime_s": snap["uptime_s"],
            "receiver": {
                "listening": self._listener is not None,
                "bind": "%s:%d" % (self.config.bind_host, self.local_port),
                "active": snap["connections"]["active"],
                "max_concurrent": snap["connections"]["max_concurrent"],
                "accepted": snap["connections"]["accepted"],
            },
            "a2": {
                "accepts_total": snap["connections"]["a2_accepts"],
                "accepts_per_s": round(a2_rate, 2),
                "rst_closes": snap["connections"]["a2_rst"],
                "rejected": snap["connections"]["rejected"],
            },
            "frame_rate": snap["frame_rate"],
            "modules": snap["modules"],
            "events": snap["events"],
            "integrity": integrity,
            "sink": {
                "telemetry_bytes": self.raw_sink.size if self.raw_sink else 0,
                "events_bytes": self.event_sink.size if self.event_sink else 0,
            },
        }

    # ---------------------------------------------------------------- shutdown
    def _shutdown(self) -> None:
        for conn in list(self._conns.values()):
            self._drop(conn, eof=True)
        if self._selector is not None:
            try:
                self._selector.close()
            except OSError:
                pass
            self._selector = None
        if self._listener is not None:
            try:
                self._listener.close()
            except OSError:
                pass
            self._listener = None
