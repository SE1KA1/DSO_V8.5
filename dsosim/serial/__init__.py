"""
CH340 serial adapter — PC-side bridge to the DSO debug UART.

Usage:
    from dsosim.serial import CH340Adapter, discover_ch340

    ports = discover_ch340()          # find attached CH340 adapters
    a = CH340Adapter(ports[0]["port"], baudrate=1152000)
    a.open()
    line = a.read_line()
    a.write_text("help")
    a.close()

CLI:
    python -m dsosim.serial.cli list
    python -m dsosim.serial.cli monitor COM3
    python -m dsosim.serial.cli log COM3 -o boot.jsonl
    python -m dsosim.serial.cli send COM3 "reset"
    python -m dsosim.serial.cli reset-reason COM3
    python -m dsosim.serial.cli rescue COM3 recover
"""

from .adapter import CH340Adapter, discover_ch340, DEFAULT_BAUD
from .demux import StreamDemux, TextLine, BinaryFrame, parse_reset_reason, SerialLogger
from .rescue import RESCUE_COMMANDS, rescue_commands

__all__ = ["CH340Adapter", "discover_ch340", "DEFAULT_BAUD",
           "StreamDemux", "TextLine", "BinaryFrame",
           "parse_reset_reason", "SerialLogger",
           "RESCUE_COMMANDS", "rescue_commands"]
