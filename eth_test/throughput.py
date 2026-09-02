#!/usr/bin/env python3
"""Ethernet throughput & packet-loss test against the DSO echo endpoint.

Protocol (STM32 回显模式):
    send    0xE0 0xA5 | <len:4 LE> | <payload len bytes>
    echo    STM32 原样回显整帧

Metrics per (size, repeats): sent / received / ok / corrupt / timeout,
loss %, round-trip throughput (2 * size / elapsed).

Usage:
    python throughput.py --host 192.168.100.50 --port 5005 \
        --sizes 0.5,1,2,5,10,20,30,50 --repeats 10,20,50,100
    python throughput.py --self-test          # 本地回环自测（离线）
"""

from __future__ import annotations

import argparse
import socket
import sys
import threading
import time

MAGIC = b"\xE0\xA5"
DEFAULT_SIZES = [0.5, 1, 2, 5, 10, 20, 30, 50]
DEFAULT_REPEATS = [10, 20, 50, 100]


def build_payload(size: int) -> bytes:
    """可校验载荷：251 字节序列重复填充。"""
    pattern = bytes(i % 251 for i in range(1024))
    reps, rem = divmod(size, len(pattern))
    return pattern * reps + pattern[:rem]


def recv_exact(sock: socket.socket, n: int, deadline: float) -> bytes | None:
    """在 deadline 前收满 n 字节；超时/断连返回 None。"""
    chunks: list[bytes] = []
    remaining = n
    while remaining > 0:
        wait = deadline - time.monotonic()
        if wait <= 0:
            return None
        sock.settimeout(max(0.1, wait))
        try:
            d = sock.recv(min(65536, remaining))
        except socket.timeout:
            return None
        if not d:
            return None
        chunks.append(d)
        remaining -= len(d)
    return b"".join(chunks)


class CaseStats:
    def __init__(self, size_mb: float) -> None:
        self.size_mb = size_mb
        self.sent = 0
        self.ok = 0
        self.corrupt = 0
        self.timeout = 0
        self.elapsed_sum = 0.0

    @property
    def loss_rate(self) -> float:
        if self.sent == 0:
            return 0.0
        return 100.0 * (self.sent - self.ok) / self.sent

    @property
    def rate_mbs(self) -> float:
        if self.elapsed_sum <= 0:
            return 0.0
        # 回环总量 = 2 × size（发送 + 回显）
        bytes_total = 2.0 * self.sent * self.size_mb * 1024 * 1024
        return bytes_total / self.elapsed_sum / (1024 * 1024)


def run_case(host: str, port: int, size_mb: float, repeats: int,
             timeout: float, verbose: bool) -> CaseStats:
    size = int(size_mb * 1024 * 1024)
    frame = MAGIC + size.to_bytes(4, "little") + build_payload(size)
    stats = CaseStats(size_mb)

    try:
        with socket.create_connection((host, port), timeout=10) as s:
            s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            for r in range(1, repeats + 1):
                t0 = time.perf_counter()
                s.sendall(frame)
                echo = recv_exact(s, len(frame), time.monotonic() + timeout)
                t1 = time.perf_counter()
                stats.sent += 1
                stats.elapsed_sum += t1 - t0
                if echo is None:
                    stats.timeout += 1
                    if verbose:
                        print(f"    [{r}/{repeats}] {size_mb}MB: TIMEOUT")
                elif echo == frame:
                    stats.ok += 1
                    if verbose:
                        print(f"    [{r}/{repeats}] {size_mb}MB: OK  "
                              f"{t1 - t0:.3f}s  {2 * size / (t1 - t0) / 1e6:.2f}MB/s")
                else:
                    stats.corrupt += 1
                    if verbose:
                        first = min(len(echo), len(frame))
                        j = 0
                        while j < first and echo[j] == frame[j]:
                            j += 1
                        print(f"    [{r}/{repeats}] {size_mb}MB: CORRUPT "
                              f"(echo_len={len(echo)} expect={len(frame)} "
                              f"first_diff={j} echo[{j}]={echo[j]:02x} "
                              f"frame[{j}]={frame[j]:02x})")
    except OSError as e:
        print(f"  {size_mb:>5}MB: connection error after "
              f"{stats.sent}/{repeats} packets: {e}")

    return stats


def fmt_stats(st: CaseStats) -> str:
    return (f"{st.rate_mbs:7.2f}MB/s  loss={st.loss_rate:4.1f}%  "
            f"(sent={st.sent} ok={st.ok} corrupt={st.corrupt} timeout={st.timeout})")


def run_matrix(host: str, port: int, sizes, repeats, timeout, verbose) -> None:
    print(f"\n== Ethernet test  {host}:{port}  sizes={sizes}MB  repeats={repeats} ==")
    print(f"{'size':>6} | " + " | ".join(f"rep={r}" for r in repeats))
    print("-" * (8 + 10 * len(repeats)))
    for size in sizes:
        cells = []
        detail = []
        for rep in repeats:
            t = timeout if timeout > 0 else max(15.0, size * 20.0)
            st = run_case(host, port, size, rep, t, verbose)
            cells.append(f"{st.rate_mbs:6.2f}MB/s {st.loss_rate:3.0f}%")
            detail.append(f"  {size}MB x{rep}: " + fmt_stats(st))
        print(f"{size:>5}MB | " + " | ".join(cells))
        for d in detail:
            print(d)
    print()


# ---------------------------------------------------------------------------
# 离线自测：本地回显服务器
# ---------------------------------------------------------------------------

def _echo_handle(conn: socket.socket) -> None:
    try:
        while True:
            d = conn.recv(65536)
            if not d:
                break
            conn.sendall(d)   # 原样回显
    finally:
        conn.close()


def _echo_server(port: int, stop: threading.Event) -> None:
    srv = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", port))
    srv.listen(5)
    while not stop.is_set():
        try:
            conn, _ = srv.accept()
        except OSError:
            break
        threading.Thread(target=_echo_handle, args=(conn,), daemon=True).start()
    srv.close()


def self_test() -> int:
    port = 5505
    stop = threading.Event()
    th = threading.Thread(target=_echo_server, args=(port, stop), daemon=True)
    th.start()
    try:
        run_matrix("127.0.0.1", port, [0.5, 1], [3, 5], 30.0, verbose=True)
    finally:
        stop.set()
    print("SELF-TEST OK")
    return 0


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description="Ethernet throughput/loss test vs DSO echo")
    p.add_argument("--host", default="192.168.100.50")
    p.add_argument("--port", type=int, default=5005)
    p.add_argument("--sizes", default=",".join(str(x) for x in DEFAULT_SIZES),
                   help="comma-separated sizes in MB")
    p.add_argument("--repeats", default=",".join(str(x) for x in DEFAULT_REPEATS),
                   help="comma-separated repeat counts")
    p.add_argument("--timeout", type=float, default=0.0,
                   help="per-packet deadline in s (0 = auto: max(15, size*20))")
    p.add_argument("--self-test", action="store_true", help="offline loopback self-test")
    p.add_argument("-v", "--verbose", action="store_true")
    args = p.parse_args(argv)

    if args.self_test:
        return self_test()

    sizes = [float(x) for x in args.sizes.split(",") if x]
    repeats = [int(x) for x in args.repeats.split(",") if x]
    if not sizes or not repeats:
        p.error("sizes/repeats empty")

    print("Note: STM32 TCP 5005 is single-client — close the switch GUI first.")
    run_matrix(args.host, args.port, sizes, repeats, args.timeout, args.verbose)
    return 0


if __name__ == "__main__":
    sys.exit(main())
