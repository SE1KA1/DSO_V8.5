#!/usr/bin/env python3
"""Unified envelope codec for device→server module telemetry frames.

Spec: module_telemetry/packet_specs/00_envelope_framework.md

Frame (one JSON object per TCP line, CRLF terminated):
    {"ver":"1.0","type":"module.data","module":"atecc608b",
     "seq":42,"ts_ms":1234567,"data":{...}}

Pure stdlib — usable by backend colleagues without extra dependencies.

Usage:
    import envelope
    line = envelope.build("atecc608b", "module.data", data, seq=1, ts_ms=1000)
    frame = envelope.parse(line)          # raises ValueError on bad frames
    errors = envelope.validate(frame)     # [] if ok
    python envelope.py                    # self-test
"""

from __future__ import annotations

import json
import re
import sys

VER = "1.0"

# 上行新增帧类型（既有 device.status / telemetry.* / event.key / command.result 不在本模块处理）
UP_TYPES = ("module.data", "module.event")

# 模块标识：小写字母/数字/下划线，≤24 字符（与规格 00 §3 一致）
_MODULE_RE = re.compile(r"^[a-z0-9_]{1,24}$")

_REQUIRED_TOP = ("ver", "type", "module", "seq", "ts_ms", "data")


def validate(frame: dict) -> list[str]:
    """Return a list of human-readable problems; empty list means OK."""
    errs: list[str] = []
    if not isinstance(frame, dict):
        return ["frame is not an object"]
    for k in _REQUIRED_TOP:
        if k not in frame:
            errs.append(f"missing required field '{k}'")
    if "ver" in frame and frame["ver"] != VER:
        errs.append(f"unsupported ver {frame['ver']!r} (expect {VER})")
    if "type" in frame and frame["type"] not in UP_TYPES:
        errs.append(f"unknown type {frame['type']!r}")
    if "module" in frame and not _MODULE_RE.match(str(frame["module"])):
        errs.append(f"bad module id {frame['module']!r}")
    if "seq" in frame and not isinstance(frame["seq"], int):
        errs.append("seq must be int")
    if "ts_ms" in frame and not isinstance(frame["ts_ms"], int):
        errs.append("ts_ms must be int")
    if "data" in frame and not isinstance(frame["data"], dict):
        errs.append("data must be an object")
    return errs


def build(module: str, type_: str, data: dict, seq: int, ts_ms: int) -> str:
    """Build a compact one-line JSON frame string (no trailing CRLF)."""
    frame = {
        "ver": VER,
        "type": type_,
        "module": module,
        "seq": seq,
        "ts_ms": ts_ms,
        "data": data,
    }
    errs = validate(frame)
    if errs:
        raise ValueError("; ".join(errs))
    # compact separators 省字节；ensure_ascii=False 保留中文（可选）。默认 ASCII 安全
    return json.dumps(frame, separators=(",", ":"), ensure_ascii=True)


def parse(line: str) -> dict:
    """Parse one frame line; raises ValueError (bad json / bad frame)."""
    try:
        obj = json.loads(line)
    except json.JSONDecodeError as e:
        raise ValueError(f"bad json: {e}") from e
    errs = validate(obj)
    if errs:
        raise ValueError("; ".join(errs))
    return obj


def _self_test() -> int:
    frame = build("atecc608b", "module.data",
                  {"present": 1, "comm_ok": 1}, seq=42, ts_ms=1234567)
    assert '"module":"atecc608b"' in frame and '"type":"module.data"' in frame
    obj = parse(frame)
    assert obj["seq"] == 42 and obj["data"]["present"] == 1

    # 坏帧拒绝
    for bad in ("not json", '{"type":"module.data"}',
                '{"ver":"1.0","type":"module.data","module":"Atecc608B!","seq":1,"ts_ms":1,"data":{}}',
                '{"ver":"1.0","type":"module.data","module":"atecc608b","seq":"x","ts_ms":1,"data":{}}'):
        try:
            parse(bad)
        except ValueError:
            pass
        else:
            raise AssertionError(f"bad frame accepted: {bad}")

    # 未知类型/模块
    try:
        build("atecc608b!", "module.data", {}, seq=1, ts_ms=1)
    except ValueError:
        pass
    else:
        raise AssertionError("uppercase module id accepted")
    print("envelope self-test OK")
    print("example:", frame)
    return 0


if __name__ == "__main__":
    sys.exit(_self_test())
