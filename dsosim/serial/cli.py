"""
CH340 serial adapter CLI.

Commands:
  list                      enumerate CH340 ports
  monitor <port> [-b B]     print text lines / binary frame stats until Ctrl-C
  log <port> -o FILE [-b B] capture with JSONL saving
  send <port> "<text>"      send one text line
  reset-reason <port> [-d S] capture boot log, infer reset reason
  rescue <port> <name>      send a rescue preset and show response
"""

from __future__ import annotations

import argparse
import sys
import time

from .adapter import CH340Adapter, discover_ch340, DEFAULT_BAUD
from .rescue import RESCUE_COMMANDS


def _pick_port(name: str | None) -> str:
    ports = discover_ch340()
    if not ports:
        print("no CH340 adapter found")
        raise SystemExit(1)
    if name is None:
        if len(ports) == 1:
            return ports[0]["port"]
        print("multiple adapters; choose one:")
        for p in ports:
            print(f"  {p['port']}  {p['description']}")
        raise SystemExit(1)
    if name not in {p["port"] for p in ports}:
        print(f"port {name} not found among: {[p['port'] for p in ports]}")
        raise SystemExit(1)
    return name


def _open(port: str, baud: int) -> CH340Adapter:
    a = CH340Adapter(port, baudrate=baud)
    a.open()
    print(f"[ch340] open {port} @ {baud}")
    return a


def cmd_list(_args) -> int:
    ports = discover_ch340()
    if not ports:
        print("no CH340 adapter found")
        return 0
    for p in ports:
        print(f"{p['port']}  vid={p['vid']:#06x} pid={p['pid']:#06x}  {p['description']}")
    return 0


def cmd_monitor(args) -> int:
    port = _pick_port(args.port)
    a = _open(port, args.baud)
    nbin = 0
    try:
        while True:
            line = a.read_line(timeout=0.2)
            if line is not None:
                print(f"{line.ts_ms:>12} | {line.text}")
            frame = a.read_binary_frame(timeout=0.0)
            if frame is not None:
                nbin += 1
                print(f"{frame.ts_ms:>12} | [BIN] DSOW frame {len(frame.data)}B "
                      f"(total {nbin})")
    except KeyboardInterrupt:
        pass
    finally:
        a.close()
    return 0


def cmd_log(args) -> int:
    port = _pick_port(args.port)
    a = _open(port, args.baud)
    a.start_logging(args.output)
    try:
        print(f"[ch340] saving to {args.output} — Ctrl-C to stop")
        while True:
            line = a.read_line(timeout=0.2)
            if line is not None:
                print(f"{line.ts_ms:>12} | {line.text}")
            frame = a.read_binary_frame(timeout=0.0)
            if frame is not None:
                print(f"{frame.ts_ms:>12} | [BIN] {len(frame.data)}B")
    except KeyboardInterrupt:
        pass
    finally:
        a.stop_logging()
        a.close()
    print(f"[ch340] log saved")
    return 0


def cmd_send(args) -> int:
    port = _pick_port(args.port)
    a = _open(port, args.baud)
    try:
        n = a.write_text(args.text)
        print(f"[ch340] sent {n}B: {args.text!r}")
        # brief capture of the response
        deadline = time.monotonic() + args.wait
        while time.monotonic() < deadline:
            line = a.read_line(timeout=0.2)
            if line is not None:
                print(f"  {line.text}")
    finally:
        a.close()
    return 0


def cmd_reset_reason(args) -> int:
    port = _pick_port(args.port)
    a = _open(port, args.baud)
    try:
        lines = a.capture_boot_log(duration=args.duration)
        for l in lines:
            print(f"  {l}")
        r = parse_reset_reason(lines)
        print(f"reset reason: {r['reason']} "
              f"(conf={r['confidence']:.2f}, matched={r['matched']!r})")
    finally:
        a.close()
    return 0


def cmd_rescue(args) -> int:
    if args.name not in RESCUE_COMMANDS:
        print(f"unknown rescue command {args.name!r}; available: "
              f"{', '.join(rescue_commands())}")
        return 1
    desc, cmd = RESCUE_COMMANDS[args.name]
    print(f"[ch340] rescue {args.name} ({desc}): send {cmd!r}")
    port = _pick_port(args.port)
    a = _open(port, args.baud)
    try:
        a.write_text(cmd)
        deadline = time.monotonic() + args.wait
        while time.monotonic() < deadline:
            line = a.read_line(timeout=0.2)
            if line is not None:
                print(f"  {line.text}")
    finally:
        a.close()
    return 0


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="ch340", description="CH340 serial adapter CLI")
    sub = p.add_subparsers(dest="cmd", required=True)

    sub.add_parser("list", help="list CH340 adapters")

    for name in ("monitor", "log", "send", "reset-reason", "rescue"):
        sp = sub.add_parser(name)
        sp.add_argument("port", nargs="?", default=None)
        sp.add_argument("-b", "--baud", type=int, default=DEFAULT_BAUD)

    sp = sub.choices["log"]
    sp.add_argument("-o", "--output", required=True)
    sp = sub.choices["send"]
    sp.add_argument("text")
    sp.add_argument("-w", "--wait", type=float, default=2.0)
    sp = sub.choices["reset-reason"]
    sp.add_argument("-d", "--duration", type=float, default=2.0)
    sp = sub.choices["rescue"]
    sp.add_argument("name")
    sp.add_argument("-w", "--wait", type=float, default=2.0)

    args = p.parse_args(argv)
    handlers = {
        "list": cmd_list,
        "monitor": cmd_monitor,
        "log": cmd_log,
        "send": cmd_send,
        "reset-reason": cmd_reset_reason,
        "rescue": cmd_rescue,
    }
    return handlers[args.cmd](args)


if __name__ == "__main__":
    sys.exit(main())
