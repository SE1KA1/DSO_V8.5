"""
dsosim — DSO_V8.5 device simulator v1.0

Scriptable, hardware-free device simulator that reproduces the DSO
telemetry/control protocol (接口协议 v0.9) with semantic fidelity to the
real firmware.

Usage:
    from dsosim import DSOSimulator
    dev = DSOSimulator()
    dev.set_waveform(ch=1, kind="sine", freq_hz=1000, amplitude_v=1.0)
    frame = dev.read_frame()
"""

from .device import DSOSimulator
from .protocol import TelemetryFrame, ControlCommand
from .errors import DSOErrorCode, DSOError

__version__ = "1.0.0"
__all__ = ["DSOSimulator", "TelemetryFrame", "ControlCommand",
           "DSOErrorCode", "DSOError"]
