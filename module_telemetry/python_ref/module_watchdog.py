#!/usr/bin/env python3
"""SP706T watchdog module telemetry reference codec.

Spec: module_telemetry/packet_specs/08_watchdog.md
Pure stdlib. Usage: import module_watchdog as wd;
    python module_watchdog.py       # self-test
"""

from __future__ import annotations

import sys

import envelope

MODULE = "watchdog"
RESET_CAUSE_BITS = {
    0x01: "LPWRRSTF", 0x02: "WWDGRSTF", 0x04: "IWDGRSTF", 0x08: "SFTRSTF",
    0x10: "PORRSTF", 0x20: "PINRSTF", 0x40: "BORRSTF",
}


def data(feed_en: int = 1, kick_count: int = 0, pfo: int = 0,
         reset_cause: int = 0, uptime_ms: int = 0) -> dict:
    return {"feed_en": int(feed_en), "kick_count": int(kick_count),
            "pfo": int(pfo), "reset_cause": int(reset_cause),
            "uptime_ms": int(uptime_ms)}


def reset_cause_names(bits: int) -> list[str]:
    return [n for m, n in RESET_CAUSE_BITS.items() if bits & m]


def validate_data(d: dict) -> list[str]:
    errs: list[str] = []
    if not isinstance(d, dict):
        return ["data not an object"]
    if d.get("feed_en") not in (0, 1):
        errs.append("feed_en must be 0/1")
    p = d.get("pfo")
    if not isinstance(p, int) or p not in (0, 1, 255):
        errs.append(f"pfo must be 0/1/255: {p!r}")
    for k in ("kick_count", "reset_cause", "uptime_ms"):
        v = d.get(k)
        if not isinstance(v, int) or v < 0:
            errs.append(f"{k} must be non-negative int")
    if not (0 <= d.get("reset_cause", 0) <= 0x7F):
        errs.append("reset_cause out of range")
    return errs


def make_frame(data_: dict, seq: int, ts_ms: int) -> str:
    errs = validate_data(data_)
    if errs:
        raise ValueError("; ".join(errs))
    return envelope.build(MODULE, "module.data", data_, seq=seq, ts_ms=ts_ms)


def decode(line: str) -> dict:
    frame = envelope.parse(line)
    if frame["module"] != MODULE:
        raise ValueError(f"not a {MODULE} frame")
    errs = validate_data(frame["data"])
    if errs:
        raise ValueError("; ".join(errs))
    return frame


def _self_test() -> int:
    l1 = make_frame(data(kick_count=12345, reset_cause=0x20), seq=60,
                    ts_ms=6200000)
    assert decode(l1)["data"]["kick_count"] == 12345
    assert reset_cause_names(0x24) == ["IWDGRSTF", "PINRSTF"]
    for bad in (data(feed_en=2), data(pfo=7), data(reset_cause=0x80)):
        try:
            make_frame(bad, seq=1, ts_ms=1)
        except ValueError:
            pass
        else:
            raise AssertionError(f"accepted bad data {bad}")
    print("module_watchdog self-test OK")
    print("example:", l1)
    return 0


if __name__ == "__main__":
    sys.exit(_self_test())
