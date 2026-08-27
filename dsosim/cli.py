"""
Command-line entry for the device simulator.

Modes:
    --server         run a TCP server on port 5005 (line-delimited JSON)
    --interactive    run an interactive text session
    --dump           print a single telemetry frame as JSON and exit
"""

from __future__ import annotations

import argparse
import json
import time

from .device import DSOSimulator
from .server import SimulatorServer


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="dsosim", description="DSO_V8.5 simulator v1.0")
    p.add_argument("--server", action="store_true", help="run TCP server on 5005")
    p.add_argument("--port", type=int, default=5005)
    p.add_argument("--interactive", action="store_true")
    p.add_argument("--dump", action="store_true", help="print one frame as JSON")
    p.add_argument("--seed", type=int, default=None)
    args = p.parse_args(argv)

    dev = DSOSimulator(seed=args.seed)

    if args.dump:
        print(dev.read_frame().to_json())
        return 0

    if args.server:
        srv = SimulatorServer(dev, port=args.port)
        srv.start()
        print(f"dsosim server listening on :{args.port}")
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            srv.stop()
        return 0

    if args.interactive:
        print("dsosim interactive — type 'help' for commands, Ctrl-D to exit")
        while True:
            try:
                raw = input("dsosim> ").strip()
            except (EOFError, KeyboardInterrupt):
                print()
                return 0
            if not raw:
                continue
            if raw == "help":
                print("  wave <ch> <sine|square|noise|triangle> <freq_hz> [ampl_v]")
                print("  vdiv <ch> <mv_per_div>")
                print("  run <0|1>          — RUN/STOP")
                print("  frame              — print one JSON frame")
                print("  state <SAFE|LOCAL|PC|CLOUD|DEBUG|CONFIG|RECOVERY>")
                print("  err <E001..E009>   — inject error")
                print("  quit")
                continue
            parts = raw.split()
            try:
                if parts[0] == "wave":
                    ch = int(parts[1]); kind = parts[2]; freq = float(parts[3])
                    amp = float(parts[4]) if len(parts) > 4 else 1.0
                    dev.set_waveform(ch, kind, freq_hz=freq, amplitude_v=amp)
                elif parts[0] == "vdiv":
                    dev.set_vdiv(int(parts[1]), int(parts[2]))
                elif parts[0] == "run":
                    dev.running = bool(int(parts[1]))
                elif parts[0] == "frame":
                    print(dev.read_frame().to_json())
                elif parts[0] == "state":
                    dev.set_state(parts[1])
                elif parts[0] == "err":
                    from .errors import DSOErrorCode
                    dev.inject_error(DSOErrorCode(parts[1]))
                elif parts[0] == "quit":
                    return 0
            except (IndexError, ValueError):
                print("bad command")
        return 0

    p.print_help()
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
