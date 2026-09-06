#!/usr/bin/env python3
"""PCAL6524 (13 switches + 5 LEDs YDS1-5) module telemetry reference codec.

Spec: module_telemetry/packet_specs/02_pcal6524.md
Pure stdlib. Usage: import module_pcal6524 as pcal;
    python module_pcal6524.py      # self-test + examples
"""

from __future__ import annotations

import sys

import envelope

MODULE = "pcal6524"


def data(sw: int, yds: int = 0, eth_link: int = 0, usb_active: int = 0) -> dict:
    return {"sw": int(sw), "yds": int(yds), "eth_link": int(eth_link),
            "usb_active": int(usb_active)}


def event_data(event: str, level: str = "info", detail: str = "") -> dict:
    return {"event": event, "level": level, "detail": detail}


def validate_data(d: dict) -> list[str]:
    errs: list[str] = []
    if not isinstance(d, dict):
        return ["data not an object"]
    sw = d.get("sw")
    if not isinstance(sw, int) or not (0 <= sw <= 0x3FFF):
        errs.append(f"sw out of 14-bit range: {sw!r}")
    for k, hi in (("yds", 0x1F), ("eth_link", 1), ("usb_active", 1)):
        v = d.get(k)
        if not isinstance(v, int) or not (0 <= v <= hi):
            errs.append(f"{k} out of range: {v!r}")
    return errs


def validate_event(d: dict) -> list[str]:
    errs: list[str] = []
    if d.get("event") not in ("switch_changed",):
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
    l1 = make_frame("module.data", data(0x3FFE, yds=17, eth_link=1), seq=30,
                    ts_ms=3300000)
    assert decode(l1)["data"]["sw"] == 0x3FFE
    l2 = make_frame("module.event", event_data("switch_changed",
                                               detail="sw 0x3FFE"), seq=31,
                    ts_ms=3301000)
    assert decode(l2)["data"]["event"] == "switch_changed"
    for bad in (data(-1), data(0x4000), data(0, yds=32), data(0, eth_link=2)):
        try:
            make_frame("module.data", bad, seq=1, ts_ms=1)
        except ValueError:
            pass
        else:
            raise AssertionError(f"accepted bad data {bad}")
    print("module_pcal6524 self-test OK")
    print("example:", l1)
    return 0


if __name__ == "__main__":
    sys.exit(_self_test())
