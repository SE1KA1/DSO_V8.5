#!/usr/bin/env python3
"""U44-INA226 (core-board current) module telemetry reference codec.

Spec: module_telemetry/packet_specs/05_ina226_u44.md
Format identical to module_ina226_u10, only module id differs (0x40 / 20mΩ).
Pure stdlib. Usage: import module_ina226_u44 as ina44;
    python module_ina226_u44.py      # self-test
"""

from __future__ import annotations

import sys

import envelope

MODULE = "ina226_u44"

EVENTS = ("overcurrent", "recovered", "comm_lost")


def data(cur_ma: int, over_cur: int = 0, valid: int = 1,
         level: int = 0) -> dict:
    return {"cur_ma": int(cur_ma), "over_cur": int(over_cur),
            "valid": int(valid), "level": int(level)}


def event_data(event: str, level: str = "warning", detail: str = "") -> dict:
    return {"event": event, "level": level, "detail": detail}


def validate_data(d: dict) -> list[str]:
    errs: list[str] = []
    if not isinstance(d, dict):
        return ["data not an object"]
    c = d.get("cur_ma")
    if not isinstance(c, int) or not (-20000 <= c <= 20000):
        errs.append(f"cur_ma out of range: {c!r}")
    for k in ("over_cur", "valid"):
        if d.get(k) not in (0, 1):
            errs.append(f"{k} must be 0/1")
    lv = d.get("level")
    if not isinstance(lv, int) or not (0 <= lv <= 3):
        errs.append(f"level out of range: {lv!r}")
    return errs


def validate_event(d: dict) -> list[str]:
    errs: list[str] = []
    if d.get("event") not in EVENTS:
        errs.append(f"bad event {d.get('event')!r}")
    if d.get("level") not in ("info", "warning", "error"):
        errs.append("bad level")
    return errs


def make_frame(type_: str, data_: dict, seq: int, ts_ms: int) -> str:
    errs = validate_data(data_) if type_ == "module.data" else \
           validate_event(data_) if type_ == "module.event" else \
           [f"unsupported type {type_}"]
    if errs:
        raise ValueError("; ".join(errs))
    return envelope.build(MODULE, type_, data_, seq=seq, ts_ms=ts_ms)


def decode(line: str) -> dict:
    frame = envelope.parse(line)
    if frame["module"] != MODULE:
        raise ValueError(f"not a {MODULE} frame")
    errs = validate_data(frame["data"]) if frame["type"] == "module.data" \
        else validate_event(frame["data"]) if frame["type"] == "module.event" \
        else [f"unsupported type {frame['type']}"]
    if errs:
        raise ValueError("; ".join(errs))
    return frame


def _self_test() -> int:
    l1 = make_frame("module.data", data(125), seq=40, ts_ms=4200000)
    assert decode(l1)["data"]["level"] == 0
    assert decode(l1)["data"]["cur_ma"] == 125
    l2 = make_frame("module.event", event_data("overcurrent"), seq=41, ts_ms=1)
    assert decode(l2)["data"]["event"] == "overcurrent"
    for bad in (data(30000), data(0, over_cur=7)):
        try:
            make_frame("module.data", bad, seq=1, ts_ms=1)
        except ValueError:
            pass
        else:
            raise AssertionError(f"accepted bad data {bad}")
    print("module_ina226_u44 self-test OK")
    print("example:", l1)
    return 0


if __name__ == "__main__":
    sys.exit(_self_test())
