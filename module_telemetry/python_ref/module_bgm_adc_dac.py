#!/usr/bin/env python3
"""BGM_ADC_DAC (DSO sampling/freq readouts) module telemetry reference codec.

Spec: module_telemetry/packet_specs/06_bgm_adc_dac.md
Pure stdlib. Usage: import module_bgm_adc_dac as bgm;
    python module_bgm_adc_dac.py      # self-test
"""

from __future__ import annotations

import sys

import envelope

MODULE = "bgm_adc_dac"
FREQ_STATES = ("STOPPED", "WAIT_EDGE", "MEASURING", "VALID",
               "NO_SIGNAL", "OVERRANGE", "FAULT")


def data(*, freq_valid: int = 0, freq_state: int = 0, freq_millihz: int = 0,
         gate_us: int = 0, edge_count: int = 0, ref_ticks: int = 0,
         overflow_count: int = 0, last_edge_age_ms: int = 0, overrange: int = 0,
         sample_format: int = 0, sample_rate: int = 0,
         overrun_count: int = 0) -> dict:
    return {"freq_valid": int(freq_valid), "freq_state": int(freq_state),
            "freq_millihz": int(freq_millihz), "gate_us": int(gate_us),
            "edge_count": int(edge_count), "ref_ticks": int(ref_ticks),
            "overflow_count": int(overflow_count),
            "last_edge_age_ms": int(last_edge_age_ms),
            "overrange": int(overrange), "sample_format": int(sample_format),
            "sample_rate": int(sample_rate), "overrun_count": int(overrun_count)}


def validate_data(d: dict) -> list[str]:
    errs: list[str] = []
    if not isinstance(d, dict):
        return ["data not an object"]
    for k in ("freq_valid", "overrange"):
        if d.get(k) not in (0, 1):
            errs.append(f"{k} must be 0/1")
    st = d.get("freq_state")
    if not isinstance(st, int) or not (0 <= st < len(FREQ_STATES)):
        errs.append(f"freq_state out of range: {st!r}")
    for k in ("freq_millihz", "gate_us", "edge_count", "ref_ticks",
              "overflow_count", "last_edge_age_ms", "sample_format",
              "sample_rate", "overrun_count"):
        v = d.get(k)
        if not isinstance(v, int) or v < 0:
            errs.append(f"{k} must be non-negative int")
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
    l1 = make_frame(data(freq_valid=1, freq_state=3, freq_millihz=1_000_000,
                         gate_us=100_000, edge_count=100_000, ref_ticks=200_000,
                         sample_format=3, sample_rate=100_000_000),
                    seq=50, ts_ms=5100000)
    assert decode(l1)["data"]["freq_millihz"] == 1_000_000
    for bad in (data(freq_valid=2), data(freq_state=9), data(gate_us=-1)):
        try:
            make_frame(bad, seq=1, ts_ms=1)
        except ValueError:
            pass
        else:
            raise AssertionError(f"accepted bad data {bad}")
    print("module_bgm_adc_dac self-test OK")
    print("example:", l1)
    return 0


if __name__ == "__main__":
    sys.exit(_self_test())
