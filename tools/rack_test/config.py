"""Configuration for the rack test harness.

Precedence: built-in defaults < JSON file (``--config``) < explicit CLI flags.
Every threshold carries the firmware source it was taken from so the report can
show where a limit came from.
"""

from __future__ import annotations

import dataclasses
import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional

REPO_ROOT = Path(__file__).resolve().parents[2]

# Modules under test. ``watchdog`` is deliberately absent: its frames are only
# used for seq/ts continuity accounting.
MODULES = (
    "atecc608b",
    "sht40",
    "ina226_u10",
    "ina226_u44",
    "bgm_adc_dac",
    "pcal6524",
)
WATCHDOG = "watchdog"

# Firmware frame order inside one 1 s pass (User/dso/dso_module_upload.c).
ALL_MODULES = MODULES + (WATCHDOG,)


@dataclass
class RackConfig:
    # --- network -----------------------------------------------------------
    bind_host: str = "0.0.0.0"
    bind_port: int = 5005
    device_host: str = "192.168.100.50"
    device_port: int = 5005

    # --- paths -------------------------------------------------------------
    decoder_dir: str = str(REPO_ROOT / "module_telemetry" / "python_ref")
    state_dir: str = str(REPO_ROOT / "tools" / "rack_test" / "state")

    # --- receiver ----------------------------------------------------------
    max_conns: int = 8
    unknown_idle_timeout_s: float = 60.0
    max_line_bytes: int = 4096
    raw_max_bytes: int = 32 * 1024 * 1024
    raw_backups: int = 3
    raw_sample: int = 1            # write 1 of every N telemetry frames
    status_interval_s: float = 1.0
    summary_interval_s: float = 5.0
    storm_interval_s: float = 10.0
    expected_frame_rate: float = 7.0  # frames/s incl. watchdog

    # --- control -----------------------------------------------------------
    control_timeout_s: float = 20.0
    control_retries: int = 5
    control_retry_delay_s: float = 0.2
    control_wait_s: float = 20.0
    auth_user: str = ""
    auth_password: str = ""
    auth_ttl_s: int = 900
    allow_reboot: bool = False
    allow_license_clear: bool = False

    # --- scenario timing ---------------------------------------------------
    baseline_s: float = 60.0
    soak_s: float = 120.0
    settle_s: float = 3.0
    telemetry_wait_s: float = 20.0
    reboot_wait_s: float = 30.0

    # --- thresholds (latest firmware) --------------------------------------
    # sht40: dso_sht40.h:30-36 (centi °C -> deci °C)
    sht_warn_deci: int = 580       # >58.0 °C = level 1
    sht_alarm_deci: int = 600      # >60.0 °C = level 2
    sht_emerg_deci: int = 650      # >65.0 °C = level 3
    sht_near_warn_deci: int = 550  # informational WARN band
    # ina226_u10: dso_ina226.h:33-39 (LSB / 40 = mA)
    ina10_warn_ma: int = 59        # 2380 LSB -> 59.5 mA
    ina10_alert_ma: int = 70       # 2800 LSB
    ina10_emerg_ma: int = 105      # 4200 LSB
    ina10_restore_ma: int = 56     # 2240 LSB
    # ina226_u44: dso_ina226_u44.h:28-33 (LSB / 8 = mA)
    ina44_warn_ma: int = 136       # 1088 LSB
    ina44_alert_ma: int = 160      # 1280 LSB
    ina44_emerg_ma: int = 240      # 1920 LSB
    ina44_restore_ma: int = 128    # 1024 LSB
    # continuity
    max_gap_ms: int = 3000
    warn_gap_ms: int = 5000
    # bgm_adc_dac: dso_freq_meter.c:13-14
    bgm_no_signal_ms: int = 2500

    # --- optional physical stimulus ---------------------------------------
    freq_hz: Optional[float] = None   # set when a generator feeds PB15

    # --- run bookkeeping ---------------------------------------------------
    operator: str = ""
    note: str = ""
    firmware: str = ""

    # ------------------------------------------------------------------ utils
    def to_dict(self) -> Dict[str, Any]:
        return dataclasses.asdict(self)

    def redacted_dict(self) -> Dict[str, Any]:
        """Config for logs/reports: never persist the auth password."""
        data = self.to_dict()
        if data.get("auth_password"):
            data["auth_password"] = "***"
        return data

    def validate(self) -> List[str]:
        errs: List[str] = []
        if not 0 <= int(self.bind_port) <= 65535:
            errs.append("bind_port out of range")
        if not 1 <= int(self.device_port) <= 65535:
            errs.append("device_port out of range")
        if int(self.max_conns) < 2:
            errs.append("max_conns must be >= 2 (telemetry + legacy)")
        if int(self.max_line_bytes) < 512:
            errs.append("max_line_bytes must be >= 512")
        if int(self.raw_sample) < 1:
            errs.append("raw_sample must be >= 1")
        if float(self.baseline_s) < 5:
            errs.append("baseline_s must be >= 5")
        if float(self.soak_s) < 0:
            errs.append("soak_s must be >= 0")
        if int(self.max_gap_ms) <= 0 or int(self.warn_gap_ms) < int(self.max_gap_ms):
            errs.append("gap thresholds invalid")
        if not Path(self.decoder_dir).is_dir():
            errs.append("decoder_dir does not exist: %s" % self.decoder_dir)
        return errs


def defaults() -> RackConfig:
    return RackConfig()


def from_dict(data: Dict[str, Any], base: Optional[RackConfig] = None) -> RackConfig:
    cfg = base or RackConfig()
    known = {f.name for f in dataclasses.fields(RackConfig)}
    data = {k: v for k, v in data.items() if not k.startswith("_")}
    unknown = sorted(set(data) - known)
    if unknown:
        raise ValueError("unknown config keys: %s" % ", ".join(unknown))
    for key, value in data.items():
        setattr(cfg, key, value)
    return cfg


def load(path: str, base: Optional[RackConfig] = None) -> RackConfig:
    with open(path, "r", encoding="utf-8") as fh:
        data = json.load(fh)
    if not isinstance(data, dict):
        raise ValueError("config file must contain a JSON object")
    return from_dict(data, base)


def example_config() -> Dict[str, Any]:
    """Values for ``config/rack.example.json`` (kept in sync with defaults)."""
    cfg = RackConfig()
    cfg.bind_host = "0.0.0.0"
    cfg.state_dir = "/var/lib/dso_rack_test"
    cfg.decoder_dir = "/opt/dso_rack_test/module_telemetry/python_ref"
    data = cfg.to_dict()
    data["_comment"] = (
        "Rack config: the server must own 192.168.100.100/24 on the board's L2 "
        "segment. Watchdog module is intentionally excluded from MODULES."
    )
    return data
