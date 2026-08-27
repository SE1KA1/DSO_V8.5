"""
pytest test suite for dsosim v1.0.

Verifies semantic fidelity to the DSO protocol: waveform synthesis,
measurement consistency, state machine transitions, error injection, and
JSON frame schema compliance.
"""

import json

import pytest

from dsosim import DSOSimulator, DSOErrorCode
from dsosim.waveform import generate_display_wave, DISPLAY_WIDTH, WaveformSpec
from dsosim.fft import fft_magnitude, spectrum_db


# ---- waveform ------------------------------------------------------------

def test_sine_wave_has_expected_length():
    ys = generate_display_wave(WaveformSpec(kind="sine", freq_hz=1000),
                               time_per_pixel_ns=1000)
    assert len(ys) == DISPLAY_WIDTH
    assert all(0 <= y <= 479 for y in ys)


def test_square_wave_duty_cycle():
    spec = WaveformSpec(kind="square", freq_hz=100, duty_pct=50)
    ys = generate_display_wave(spec, time_per_pixel_ns=100_000)
    # half the samples above mid, half below (within tolerance)
    mid = 240
    above = sum(1 for y in ys if y > mid)
    below = sum(1 for y in ys if y < mid)
    assert abs(above - below) < DISPLAY_WIDTH * 0.1


def test_noise_is_reproducible_with_seed():
    dev1 = DSOSimulator(seed=7)
    dev2 = DSOSimulator(seed=7)
    f1 = dev1.read_frame()
    f2 = dev2.read_frame()
    assert f1.ch1["y"] == f2.ch1["y"]


def test_unknown_waveform_raises():
    with pytest.raises(ValueError):
        generate_display_wave(WaveformSpec(kind="bogus"), 1000)


# ---- measurement ---------------------------------------------------------

def test_measurement_sine_freq_reported():
    dev = DSOSimulator(seed=1)
    dev.set_waveform(1, "sine", freq_hz=2000, amplitude_v=1.0)
    frame = dev.read_frame()
    assert frame.ch1["freq_valid"] is True
    assert frame.ch1["freq_hz"] == pytest.approx(2000.0)


def test_measurement_noise_freq_invalid():
    dev = DSOSimulator(seed=1)
    dev.set_waveform(1, "noise")
    frame = dev.read_frame()
    assert frame.ch1["freq_valid"] is False


# ---- FFT ------------------------------------------------------------------

def test_fft_power_of_two():
    with pytest.raises(ValueError):
        fft_magnitude([1.0, 2.0, 3.0])


def test_fft_sine_single_bin():
    # a pure sine should concentrate energy in one bin
    import math
    n = 256
    samples = [math.sin(2 * math.pi * 8 * i / n) for i in range(n)]
    mag = fft_magnitude(samples)
    peak = max(range(n // 2), key=lambda i: mag[i])
    assert peak == 8


def test_spectrum_db_returns_2048_bins():
    dev = DSOSimulator(seed=1)
    frame = dev.read_frame()
    freq, db = spectrum_db(frame.ch1["y"], 33_333_333)
    assert len(freq) == 1024
    assert len(db) == 1024


# ---- state machine -------------------------------------------------------

def test_state_transition_legal():
    dev = DSOSimulator()
    assert dev.set_state("PC") is True
    assert dev.state == "PC"
    assert dev.set_state("CLOUD") is True
    assert dev.state == "CLOUD"


def test_state_transition_illegal():
    dev = DSOSimulator()
    # SAFE cannot go directly to CLOUD
    dev.safe_mode()
    assert dev.set_state("CLOUD") is False
    assert dev.state == "SAFE"


# ---- control commands ----------------------------------------------------

def test_set_run_command():
    dev = DSOSimulator()
    ack = dev.send_cmd("set_run", {"running": False})
    assert ack.result == "ok"
    assert dev.running is False
    frame = dev.read_frame()
    assert frame.status["running"] is False


def test_set_trig_mode_command():
    dev = DSOSimulator()
    dev.send_cmd("set_trig_mode", {"mode": 1})
    assert dev.trig.mode == "NORMAL"


def test_unknown_command_errors():
    dev = DSOSimulator()
    ack = dev.send_cmd("no_such_cmd")
    assert ack.result == "error"


def test_reboot_requires_safe_state():
    dev = DSOSimulator()
    ack = dev.send_cmd("reboot")
    assert ack.result == "error"


# ---- error injection -----------------------------------------------------

def test_inject_ethernet_link_down():
    dev = DSOSimulator()
    dev.disconnect_ethernet()
    frame = dev.read_frame()
    assert frame.status["link_up"] is False
    codes = [e["code"] for e in frame.status["errors"]]
    assert "E003" in codes


def test_inject_overrun_increments():
    dev = DSOSimulator()
    dev.inject_error(DSOErrorCode.OVR)
    dev.inject_error(DSOErrorCode.OVR)
    assert dev.overrun == 2


def test_clear_errors():
    dev = DSOSimulator()
    dev.disconnect_ethernet()
    dev.clear_errors(DSOErrorCode.ETH_LINK_DOWN)
    frame = dev.read_frame()
    assert frame.status["link_up"] is True


# ---- JSON protocol schema ------------------------------------------------

def test_frame_json_schema_complete():
    dev = DSOSimulator(seed=1)
    payload = dev.read_frame().to_dict()
    assert payload["ver"] == "0.9"
    for key in ("ch1", "ch2", "tb", "trig", "awg", "status", "sys", "freq_meter"):
        assert key in payload, f"missing {key}"
    # ch1 required fields
    for f in ("y", "freq_hz", "vpp_v", "vrms_v", "vdiv_mv", "coupling"):
        assert f in payload["ch1"]


def test_frame_y_is_608_points():
    dev = DSOSimulator(seed=1)
    frame = dev.read_frame()
    assert len(frame.ch1["y"]) == 608


def test_control_command_round_trip():
    from dsosim.protocol import ControlCommand
    cmd = ControlCommand.from_json('{"cmd":"set_run","params":{"running":true},"seq":5}')
    assert cmd.cmd == "set_run"
    assert cmd.params["running"] is True
    assert cmd.seq == 5


# ---- AWG ------------------------------------------------------------------

def test_awg_square_underrun_at_high_freq():
    dev = DSOSimulator()
    dev.send_cmd("set_awg_wave", {"wave": 2})  # square
    dev.send_cmd("set_awg_hz", {"hz": 500000})
    dev.send_cmd("set_awg_start")
    frame = dev.read_frame()
    assert frame.awg["running"] is True
    assert frame.awg["underrun"] is True
