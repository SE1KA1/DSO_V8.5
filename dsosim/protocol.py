"""
JSON protocol layer (接口协议 v0.9).

Serializes telemetry frames and deserializes control commands. Field
names and semantics are frozen to match the real firmware's JSON schema —
do NOT rename or retype fields without bumping the protocol version.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field

PROTOCOL_VERSION = "0.9"


@dataclass
class TelemetryFrame:
    """One outgoing telemetry frame (DSO → PC/Cloud)."""
    seq: int = 0
    ts: int = 0
    src: str = "DSO"
    state: str = "LOCAL"
    ch1: dict = field(default_factory=dict)
    ch2: dict = field(default_factory=dict)
    tb: dict = field(default_factory=dict)
    trig: dict = field(default_factory=dict)
    awg: dict = field(default_factory=dict)
    status: dict = field(default_factory=dict)
    freq_meter: dict = field(default_factory=dict)
    sys: dict = field(default_factory=dict)

    def to_json(self) -> str:
        payload = {
            "ver": PROTOCOL_VERSION,
            "seq": self.seq,
            "ts": self.ts,
            "src": self.src,
            "state": self.state,
            "ch1": self.ch1,
            "ch2": self.ch2,
            "tb": self.tb,
            "trig": self.trig,
            "awg": self.awg,
            "status": self.status,
            "freq_meter": self.freq_meter,
            "sys": self.sys,
        }
        return json.dumps(payload)

    def to_dict(self) -> dict:
        return json.loads(self.to_json())


@dataclass
class ControlCommand:
    """One incoming control command (PC/Cloud → DSO)."""
    cmd: str = ""
    params: dict = field(default_factory=dict)
    seq: int = 0

    @classmethod
    def from_json(cls, line: str) -> "ControlCommand":
        obj = json.loads(line)
        return cls(cmd=obj.get("cmd", ""),
                   params=obj.get("params", {}),
                   seq=obj.get("seq", 0))


class AckFrame:
    """Control-command acknowledgement (DSO → PC/Cloud)."""

    def __init__(self, seq: int, cmd: str, result: str = "ok",
                 error: str = "", data: dict | None = None):
        self.seq = seq
        self.cmd = cmd
        self.result = result
        self.error = error
        self.data = data

    def to_json(self) -> str:
        payload = {
            "ver": PROTOCOL_VERSION,
            "type": "ack",
            "seq": self.seq,
            "cmd": self.cmd,
            "result": self.result,
            "error": self.error,
        }
        if self.data is not None:
            payload["data"] = self.data
        return json.dumps(payload)
