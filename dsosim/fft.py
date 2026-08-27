"""
FFT computation for the simulator.

The real DSO firmware does NOT implement FFT — the PC client computes it
from waveform Y-coordinates. The simulator reproduces this: it exposes
the same waveform data, and this module provides a reference FFT the PC
client or tests can use.
"""

from __future__ import annotations

import cmath
import math


def fft_magnitude(samples: list[float]) -> list[float]:
    """Radix-2 iterative Cooley-Tukey FFT, returns magnitude spectrum."""
    n = len(samples)
    if n & (n - 1):
        raise ValueError("sample count must be a power of two")

    buf = [complex(s) for s in samples]

    # bit-reversal permutation
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j & bit:
            j ^= bit
            bit >>= 1
        j ^= bit
        if i < j:
            buf[i], buf[j] = buf[j], buf[i]

    # butterfly
    length = 2
    while length <= n:
        ang = 2.0 * math.pi / length
        wlen = complex(math.cos(ang), -math.sin(ang))
        for i in range(0, n, length):
            w = complex(1.0, 0.0)
            for k in range(length // 2):
                u = buf[i + k]
                v = buf[i + k + length // 2] * w
                buf[i + k] = u + v
                buf[i + k + length // 2] = u - v
                w *= wlen
        length <<= 1

    return [abs(x) / n for x in buf]


def spectrum_db(wave: list[int], sample_rate_hz: float) -> tuple[list[float], list[float]]:
    """
    Return (freq_bins_hz, magnitude_db) for a 2048-point FFT.
    Mirrors the PC-side FFT page (DSO_FFT_SIZE = 2048).
    """
    n = 2048
    if len(wave) < n:
        wave = (wave * (n // len(wave) + 1))[:n]
    else:
        wave = wave[:n]

    # center the waveform (remove DC) to match AC-coupled FFT semantics
    mean = sum(wave) / n
    centered = [w - mean for w in wave]

    mag = fft_magnitude(centered)
    half = mag[: n // 2]
    db = [20.0 * math.log10(max(m, 1e-12)) for m in half]
    freq = [i * sample_rate_hz / n for i in range(n // 2)]
    return freq, db
