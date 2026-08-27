"""
Tests for the DSOW binary waveform frame codec.

Focus:
  - golden-frame field correctness and CRC integrity
  - zero-copy / low-copy guarantees (memoryview payload, cached arrays)
  - corruption / CRC rejection
  - large-array handling without repeated copies
"""

import struct
import zlib

import pytest

from dsosim.frame import (FrameType, WaveformFrameDecoder, FrameError,
                          encode_frame, HEADER_SIZE, TRAILER_SIZE)
from dsosim import golden

DEC = WaveformFrameDecoder()


# ---- golden frame decoding -----------------------------------------------

def test_golden_raw_fields():
    frame = DEC.parse(bytes.fromhex(golden.GOLDEN_RAW_HEX))
    e = golden.GOLDEN_RAW_EXPECTED
    assert frame.frame_type == e["frame_type"]
    assert frame.channel == e["channel"]
    assert frame.bit_width == e["bit_width"]
    assert frame.sample_rate_hz == e["sample_rate_hz"]
    assert frame.tb_sec_div_ns == e["tb_sec_div_ns"]
    assert frame.time_per_pixel_ns == pytest.approx(e["time_per_pixel_ns"])
    assert frame.trigger_index == e["trigger_index"]
    assert frame.point_count == e["point_count"]
    assert frame.payload_size == e["payload_size"]
    assert frame.samples_array() == e["samples"]


def test_golden_raw_measurements():
    frame = DEC.parse(bytes.fromhex(golden.GOLDEN_RAW_HEX))
    e = golden.GOLDEN_RAW_EXPECTED
    m = frame.measurements
    assert m["freq_hz"] == pytest.approx(e["meas_freq_hz"])
    assert m["vpp_v"] == pytest.approx(e["meas_vpp_v"])
    assert m["vrms_v"] == pytest.approx(e["meas_vrms_v"])
    assert m["duty_pct"] == pytest.approx(e["meas_duty_pct"])


def test_golden_minmax_fields():
    frame = DEC.parse(bytes.fromhex(golden.GOLDEN_MINMAX_HEX))
    e = golden.GOLDEN_MINMAX_EXPECTED
    assert frame.frame_type == FrameType.MIN_MAX
    assert frame.point_count == e["point_count"]
    mins, maxs = frame.minmax_arrays()
    assert mins == e["mins"]
    assert maxs == e["maxs"]


def test_golden_fft_fields():
    frame = DEC.parse(bytes.fromhex(golden.GOLDEN_FFT_HEX))
    e = golden.GOLDEN_FFT_EXPECTED
    assert frame.frame_type == FrameType.FFT
    assert frame.channel == e["channel"]
    assert frame.fft_magnitude() == pytest.approx(e["magnitude"], abs=1e-6)


# ---- CRC integrity --------------------------------------------------------

def test_golden_crc_valid():
    for hx in (golden.GOLDEN_RAW_HEX, golden.GOLDEN_MINMAX_HEX,
               golden.GOLDEN_FFT_HEX):
        frame = DEC.parse(bytes.fromhex(hx))
        assert frame.crc32 != 0


def test_tampered_payload_rejected():
    buf = bytearray(bytes.fromhex(golden.GOLDEN_RAW_HEX))
    buf[HEADER_SIZE] ^= 0x01  # flip one payload byte
    with pytest.raises(FrameError, match="CRC"):
        DEC.parse(bytes(buf))


def test_tampered_header_rejected():
    buf = bytearray(bytes.fromhex(golden.GOLDEN_RAW_HEX))
    buf[10] ^= 0xFF  # corrupt sample_rate_hz in header
    with pytest.raises(FrameError, match="CRC"):
        DEC.parse(bytes(buf))


# ---- structural validation ------------------------------------------------

def test_bad_magic():
    buf = bytearray(bytes.fromhex(golden.GOLDEN_RAW_HEX))
    buf[0:4] = b"NOPE"
    with pytest.raises(FrameError, match="magic"):
        DEC.parse(bytes(buf))


def test_short_buffer():
    with pytest.raises(FrameError, match="short"):
        DEC.parse(b"\x00" * 10)


def test_payload_size_mismatch():
    # craft a frame where header point_count disagrees with payload size.
    # recompute CRC so we reach the payload_size validation (not CRC).
    buf = bytearray(bytes.fromhex(golden.GOLDEN_RAW_HEX))
    struct.pack_into("<I", buf, 26, 999)  # lie about point_count
    crc = zlib.crc32(buf[:34]) & 0xFFFFFFFF
    crc = zlib.crc32(bytes(buf[HEADER_SIZE:]), crc) & 0xFFFFFFFF
    struct.pack_into("<I", buf, 34, crc)
    with pytest.raises(FrameError, match="payload_size"):
        DEC.parse(bytes(buf))


# ---- zero-copy / low-copy -------------------------------------------------

def test_payload_is_memoryview_not_copy():
    buf = bytes.fromhex(golden.GOLDEN_RAW_HEX)
    frame = DEC.parse(buf)
    assert isinstance(frame.raw_bytes, memoryview)
    # zero-copy: payload view points into the same buffer
    assert frame.raw_bytes.tobytes() == buf[HEADER_SIZE:HEADER_SIZE + 16]


def test_samples_array_cached_no_recopy():
    buf = bytes.fromhex(golden.GOLDEN_RAW_HEX)
    frame = DEC.parse(buf)
    a1 = frame.samples_array()
    a2 = frame.samples_array()
    assert a1 is a2  # cached: same object, no re-allocation


def test_minmax_arrays_cached():
    buf = bytes.fromhex(golden.GOLDEN_MINMAX_HEX)
    frame = DEC.parse(buf)
    m1 = frame.minmax_arrays()
    m2 = frame.minmax_arrays()
    assert m1 is m2


def test_fft_magnitude_cached():
    buf = bytes.fromhex(golden.GOLDEN_FFT_HEX)
    frame = DEC.parse(buf)
    f1 = frame.fft_magnitude()
    f2 = frame.fft_magnitude()
    assert f1 is f2


def test_large_raw_frame_no_excess_copy():
    """A 100k-point RAW frame parses with constant-time header work and a
    single cached sample materialisation (no repeated copies)."""
    n = 100_000
    # uint16 values (encoder masks & 0xFFFF)
    samples = [i & 0xFFFF for i in range(n)]
    buf = encode_frame(channel=0, bit_width=8, sample_rate_hz=33333333,
                       tb_sec_div_ns=1000000, time_per_pixel_ns=1000.0,
                       trigger_index=50000, frame_type=FrameType.RAW,
                       samples=samples, measurements={})
    frame = DEC.parse(buf)
    assert frame.point_count == n
    assert len(frame.raw_bytes) == n * 2  # zero-copy view, no copy
    s = frame.samples_array()
    assert len(s) == n
    assert s[0] == 0 and s[-1] == (n - 1) & 0xFFFF


# ---- round-trip encoder/decoder -------------------------------------------

def test_round_trip_raw():
    meas = {"freq_hz": 1234.5, "vpp_v": 3.3, "vrms_v": 1.17, "duty_pct": 25.0,
            "vmin_v": -1.5, "vmax_v": 1.8}
    buf = encode_frame(channel=1, bit_width=10, sample_rate_hz=10000000,
                       tb_sec_div_ns=500000, time_per_pixel_ns=500.0,
                       trigger_index=7, frame_type=FrameType.RAW,
                       samples=[10, 20, 30, 40, 50], measurements=meas)
    frame = DEC.parse(buf)
    assert frame.channel == 1
    assert frame.bit_width == 10
    assert frame.sample_rate_hz == 10000000
    assert frame.trigger_index == 7
    assert frame.samples_array() == [10, 20, 30, 40, 50]
    assert frame.measurements["freq_hz"] == pytest.approx(1234.5)


def test_round_trip_minmax():
    buf = encode_frame(channel=0, bit_width=8, sample_rate_hz=1, tb_sec_div_ns=1,
                       time_per_pixel_ns=1.0, trigger_index=0,
                       frame_type=FrameType.MIN_MAX,
                       samples=[(5, 9), (6, 10), (7, 11)])
    frame = DEC.parse(buf)
    mins, maxs = frame.minmax_arrays()
    assert mins == [5, 6, 7]
    assert maxs == [9, 10, 11]
