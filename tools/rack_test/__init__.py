"""Rack-side remote test harness for the DSO bridge-board modules.

Receives the board's ``module.*`` telemetry, validates it with the reference
codecs in ``module_telemetry/python_ref``, drives the board's v0.9 command
channel, runs a phased test scenario and writes a Markdown/JSON report.

Watchdog frames are carried in the stream only for ``seq``/``ts_ms``
continuity accounting; the watchdog module itself is explicitly out of scope.
"""

__version__ = "0.1.0"
