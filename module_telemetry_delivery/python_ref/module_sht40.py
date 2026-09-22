#!/usr/bin/env python3
"""SHT40 (temp sensor) module telemetry reference codec.

Spec: module_telemetry/packet_specs/03_sht40.md
Pure stdlib. Usage: import module_sht40 as sht40;
    line = sht40.make_frame("module.data", sht40.data(temp_dezi=245), seq=1, ts_ms=0)
    python module_sht40.py      # self-test + examples
"""

from __future__ import annotations

import sys

import envelope

MODULE = "sht40"


def data(temp_dezi: int, rh_centi: int = 0, temp_ok: int = 1,
         hi_alarm: int = 0, level: int = 0) -> dict:
    return {"temp_dezi": int(temp_dezi), "rh_centi": int(rh_centi),
            "temp_ok": int(temp_ok), "hi_alarm": int(hi_alarm),
            "level": int(level)}


def event_data(event: str, level: str = "warning", detail: str = "") -> dict:
    return {"event": event, "level": level, "detail": detail}


def validate_data(d: dict) -> list[str]:
    errs: list[str] = []
    if not isinstance(d, dict):
        return ["data not an object"]
    t = d.get("temp_dezi")
    if not isinstance(t, int) or not (-4000 <= t <= 1250):
        errs.append(f"temp_dezi out of range: {t!r}")
    rh = d.get("rh_centi")
    if not isinstance(rh, int) or not (0 <= rh <= 10000):
        errs.append(f"rh_centi out of range: {rh!r}")
    for k in ("temp_ok", "hi_alarm"):
        if d.get(k) not in (0, 1):
            errs.append(f"{k} must be 0/1")
    lv = d.get("level")
    if not isinstance(lv, int) or not (0 <= lv <= 3):
        errs.append(f"level out of range: {lv!r}")
    return errs


def validate_event(d: dict) -> list[str]:
    errs: list[str] = []
    if d.get("event") not in ("sensor_lost",):
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
    l1 = make_frame("module.data", data(245, rh_centi=4523), seq=10, ts_ms=1500000)
    assert decode(l1)["data"]["level"] == 0
    assert decode(l1)["data"]["temp_dezi"] == 245
    assert decode(l1)["data"]["rh_centi"] == 4523
    l2 = make_frame("module.event", event_data("sensor_lost"), seq=11, ts_ms=1)
    assert decode(l2)["data"]["event"] == "sensor_lost"
    for bad in (data(99999), data(245, rh_centi=10001), data(245, temp_ok=2),
                event_data("mystery")):
        try:
            make_frame("module.data" if "temp_dezi" in bad else "module.event",
                       bad, seq=1, ts_ms=1)
        except ValueError:
            pass
        else:
            raise AssertionError(f"accepted bad data {bad}")
    print("module_sht40 self-test OK")
    print("example:", l1)
    return 0


if __name__ == "__main__":
    sys.exit(_self_test())
