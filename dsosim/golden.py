"""
Golden frames for the DSOW binary format.

These byte strings are LOCKED. Any change to the frame layout (magic,
field offsets, CRC algorithm, element size) will break decoding and the
golden tests will fail — that is intentional: it prevents silent format
drift between the DSO firmware/encoder and the PC decoder.
"""

from .frame import FrameType

# Golden RAW frame: CH1, 8-bit, 8 points
GOLDEN_RAW_HEX = (
    "44534f5701000008000055a0fc0140420f0000007a44040000000800000010000000"
    "c1f002e08000c8002c019001f40190012c01c8000000fa4400000040f4fd343f0000"
    "4842000080bf0000803f"
)

# Golden MIN_MAX frame: CH1, 10-bit, 4 min/max pairs
GOLDEN_MINMAX_HEX = (
    "44534f570101000a000080969800400d030000007a44020000000400000010000000"
    "4cd124ec6400c8009600fa00c8002c01fa005e010000fa4400000040f4fd343f0000"
    "4842000080bf0000803f"
)

# Golden FFT frame: CH2, 10-bit, 4 magnitude floats
GOLDEN_FFT_HEX = (
    "44534f570102010a000055a0fc0140420f0000007a44000000000400000010000000"
    "533416a30000803f0000003f0000803e0000003e0000fa4400000040f4fd343f0000"
    "4842000080bf0000803f"
)

# Expected decoded values for the golden frames.
GOLDEN_RAW_EXPECTED = {
    "frame_type": FrameType.RAW,
    "channel": 0,
    "bit_width": 8,
    "sample_rate_hz": 33333333,
    "tb_sec_div_ns": 1000000,
    "time_per_pixel_ns": 1000.0,
    "trigger_index": 4,
    "point_count": 8,
    "payload_size": 16,
    "samples": [128, 200, 300, 400, 500, 400, 300, 200],
    "meas_freq_hz": 2000.0,
    "meas_vpp_v": 2.0,
    "meas_vrms_v": 0.707,
    "meas_duty_pct": 50.0,
}

GOLDEN_MINMAX_EXPECTED = {
    "frame_type": FrameType.MIN_MAX,
    "channel": 0,
    "bit_width": 10,
    "sample_rate_hz": 10000000,
    "tb_sec_div_ns": 200000,
    "time_per_pixel_ns": 1000.0,
    "trigger_index": 2,
    "point_count": 4,
    "payload_size": 16,
    "mins": [100, 150, 200, 250],
    "maxs": [200, 250, 300, 350],
}

GOLDEN_FFT_EXPECTED = {
    "frame_type": FrameType.FFT,
    "channel": 1,
    "bit_width": 10,
    "sample_rate_hz": 33333333,
    "tb_sec_div_ns": 1000000,
    "time_per_pixel_ns": 1000.0,
    "trigger_index": 0,
    "point_count": 4,
    "payload_size": 16,
    "magnitude": [1.0, 0.5, 0.25, 0.125],
}
