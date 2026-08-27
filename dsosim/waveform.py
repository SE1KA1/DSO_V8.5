"""
Waveform synthesis for the device simulator.

Produces 608-point display-Y arrays (int16) matching the semantics of
DSO_Frame.y_coords[608] in the real firmware: one display column per
horizontal pixel, values mapped to the LCD's vertical pixel range.

The generated signal is computed against the DSO's *screen* time window
(tb_time_per_pixel_ns * 608), NOT a raw ADC sample buffer, so the PC
client rendering is pixel-accurate relative to the real device.
"""

from __future__ import annotations

import math
import random
from dataclasses import dataclass, field

# Display geometry — must match DSO_Frame layout (800x480 logical landscape).
DISPLAY_WIDTH = 608          # display columns (pixel columns with waveform data)
DISPLAY_HEIGHT = 480         # logical landscape height
Y_MIN = 0                    # top of screen
Y_MAX = DISPLAY_HEIGHT - 1   # bottom of screen
Y_MID = DISPLAY_HEIGHT // 2


@dataclass
class WaveformSpec:
    """Parameters describing one channel's signal."""
    kind: str = "sine"        # "sine" | "square" | "noise" | "triangle"
    freq_hz: float = 1000.0
    amplitude_v: float = 1.0   # peak-to-peak / 2 in volts
    offset_v: float = 0.0
    duty_pct: float = 50.0     # square duty cycle
    noise_v: float = 0.1       # gaussian noise sigma (for noise kind)
    phase_deg: float = 0.0


@dataclass
class ChannelModel:
    """Runtime model of one acquisition channel."""
    spec: WaveformSpec = field(default_factory=WaveformSpec)
    vdiv_mv: int = 1000         # mV/div
    offset_display_v: float = 0.0  # user vertical offset (V)
    coupling: str = "DC"        # DC | AC | GND
    probe: int = 1              # x1 | x10
    present: bool = True        # CH2 unavailable in 8-bit mode

    @property
    def scale_min_v(self) -> float:
        return -4.0 * self.vdiv_mv / 1000.0 + self.offset_display_v

    @property
    def scale_max_v(self) -> float:
        return 4.0 * self.vdiv_mv / 1000.0 + self.offset_display_v

    def voltage_to_y(self, v: float) -> int:
        """Map a voltage to a display Y coordinate (0=top, 479=bottom)."""
        span = self.scale_max_v - self.scale_min_v
        if span == 0.0:
            return Y_MID
        frac = (v - self.scale_min_v) / span
        y = Y_MIN + frac * (Y_MAX - Y_MIN)
        return int(round(max(Y_MIN, min(Y_MAX, y))))


def _signal_value(spec: WaveformSpec, t: float) -> float:
    """Return the instantaneous signal voltage at time t (seconds)."""
    w = 2.0 * math.pi * spec.freq_hz
    if spec.kind == "sine":
        return spec.amplitude_v * math.sin(w * t + math.radians(spec.phase_deg))
    if spec.kind == "triangle":
        phase = (spec.freq_hz * t + spec.phase_deg / 360.0) % 1.0
        # triangle wave: 0..1 ramp up then down
        if phase < 0.5:
            return spec.amplitude_v * (4.0 * phase - 1.0)
        return spec.amplitude_v * (3.0 - 4.0 * phase)
    if spec.kind == "square":
        phase = (spec.freq_hz * t + spec.phase_deg / 360.0) % 1.0
        high = phase < (spec.duty_pct / 100.0)
        return spec.amplitude_v if high else -spec.amplitude_v
    if spec.kind == "noise":
        return random.gauss(0.0, spec.noise_v)
    raise ValueError(f"unknown waveform kind: {spec.kind}")


def generate_display_wave(spec: WaveformSpec,
                          time_per_pixel_ns: float = 1000.0,
                          columns: int = DISPLAY_WIDTH,
                          vdiv_mv: int = 1000,
                          rng: random.Random | None = None) -> list[int]:
    """
    Generate a 608-point int16 display-Y array for one channel.

    `time_per_pixel_ns` is the horizontal timebase (ns per display column),
    matching DSO_Frame.tb_time_per_pixel_ns.  `vdiv_mv` is the vertical
    scale (mV/div) used to map voltage to display Y.
    """
    rng = rng or random.Random()
    ys: list[int] = []
    dt = time_per_pixel_ns * 1e-9
    for col in range(columns):
        t = col * dt
        v = _signal_value(spec, t)
        if spec.kind == "noise":
            # re-roll per column for crisp noise
            v = rng.gauss(0.0, spec.noise_v)
        ys.append(_voltage_to_y_direct(v, vdiv_mv))
    return ys


def _voltage_to_y_direct(v: float, vdiv_mv: int) -> int:
    """Voltage→Y mapping over a fixed 8-div vertical span."""
    span = 8.0 * vdiv_mv / 1000.0
    if span == 0.0:
        return Y_MID
    frac = (v + span / 2.0) / span
    return int(round(max(Y_MIN, min(Y_MAX, Y_MIN + frac * (Y_MAX - Y_MIN)))))


def measure_wave(spec: WaveformSpec, time_per_pixel_ns: float,
                 columns: int = DISPLAY_WIDTH, vdiv_mv: int = 1000) -> dict:
    """
    Compute the measurement bundle (freq, vpp, vrms, duty) for a signal.
    This mirrors DSO_Process_AnalyzeSamples() output semantics.
    """
    ys = generate_display_wave(spec, time_per_pixel_ns, columns, vdiv_mv)
    # recover approximate voltage range from Y (mid = 0)
    n = len(ys)
    vmin = float("inf")
    vmax = float("-inf")
    vsum = 0.0
    vsum2 = 0.0
    for y in ys:
        v = (y - Y_MID) / (Y_MAX - Y_MIN) * (8.0 * vdiv_mv / 1000.0)
        vmin = min(vmin, v)
        vmax = max(vmax, v)
        vsum += v
        vsum2 += v * v
    vpp = vmax - vmin
    mean = vsum / n
    vrms = math.sqrt(max(0.0, vsum2 / n - mean * mean))
    duty = spec.duty_pct if spec.kind == "square" else 50.0
    return {
        "freq_hz": spec.freq_hz,
        "freq_valid": spec.kind != "noise",
        "vpp_v": vpp,
        "vrms_v": vrms,
        "vmin_v": vmin,
        "vmax_v": vmax,
        "duty_pct": duty,
    }
