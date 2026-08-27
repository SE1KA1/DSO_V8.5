"""
Core device simulator state machine.

Reproduces the DSO's 7-state model (SAFE/LOCAL/PC/CLOUD/DEBUG/CONFIG/
RECOVERY), the telemetry frame assembly, control command handling, and
error event injection — all with semantic fidelity to the real firmware.
"""

from __future__ import annotations

import random
import time
from dataclasses import dataclass, field

from .awg import AWGState, WAVE_KINDS
from .errors import DSOError, DSOErrorCode
from .protocol import TelemetryFrame, ControlCommand, AckFrame
from .waveform import (ChannelModel, WaveformSpec, generate_display_wave,
                       measure_wave, DISPLAY_WIDTH)

# 7-state machine (云桥安全状态机 v0.9 §一)
STATES = ("SAFE", "LOCAL", "PC", "CLOUD", "DEBUG", "CONFIG", "RECOVERY")

# Legal transitions (state → allowed targets)
TRANSITIONS = {
    "SAFE":     {"LOCAL"},
    "LOCAL":    {"SAFE", "PC", "CONFIG"},
    "PC":       {"SAFE", "LOCAL", "CLOUD", "DEBUG", "CONFIG"},
    "CLOUD":    {"SAFE", "LOCAL", "PC", "DEBUG", "CONFIG"},
    "DEBUG":    {"SAFE", "PC", "CLOUD"},
    "CONFIG":   {"SAFE", "LOCAL", "PC", "CLOUD", "RECOVERY"},
    "RECOVERY": {"SAFE"},
}


@dataclass
class TriggerModel:
    mode: str = "AUTO"     # AUTO | NORMAL | SINGLE
    slope: str = "RISE"    # RISE | FALL | BOTH
    source: str = "CH1"    # CH1 | CH2
    level_v: float = 0.0
    level_enabled: bool = True
    pos_pct: int = 50
    triggered: bool = True


@dataclass
class TimebaseModel:
    sec_div_ns: int = 1_000_000   # 1ms/div
    sample_rate_hz: int = 33_333_333
    mode: str = "NORMAL"          # FAST | NORMAL | ROLL


class DSOSimulator:
    """
    Scriptable DSO device simulator.

    Example:
        dev = DSOSimulator(seed=42)
        dev.set_waveform(1, "sine", freq_hz=2000, amplitude_v=1.0)
        dev.set_waveform(2, "square", freq_hz=500, amplitude_v=0.5)
        dev.send_cmd("set_run", {"running": False})
        frame = dev.read_frame()
        print(frame.ch1["freq_hz"])
    """

    def __init__(self, seed: int | None = None):
        self._rng = random.Random(seed)
        self.seq = 0
        self.ts = 0
        self.state = "LOCAL"          # boots to SAFE then auto→LOCAL on healthy

        # channels
        self.ch1 = ChannelModel(spec=WaveformSpec(kind="sine", freq_hz=1000,
                                                  amplitude_v=1.0),
                                vdiv_mv=1000, present=True)
        self.ch2 = ChannelModel(spec=WaveformSpec(kind="sine", freq_hz=500,
                                                  amplitude_v=0.5),
                                vdiv_mv=500, present=True)

        # subsystems
        self.trig = TriggerModel()
        self.tb = TimebaseModel()
        self.awg = AWGState()
        self.acquire_mode = "SAMPLE"   # SAMPLE | PEAK | HIRES | AVERAGE
        self.average_depth = 4

        # switch panel model (mirror dso_switch_panel.c)
        self.sw_bits = 0          # 14-bit bitmap, bit0=SW1 .. bit13=SW14
        self.led_bits = 0b011     # LED1 heartbeat + LED2 power on

        # status / telemetry
        self.running = True
        self.overrun = 0
        self.phase_sync_err = 0
        self.cal_state = 3             # 3 = VALID (matches DSO_FreqMeterState VALID)
        self.cal_pct = 100
        self.ets_active = False
        self.ets_building = False
        self.adc_bits = 8
        self.sample_format = "8BIT_HIGH_SPEED"
        self.heap_free = 148 * 1024
        self.fw_ver = "V8.4"
        self.hw_rev = "H7"

        # external frequency meter
        self.freq_meter = {
            "gate_us": 1000, "edge_count": 1000, "ref_ticks": 200000,
            "overflow_count": 0, "last_edge_age_ms": 0,
        }
        self.ext_freq_hz = 0.0
        self.ext_freq_valid = False
        self.ext_freq_state = "STOPPED"

        # error / exception injection
        self._active_errors: list[DSOError] = []
        self._error_log: list[DSOError] = []
        self._link_up = True
        self._client_connected = False

    # ---- error / event injection ----------------------------------------

    def inject_error(self, code: DSOErrorCode, message: str = "") -> None:
        """Inject an error event, mirroring a real fault condition."""
        err = DSOError(code=code, message=message, ts_ms=self.ts)
        self._active_errors.append(err)
        self._error_log.append(err)
        if code == DSOErrorCode.ETH_LINK_DOWN:
            self._link_up = False
        if code == DSOErrorCode.TCP_DISCONNECT:
            self._client_connected = False
        if code == DSOErrorCode.OVR:
            self.overrun += 1
        if code == DSOErrorCode.PHASE_SYNC:
            self.phase_sync_err += 1
        if code == DSOErrorCode.AWG_UNDERRUN:
            self.awg.underrun = True

    def clear_errors(self, code: DSOErrorCode | None = None) -> None:
        if code is None:
            self._active_errors.clear()
        else:
            self._active_errors = [e for e in self._active_errors if e.code != code]
        if code in (None, DSOErrorCode.ETH_LINK_DOWN):
            self._link_up = True
        if code in (None, DSOErrorCode.AWG_UNDERRUN):
            self.awg.underrun = False

    def disconnect_ethernet(self) -> None:
        """Simulate an Ethernet cable unplug."""
        self.inject_error(DSOErrorCode.ETH_LINK_DOWN, "cable unplugged")

    def disconnect_tcp(self) -> None:
        """Simulate a TCP client disconnect."""
        self.inject_error(DSOErrorCode.TCP_DISCONNECT, "client closed")

    def connect_client(self) -> None:
        self._client_connected = True

    # ---- state machine ---------------------------------------------------

    def set_state(self, target: str) -> bool:
        target = target.upper()
        if target not in STATES:
            return False
        if target not in TRANSITIONS.get(self.state, set()):
            return False
        # high-risk transitions require confirmation (handled by caller)
        self.state = target
        return True

    def safe_mode(self) -> None:
        self.state = "SAFE"

    # ---- waveform config --------------------------------------------------

    def set_waveform(self, ch: int, kind: str, freq_hz: float = 1000.0,
                     amplitude_v: float = 1.0, offset_v: float = 0.0,
                     duty_pct: float = 50.0, noise_v: float = 0.1) -> None:
        model = self.ch1 if ch == 1 else self.ch2
        model.spec = WaveformSpec(kind=kind, freq_hz=freq_hz,
                                  amplitude_v=amplitude_v, offset_v=offset_v,
                                  duty_pct=duty_pct, noise_v=noise_v)

    def set_vdiv(self, ch: int, mv_per_div: int) -> None:
        model = self.ch1 if ch == 1 else self.ch2
        model.vdiv_mv = mv_per_div

    # ---- control command handling -----------------------------------------

    def send_cmd(self, cmd: str, params: dict | None = None) -> AckFrame:
        params = params or {}
        self.seq += 1
        ack = AckFrame(self.seq, cmd)

        try:
            if cmd == "set_trig_mode":
                mode = params.get("mode", 0)
                self.trig.mode = ("AUTO", "NORMAL", "SINGLE")[mode]
            elif cmd == "set_trig_slope":
                slope = params.get("slope", 0)
                self.trig.slope = ("RISE", "FALL", "BOTH")[slope]
            elif cmd == "set_trig_source":
                src = params.get("source", 0)
                self.trig.source = ("CH1", "CH2")[src]
            elif cmd == "set_trig_level":
                self.trig.level_v = float(params.get("level_v", 0.0))
            elif cmd == "set_trig_pos":
                self.trig.pos_pct = int(params.get("pos_pct", 50))
            elif cmd == "set_run":
                self.running = bool(params.get("running", True))
            elif cmd == "set_ch_coupling":
                ch = int(params.get("ch", 0))
                cpl = ("DC", "AC", "GND")[int(params.get("coupling", 0))]
                (self.ch1 if ch == 0 else self.ch2).coupling = cpl
            elif cmd == "set_ch_probe":
                ch = int(params.get("ch", 0))
                (self.ch1 if ch == 0 else self.ch2).probe = int(params.get("probe", 1))
            elif cmd == "set_ch_offset":
                ch = int(params.get("ch", 0))
                (self.ch1 if ch == 0 else self.ch2).offset_display_v = \
                    float(params.get("offset_v", 0.0))
            elif cmd == "set_ch_vdiv":
                ch = int(params.get("ch", 0))
                idx = int(params.get("vdiv_index", 0))
                # map index to a V/div ladder (10mV..1V, 1-2-5)
                ladder = [10, 20, 50, 100, 200, 500, 1000]
                (self.ch1 if ch == 0 else self.ch2).vdiv_mv = ladder[idx % len(ladder)]
            elif cmd == "set_acq_mode":
                self.acquire_mode = ("SAMPLE", "PEAK", "HIRES", "AVERAGE")[
                    int(params.get("mode", 0))]
            elif cmd == "set_awg_start":
                self.awg.running = True
            elif cmd == "set_awg_stop":
                self.awg.running = False
            elif cmd == "set_awg_wave":
                w = int(params.get("wave", 0))
                self.awg.wave = WAVE_KINDS[w]
            elif cmd == "set_awg_hz":
                self.awg.wave_hz = int(params.get("hz", 10000))
            elif cmd == "set_awg_duty":
                self.awg.duty_permille = int(params.get("permille", 500))
            elif cmd == "set_state":
                target = params.get("target", "LOCAL")
                if not self.set_state(target):
                    ack.result = "error"
                    ack.error = f"illegal transition {self.state}->{target}"
            elif cmd == "set_sw":
                sw = int(params.get("sw", -1))
                on = int(params.get("on", 0))
                if 0 <= sw < 14:
                    if on:
                        self.sw_bits |= 1 << sw
                    else:
                        self.sw_bits &= ~(1 << sw)
                else:
                    ack.result = "error"
                    ack.error = "bad_sw"
            elif cmd == "get_switch":
                ack.data = {"sw": self.sw_bits, "led": self.led_bits}
            elif cmd == "get_config":
                pass  # no-op, config is exposed via read_frame()
            elif cmd == "reboot":
                if self.state not in ("CONFIG", "RECOVERY"):
                    ack.result = "error"
                    ack.error = "reboot requires CONFIG/RECOVERY state"
            else:
                ack.result = "error"
                ack.error = f"unknown cmd {cmd}"
        except (IndexError, ValueError, KeyError) as e:
            ack.result = "error"
            ack.error = str(e)

        return ack

    # ---- telemetry assembly ----------------------------------------------

    def read_frame(self, advance: bool = True) -> TelemetryFrame:
        """Build and return the current telemetry frame."""
        if advance:
            self.ts += 1
            self.seq += 1

        time_per_pixel_ns = float(self.tb.sec_div_ns) / 100.0  # 10 div / 100? use 608/10

        ch1 = self._channel_frame(self.ch1, 1, time_per_pixel_ns)
        ch2 = self._channel_frame(self.ch2, 2, time_per_pixel_ns)

        # AWG may inject error if running with extreme freq (underrun model)
        if self.awg.running and self.awg.wave_hz > 400_000:
            self.awg.underrun = True

        frame = TelemetryFrame(
            seq=self.seq,
            ts=self.ts,
            state=self.state,
            ch1=ch1["ch"],
            ch2=ch2["ch"],
            tb={
                "sec_div_ns": self.tb.sec_div_ns,
                "sample_rate_hz": self.tb.sample_rate_hz,
                "eff_sample_rate_hz": float(self.tb.sample_rate_hz),
                "mode": self.tb.mode,
                "time_per_pixel_ns": time_per_pixel_ns,
            },
            trig={
                "mode": self.trig.mode,
                "source": self.trig.source,
                "slope": self.trig.slope,
                "level_v": self.trig.level_v,
                "pos_pct": self.trig.pos_pct,
                "triggered": self.trig.triggered,
                "level_enabled": self.trig.level_enabled,
            },
            awg=self.awg.to_dict(),
            status={
                "running": self.running,
                "overrun": self.overrun,
                "phase_sync_err": self.phase_sync_err,
                "cal_state": self.cal_state,
                "cal_pct": self.cal_pct,
                "ets_active": self.ets_active,
                "ets_building": self.ets_building,
                "ext_freq_hz": self.ext_freq_hz,
                "ext_freq_valid": self.ext_freq_valid,
                "ext_freq_state": self.ext_freq_state,
                "ext_freq_mode": "GATED_COUNT",
                "state": self.state,
                "link_up": self._link_up,
                "client_connected": self._client_connected,
                "errors": [e.to_dict() for e in self._active_errors],
            },
            freq_meter=self.freq_meter,
            sys={
                "fw_ver": self.fw_ver,
                "hw_rev": self.hw_rev,
                "mcu": "STM32H743",
                "sample_format": self.sample_format,
                "heap_free": self.heap_free,
                "adc_bits": self.adc_bits,
                "acquire_mode": self.acquire_mode,
                "average_depth": self.average_depth,
            },
        )
        return frame

    def _channel_frame(self, model: ChannelModel, ch: int,
                       tpp_ns: float) -> dict:
        """Build one channel's telemetry dict."""
        ys = generate_display_wave(model.spec, tpp_ns, DISPLAY_WIDTH,
                                   model.vdiv_mv, self._rng)
        meas = measure_wave(model.spec, tpp_ns, DISPLAY_WIDTH, model.vdiv_mv)

        ch_dict = {
            "y": ys,
            "env_top": None,
            "env_bot": None,
            "freq_hz": meas["freq_hz"],
            "freq_valid": meas["freq_valid"],
            "vpp_v": meas["vpp_v"],
            "vrms_v": meas["vrms_v"],
            "vmin_v": meas["vmin_v"],
            "vmax_v": meas["vmax_v"],
            "duty_pct": meas["duty_pct"],
            "vdiv_mv": model.vdiv_mv,
            "coupling": model.coupling,
            "probe": model.probe,
            "offset_v": model.offset_display_v,
            "scale_min_v": model.scale_min_v,
            "scale_max_v": model.scale_max_v,
            "ch2_present": model.present if ch == 2 else True,
            "adc_bits": self.adc_bits if ch == 1 else self.adc_bits,
            "recon_mode": 0,
            "display_quality": 0,
            "effective_sample_rate_hz": float(self.tb.sample_rate_hz),
        }
        return {"ch": ch_dict, "y": ys}
