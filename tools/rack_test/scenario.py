"""Phased test scenario: baseline, switch sweep, module checks, ATECC, soak."""

from __future__ import annotations

import os
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional

from . import checks, metadata, report
from .checks import CheckResult, FAIL, PASS, SKIP
from .config import MODULES, RackConfig
from .control import RackControl
from .integrity import IntegrityTracker
from .receiver import RackReceiver
from .sink import RotatingJsonl, write_atomic_json
from .stats import RunStats


def _iso() -> str:
    return datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds")


class Scenario:
    def __init__(self, config: RackConfig,
                 receiver: Optional[RackReceiver],
                 control: RackControl, run_dir: Path, stats: RunStats,
                 integrity: IntegrityTracker,
                 event_sink: Optional[RotatingJsonl] = None,
                 log=print) -> None:
        self.config = config
        self.receiver = receiver
        self.control = control
        self.run_dir = Path(run_dir)
        self.stats = stats
        self.integrity = integrity
        self.event_sink = event_sink
        self.log = log
        self.control_ok = False
        self.control_error = ""

    # ------------------------------------------------------------------ entry
    def run(self, soak_s: Optional[float] = None, do_sweep: bool = True,
            human_answers: Optional[dict] = None,
            do_control: bool = True) -> dict:
        started = _iso()
        soak_s = float(self.config.soak_s if soak_s is None else soak_s)

        self.log("[scenario] waiting for telemetry (up to %.0fs)"
                 % self.config.telemetry_wait_s)
        seen = self._wait_for_telemetry(float(self.config.telemetry_wait_s))
        self._event({"kind": "scenario", "step": "telemetry_seen", "ok": seen})

        initial_switch: Optional[dict] = None
        if do_control:
            self.log("[scenario] control preflight")
            initial_switch = self._control_preflight()
        else:
            self.control_ok = False
            self.control_error = "telemetry-only mode"
            self._event({"kind": "control_preflight", "ok": False,
                         "reason": "telemetry-only mode"})

        self.log("[scenario] baseline %.0fs" % self.config.baseline_s)
        time.sleep(max(0.0, float(self.config.baseline_s)))
        baseline = self.stats.snapshot()

        sweep: Optional[dict] = None
        if do_control and do_sweep:
            sweep = self._sweep(initial_switch)
        else:
            reason = ("telemetry-only mode" if not do_control
                      else "sweep disabled by operator")
            sweep = {"skipped": True, "reason": reason}

        # The legacy A2 storm can make the board's single-client listener
        # intermittently unavailable.  Retry once before the ATECC phase and
        # restore any switches that were left toggled.
        if do_control and not self.control_ok:
            self.log("[scenario] control lost during sweep; retrying")
            self._control_preflight()
            if (self.control_ok and sweep and not sweep.get("skipped")
                    and not sweep.get("restored")
                    and sweep.get("baseline_sw") is not None):
                self._restore_switches(int(sweep["baseline_sw"]), sweep)

        # Let the last restored frame reach the receiver before ATECC checks.
        time.sleep(min(2.0, float(self.config.settle_s)))

        if do_control:
            atecc = self._atecc()
        else:
            atecc = {
                "control_ok": False,
                "telemetry_device_id": (self.stats.snapshot()["modules"]
                                        ["atecc608b"]["last_data"] or {}).get(
                                            "device_id"),
            }

        planned_reboots = 0
        reboot_result = CheckResult(
            "REBOOT-01", "board", "remote reboot resilience", SKIP,
            detail=("telemetry-only mode" if not do_control else "not requested"),
            source=["User/dso/dso_switch_cmd.c:229-242"])
        if do_control and self.config.allow_reboot and self.control_ok:
            planned_reboots, reboot_result = self._reboot()
        elif do_control and self.config.allow_reboot:
            reboot_result.status = FAIL
            reboot_result.detail = "control channel unavailable"

        self.log("[scenario] soak %.0fs" % soak_s)
        soak_base = self.stats.snapshot()
        if soak_s > 0:
            time.sleep(soak_s)
        final = self.stats.snapshot()
        integrity = self.integrity.snapshot()

        ctx = {
            "baseline": baseline,
            "final": final,
            "soak_base": soak_base,
            "integrity": integrity,
            "sweep": sweep,
            "atecc": atecc,
            "freq_hz": self.config.freq_hz,
            "soak_s": soak_s,
            "planned_reboots": planned_reboots,
            "human_answers": human_answers,
        }
        results = checks.evaluate_all(self.config, ctx)
        results.append(reboot_result)

        ended = _iso()
        write_atomic_json(self.run_dir / "human_todo.json", {
            "schema": "dso.rack_test.human_todo/1",
            "run_id": self.run_dir.name,
            "items": metadata.HUMAN_TODO,
            "answers_template": metadata.answers_template(),
        })
        summary = report.build_summary(
            self.config, self.run_dir, results, ctx, started, ended,
            control_ok=self.control_ok, control_error=self.control_error,
            telemetry_seen=seen)
        write_atomic_json(self.run_dir / "summary.json", summary)
        self.log("[scenario] verdict=%s counts=%s"
                 % (summary["verdict"], summary["counts"]))
        return summary

    # -------------------------------------------------------- control-only
    def run_control_only(self, human_answers: Optional[dict] = None,
                         do_sweep: bool = True,
                         do_reboot: bool = False) -> dict:
        """Switch/ATECC test with the telemetry receiver deliberately OFF.

        The board's module-upload and legacy clients cannot connect, so their
        LwIP netconns/PCBs are free for the single-client 5005 listener.  The
        switch sweep is verified by the command ack plus get_switch readback;
        the telemetry cross-check is reported as unavailable.
        """
        started = _iso()
        self.log("[scenario] control-only: telemetry receiver disabled")
        initial = self._control_preflight()
        if do_sweep:
            sweep = self._sweep(initial, verify_telemetry=False)
        else:
            sweep = {"skipped": True, "reason": "sweep disabled by operator"}
        atecc = self._atecc()

        planned_reboots = 0
        reboot_result = CheckResult(
            "REBOOT-01", "board", "remote reboot resilience", SKIP,
            detail="not requested", source=["User/dso/dso_switch_cmd.c:229-242"])
        if do_reboot and self.control_ok:
            planned_reboots, reboot_result = self._reboot_control_only()
        elif do_reboot:
            reboot_result.status = FAIL
            reboot_result.detail = "control channel unavailable"

        final = self.stats.snapshot()
        integrity = self.integrity.snapshot()
        ctx = {
            "baseline": final,
            "final": final,
            "soak_base": final,
            "integrity": integrity,
            "sweep": sweep,
            "atecc": atecc,
            "freq_hz": None,
            "soak_s": 0,
            "planned_reboots": planned_reboots,
            "human_answers": human_answers,
            "mode": "control-only",
        }
        results = []
        results.extend(checks.telemetry_skipped_checks(
            self.config, "control-only mode: telemetry receiver disabled"))
        results.extend(checks.switch_checks(self.config, sweep))
        results.extend(checks.atecc_command_checks(atecc))
        results.extend(checks.human_checks(self.config, human_answers))
        results.append(reboot_result)

        ended = _iso()
        write_atomic_json(self.run_dir / "human_todo.json", {
            "schema": "dso.rack_test.human_todo/1",
            "run_id": self.run_dir.name,
            "items": metadata.HUMAN_TODO,
            "answers_template": metadata.answers_template(),
        })
        summary = report.build_summary(
            self.config, self.run_dir, results, ctx, started, ended,
            control_ok=self.control_ok, control_error=self.control_error,
            telemetry_seen=False)
        write_atomic_json(self.run_dir / "summary.json", summary)
        self.log("[scenario] verdict=%s counts=%s"
                 % (summary["verdict"], summary["counts"]))
        return summary

    def _reboot_control_only(self) -> tuple:
        try:
            ack = self.control.reboot()
        except Exception as exc:
            self.control_ok = False
            self.control_error = str(exc)
            return (0, CheckResult(
                "REBOOT-01", "board", "remote reboot resilience", FAIL,
                measured={"error": str(exc)},
                source=["User/dso/dso_switch_cmd.c:229-242"]))
        self.control.close()
        deadline = time.monotonic() + float(self.config.reboot_wait_s)
        recovered = False
        while time.monotonic() < deadline:
            time.sleep(2.0)
            try:
                state = self.control.get_switch()
                if isinstance(state.get("sw"), int):
                    recovered = True
                    break
            except Exception:
                self.control.close()
        status = PASS if (ack.get("result") == "ok" and recovered) else FAIL
        detail = "" if status == PASS else (
            "ack=%s recovered=%s" % (ack.get("result"), recovered))
        self._event({"kind": "reboot", "ack": ack.get("result"),
                     "recovered": recovered})
        return (1 if recovered else 0, CheckResult(
            "REBOOT-01", "board", "remote reboot resilience", status,
            measured={"ack": ack.get("result"), "recovered": recovered},
            thresholds={"ack": "ok", "control_recovered": True},
            detail=detail,
            source=["User/dso/dso_switch_cmd.c:229-242",
                    "User/dso/dso_watchdog.c:83-90"]))

    # ---------------------------------------------------------------- helpers
    def _event(self, record: dict) -> None:
        if self.event_sink is None:
            return
        rec = dict(record)
        rec.setdefault("kind", "scenario")
        try:
            self.event_sink.write(rec)
        except OSError:
            pass

    def _wait_for_telemetry(self, timeout: float) -> bool:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            snap = self.stats.snapshot()
            if all(snap["modules"][m]["count"] >= 2 for m in MODULES):
                return True
            time.sleep(0.2)
        return False

    def _control_preflight(self) -> Optional[dict]:
        # The board's own listener comes up ~8 s after boot, later than the
        # module-upload client.  Wait for it instead of declaring the control
        # channel dead on the first attempt.
        deadline = time.monotonic() + float(self.config.control_wait_s)
        last_error = ""
        while time.monotonic() < deadline:
            try:
                # Short probe: fail fast and distinguish refused vs no-ack.
                state = self.control.probe(
                    timeout_s=min(5.0, float(self.config.control_timeout_s)))
                self.control.connect()
                self.control_ok = True
                self._event({"kind": "control_preflight", "ok": True,
                             "sw": state.get("sw"),
                             "eth_link": state.get("eth_link"),
                             "usb_active": state.get("usb_active"),
                             "buzzer_mute": state.get("buzzer_mute")})
                return state
            except Exception as exc:
                last_error = str(exc)
                self.control.close()
                time.sleep(0.5)
        self.control_ok = False
        self.control_error = last_error or "control channel unavailable"
        self._event({"kind": "control_preflight", "ok": False,
                     "error": self.control_error})
        self.log("[scenario] control unavailable: %s" % self.control_error)
        low = self.control_error.lower()
        if "timed out" in low or "no ack" in low or "timeout" in low:
            self.log("[scenario] hint: TCP handshake succeeded but no ack; "
                     "the 5005 listener task is not accepting/replying. "
                     "Capture the board UART (1,152,000): [ETH] listening "
                     ":5005 / [ETH] client connected / [ETH] acc errno.")
        else:
            self.log("[scenario] hint: no listener (connection refused); "
                     "reset/power-cycle the DSO board, wait ~15 s, then retry "
                     "--control-only.")
        return None

    # ------------------------------------------------------------------ sweep
    def _pcal_state(self) -> dict:
        return self.stats.snapshot()["modules"]["pcal6524"]

    def _wait_bit(self, index: int, value: int, count_before: int,
                  timeout: float) -> bool:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            slot = self._pcal_state()
            if int(slot.get("count", 0)) > count_before:
                last = slot.get("last_data") or {}
                if last and ((int(last.get("sw", 0)) >> index) & 1) == value:
                    return True
            time.sleep(0.1)
        return False

    def _sweep(self, initial: Optional[dict],
               verify_telemetry: bool = True) -> dict:
        if not self.control_ok or initial is None:
            return {"skipped": True, "reason": "control channel unavailable"}
        baseline_sw = int(initial.get("sw", 0))
        self.log("[scenario] switch sweep (baseline sw=0x%04X%s)"
                 % (baseline_sw, "" if verify_telemetry
                    else ", control-only"))
        steps: List[dict] = []
        for index in range(14):
            if index == 9:
                continue
            step = {"index": index, "ok": False}
            try:
                before = (int(self._pcal_state().get("count", 0))
                          if verify_telemetry else 0)
                started = time.monotonic()
                ack_on = self.control.set_sw(index, 1)
                state_on = self.control.get_switch()
                tel_on = (self._wait_bit(index, 1, before,
                                         float(self.config.settle_s))
                          if verify_telemetry else None)
                if verify_telemetry and not tel_on:
                    # One retry: a single missed 1 Hz frame must not fail the
                    # sweep when the ack/readback are correct.
                    tel_on = self._wait_bit(index, 1, before,
                                            float(self.config.settle_s))
                step.update({
                    "ack_on": ack_on.get("result"),
                    "bit_get_on": (int(state_on.get("sw", 0)) >> index) & 1,
                    # Actual telemetry bit value, or None if the change was
                    # never observed in a fresh frame / not checked.
                    "bit_tel_on": (1 if tel_on else None) if verify_telemetry else None,
                })
                before_off = (int(self._pcal_state().get("count", 0))
                              if verify_telemetry else 0)
                ack_off = self.control.set_sw(index, 0)
                state_off = self.control.get_switch()
                tel_off = (self._wait_bit(index, 0, before_off,
                                          float(self.config.settle_s))
                           if verify_telemetry else None)
                if verify_telemetry and not tel_off:
                    tel_off = self._wait_bit(index, 0, before_off,
                                             float(self.config.settle_s))
                step.update({
                    "ack_off": ack_off.get("result"),
                    "bit_get_off": (int(state_off.get("sw", 0)) >> index) & 1,
                    "bit_tel_off": (0 if tel_off else None) if verify_telemetry else None,
                    "latency_ms": int((time.monotonic() - started) * 1000),
                })
                cmd_ok = (ack_on.get("result") == "ok"
                          and ack_off.get("result") == "ok"
                          and step["bit_get_on"] == 1
                          and step["bit_get_off"] == 0)
                telemetry_ok = ((bool(tel_on) and bool(tel_off))
                                if verify_telemetry else None)
                step["cmd_ok"] = cmd_ok
                step["telemetry_ok"] = telemetry_ok
                step["ok"] = cmd_ok and (telemetry_ok
                                         if verify_telemetry else True)
                if cmd_ok:
                    # The board's control listener can be intermittently
                    # unavailable under the legacy A2 storm; a later success
                    # means the channel recovered.
                    self.control_ok = True
                    self.control_error = ""
            except Exception as exc:
                step["error"] = str(exc)
                self.control_ok = False
                self.control_error = str(exc)
            steps.append(step)
            self._event({"kind": "switch_step", **step})

        invalid: List[dict] = []
        idx9: Dict[str, Any] = {}
        restored = False
        final_sw = None
        if self.control_ok:
            for index in (14, -1):
                try:
                    ack = self.control.set_sw(index, 1)
                    invalid.append({"index": index,
                                    "rejected": ack.get("result") == "error",
                                    "error": ack.get("error", "")})
                except Exception as exc:
                    invalid.append({"index": index, "rejected": False,
                                    "error": str(exc)})
            try:
                before = self.control.get_switch()
                ack = self.control.set_sw(9, 1)
                after = self.control.get_switch()
                changed = ((int(after.get("sw", 0))
                            ^ int(before.get("sw", 0))) & 0x200) != 0
                idx9 = {"ack_ok": ack.get("result") == "ok",
                        "bit_changed": changed,
                        "sw_before": before.get("sw"),
                        "sw_after": after.get("sw")}
                self.control.set_sw(9, 0)
            except Exception as exc:
                idx9 = {"ack_ok": False, "bit_changed": False,
                        "error": str(exc)}
            try:
                current = int(self.control.get_switch().get("sw", 0))
                for index in range(14):
                    if index == 9:
                        continue
                    want = (baseline_sw >> index) & 1
                    if ((current >> index) & 1) != want:
                        self.control.set_sw(index, want)
                final_sw = int(self.control.get_switch().get("sw", 0))
                restored = final_sw == baseline_sw
            except Exception as exc:
                self._event({"kind": "sweep_restore_error", "error": str(exc)})
        self._event({"kind": "sweep_done", "steps": len(steps),
                     "failed": len([s for s in steps if not s["ok"]]),
                     "restored": restored})
        return {
            "baseline_sw": baseline_sw,
            "steps": steps,
            "invalid": invalid,
            "idx9": idx9,
            "restored": restored,
            "final_sw": final_sw,
            "control_lost": not self.control_ok,
            "telemetry_checked": verify_telemetry,
        }

    def _restore_switches(self, baseline_sw: int, sweep: dict) -> None:
        """Best-effort restore after a partial sweep, then update the evidence."""
        try:
            current = int(self.control.get_switch().get("sw", 0))
            for index in range(14):
                if index == 9:
                    continue
                want = (baseline_sw >> index) & 1
                if ((current >> index) & 1) != want:
                    self.control.set_sw(index, want)
            final_sw = int(self.control.get_switch().get("sw", 0))
            sweep["final_sw"] = final_sw
            sweep["restored"] = final_sw == baseline_sw
            self._event({"kind": "sweep_restore_after_recovery",
                         "restored": sweep["restored"],
                         "final_sw": final_sw})
        except Exception as exc:
            self._event({"kind": "sweep_restore_error", "error": str(exc)})

    # ------------------------------------------------------------------ ATECC
    def _atecc(self) -> dict:
        out: Dict[str, Any] = {
            "control_ok": self.control_ok,
            "telemetry_device_id": (self.stats.snapshot()["modules"]
                                    ["atecc608b"]["last_data"] or {}).get("device_id"),
        }
        if not self.control_ok:
            return out
        try:
            out["devid"] = self.control.atecc_get_devid()
            out["pubkey"] = self.control.atecc_get_pubkey()
            out["pubkey2"] = self.control.atecc_get_pubkey()
            challenge = os.urandom(32).hex()
            out["challenge_hex"] = challenge
            out["auth_start"] = self.control.atecc_auth_start(challenge)
            out["license"] = self.control.atecc_get_license()
            if self.config.auth_user and self.config.auth_password:
                self.log("[scenario] running full authorize flow")
                ok, stdout, stderr = self.control.run_auth_flow(
                    self.config.auth_user, self.config.auth_password)
                out["auth_flow"] = {"ok": ok, "stdout": stdout[-2000:],
                                    "stderr": stderr[-2000:]}
                out["license_after_flow"] = self._safe(
                    self.control.atecc_get_license)
            if self.config.allow_license_clear:
                out["license_clear"] = self._safe(self.control.atecc_license_clear)
                out["license_after_clear"] = self._safe(
                    self.control.atecc_get_license)
        except Exception as exc:
            out["error"] = str(exc)
            self.control_ok = False
            self.control_error = str(exc)
            out["control_ok"] = False
            self._event({"kind": "atecc_error", "error": str(exc)})
        return out

    def _safe(self, fn) -> dict:
        try:
            return fn()
        except Exception as exc:
            return {"result": "error", "error": str(exc)}

    # ----------------------------------------------------------------- reboot
    def _reboot(self) -> tuple:
        before = self.integrity.snapshot().get("reboots", 0)
        before_count = int(self.stats.snapshot()["modules"]["sht40"]["count"])
        try:
            ack = self.control.reboot()
        except Exception as exc:
            self.control_ok = False
            self.control_error = str(exc)
            return (0, CheckResult(
                "REBOOT-01", "board", "remote reboot resilience", FAIL,
                measured={"error": str(exc)},
                source=["User/dso/dso_switch_cmd.c:229-242"]))
        self.control.close()
        deadline = time.monotonic() + float(self.config.reboot_wait_s)
        observed = False
        resumed = False
        while time.monotonic() < deadline:
            integ = self.integrity.snapshot()
            if int(integ.get("reboots", 0)) > before:
                observed = True
            if observed:
                snap = self.stats.snapshot()
                if int(snap["modules"]["sht40"]["count"]) > before_count:
                    resumed = True
                    break
            time.sleep(0.5)
        status = PASS if (ack.get("result") == "ok" and observed and resumed) else FAIL
        detail = "" if status == PASS else (
            "ack=%s observed=%s resumed=%s"
            % (ack.get("result"), observed, resumed))
        self._event({"kind": "reboot", "ack": ack.get("result"),
                     "observed": observed, "resumed": resumed})
        return (1 if observed else 0, CheckResult(
            "REBOOT-01", "board", "remote reboot resilience", status,
            measured={"ack": ack.get("result"), "observed": observed,
                      "resumed": resumed},
            thresholds={"ack": "ok", "reboot_event": True,
                        "telemetry_resumed": True},
            detail=detail,
            source=["User/dso/dso_switch_cmd.c:229-242",
                    "User/dso/dso_watchdog.c:83-90"]))
