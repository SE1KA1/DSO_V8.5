#!/usr/bin/env python3
"""Raw UDP bandwidth/loss test: PC sends UDP to the DSO, the DSO counts and reports.

Direction: PC → STM32:5006 (UDP). The STM32's UDP_RX_Task counts every datagram and
sends back a status line "UDP:<count>:<bytes>" every 1 s; the client compares its
own sent count against the STM32's received count to derive loss and throughput.

Usage:
    python udp_throughput.py --host 192.168.100.50 --port 5006 \
        --size 1472 --rate 2000 --duration 10
    python udp_throughput.py --host 192.168.100.50 --size 1472 --sweep
    python udp_throughput.py --self-test
"""

from __future__ import annotations

import argparse
import socket
import sys
import threading
import time

STATUS_PREFIX = b"UDP:"


def parse_status(data: bytes):
    """b'UDP:<count>:<bytes>' → (count, bytes) or None."""
    parts = data.strip().split(b":")
    if len(parts) == 3 and parts[0] == b"UDP":
        try:
            return int(parts[1]), int(parts[2])
        except ValueError:
            return None
    return None


def run_case(host: str, port: int, size: int, rate: int, duration: float):
    """Send UDP to the DSO at `rate` pps for `duration`; return stats dict.

    Sends "RST" first to reset the DSO's cumulative counter, drains stale
    status, then runs the burst; the final status gives the received count.
    """
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("0.0.0.0", 0))
    payload = b"\xA5" * size
    interval = 1.0 / rate if rate > 0 else 0.0
    sent = 0
    st_count = 0
    st_bytes = 0

    # reset the DSO counter, then drain for ~1.2 s to get a clean base
    s.sendto(b"RST", (host, port))
    drain_deadline = time.monotonic() + 1.2
    while time.monotonic() < drain_deadline:
        s.settimeout(0.2)
        try:
            d, _ = s.recvfrom(256)
        except socket.timeout:
            break
        st = parse_status(d)
        if st:
            st_count, st_bytes = st

    t0 = time.monotonic()
    t_end = t0 + duration
    deadline = t_end + 1.0  # +1 s to flush the final status

    while True:
        now = time.monotonic()
        if now >= deadline:
            break
        # pacing
        if interval > 0:
            nxt = t0 + (sent + 1) * interval
            if now < nxt:
                s.settimeout(min(0.05, max(0.0, nxt - now)))
                try:
                    d, _ = s.recvfrom(256)
                except socket.timeout:
                    pass
                else:
                    st = parse_status(d)
                    if st:
                        st_count, st_bytes = st
                time.sleep(max(0.0, nxt - time.monotonic()))
                continue
            s.sendto(payload, (host, port))
            sent += 1
        else:
            # max rate: blast and drain status in the same loop
            try:
                s.sendto(payload, (host, port))
                sent += 1
            except OSError:
                pass
            s.settimeout(0.0)
            try:
                while True:
                    d, _ = s.recvfrom(256)
                    st = parse_status(d)
                    if st:
                        st_count, st_bytes = st
            except socket.timeout:
                pass
            time.sleep(0.002)

    # 结束：发 GET 让 STM32 立即回当前计数（精确到用例边界）
    s.sendto(b"GET", (host, port))
    s.settimeout(0.5)
    for _ in range(4):
        try:
            d, _ = s.recvfrom(256)
        except socket.timeout:
            break
        st = parse_status(d)
        if st:
            st_count, st_bytes = st
    s.close()

    elapsed = max(1e-9, time.monotonic() - t0)
    loss = (sent - st_count) / sent * 100 if sent else 0.0
    rate_mbps = st_bytes * 8.0 / elapsed / 1e6
    return {
        "size": size, "rate": rate, "sent": sent,
        "recv": st_count, "bytes": st_bytes,
        "loss_pct": loss, "mbps": rate_mbps, "sec": elapsed,
    }


def fmt(st: dict) -> str:
    return (f"size={st['size']}B rate={st['rate']:>6}/s "
            f"sent={st['sent']} recv={st['recv']} loss={st['loss_pct']:6.2f}% "
            f"throughput={st['mbps']:7.2f}Mbps ({st['sec']:.1f}s)")


def run_matrix(host, port, sizes, rates, duration):
    print(f"\n== UDP test  {host}:{port}  sizes={sizes}  rates={rates} ==")
    for size in sizes:
        for rate in rates:
            st = run_case(host, port, size, rate, duration)
            print("  " + fmt(st))


def sweep(host, port, size, duration, max_loss=5.0):
    print(f"\n== UDP sweep  {host}:{port}  size={size}B  (stop when loss > {max_loss}%) ==")
    rates = [100, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000]
    for rate in rates:
        st = run_case(host, port, size, rate, duration)
        print("  " + fmt(st))
        if st["loss_pct"] > max_loss:
            print(f"  >> loss exceeded {max_loss}% at {rate}/s — upper limit found")
            break


# ---------------------------------------------------------------------------
# 离线自测：本地模拟 STM32 接收端（计数 + 每秒回传状态）
# ---------------------------------------------------------------------------

def _mock_stm32(port: int, stop: threading.Event) -> None:
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", port))
    count = 0
    total = 0
    last = time.monotonic()
    s.settimeout(0.2)
    while not stop.is_set():
        try:
            d, addr = s.recvfrom(2048)
        except socket.timeout:
            d = None
        if d:
            if d.startswith(b"RST"):
                count = 0
                total = 0
            elif d.startswith(b"GET"):
                s.sendto(f"UDP:{count}:{total}\r\n".encode(), addr)
            else:
                count += 1
                total += len(d)
        if time.monotonic() - last >= 1.0:
            last = time.monotonic()
            s.sendto(f"UDP:{count}:{total}\r\n".encode(), addr if d else ("127.0.0.1", port))
    s.close()


def self_test() -> int:
    port = 5506
    stop = threading.Event()
    th = threading.Thread(target=_mock_stm32, args=(port, stop), daemon=True)
    th.start()
    try:
        run_matrix("127.0.0.1", port, [500, 1472], [100, 1000], 2)
    finally:
        stop.set()
    print("SELF-TEST OK")
    return 0


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description="Raw UDP bandwidth/loss test vs DSO")
    p.add_argument("--host", default="192.168.100.50")
    p.add_argument("--port", type=int, default=5006)
    p.add_argument("--size", type=int, default=1472, help="payload bytes per datagram (max 1472)")
    p.add_argument("--rate", type=int, default=0, help="target datagrams/sec (0 = max rate)")
    p.add_argument("--duration", type=float, default=5.0, help="test duration in seconds")
    p.add_argument("--sizes", default="", help="comma list of sizes for matrix (overrides --size)")
    p.add_argument("--rates", default="", help="comma list of rates for matrix")
    p.add_argument("--sweep", action="store_true", help="sweep rate upward until loss")
    p.add_argument("--self-test", action="store_true", help="offline loopback self-test")
    args = p.parse_args(argv)

    if args.self_test:
        return self_test()

    if args.sweep:
        sweep(args.host, args.port, args.size, args.duration)
        return 0

    sizes = [int(x) for x in args.sizes.split(",") if x] or [args.size]
    rates = [int(x) for x in args.rates.split(",") if x] or [args.rate]
    run_matrix(args.host, args.port, sizes, rates, args.duration)
    return 0


if __name__ == "__main__":
    sys.exit(main())
