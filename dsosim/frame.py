"""
Binary waveform frame codec (DSOW format) for the PC client.

Layout (all little-endian, fixed header + variable payload + fixed trailer):

    Offset  Size  Field
    0       4     magic            b'DSOW'
    4       1     version          1
    5       1     frame_type       0=RAW 1=MIN_MAX 2=FFT
    6       1     channel          0=CH1 1=CH2
    7       1     bit_width        8 or 10
    8       2     reserved         (pad, zero)
    10      4     sample_rate_hz   uint32
    14      4     tb_sec_div_ns    uint32
    18      4     time_per_pixel_ns float32
    22      4     trigger_index    int32
    26      4     point_count      uint32
    30      4     payload_size     uint32
    34      4     crc32            uint32 (over header[0:34]+payload+trailer)
    38      N     payload
    38+N   24     measurements     trailer (6 x float32)

    RAW     payload: uint16[point_count]
    MIN_MAX payload: uint16 min,max interleaved  [min0,max0,min1,max1,...]
    FFT     payload: float32[point_count]

    measurements trailer (24 bytes, 6 x float32 LE):
        freq_hz, vpp_v, vrms_v, duty_pct, vmin_v, vmax_v

The decoder is zero-copy for the header and payload (memoryview slices);
the sample arrays are materialised once and cached to avoid repeated
copying of large buffers.
"""

from __future__ import annotations

import struct
import zlib
from dataclasses import dataclass, field
from enum import IntEnum

MAGIC = b"DSOW"
VERSION = 1
HEADER_SIZE = 38
TRAILER_SIZE = 24  # 6 x float32
MEAS_TRAILER_FMT = "<6f"


class FrameType(IntEnum):
    RAW = 0
    MIN_MAX = 1
    FFT = 2


def element_size(frame_type: int, bit_width: int = 0) -> int:
    """Bytes per point in the payload."""
    if frame_type == FrameType.RAW:
        return 2  # uint16 (samples stored as 16-bit, low byte valid for 8-bit)
    if frame_type == FrameType.MIN_MAX:
        return 4  # uint16 min + uint16 max
    if frame_type == FrameType.FFT:
        return 4  # float32 magnitude
    raise ValueError(f"unknown frame type {frame_type}")


def _trailer_bytes(meas: dict) -> bytes:
    return struct.pack(MEAS_TRAILER_FMT,
                       meas.get("freq_hz", 0.0),
                       meas.get("vpp_v", 0.0),
                       meas.get("vrms_v", 0.0),
                       meas.get("duty_pct", 0.0),
                       meas.get("vmin_v", 0.0),
                       meas.get("vmax_v", 0.0))


# ---------------------------------------------------------------------------
# Encoder
# ---------------------------------------------------------------------------

def encode_frame(*, channel: int, bit_width: int, sample_rate_hz: int,
                 tb_sec_div_ns: int, time_per_pixel_ns: float,
                 trigger_index: int, frame_type: int,
                 samples, measurements: dict | None = None) -> bytes:
    """
    Build a binary waveform frame.

    `samples`:
      RAW     -> iterable of ints (uint16)
      MIN_MAX -> iterable of (min, max) pairs
      FFT     -> iterable of floats (magnitude)
    """
    ftype = int(frame_type)
    if ftype == FrameType.RAW:
        payload = struct.pack(f"<{len(samples)}H", *[int(s) & 0xFFFF for s in samples])
    elif ftype == FrameType.MIN_MAX:
        pairs = list(samples)
        payload = struct.pack(f"<{len(pairs) * 2}H",
                              *[v for p in pairs for v in (int(p[0]) & 0xFFFF, int(p[1]) & 0xFFFF)])
    elif ftype == FrameType.FFT:
        payload = struct.pack(f"<{len(samples)}f", *[float(s) for s in samples])
    else:
        raise ValueError(f"unknown frame type {ftype}")

    meas = measurements or {}
    trailer = _trailer_bytes(meas)
    payload_size = len(payload)
    point_count = len(samples) if ftype != FrameType.MIN_MAX else len(list(samples))

    header_prefix = struct.pack("<4sBBBBHIIfiII",  # placeholder for crc
                                MAGIC, VERSION, ftype, int(channel), int(bit_width),
                                0,
                                int(sample_rate_hz), int(tb_sec_div_ns),
                                float(time_per_pixel_ns), int(trigger_index),
                                int(point_count), int(payload_size))
    # header_prefix is 34 bytes: 4+1+1+1+1+2+4+4+4+4+4+4
    assert len(header_prefix) == 34, len(header_prefix)
    crc = zlib.crc32(header_prefix) & 0xFFFFFFFF
    crc = zlib.crc32(payload, crc) & 0xFFFFFFFF
    crc = zlib.crc32(trailer, crc) & 0xFFFFFFFF

    header = header_prefix + struct.pack("<I", crc)  # 34 + 4 = 38
    assert len(header) == HEADER_SIZE
    return header + payload + trailer


# ---------------------------------------------------------------------------
# Decoded frame (zero-copy / lazy)
# ---------------------------------------------------------------------------

@dataclass
class WaveformFrame:
    """Decoded waveform frame. Payload kept as memoryview; arrays lazily cached."""
    frame_type: int
    channel: int
    bit_width: int
    sample_rate_hz: int
    tb_sec_div_ns: int
    time_per_pixel_ns: float
    trigger_index: int
    point_count: int
    payload_size: int
    crc32: int
    _payload: memoryview = field(repr=False)
    _measurements: dict = field(default_factory=dict)

    _samples_cache: list = field(default=None, repr=False)
    _minmax_cache: tuple = field(default=None, repr=False)
    _fft_cache: list = field(default=None, repr=False)

    # ---- payload accessors (materialise once, cached) ---------------------

    @property
    def raw_bytes(self) -> memoryview:
        """Zero-copy view of the payload (bytes)."""
        return self._payload

    def samples_array(self) -> list:
        """RAW samples as uint16 list (built once, cached thereafter)."""
        if self.frame_type != FrameType.RAW:
            raise TypeError("not a RAW frame")
        if self._samples_cache is None:
            self._samples_cache = list(struct.unpack(
                f"<{self.point_count}H", self._payload))
        return self._samples_cache

    def minmax_arrays(self) -> tuple:
        """MIN_MAX frame -> (min_list, max_list), cached."""
        if self.frame_type != FrameType.MIN_MAX:
            raise TypeError("not a MIN_MAX frame")
        if self._minmax_cache is None:
            raw = list(struct.unpack(f"<{self.point_count * 2}H", self._payload))
            self._minmax_cache = (raw[0::2], raw[1::2])
        return self._minmax_cache

    def fft_magnitude(self) -> list:
        """FFT frame -> magnitude list (cached)."""
        if self.frame_type != FrameType.FFT:
            raise TypeError("not an FFT frame")
        if self._fft_cache is None:
            self._fft_cache = list(struct.unpack(
                f"<{self.point_count}f", self._payload))
        return self._fft_cache

    @property
    def measurements(self) -> dict:
        return self._measurements

    def to_dict(self) -> dict:
        d = {
            "frame_type": self.frame_type,
            "channel": self.channel,
            "bit_width": self.bit_width,
            "sample_rate_hz": self.sample_rate_hz,
            "tb_sec_div_ns": self.tb_sec_div_ns,
            "time_per_pixel_ns": self.time_per_pixel_ns,
            "trigger_index": self.trigger_index,
            "point_count": self.point_count,
            "crc32": hex(self.crc32),
            "measurements": self._measurements,
        }
        return d


class FrameError(ValueError):
    """Raised when a buffer fails structural/CRC validation."""


# ---------------------------------------------------------------------------
# Decoder (zero-copy)
# ---------------------------------------------------------------------------

class WaveformFrameDecoder:
    """Parse DSOW frames with minimal copying."""

    def parse(self, data) -> WaveformFrame:
        mv = memoryview(data).cast("B")
        if len(mv) < HEADER_SIZE:
            raise FrameError("buffer too short for header")

        (magic, version, ftype, channel, bit_width, _reserved,
         sample_rate_hz, tb_sec_div_ns, tpp_ns, trigger_index,
         point_count, payload_size) = struct.unpack_from("<4sBBBBHIIfiII", mv, 0)

        if magic != MAGIC:
            raise FrameError(f"bad magic {magic!r}")
        if version != VERSION:
            raise FrameError(f"unsupported version {version}")
        if ftype not in (FrameType.RAW, FrameType.MIN_MAX, FrameType.FFT):
            raise FrameError(f"unknown frame type {ftype}")

        total = HEADER_SIZE + payload_size + TRAILER_SIZE
        if len(mv) < total:
            raise FrameError(
                f"buffer too short: have {len(mv)}, need {total} "
                f"(payload={payload_size})")

        # zero-copy slices for CRC + payload
        header_prefix = mv[:34]
        payload_view = mv[HEADER_SIZE:HEADER_SIZE + payload_size]
        trailer_view = mv[HEADER_SIZE + payload_size:total]

        (crc_expected,) = struct.unpack_from("<I", mv, 34)
        crc = zlib.crc32(header_prefix) & 0xFFFFFFFF
        crc = zlib.crc32(payload_view, crc) & 0xFFFFFFFF
        crc = zlib.crc32(trailer_view, crc) & 0xFFFFFFFF
        if crc != crc_expected:
            raise FrameError(
                f"CRC mismatch: got {crc_expected:#010x}, calc {crc:#010x}")

        # sanity: point_count vs payload size
        esz = element_size(ftype)
        expected_payload = point_count * esz
        if expected_payload != payload_size:
            raise FrameError(
                f"payload_size mismatch: header says {payload_size}, "
                f"point_count*{esz}={expected_payload}")

        meas = dict(zip(
            ("freq_hz", "vpp_v", "vrms_v", "duty_pct", "vmin_v", "vmax_v"),
            struct.unpack_from(MEAS_TRAILER_FMT, trailer_view, 0)))

        return WaveformFrame(
            frame_type=ftype,
            channel=channel,
            bit_width=bit_width,
            sample_rate_hz=sample_rate_hz,
            tb_sec_div_ns=tb_sec_div_ns,
            time_per_pixel_ns=tpp_ns,
            trigger_index=trigger_index,
            point_count=point_count,
            payload_size=payload_size,
            crc32=crc_expected,
            _payload=payload_view,
            _measurements=meas,
        )
