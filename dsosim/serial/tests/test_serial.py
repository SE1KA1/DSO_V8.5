"""
Tests for the CH340 adapter demux, reset-reason parser, logger and CLI.

Hardware-free: the demux/parser/logger logic is exercised with synthetic
byte streams; the pyserial layer is validated via a loopback URL.
"""

import json
import os
import threading
import time

import pytest

from dsosim.serial.demux import (StreamDemux, parse_reset_reason,
                                 SerialLogger)
from dsosim.serial.adapter import CH340Adapter
from dsosim.serial import rescue
from dsosim.frame import encode_frame, FrameType
from dsosim.serial.cli import main as cli_main


# ---- demux: text/binary separation ----------------------------------------

def _frame_bytes() -> bytes:
    return encode_frame(channel=0, bit_width=8, sample_rate_hz=1000,
                        tb_sec_div_ns=1000000, time_per_pixel_ns=1000.0,
                        trigger_index=0, frame_type=FrameType.RAW,
                        samples=[1, 2, 3, 4], measurements={})


def test_demux_pure_text():
    d = StreamDemux()
    d.feed(b"[BOOT] UART1 direct OK\r\n[BOOT] SDRAM OK\r\n")
    lines = d.pop_text()
    assert [l.text for l in lines] == ["[BOOT] UART1 direct OK",
                                       "[BOOT] SDRAM OK"]
    assert d.pop_frames() == []


def test_demux_text_and_binary_not_mixed():
    d = StreamDemux()
    frame = _frame_bytes()
    d.feed(b"line1\r\n" + frame + b"line2\r\n")
    lines = [l.text for l in d.pop_text()]
    frames = d.pop_frames()
    assert lines == ["line1", "line2"]
    assert len(frames) == 1
    assert frames[0].data == frame


def test_demux_binary_split_across_feeds():
    """Frame arriving in 3 chunks must reassemble; text around it kept."""
    d = StreamDemux()
    frame = _frame_bytes()
    third = max(1, len(frame) // 3)
    d.feed(b"pre\r\n")
    d.feed(frame[:third])
    d.feed(frame[third:2 * third])
    d.feed(frame[2 * third:] + b"post\r\n")
    assert [l.text for l in d.pop_text()] == ["pre", "post"]
    frames = d.pop_frames()
    assert len(frames) == 1
    assert frames[0].data == frame


def test_demux_magic_inside_text_line():
    """A text line containing 'DSOW' followed by non-frame bytes must not
    produce a frame if the header is incomplete/invalid."""
    d = StreamDemux()
    d.feed(b"look DSOW at here\r\n")
    lines = d.pop_text()
    assert any("DSOW" in l.text for l in lines)
    assert d.pop_frames() == []


def test_demux_encoding_errors_replaced():
    """Invalid UTF-8 bytes in text never raise; they become U+FFFD."""
    d = StreamDemux()
    d.feed(b"bad \xff\xfe bytes\r\n")
    lines = d.pop_text()
    assert len(lines) == 1
    assert "�" in lines[0].text


def test_demux_partial_line_buffered():
    d = StreamDemux()
    d.feed(b"[BOOT] UART1 di")
    assert d.pop_text() == []
    d.feed(b"rect OK\r\n")
    assert [l.text for l in d.pop_text()] == ["[BOOT] UART1 direct OK"]


# ---- reset reason ---------------------------------------------------------

def test_reset_reason_hardfault():
    lines = ["[BOOT] UART1 direct OK",
             "Enter HardFault_Handler, System Halt."]
    r = parse_reset_reason(lines)
    assert r["reason"] == "HARD_FAULT"
    assert r["confidence"] == 1.0


def test_reset_reason_error_handler():
    r = parse_reset_reason(["[ERR] Error_Handler file=main.c line=361"])
    assert r["reason"] == "ERROR_HANDLER"


def test_reset_reason_clean_boot():
    r = parse_reset_reason(["[BOOT] UART1 direct OK", "[BOOT] SDRAM OK"])
    assert r["reason"] == "POWER_ON"


def test_reset_reason_unknown():
    r = parse_reset_reason([])
    assert r["reason"] == "UNKNOWN"


# ---- logger ---------------------------------------------------------------

def test_logger_separates_text_binary(tmp_path):
    path = tmp_path / "boot.jsonl"
    lg = SerialLogger(str(path))
    lg.log_text("hello", ts_ms=1)
    lg.log_binary(b"\x44\x53\x4f\x57", ts_ms=2)
    lg.close()
    with open(path, encoding="utf-8") as fp:
        recs = [json.loads(l) for l in fp if l.strip()]
    assert len(recs) == 2
    assert recs[0]["k"] == "text" and recs[0]["d"] == "hello"
    assert recs[1]["k"] == "bin" and recs[1]["d"] == "44534f57"


# ---- adapter via loopback URL --------------------------------------------

def test_adapter_loopback_echo():
    """pyserial loop:// echoes TX back to RX — validates continuous TX/RX."""
    a = CH340Adapter("loop://", baudrate=1152000)
    a.open()
    try:
        a.write_text("ping")
        got = a.read_line(timeout=2.0)
        assert got is not None
        assert got.text == "ping"
    finally:
        a.close()


def test_adapter_write_before_open_raises():
    a = CH340Adapter("loop://")
    with pytest.raises(RuntimeError):
        a.write_text("x")


def test_adapter_close_is_idempotent():
    a = CH340Adapter("loop://")
    a.open()
    a.close()
    a.close()  # second close must not raise


# ---- rescue presets -------------------------------------------------------

def test_rescue_presets_defined():
    names = rescue.rescue_commands()
    assert "reboot" in names
    assert "recover" in names
    for n in names:
        desc, cmd = rescue.RESCUE_COMMANDS[n]
        assert isinstance(desc, str) and isinstance(cmd, str)


# ---- CLI (no hardware → expected failures/exit codes) ---------------------

def test_cli_list_returns_ok():
    # list never needs hardware; must exit 0 even with zero adapters
    assert cli_main(["list"]) == 0


def test_cli_monitor_no_port_fails_gracefully():
    with pytest.raises(SystemExit) as e:
        cli_main(["monitor", "COM_NOT_EXIST", "-b", "1152000"])
    assert e.value.code == 1
