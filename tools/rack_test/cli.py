"""Command-line entry points for the rack test harness."""

from __future__ import annotations

import argparse
import dataclasses
import json
import secrets
import signal
import socket
import sys
import time
from pathlib import Path
from typing import List, Optional, Tuple

from . import decoders, metadata, report
from .config import RackConfig, load as load_config
from .control import RackControl
from .fakedev import FakeDevice
from .integrity import IntegrityTracker
from .receiver import RackReceiver
from .scenario import Scenario
from .sink import RotatingJsonl, read_json, write_atomic_json
from .stats import RunStats


def _parse_hostport(text: str) -> Tuple[str, int]:
    host, _, port = text.rpartition(":")
    if not host:
        raise argparse.ArgumentTypeError("expected HOST:PORT, got %r" % text)
    return (host, int(port))


def _port_is_free(port: int) -> bool:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.bind(("0.0.0.0", int(port)))
        return True
    except OSError:
        return False
    finally:
        try:
            sock.close()
        except OSError:
            pass


def _new_run_dir(state_dir: str) -> Path:
    run_id = time.strftime("%Y%m%d-%H%M%S") + "-" + secrets.token_hex(2)
    path = Path(state_dir) / "runs" / run_id
    path.mkdir(parents=True, exist_ok=True)
    return path


def _load_cfg(args) -> RackConfig:
    cfg = RackConfig()
    if getattr(args, "config", None):
        cfg = load_config(args.config, cfg)
    if getattr(args, "bind", None):
        host, port = _parse_hostport(args.bind)
        cfg.bind_host, cfg.bind_port = host, port
    if getattr(args, "state_dir", None):
        cfg.state_dir = args.state_dir
    if getattr(args, "device", None):
        cfg.device_host = args.device
    if getattr(args, "device_port", None):
        cfg.device_port = args.device_port
    if getattr(args, "decoder_dir", None):
        cfg.decoder_dir = args.decoder_dir
    if getattr(args, "baseline", None) is not None:
        cfg.baseline_s = args.baseline
    if getattr(args, "soak", None) is not None:
        cfg.soak_s = args.soak
    if getattr(args, "settle", None) is not None:
        cfg.settle_s = args.settle
    if getattr(args, "operator", None):
        cfg.operator = args.operator
    if getattr(args, "firmware", None):
        cfg.firmware = args.firmware
    if getattr(args, "note", None):
        cfg.note = args.note
    if getattr(args, "allow_reboot", False):
        cfg.allow_reboot = True
    if getattr(args, "allow_license_clear", False):
        cfg.allow_license_clear = True
    if getattr(args, "auth_user", None):
        cfg.auth_user = args.auth_user
    if getattr(args, "auth_password", None):
        cfg.auth_password = args.auth_password
    if getattr(args, "freq_hz", None) is not None:
        cfg.freq_hz = args.freq_hz
    return cfg


def _install(cfg: RackConfig) -> None:
    decoders.install_decoder_path(cfg.decoder_dir)


def _sinks(run_dir: Path, cfg: RackConfig):
    raw = RotatingJsonl(run_dir / "telemetry_raw.jsonl",
                        cfg.raw_max_bytes, cfg.raw_backups)
    events = RotatingJsonl(run_dir / "events.jsonl",
                           cfg.raw_max_bytes, cfg.raw_backups)
    return raw, events


def _write_run_info(run_dir: Path, cfg: RackConfig) -> None:
    write_atomic_json(run_dir / "run.json", {
        "schema": "dso.rack_test.run/1",
        "run_id": run_dir.name,
        "started_at": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "config": cfg.redacted_dict(),
    })


def _fail(msg: str, code: int = 2) -> int:
    print("error: %s" % msg, file=sys.stderr)
    return code


# ---------------------------------------------------------------------------
# serve
# ---------------------------------------------------------------------------

def cmd_serve(args) -> int:
    cfg = _load_cfg(args)
    errors = cfg.validate()
    if errors:
        return _fail("; ".join(errors))
    _install(cfg)
    run_dir = _new_run_dir(cfg.state_dir)
    _write_run_info(run_dir, cfg)
    raw, events = _sinks(run_dir, cfg)
    stats = RunStats(cfg)
    integrity = IntegrityTracker()
    receiver = RackReceiver(cfg, stats, integrity, run_dir, raw, events)
    try:
        receiver.bind()
    except OSError as exc:
        return _fail("cannot bind %s:%d: %s"
                     % (cfg.bind_host, cfg.bind_port, exc))
    stop = {"flag": False}

    def _handler(_signum, _frame):
        stop["flag"] = True

    for sig in (signal.SIGINT, getattr(signal, "SIGTERM", signal.SIGINT)):
        try:
            signal.signal(sig, _handler)
        except (ValueError, OSError):
            pass
    receiver.start()
    print("[rack] run dir: %s" % run_dir)
    print("[rack] serving on %s:%d; Ctrl-C to stop"
          % (cfg.bind_host, receiver.local_port))
    try:
        while not stop["flag"]:
            time.sleep(0.5)
    except KeyboardInterrupt:
        pass
    finally:
        receiver.stop()
        raw.close()
        events.close()
    print("[rack] stopped; artifacts in %s" % run_dir)
    return 0


# ---------------------------------------------------------------------------
# watch
# ---------------------------------------------------------------------------

def _latest_run(state_dir: str) -> Optional[Path]:
    root = Path(state_dir) / "runs"
    if not root.is_dir():
        return None
    runs = sorted([p for p in root.iterdir() if p.is_dir()])
    return runs[-1] if runs else None


def cmd_watch(args) -> int:
    state_dir = args.state_dir or RackConfig().state_dir
    run_dir = Path(args.run_dir) if args.run_dir else _latest_run(state_dir)
    if run_dir is None:
        return _fail("no run directory found under %s" % args.state_dir)
    print("[rack] watching %s" % run_dir)
    try:
        while True:
            status = read_json(run_dir / "status.json")
            if not status:
                print("[rack] waiting for status.json ...")
            else:
                modules = status.get("modules") or {}
                counts = ",".join("%s:%d" % (m, v.get("count", 0))
                                  for m, v in modules.items())
                integ = status.get("integrity") or {}
                print("[rack] up=%ss fps=%.1f a2=%s(%.1f/s) active=%s "
                      "seq_gap=%s reboot=%s mods=%s"
                      % (int(status.get("uptime_s") or 0),
                         float(status.get("frame_rate") or 0),
                         (status.get("a2") or {}).get("accepts_total"),
                         float((status.get("a2") or {}).get("accepts_per_s") or 0),
                         (status.get("receiver") or {}).get("active"),
                         integ.get("seq_gaps"), integ.get("reboots"), counts))
            time.sleep(args.interval)
    except KeyboardInterrupt:
        return 0


# ---------------------------------------------------------------------------
# cmd
# ---------------------------------------------------------------------------

def cmd_cmd(args) -> int:
    cfg = _load_cfg(args)
    _install(cfg)
    control = RackControl(cfg)
    try:
        if args.action == "get_switch":
            ack = {"result": "ok", "data": control.get_switch()}
        elif args.action == "set_sw":
            if args.index is None or args.on is None:
                return _fail("set_sw needs INDEX and ON")
            ack = control.set_sw(args.index, args.on)
        elif args.action == "set_buzzer":
            if args.on is None:
                return _fail("set_buzzer needs ON")
            ack = control.set_buzzer(args.on)
        elif args.action == "reboot":
            ack = control.reboot()
        elif args.action == "get_devid":
            ack = control.atecc_get_devid()
        elif args.action == "get_pubkey":
            ack = control.atecc_get_pubkey()
        elif args.action == "auth_start":
            if not args.hex:
                return _fail("auth_start needs 64 hex chars")
            ack = control.atecc_auth_start(args.hex)
        elif args.action == "auth_verify":
            if not args.token:
                return _fail("auth_verify needs a token")
            ack = control.atecc_auth_verify(args.token)
        elif args.action == "get_license":
            ack = control.atecc_get_license()
        elif args.action == "license_clear":
            ack = control.atecc_license_clear()
        else:
            return _fail("unknown action %s" % args.action)
    except Exception as exc:
        return _fail(str(exc), 1)
    finally:
        control.close()
    print(json.dumps(ack, ensure_ascii=False, indent=1))
    return 0 if ack.get("result") == "ok" else 1


# ---------------------------------------------------------------------------
# run / dryrun
# ---------------------------------------------------------------------------

def _execute(cfg: RackConfig, run_dir: Path, soak_s: Optional[float],
             do_sweep: bool, human_answers: Optional[dict],
             do_control: bool = True) -> dict:
    _install(cfg)
    _write_run_info(run_dir, cfg)
    raw, events = _sinks(run_dir, cfg)
    stats = RunStats(cfg)
    integrity = IntegrityTracker()
    receiver = RackReceiver(cfg, stats, integrity, run_dir, raw, events)
    try:
        receiver.bind()
    except OSError as exc:
        raw.close()
        events.close()
        raise RuntimeError("cannot bind %s:%d: %s"
                           % (cfg.bind_host, cfg.bind_port, exc))
    receiver.start()
    control = RackControl(cfg, events)
    try:
        scenario = Scenario(cfg, receiver, control, run_dir, stats, integrity,
                            events)
        summary = scenario.run(soak_s=soak_s, do_sweep=do_sweep,
                               human_answers=human_answers,
                               do_control=do_control)
    finally:
        control.close()
        receiver.stop()
        raw.close()
        events.close()
    report.generate(run_dir)
    return summary


def _execute_control_only(cfg: RackConfig, run_dir: Path, do_sweep: bool,
                          do_reboot: bool,
                          human_answers: Optional[dict]) -> dict:
    _install(cfg)
    _write_run_info(run_dir, cfg)
    raw, events = _sinks(run_dir, cfg)
    stats = RunStats(cfg)
    integrity = IntegrityTracker()
    control = RackControl(cfg, events)
    try:
        scenario = Scenario(cfg, None, control, run_dir, stats, integrity,
                            events)
        summary = scenario.run_control_only(
            human_answers=human_answers, do_sweep=do_sweep,
            do_reboot=do_reboot)
    finally:
        control.close()
        raw.close()
        events.close()
    report.generate(run_dir)
    return summary


def cmd_run(args) -> int:
    cfg = _load_cfg(args)
    errors = cfg.validate()
    if errors:
        return _fail("; ".join(errors))
    if args.control_only and args.telemetry_only:
        return _fail("--control-only and --telemetry-only are mutually exclusive")
    answers = None
    if args.human_answers:
        try:
            answers = report.load_answers(args.human_answers)
        except ValueError as exc:
            return _fail(str(exc))
        if not isinstance(answers, dict):
            return _fail("human answers must be a JSON object")
    run_dir = _new_run_dir(cfg.state_dir)
    print("[rack] run dir: %s" % run_dir)
    print("[rack] device %s:%d, bind %s:%d"
          % (cfg.device_host, cfg.device_port, cfg.bind_host, cfg.bind_port))
    try:
        if args.control_only:
            if not _port_is_free(cfg.bind_port):
                return _fail("port %d already has a listener; stop serve "
                             "before --control-only" % cfg.bind_port)
            print("[rack] control-only: telemetry receiver disabled")
            summary = _execute_control_only(
                cfg, run_dir, not args.no_sweep, args.allow_reboot, answers)
        else:
            summary = _execute(cfg, run_dir, args.soak, not args.no_sweep,
                               answers, do_control=not args.telemetry_only)
    except KeyboardInterrupt:
        return 130
    except Exception as exc:
        return _fail(str(exc), 1)
    if args.human_answers_after:
        report_path, summary_path = report.generate(
            run_dir, args.human_answers_after)
    else:
        report_path = run_dir / "report.md"
        summary_path = run_dir / "summary.json"
    print("[rack] verdict=%s counts=%s"
          % (summary.get("verdict"), summary.get("counts")))
    print("[rack] report: %s" % report_path)
    print("[rack] summary: %s" % summary_path)
    return 1 if summary.get("verdict") == "FAIL" else 0


def cmd_dryrun(args) -> int:
    cfg = _load_cfg(args)
    # Dry run defaults: loopback, fast frames, short phases.
    if not args.bind:
        cfg.bind_host, cfg.bind_port = "127.0.0.1", 0
    cfg.baseline_s = args.baseline if args.baseline is not None else 5.0
    cfg.soak_s = args.soak if args.soak is not None else 2.0
    cfg.settle_s = args.settle if args.settle is not None else 0.5
    cfg.telemetry_wait_s = 10.0
    errors = cfg.validate()
    if errors:
        return _fail("; ".join(errors))
    _install(cfg)
    run_dir = _new_run_dir(cfg.state_dir)
    _write_run_info(run_dir, cfg)
    raw, events = _sinks(run_dir, cfg)
    stats = RunStats(cfg)
    integrity = IntegrityTracker()
    receiver = RackReceiver(cfg, stats, integrity, run_dir, raw, events)
    try:
        receiver.bind()
    except OSError as exc:
        return _fail("cannot bind %s:%d: %s"
                     % (cfg.bind_host, cfg.bind_port, exc))
    receiver.start()
    fake = FakeDevice(host="127.0.0.1", port=receiver.local_port,
                      control_port=0, interval=args.interval,
                      inject=args.inject, a2_storm=args.a2_storm,
                      a2_interval=args.a2_interval)
    fake.start()
    if not fake.wait_ready(5.0):
        receiver.stop()
        return _fail("fake control server did not start")
    host, port = fake.control_address
    cfg.device_host, cfg.device_port = host, port
    control = RackControl(cfg, events)
    print("[rack] dryrun: receiver :%d, fake control %s:%d"
          % (receiver.local_port, host, port))
    try:
        scenario = Scenario(cfg, receiver, control, run_dir, stats, integrity,
                            events)
        summary = scenario.run(soak_s=cfg.soak_s, do_sweep=not args.no_sweep,
                               do_control=not args.telemetry_only)
    finally:
        control.close()
        fake.stop()
        receiver.stop()
        raw.close()
        events.close()
    report.generate(run_dir)
    print("[rack] verdict=%s counts=%s"
          % (summary.get("verdict"), summary.get("counts")))
    print("[rack] report: %s" % (run_dir / "report.md"))
    return 1 if summary.get("verdict") == "FAIL" else 0


def cmd_report(args) -> int:
    run_dir = Path(args.run_dir)
    try:
        report_path, summary_path = report.generate(run_dir, args.human_answers)
    except (OSError, ValueError) as exc:
        return _fail(str(exc), 1)
    print("[rack] report: %s" % report_path)
    print("[rack] summary: %s" % summary_path)
    return 0


# ---------------------------------------------------------------------------
# parser
# ---------------------------------------------------------------------------

def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="python -m tools.rack_test",
        description="DSO bridge-board rack test harness (watchdog excluded)")
    sub = parser.add_subparsers(dest="command", required=True)

    def common(p):
        p.add_argument("--config", help="JSON config file")
        p.add_argument("--state-dir", default=None)
        p.add_argument("--decoder-dir", default=None)

    p = sub.add_parser("serve", help="run the telemetry receiver")
    common(p)
    p.add_argument("--bind", help="HOST:PORT (default 0.0.0.0:5005)")
    p.set_defaults(func=cmd_serve)

    p = sub.add_parser("watch", help="print status.json periodically")
    common(p)
    p.add_argument("--run-dir", default=None)
    p.add_argument("--interval", type=float, default=2.0)
    p.set_defaults(func=cmd_watch)

    p = sub.add_parser("cmd", help="send one v0.9 command to the board")
    common(p)
    p.add_argument("--device", default=None)
    p.add_argument("--device-port", type=int, default=None)
    p.add_argument("action", choices=[
        "get_switch", "set_sw", "set_buzzer", "reboot", "get_devid",
        "get_pubkey", "auth_start", "auth_verify", "get_license",
        "license_clear"])
    p.add_argument("index", nargs="?", type=int)
    p.add_argument("on", nargs="?", type=int)
    p.add_argument("--hex", default=None)
    p.add_argument("--token", default=None)
    p.set_defaults(func=cmd_cmd)

    p = sub.add_parser("run", help="run the full scenario against the board")
    common(p)
    p.add_argument("--bind", help="HOST:PORT (default 0.0.0.0:5005)")
    p.add_argument("--device", default=None)
    p.add_argument("--device-port", type=int, default=None)
    p.add_argument("--baseline", type=float, default=None)
    p.add_argument("--soak", type=float, default=None)
    p.add_argument("--settle", type=float, default=None)
    p.add_argument("--no-sweep", action="store_true")
    p.add_argument("--telemetry-only", action="store_true",
                   help="skip the control channel entirely (module data plane)")
    p.add_argument("--control-only", action="store_true",
                   help="no telemetry receiver; switch sweep + ATECC only")
    p.add_argument("--allow-reboot", action="store_true")
    p.add_argument("--allow-license-clear", action="store_true")
    p.add_argument("--auth-user", default=None)
    p.add_argument("--auth-password", default=None)
    p.add_argument("--freq-hz", type=float, default=None)
    p.add_argument("--operator", default=None)
    p.add_argument("--firmware", default=None)
    p.add_argument("--note", default=None)
    p.add_argument("--human-answers", default=None,
                   help="JSON answers merged into the checks")
    p.add_argument("--human-answers-after", default=None,
                   help="answers to merge when regenerating the report")
    p.set_defaults(func=cmd_run)

    p = sub.add_parser("dryrun", help="offline fake device end-to-end test")
    common(p)
    p.add_argument("--bind", help="HOST:PORT (default 127.0.0.1:0)")
    p.add_argument("--baseline", type=float, default=None)
    p.add_argument("--soak", type=float, default=None)
    p.add_argument("--settle", type=float, default=None)
    p.add_argument("--interval", type=float, default=0.1)
    p.add_argument("--inject", action="append", default=[])
    p.add_argument("--a2-storm", type=int, default=50)
    p.add_argument("--a2-interval", type=float, default=0.02)
    p.add_argument("--no-sweep", action="store_true")
    p.add_argument("--telemetry-only", action="store_true")
    p.add_argument("--allow-reboot", action="store_true")
    p.set_defaults(func=cmd_dryrun)

    p = sub.add_parser("report", help="regenerate report.md from summary.json")
    p.add_argument("--run-dir", required=True)
    p.add_argument("--human-answers", default=None)
    p.set_defaults(func=cmd_report)
    return parser


def main(argv: Optional[List[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())
