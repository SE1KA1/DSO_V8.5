"""Pass/fail evaluators for the rack test matrix.

Every check carries the firmware source it was derived from so the report can
show measured value, threshold and provenance side by side.  ``WARN`` is
informational and does not affect the overall verdict.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional

PASS = "PASS"
FAIL = "FAIL"
WARN = "WARN"
SKIP = "SKIP"
NA = "N/A"
PENDING = "PENDING"

GOOD = (PASS, WARN, NA)


@dataclass
class CheckResult:
    id: str
    module: str
    title: str
    status: str
    measured: Dict[str, Any] = field(default_factory=dict)
    thresholds: Dict[str, Any] = field(default_factory=dict)
    detail: str = ""
    source: List[str] = field(default_factory=list)
    evidence: Dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> dict:
        return {
            "id": self.id, "module": self.module, "title": self.title,
            "status": self.status, "measured": self.measured,
            "thresholds": self.thresholds, "detail": self.detail,
            "source": list(self.source), "evidence": dict(self.evidence),
        }


def _slot(stats: dict, module: str) -> dict:
    return (stats.get("modules") or {}).get(module) or {}


def _field(stats: dict, module: str, name: str) -> Optional[dict]:
    slot = _slot(stats, module)
    return (slot.get("fields") or {}).get(name)


def _last(stats: dict, module: str) -> dict:
    return _slot(stats, module).get("last_data") or {}


def _events(stats: dict, module: str) -> Dict[str, int]:
    return (stats.get("events") or {}).get(module) or {}


def _has_frames(stats: dict, module: str) -> bool:
    return int(_slot(stats, module).get("count", 0)) > 0


_TELEMETRY_SKIP_IDS = (
    ("INT-01", "all", "baseline frame coverage"),
    ("INT-02", "all", "per-module frame gap"),
    ("INT-03", "all", "seq continuity (incl. watchdog)"),
    ("INT-04", "all", "ts_ms monotonic / reboot detection"),
    ("INT-05", "all", "envelope/schema validation"),
    ("INT-06", "all", "telemetry reconnects"),
    ("INT-07", "legacy", "A2 storm handled"),
    ("SHT-01", "sht40", "temperature valid and at level 0"),
    ("SHT-02", "sht40", "humidity range"),
    ("SHT-03", "sht40", "no sensor_lost event"),
    ("INA10-01", "ina226_u10", "U10 bridge-board current at level 0"),
    ("INA10-02", "ina226_u10", "no overcurrent/recovered event"),
    ("INA44-01", "ina226_u44", "U44 core-board current at level 0"),
    ("INA44-02", "ina226_u44", "events"),
    ("BGM-01", "bgm_adc_dac", "frequency meter state"),
    ("BGM-02", "bgm_adc_dac", "frame fields complete"),
    ("PCI-01", "pcal6524", "switch bitmap range / B10 fixed 0"),
    ("PCI-02", "pcal6524", "yds range and derived bits"),
    ("PCI-03", "pcal6524", "steady-state LEDs"),
    ("ATECC-01", "atecc608b", "chip present / identity snapshot"),
)


def telemetry_skipped_checks(cfg, reason: str) -> List[CheckResult]:
    """SKIP results for every check that needs the telemetry receiver."""
    return [CheckResult(cid, module, title, SKIP, detail=reason,
                        source=["control-only mode"])
            for cid, module, title in _TELEMETRY_SKIP_IDS]


def _frange(stats: dict, module: str, name: str) -> Optional[dict]:
    agg = _field(stats, module, name)
    if not agg:
        return None
    return {"min": agg["min"], "max": agg["max"], "last": agg["last"],
            "count": agg["count"]}


# ---------------------------------------------------------------------------
# integrity
# ---------------------------------------------------------------------------

def integrity_checks(cfg, baseline: dict, final: dict, integrity: dict,
                     planned_reboots: int = 0) -> List[CheckResult]:
    out: List[CheckResult] = []
    modules = cfg_modules()
    counts = {m: _slot(baseline, m).get("count", 0) for m in modules}
    ref = max(counts.values()) if counts else 0
    missing = [m for m, n in counts.items() if n == 0 or n < 0.8 * max(1, ref)]
    out.append(CheckResult(
        "INT-01", "all", "baseline frame coverage", PASS if not missing else FAIL,
        measured={"counts": counts, "reference": ref},
        thresholds={"min_fraction": 0.8},
        detail="" if not missing else "missing/starved: %s" % ", ".join(missing),
        source=["User/dso/dso_module_upload.c:219-393"]))

    gaps = {m: _slot(baseline, m).get("max_gap_ms", 0) for m in modules}
    worst = max(gaps.values()) if gaps else 0
    if worst > int(cfg.warn_gap_ms):
        status = FAIL
    elif worst > int(cfg.max_gap_ms):
        status = WARN
    else:
        status = PASS
    out.append(CheckResult(
        "INT-02", "all", "per-module frame gap", status,
        measured={"max_gap_ms": gaps, "worst_ms": worst},
        thresholds={"pass_ms": int(cfg.max_gap_ms),
                    "fail_ms": int(cfg.warn_gap_ms)},
        source=["User/dso/dso_module_upload.c:190-219"]))

    seq_gaps = int(integrity.get("seq_gaps", 0))
    duplicates = int(integrity.get("duplicates", 0))
    reconnects = int((final.get("connections") or {}).get("reconnects", 0))
    if duplicates:
        status = FAIL
        detail = "%d duplicate seq values" % duplicates
    elif seq_gaps == 0:
        status = PASS
        detail = ""
    elif reconnects > 0:
        status = WARN
        detail = ("%d seq gap(s); %d telemetry reconnect(s) can explain them"
                  % (seq_gaps, reconnects))
    else:
        status = FAIL
        detail = "%d seq gap(s) with no reconnect" % seq_gaps
    out.append(CheckResult(
        "INT-03", "all", "seq continuity (incl. watchdog)",
        status,
        measured={"seq_gaps": seq_gaps, "max_seq_gap": integrity.get("max_seq_gap"),
                  "duplicates": duplicates, "reconnects": reconnects,
                  "gaps": integrity.get("gaps", [])[:10]},
        thresholds={"expected": "seq +1 per frame"},
        detail=detail,
        source=["User/dso/dso_module_upload.c:172-186"]))

    reboots = int(integrity.get("reboots", 0))
    unplanned = max(0, reboots - int(planned_reboots))
    out.append(CheckResult(
        "INT-04", "all", "ts_ms monotonic / reboot detection",
        PASS if unplanned == 0 else FAIL,
        measured={"reboots": reboots, "planned": int(planned_reboots),
                  "epoch": integrity.get("epoch"),
                  "reboot_events": integrity.get("reboot_events", [])[-5:]},
        thresholds={"unplanned_reboots": 0},
        detail="" if unplanned == 0 else "%d unplanned reboot(s)" % unplanned,
        source=["User/dso/dso_module_upload.c:181"]))

    bad = int(final.get("bad_frames", 0))
    invalid = int(final.get("invalid_lines", 0))
    oversize = int(final.get("oversize_lines", 0))
    if bad or oversize:
        status = FAIL
    elif invalid:
        status = WARN
    else:
        status = PASS
    out.append(CheckResult(
        "INT-05", "all", "envelope/schema validation",
        status,
        measured={"bad_frames": bad, "invalid_lines": invalid,
                  "oversize_lines": oversize},
        thresholds={"bad_frames": 0, "oversize": 0},
        detail="" if status == PASS else "see events.jsonl",
        source=["module_telemetry/python_ref/envelope.py:37-86"]))

    if reconnects == 0:
        reconnect_status, reconnect_detail = PASS, ""
    elif reconnects <= int(planned_reboots):
        reconnect_status = PASS
        reconnect_detail = "explained by the planned reboot"
    else:
        reconnect_status, reconnect_detail = WARN, "reconnects observed; check A2 storm"
    out.append(CheckResult(
        "INT-06", "all", "telemetry reconnects", reconnect_status,
        measured={"reconnects": reconnects, "planned_reboots": planned_reboots},
        thresholds={"reconnects": 0},
        detail=reconnect_detail,
        source=["User/dso/dso_module_upload.c:69-97,214-218"]))

    conn = final.get("connections") or {}
    a2 = int(conn.get("a2_accepts", 0))
    legacy_lines = int(final.get("legacy_lines", 0))
    maxc = int(conn.get("max_concurrent", 0))
    if a2 == 0 and legacy_lines > 0:
        a2_detail = "A2 stable (legacy frames received, no connect storm)"
    elif a2 == 0:
        a2_detail = "A2 client not observed"
    else:
        a2_detail = "empty connections RST-closed (legacy client churn)"
    out.append(CheckResult(
        "INT-07", "legacy", "A2 storm handled without socket exhaustion",
        PASS if maxc <= int(cfg.max_conns) else FAIL,
        measured={"a2_accepts": a2, "legacy_lines": legacy_lines,
                  "a2_rst": conn.get("a2_rst"),
                  "max_concurrent": maxc, "rejected": conn.get("rejected")},
        thresholds={"max_concurrent": int(cfg.max_conns)},
        detail=a2_detail,
        source=["User/dso/dso_fmc_telemetry.c:259-282",
                "LWIP/Target/lwipopts.h:55-60"]))
    return out


# ---------------------------------------------------------------------------
# per-module steady state
# ---------------------------------------------------------------------------

def module_checks(cfg, baseline: dict, final: dict,
                  freq_hz: Optional[float] = None) -> List[CheckResult]:
    out: List[CheckResult] = []
    out.extend(_sht40(cfg, baseline))
    out.extend(_ina(cfg, baseline, "ina226_u10", cfg.ina10_warn_ma,
                    cfg.ina10_restore_ma, "U10 bridge-board current"))
    out.extend(_ina(cfg, baseline, "ina226_u44", cfg.ina44_warn_ma,
                    cfg.ina44_restore_ma, "U44 core-board current"))
    out.extend(_bgm(cfg, baseline, freq_hz))
    out.extend(_pcal(cfg, baseline))
    out.extend(_atecc_steady(cfg, final))
    return out


def _sht40(cfg, stats: dict) -> List[CheckResult]:
    if not _has_frames(stats, "sht40"):
        return [CheckResult(
            "SHT-01", "sht40", "temperature valid and at level 0", FAIL,
            detail="no sht40 frames received",
            source=["User/dso/dso_module_upload.c:271-290"]),
            CheckResult("SHT-02", "sht40", "humidity range", SKIP,
                        detail="no sht40 frames received"),
            CheckResult("SHT-03", "sht40", "no sensor_lost event", SKIP,
                        detail="no sht40 frames received")]
    last = _last(stats, "sht40")
    temp = _frange(stats, "sht40", "temp_dezi") or {}
    rh = _frange(stats, "sht40", "rh_centi") or {}
    ok = int(last.get("temp_ok", 0))
    level = int(last.get("level", 0))
    tmax = int(temp.get("max", 0))
    tmin = int(temp.get("min", 0))
    if ok != 1:
        status, detail = FAIL, "temp_ok=0 (sensor invalid)"
    elif level != 0:
        status, detail = FAIL, "level=%d" % level
    elif tmax > int(cfg.sht_warn_deci) or tmin < -400:
        status, detail = FAIL, "temperature outside expected range"
    elif tmax > int(cfg.sht_near_warn_deci):
        status, detail = WARN, "temperature near warning band"
    else:
        status, detail = PASS, ""
    out = [CheckResult(
        "SHT-01", "sht40", "temperature valid and at level 0", status,
        measured={"temp_ok": ok, "level": level, "temp_dezi": temp,
                  "hi_alarm": last.get("hi_alarm")},
        thresholds={"warn_deci": int(cfg.sht_warn_deci),
                    "near_warn_deci": int(cfg.sht_near_warn_deci),
                    "alarm_deci": int(cfg.sht_alarm_deci),
                    "emerg_deci": int(cfg.sht_emerg_deci)},
        detail=detail,
        source=["User/dso/dso_module_upload.c:271-290",
                "User/dso/dso_sht40.h:30-39"])]

    rh_ok = rh and 0 <= int(rh.get("min", 0)) and int(rh.get("max", 0)) <= 10000
    out.append(CheckResult(
        "SHT-02", "sht40", "humidity range", PASS if rh else WARN,
        measured={"rh_centi": rh},
        thresholds={"min": 0, "max": 10000},
        detail="" if rh_ok else "no humidity aggregate / out of range",
        source=["User/dso/dso_module_upload.c:276-282",
                "User/dso/dso_sht40.c:132-146"]))

    lost = _events(stats, "sht40").get("sensor_lost", 0)
    out.append(CheckResult(
        "SHT-03", "sht40", "no sensor_lost event", PASS if lost == 0 else FAIL,
        measured={"sensor_lost": lost}, thresholds={"sensor_lost": 0},
        source=["User/dso/dso_module_upload.c:284-288"]))
    return out


def _ina(cfg, stats: dict, module: str, warn_ma: int, restore_ma: int,
         title: str) -> List[CheckResult]:
    cid = "INA10-01" if module.endswith("u10") else "INA44-01"
    eid = "INA10-02" if module.endswith("u10") else "INA44-02"
    if not _has_frames(stats, module):
        return [CheckResult(cid, module, title + " at level 0", FAIL,
                            detail="no %s frames received" % module,
                            source=["User/dso/dso_module_upload.c:292-328"]),
                CheckResult(eid, module, "events", SKIP,
                            detail="no frames received")]
    last = _last(stats, module)
    cur = _frange(stats, module, "cur_ma") or {}
    level = int(last.get("level", 0))
    over = int(last.get("over_cur", 0))
    valid = int(last.get("valid", 1))
    cmax = int(cur.get("max", 0))
    cmin = int(cur.get("min", 0))
    if level != 0 or over != 0:
        status, detail = FAIL, "level=%d over_cur=%d" % (level, over)
    elif valid != 1:
        status, detail = WARN, "valid=0 (firmware hard-codes 1 today)"
    elif cmax > warn_ma or cmin < -1000:
        status, detail = FAIL, "current outside range"
    elif cmax >= restore_ma:
        status, detail = WARN, "current near restore threshold"
    else:
        status, detail = PASS, ""
    out = [CheckResult(
        cid, module, title + " at level 0", status,
        measured={"cur_ma": cur, "level": level, "over_cur": over,
                  "valid": valid},
        thresholds={"warn_ma": warn_ma, "restore_ma": restore_ma,
                    "level": 0, "over_cur": 0},
        detail=detail,
        source=["User/dso/dso_module_upload.c:292-328",
                "User/dso/dso_ina226.h:33-39" if module.endswith("u10")
                else "User/dso/dso_ina226_u44.h:28-33"])]
    events = _events(stats, module)
    if module.endswith("u44"):
        out.append(CheckResult(
            eid, module, "events", NA,
            measured={"events": events},
            detail="firmware emits no events for U44 (known discrepancy)",
            source=["User/dso/dso_module_upload.c:316-328"]))
    else:
        bad = int(events.get("overcurrent", 0)) + int(events.get("recovered", 0))
        out.append(CheckResult(
            eid, module, "no overcurrent/recovered event",
            PASS if bad == 0 else FAIL,
            measured={"events": events}, thresholds={"events": 0},
            source=["User/dso/dso_module_upload.c:304-312"]))
    return out


def _bgm(cfg, stats: dict, freq_hz: Optional[float]) -> List[CheckResult]:
    last = _last(stats, "bgm_adc_dac")
    fields = _slot(stats, "bgm_adc_dac").get("fields") or {}
    required = ("freq_valid", "freq_state", "freq_millihz", "gate_us",
                "edge_count", "ref_ticks", "overflow_count",
                "last_edge_age_ms", "overrange", "sample_format",
                "sample_rate", "overrun_count")
    missing = [key for key in required if key not in last]
    if missing:
        return [CheckResult(
            "BGM-02", "bgm_adc_dac", "frame fields complete", FAIL,
            measured={"missing": missing}, thresholds={"required": list(required)},
            source=["User/dso/dso_module_upload.c:330-365"])]

    state = int(last.get("freq_state", 0))
    valid = int(last.get("freq_valid", 0))
    overrange = int(last.get("overrange", 0))
    if freq_hz:
        mhz = _frange(stats, "bgm_adc_dac", "freq_millihz") or {}
        target = float(freq_hz) * 1000.0
        got = int(mhz.get("last", 0))
        tol = max(1.0, target * 0.001)
        status = PASS if valid == 1 and state == 3 and abs(got - target) <= tol else FAIL
        detail = "expected %.0f mHz, got %d mHz" % (target, got)
        thresholds = {"freq_millihz": target, "tolerance": tol}
    else:
        status = PASS if valid == 0 and state in (0, 1, 2, 4) and overrange == 0 else FAIL
        detail = "no external source on PB15; expecting no-signal state"
        thresholds = {"allowed_states": [0, 1, 2, 4], "freq_valid": 0,
                      "no_signal_ms": int(cfg.bgm_no_signal_ms)}
    first = CheckResult(
        "BGM-01", "bgm_adc_dac", "frequency meter state", status,
        measured={"freq_valid": valid, "freq_state": state,
                  "overrange": overrange,
                  "freq_millihz": _frange(stats, "bgm_adc_dac", "freq_millihz"),
                  "last_edge_age_ms": last.get("last_edge_age_ms")},
        thresholds=thresholds, detail=detail,
        source=["User/dso/dso_module_upload.c:330-365",
                "User/dso/dso_freq_meter.c:13-14"])
    negative = [key for key, agg in fields.items() if int(agg.get("min", 0)) < 0]
    second = CheckResult(
        "BGM-02", "bgm_adc_dac", "frame fields complete / non-negative",
        PASS if not negative else FAIL,
        measured={"negative_fields": negative,
                  "sample_format": last.get("sample_format"),
                  "sample_rate": last.get("sample_rate"),
                  "overrun_count": last.get("overrun_count")},
        thresholds={"negative_fields": []},
        source=["User/dso/dso_module_upload.c:330-365"])
    return [first, second]


def _pcal(cfg, stats: dict) -> List[CheckResult]:
    if not _has_frames(stats, "pcal6524"):
        return [CheckResult("PCI-01", "pcal6524",
                            "switch bitmap range / B10 fixed 0", FAIL,
                            detail="no pcal6524 frames received",
                            source=["User/dso/dso_module_upload.c:381-392"]),
                CheckResult("PCI-02", "pcal6524", "yds range and derived bits",
                            SKIP, detail="no frames received"),
                CheckResult("PCI-03", "pcal6524", "steady-state LEDs", SKIP,
                            detail="no frames received")]
    last = _last(stats, "pcal6524")
    sw = int(last.get("sw", 0))
    yds = int(last.get("yds", 0))
    out = [CheckResult(
        "PCI-01", "pcal6524", "switch bitmap range / B10 fixed 0",
        PASS if 0 <= sw <= 16383 and (sw & 0x200) == 0 else FAIL,
        measured={"sw": sw}, thresholds={"range": [0, 16383], "bit9": 0},
        source=["User/dso/dso_module_upload.c:381-392",
                "User/dso/dso_switch_panel.c:148-163"])]
    eth_ok = int(last.get("eth_link", 0)) == ((yds >> 3) & 1)
    usb_ok = int(last.get("usb_active", 0)) == ((yds >> 4) & 1)
    out.append(CheckResult(
        "PCI-02", "pcal6524", "yds range and derived bits",
        PASS if 0 <= yds <= 31 and eth_ok and usb_ok else FAIL,
        measured={"yds": yds, "eth_link": last.get("eth_link"),
                  "usb_active": last.get("usb_active")},
        thresholds={"eth_link": "(yds>>3)&1", "usb_active": "(yds>>4)&1"},
        detail="eth_link is the temperature-alarm bit (known mislabel)",
        source=["User/dso/dso_module_upload.c:386-391"]))
    steady_ok = bool(yds & 0x02) and bool(yds & 0x04) and not (yds & 0x01) and not (yds & 0x08)
    out.append(CheckResult(
        "PCI-03", "pcal6524", "steady-state LEDs (YDS2/YDS3 on)",
        PASS if steady_ok else WARN,
        measured={"yds": yds}, thresholds={"bits": "0b00110 (+bit4 if USB)"},
        detail="YDS1/4 may blink only under alarm; no stimulus available",
        source=["User/dso/dso_module_upload.c:123-168"]))
    return out


def _atecc_steady(cfg, stats: dict) -> List[CheckResult]:
    last = _last(stats, "atecc608b")
    present = int(last.get("present", 0))
    comm = int(last.get("comm_ok", 0))
    info = last.get("chip_info") or {}
    serial = str(info.get("serial4", ""))
    # Digits-only serials are valid; str.islower() returns False for them.
    serial_ok = (1 <= len(serial) <= 8
                 and all(c in "0123456789abcdef" for c in serial))
    locks_ok = (info.get("lock_config") in (0, 1)
                and info.get("lock_data") in (0, 1))
    unlocked = (info.get("lock_config") == 0 or info.get("lock_data") == 0)
    fail_cnt = last.get("fail_cnt", 0)
    tamper = _events(stats, "atecc608b").get("tamper", 0)
    if present != 1 or comm != 1:
        status, detail = FAIL, "present=%d comm_ok=%d" % (present, comm)
    elif not serial_ok or not locks_ok:
        status, detail = FAIL, "chip_info malformed"
    elif int(fail_cnt) >= 10 or tamper:
        status, detail = FAIL, "fail_cnt=%s tamper=%d" % (fail_cnt, tamper)
    elif int(fail_cnt) > 0:
        status, detail = WARN, "transient fail_cnt=%s" % fail_cnt
    elif unlocked:
        status, detail = WARN, ("chip is unlocked (lock_config=%s lock_data=%s); "
                                "identity works, chip-level provisioning is incomplete"
                                % (info.get("lock_config"),
                                   info.get("lock_data")))
    else:
        status, detail = PASS, ""
    return [CheckResult(
        "ATECC-01", "atecc608b", "chip present / identity snapshot", status,
        measured={"present": present, "comm_ok": comm,
                  "device_id": last.get("device_id"),
                  "chip_info": info, "fail_cnt": fail_cnt,
                  "login_state": last.get("login_state"),
                  "tamper_events": tamper},
        thresholds={"present": 1, "comm_ok": 1, "fail_cnt": 0},
        detail=detail,
        source=["User/dso/dso_module_upload.c:224-269"])]


# ---------------------------------------------------------------------------
# control-channel checks (results produced by the scenario)
# ---------------------------------------------------------------------------

def switch_checks(cfg, sweep: dict) -> List[CheckResult]:
    if sweep.get("skipped"):
        reason = sweep.get("reason", "control channel unavailable")
        return [CheckResult(cid, "pcal6524", title, SKIP, detail=reason,
                            source=["User/dso/dso_switch_cmd.c:168-227"])
                for cid, title in (("PCI-04", "switch sweep"),
                                   ("PCI-05", "invalid switch index"),
                                   ("PCI-06", "B10 no-op"),
                                   ("PCI-07", "baseline restored"))]
    steps = sweep.get("steps") or []
    # ``cmd_ok`` is the authoritative ack + get_switch readback; a missed 1 Hz
    # telemetry confirmation downgrades the check to WARN, not FAIL.
    cmd_bad = [s for s in steps if not s.get("cmd_ok", s.get("ok"))]
    tel_missed = [s for s in steps if s.get("telemetry_ok") is False]
    lat = [s.get("latency_ms", 0) for s in steps]
    control_lost = bool(sweep.get("control_lost"))
    if not steps or cmd_bad:
        pci04_status = FAIL
    elif tel_missed:
        pci04_status = WARN
    else:
        pci04_status = PASS
    if cmd_bad and control_lost:
        pci04_detail = "control channel was lost during the sweep; see failures"
    elif tel_missed:
        pci04_detail = ("ack/readback ok; telemetry confirmation missed on %d "
                        "step(s)" % len(tel_missed))
    elif sweep.get("telemetry_checked") is False:
        pci04_detail = "telemetry cross-check unavailable (control-only)"
    else:
        pci04_detail = ""
    out = [CheckResult(
        "PCI-04", "pcal6524", "switch sweep (idx 0..13, except 9)",
        pci04_status,
        measured={"steps": len(steps), "command_failed": len(cmd_bad),
                  "telemetry_missed": len(tel_missed),
                  "max_latency_ms": max(lat) if lat else None,
                  "failures": cmd_bad[:10]},
        thresholds={"all_steps_cmd_ok": True,
                    "telemetry_confirmation": "warn only"},
        detail=pci04_detail,
        source=["User/dso/dso_switch_cmd.c:168-183",
                "User/dso/dso_switch_panel.c:148-158"])]
    if control_lost:
        reason = "control channel lost during the sweep; not evaluated"
        out.extend([
            CheckResult("PCI-05", "pcal6524", "invalid switch index rejected",
                        SKIP, detail=reason),
            CheckResult("PCI-06", "pcal6524", "B10 (idx 9) ack ok but no-op",
                        SKIP, detail=reason),
            CheckResult("PCI-07", "pcal6524", "baseline switch bitmap restored",
                        SKIP, detail=reason),
        ])
        return out
    invalid = sweep.get("invalid") or []
    # ``rejected`` is set by the scenario when the ack was a bad_param error.
    inv_ok = all(item.get("rejected") and item.get("error") == "bad_param"
                 for item in invalid) and len(invalid) >= 2
    out.append(CheckResult(
        "PCI-05", "pcal6524", "invalid switch index rejected",
        PASS if inv_ok else FAIL,
        measured={"invalid": invalid}, thresholds={"error": "bad_param"},
        source=["User/dso/dso_switch_cmd.c:171-177"]))
    nine = sweep.get("idx9") or {}
    out.append(CheckResult(
        "PCI-06", "pcal6524", "B10 (idx 9) ack ok but no-op",
        PASS if nine.get("ack_ok") and not nine.get("bit_changed") else FAIL,
        measured=nine, thresholds={"ack": "ok", "bit_changed": False},
        source=["User/dso/dso_switch_panel.c:140-158"]))
    restored = bool(sweep.get("restored"))
    out.append(CheckResult(
        "PCI-07", "pcal6524", "baseline switch bitmap restored",
        PASS if restored else FAIL,
        measured={"baseline_sw": sweep.get("baseline_sw"),
                  "final_sw": sweep.get("final_sw")},
        thresholds={"equal": True},
        source=["User/dso/dso_switch_cmd.c:220-225"]))
    return out


def human_checks(cfg, answers: Optional[dict]) -> List[CheckResult]:
    items = [
        ("HUM-01", "YDS2 green steady on",
         "baseline 时 YDS2（绿）常亮"),
        ("HUM-02", "YDS3 blue steady on",
         "baseline 时 YDS3（蓝）常亮"),
        ("HUM-03", "YDS1 red off",
         "baseline 时 YDS1（红）熄灭"),
        ("HUM-04", "YDS4 orange off",
         "baseline 时 YDS4（橙）熄灭"),
        ("HUM-05", "buzzer silent",
         "baseline 时蜂鸣器无声"),
        ("PCI-08", "mux line levels during sweep",
         "扫描时线电平 ON≈0V / OFF≈3.3V，B10 无对应线"),
    ]
    out: List[CheckResult] = []
    answers = answers or {}
    for cid, title, desc in items:
        value = answers.get(cid)
        if value is None or value == "":
            status, detail = PENDING, desc
        elif value in (True, "pass", "PASS", 1, "1", "ok", "OK"):
            status, detail = PASS, ""
        else:
            status, detail = FAIL, str(value)
        out.append(CheckResult(
            cid, "board", title, status, measured={"answer": value},
            thresholds={"expected": True}, detail=detail,
            source=["README human checklist"]))
    return out


# ---------------------------------------------------------------------------
# ATECC command checks (results produced by the scenario)
# ---------------------------------------------------------------------------

def atecc_command_checks(atecc: dict) -> List[CheckResult]:
    out: List[CheckResult] = []
    devid = atecc.get("devid") or {}
    tele = atecc.get("telemetry_device_id")
    if not atecc.get("control_ok"):
        out.append(CheckResult(
            "ATECC-02", "atecc608b", "get_devid matches telemetry", SKIP,
            detail="control channel unavailable",
            source=["User/dso/dso_switch_cmd.c:246-257"]))
    else:
        did = (devid.get("data") or {}).get("device_id")
        if tele is None:
            ok = bool(did)
            detail = "telemetry cross-check unavailable (control-only)"
        else:
            ok = bool(did) and did == tele
            detail = ""
        out.append(CheckResult(
            "ATECC-02", "atecc608b", "get_devid matches telemetry",
            PASS if ok else FAIL,
            measured={"get_devid": did, "telemetry": tele,
                      "ack_result": devid.get("result")},
            thresholds={"equal": True}, detail=detail,
            source=["User/dso/dso_switch_cmd.c:246-257",
                    "User/dso/dso_module_upload.c:245"]))

    pk = atecc.get("pubkey") or {}
    pk1 = (pk.get("data") or {}).get("pubkey_hex", "")
    pk2 = (atecc.get("pubkey2") or {}).get("data", {}).get("pubkey_hex", "")
    pk_ok = (pk.get("result") == "ok" and len(pk1) == 128 and pk1 == pk2)
    out.append(CheckResult(
        "ATECC-03", "atecc608b", "get_pubkey stable (software key)",
        PASS if pk_ok else (SKIP if not atecc.get("control_ok") else FAIL),
        measured={"len": len(pk1), "same_twice": pk1 == pk2,
                  "note": "DSO_AUTH_DEV_SOFTKEY=1 -> software P-256"},
        thresholds={"len": 128},
        detail=("" if pk_ok else
                ("control channel unavailable" if not atecc.get("control_ok")
                 else "pubkey missing or unstable")),
        source=["User/dso/dso_switch_cmd.c:259-289",
                "User/dso/dso_config.h:333-335"]))

    auth = atecc.get("auth_start") or {}
    data = auth.get("data") or {}
    sig = str(data.get("signature_hex", ""))
    auth_ok = (auth.get("result") == "ok" and len(sig) == 128
               and int(data.get("self_verify", 0)) == 1)
    out.append(CheckResult(
        "ATECC-04", "atecc608b", "auth_start signs a random challenge",
        PASS if auth_ok else (SKIP if not atecc.get("control_ok") else FAIL),
        measured={"len": len(sig), "self_verify": data.get("self_verify"),
                  "slot": data.get("slot")},
        thresholds={"len": 128, "self_verify": 1},
        detail=("" if auth_ok else
                ("control channel unavailable" if not atecc.get("control_ok")
                 else "signature missing or self_verify != 1")),
        source=["User/dso/dso_switch_cmd.c:291-367"]))

    lic = atecc.get("license") or {}
    lic_data = lic.get("data") or {}
    lic_ok = lic.get("result") == "ok" and "login_state" in lic_data
    flow = atecc.get("auth_flow")
    if not lic_ok:
        status, detail = FAIL, "get_license failed"
    elif flow is None:
        status, detail = PASS, "get_license ok; full authorize flow not requested"
    elif flow.get("ok"):
        status, detail = PASS, ""
    else:
        status, detail = FAIL, str(flow.get("stderr", ""))[-300:]
    if not atecc.get("control_ok"):
        status, detail = SKIP, "control channel unavailable"
    out.append(CheckResult(
        "ATECC-05", "atecc608b", "license query / optional full flow", status,
        measured={"get_license": lic_data, "auth_flow": flow},
        thresholds={"result": "ok"},
        detail=detail,
        source=["User/dso/dso_switch_cmd.c:426-463"]))
    return out


# ---------------------------------------------------------------------------
# soak
# ---------------------------------------------------------------------------

def soak_check(cfg, baseline: dict, final: dict, seconds: float) -> CheckResult:
    if seconds <= 0:
        return CheckResult("SOAK-01", "all", "soak window", SKIP,
                           detail="soak disabled",
                           source=["scenario settings"])
    modules = cfg_modules()
    before = {m: _slot(baseline, m).get("count", 0) for m in modules}
    after = {m: _slot(final, m).get("count", 0) for m in modules}
    grown = {m: after[m] - before[m] for m in modules}
    expected = max(1, int(seconds))  # one frame per module per second
    starved = [m for m, n in grown.items() if n < 0.8 * expected]
    new_bad = int(final.get("bad_frames", 0)) - int(baseline.get("bad_frames", 0))
    status = PASS if not starved and new_bad <= 0 else FAIL
    return CheckResult(
        "SOAK-01", "all", "soak window coverage", status,
        measured={"seconds": seconds, "grown": grown, "expected": expected,
                  "new_bad_frames": new_bad},
        thresholds={"min_fraction": 0.8, "new_bad_frames": 0},
        detail="" if status == PASS else "starved: %s" % ", ".join(starved),
        source=["User/dso/dso_module_upload.c:219"])


def cfg_modules() -> List[str]:
    from .config import MODULES
    return list(MODULES)


def evaluate_all(cfg, ctx: dict) -> List[CheckResult]:
    """Run every evaluator against a scenario context."""
    baseline = ctx.get("baseline") or {}
    final = ctx.get("final") or baseline
    integrity = ctx.get("integrity") or {}
    out: List[CheckResult] = []
    out.extend(integrity_checks(cfg, baseline, final, integrity,
                                int(ctx.get("planned_reboots", 0))))
    out.extend(module_checks(cfg, baseline, final, ctx.get("freq_hz")))
    if ctx.get("sweep") is not None:
        out.extend(switch_checks(cfg, ctx["sweep"]))
    out.extend(atecc_command_checks(ctx.get("atecc") or {}))
    out.extend(human_checks(cfg, ctx.get("human_answers")))
    out.append(soak_check(cfg, ctx.get("soak_base") or baseline, final,
                          float(ctx.get("soak_s", 0))))
    return out
