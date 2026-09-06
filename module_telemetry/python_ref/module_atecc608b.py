#!/usr/bin/env python3
"""ATECC608B (crypto chip) module telemetry reference codec.

Spec: module_telemetry/packet_specs/01_atecc608b.md
Envelope: module_telemetry/packet_specs/00_envelope_framework.md

Pure stdlib. Builds/validates the module-1 frames:

    module.data  — periodic chip/login status
    module.event — tamper / login events

Usage:
    import module_atecc608b as atecc
    line = atecc.make_frame("module.data", atecc.status_data(...), seq=1, ts_ms=1000)
    frame = atecc.decode(line)          # routed + schema-checked
    python module_atecc608b.py          # prints example frames + self-test
"""

from __future__ import annotations

import sys

import envelope

MODULE = "atecc608b"

LOGIN_STATES = ("LOGGED_OUT", "CHALLENGE", "LOGGED_IN")
EVENT_KINDS = ("tamper", "login")
LEVELS = ("info", "warning", "error")


# ---- data schema 构造 -------------------------------------------------------

def chip_info(serial4: str = "", part: str = "608B", rev: int = 0,
              lock_config: int = 1, lock_data: int = 1) -> dict:
    return {"serial4": serial4, "part": part, "rev": rev,
            "lock_config": lock_config, "lock_data": lock_data}


def status_data(present: int = 1, comm_ok: int = 1,
                info: dict | None = None,
                login_state: str = "LOGGED_OUT", fail_cnt: int = 0) -> dict:
    d: dict = {"present": int(present), "comm_ok": int(comm_ok),
               "login_state": login_state, "fail_cnt": int(fail_cnt)}
    if info is not None:          # 首次成功采集后才携带
        d["chip_info"] = info
    return d


def event_data(event: str, level: str = "warning", detail: str = "") -> dict:
    return {"event": event, "level": level, "detail": detail}


# ---- schema 校验 ------------------------------------------------------------

def validate_status(d: dict) -> list[str]:
    errs: list[str] = []
    if not isinstance(d, dict):
        return ["status data not an object"]
    for k in ("present", "comm_ok"):
        if k not in d or d[k] not in (0, 1):
            errs.append(f"{k} must be 0/1")
    if d.get("login_state") not in LOGIN_STATES:
        errs.append(f"bad login_state {d.get('login_state')!r}")
    if not isinstance(d.get("fail_cnt", 0), int):
        errs.append("fail_cnt must be int")
    ci = d.get("chip_info")
    if ci is not None:
        if not isinstance(ci, dict):
            errs.append("chip_info must be object")
        else:
            if not (isinstance(ci.get("serial4"), str)
                    and len(ci["serial4"]) <= 8 and all(
                        c in "0123456789abcdef" for c in ci["serial4"])):
                errs.append("chip_info.serial4 must be ≤8 lowercase hex")
            for k in ("lock_config", "lock_data"):
                if ci.get(k) not in (0, 1):
                    errs.append(f"chip_info.{k} must be 0/1")
    return errs


def validate_event(d: dict) -> list[str]:
    errs: list[str] = []
    if d.get("event") not in EVENT_KINDS:
        errs.append(f"bad event {d.get('event')!r}")
    if d.get("level") not in LEVELS:
        errs.append(f"bad level {d.get('level')!r}")
    if not isinstance(d.get("detail", ""), str) or len(d.get("detail", "")) > 200:
        errs.append("detail must be string ≤200")
    return errs


# ---- 帧级入口 ---------------------------------------------------------------

def make_frame(type_: str, data: dict, seq: int, ts_ms: int) -> str:
    """Build one complete ATECC608B frame line (no CRLF)."""
    if type_ == "module.data":
        errs = validate_status(data)
    elif type_ == "module.event":
        errs = validate_event(data)
    else:
        errs = [f"type {type_} not supported for {MODULE}"]
    if errs:
        raise ValueError("; ".join(errs))
    return envelope.build(MODULE, type_, data, seq=seq, ts_ms=ts_ms)


def decode(line: str) -> dict:
    """Parse + route + schema-check one frame; returns the data dict.
    Raises ValueError on envelope or module-schema violations."""
    frame = envelope.parse(line)
    if frame["module"] != MODULE:
        raise ValueError(f"not a {MODULE} frame: {frame['module']}")
    d = frame["data"]
    if frame["type"] == "module.data":
        errs = validate_status(d)
    elif frame["type"] == "module.event":
        errs = validate_event(d)
    else:
        errs = [f"unsupported type {frame['type']}"]
    if errs:
        raise ValueError("; ".join(errs))
    return frame


def _self_test() -> int:
    s = status_data(1, 1, chip_info("a2c4f16d"), "LOGGED_OUT", 0)
    line = make_frame("module.data", s, seq=42, ts_ms=1234567)
    frame = decode(line)
    assert frame["data"]["chip_info"]["lock_config"] == 1

    e = event_data("tamper", "warning", "i2c read fail x5")
    line2 = make_frame("module.event", e, seq=88, ts_ms=1290300)
    assert decode(line2)["data"]["event"] == "tamper"

    # 坏 schema 拒绝
    for bad_d in (status_data(login_state="BOGUS"),
                  status_data(info={"serial4": "ZZZZ"}),
                  event_data("mystery")):
        try:
            make_frame("module.data" if bad_d.get("event") is None else "module.event",
                       bad_d, seq=1, ts_ms=1)
        except ValueError:
            pass
        else:
            raise AssertionError(f"bad data accepted: {bad_d}")

    print("module_atecc608b self-test OK")
    print("example module.data :", line)
    print("example module.event:", line2)
    return 0


if __name__ == "__main__":
    sys.exit(_self_test())
