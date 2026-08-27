"""
AWG (arbitrary waveform generator) model.

Reproduces DSO_WavegenSnapshot / DSO_WavegenProfile semantics from
dso_wavegen_types.h. The AWG is an independent function generator on
PA4/DAC1 with its own wave/frequency/duty state.
"""

from dataclasses import dataclass

WAVE_KINDS = ("sine", "triangle", "square")
# Supported compile-time profiles (subset matching DSO_WAVEGEN_PROFILE_*)
PROFILES = {
    "sine_32_468k":    ("sine", 468000),
    "sine_64_234k":    ("sine", 234000),
    "triangle_32_250k": ("triangle", 250000),
    "square_32_250k":  ("square", 250000),
    "square_16_250k":  ("square", 250000),
}


@dataclass
class AWGState:
    running: bool = False
    wave: str = "sine"
    wave_hz: int = 10000
    duty_permille: int = 500
    underrun: bool = False
    path: str = "PA4/DAC1_CH1"
    tim_psc: int = 0
    tim_arr: int = 399

    @property
    def actual_update_hz(self) -> int:
        """DAC update rate — a square needs 2x the wave freq, others 32x LUT."""
        lut = 32
        if self.wave == "square":
            lut = 2
        return self.wave_hz * lut

    def to_dict(self) -> dict:
        return {
            "running": self.running,
            "wave": self.wave,
            "hz": self.wave_hz,
            "duty_permille": self.duty_permille,
            "underrun": self.underrun,
            "path": self.path,
            "actual_update_hz": self.actual_update_hz,
            "tim_psc": self.tim_psc,
            "tim_arr": self.tim_arr,
            "lut_count": 2 if self.wave == "square" else 32,
        }
