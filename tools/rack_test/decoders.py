"""Load the reference telemetry codecs and classify incoming lines.

``module_telemetry/python_ref`` is not a package: every module does a bare
``import envelope``.  We therefore put that directory on ``sys.path`` once and
import the modules by name.  This is the same usage pattern documented in
``module_telemetry/python_ref/README.md``.
"""

from __future__ import annotations

import json
import sys
import threading
from pathlib import Path
from typing import Any, Dict, Optional, Tuple

from .config import ALL_MODULES, MODULES, WATCHDOG

_lock = threading.Lock()
_loaded = False
_envelope = None
_modules: Dict[str, Any] = {}
_decoder_dir: Optional[str] = None


class DecoderError(RuntimeError):
    pass


def install_decoder_path(path: str) -> None:
    """Insert ``path`` on ``sys.path`` (if needed) and import all codecs."""
    global _loaded, _envelope, _modules, _decoder_dir
    resolved = str(Path(path).resolve())
    with _lock:
        if resolved not in sys.path:
            sys.path.insert(0, resolved)
        if _loaded and _decoder_dir == resolved:
            return
        try:
            import envelope  # type: ignore
            import module_atecc608b  # type: ignore
            import module_bgm_adc_dac  # type: ignore
            import module_ina226_u10  # type: ignore
            import module_ina226_u44  # type: ignore
            import module_pcal6524  # type: ignore
            import module_sht40  # type: ignore
            import module_watchdog  # type: ignore
        except Exception as exc:  # pragma: no cover - import failures are loud
            raise DecoderError("cannot import reference codecs from %s: %s"
                               % (resolved, exc)) from exc
        _envelope = envelope
        _modules = {
            module_atecc608b.MODULE: module_atecc608b,
            module_bgm_adc_dac.MODULE: module_bgm_adc_dac,
            module_ina226_u10.MODULE: module_ina226_u10,
            module_ina226_u44.MODULE: module_ina226_u44,
            module_pcal6524.MODULE: module_pcal6524,
            module_sht40.MODULE: module_sht40,
            module_watchdog.MODULE: module_watchdog,
        }
        missing = [m for m in ALL_MODULES if m not in _modules]
        if missing:
            raise DecoderError("reference codecs missing modules: %s"
                               % ", ".join(missing))
        _decoder_dir = resolved
        _loaded = True


def _require() -> None:
    if not _loaded:
        raise DecoderError("decoders not installed; call install_decoder_path()")


def decoder_dir() -> Optional[str]:
    return _decoder_dir


# ---------------------------------------------------------------------------
# classification
# ---------------------------------------------------------------------------

def classify_line(line: str) -> Tuple[str, Optional[dict], str]:
    """Return ``(kind, obj, reason)``.

    kind is one of ``"telemetry"``, ``"legacy"``, ``"invalid"``.
    """
    _require()
    try:
        obj = json.loads(line)
    except (ValueError, TypeError) as exc:
        return ("invalid", None, "bad json: %s" % exc)
    if not isinstance(obj, dict):
        return ("invalid", None, "not a json object")

    # Legacy v1.0 protocol / v0.9 ack.
    if obj.get("protocolVersion") or obj.get("messageType"):
        return ("legacy", obj, "")
    if obj.get("ver") == "0.9" and obj.get("type") in ("ack", "event"):
        return ("legacy", obj, "")

    # Spec-00 module envelope.
    if obj.get("ver") == "1.0" and "module" in obj and "type" in obj:
        errs = _envelope.validate(obj)
        if errs:
            return ("invalid", obj, "; ".join(errs))
        if obj.get("type") not in ("module.data", "module.event"):
            return ("invalid", obj, "unsupported type %r" % obj.get("type"))
        module = obj.get("module")
        if module not in ALL_MODULES:
            return ("invalid", obj, "unknown module %r" % (module,))
        return ("telemetry", obj, "")

    return ("invalid", obj, "unrecognized line")


# The firmware reports its own login vocabulary; the spec/codec enum differs.
# Values are defined in User/dso/dso_auth_soft.c (s_login).
_FIRMWARE_LOGIN_STATES = {"NO_LICENSE": "LOGGED_OUT",
                          "AUTHORIZED": "LOGGED_IN"}


def decode_module(frame: dict) -> list:
    """Validate ``frame['data']`` with the module codec (watchdog excepted).

    Returns a list of human-readable errors; empty means valid.
    The envelope must already have been validated by the caller.
    """
    _require()
    module = frame.get("module")
    if module == WATCHDOG:
        # Continuity-only frame: do not apply the watchdog schema (its
        # reset_cause can carry bits the reference validator does not expect).
        return []
    codec = _modules.get(module)
    if codec is None:
        return ["unknown module %r" % (module,)]
    data = frame.get("data")
    if frame.get("type") == "module.data":
        validator = getattr(codec, "validate_data", None)
        if validator is None:
            # The ATECC codec names its data validator validate_status.
            validator = getattr(codec, "validate_status", None)
        if validator is None:
            return ["module %s has no data schema" % module]
        if module == "atecc608b" and isinstance(data, dict):
            login = data.get("login_state")
            if login in _FIRMWARE_LOGIN_STATES:
                patched = dict(data)
                patched["login_state"] = _FIRMWARE_LOGIN_STATES[login]
                return list(validator(patched))
        return list(validator(data))
    if frame.get("type") == "module.event":
        validator = getattr(codec, "validate_event", None)
        if validator is None:
            return ["module %s has no event schema" % module]
        return list(validator(data))
    return ["unsupported type %r" % frame.get("type")]


def envelope_validate(frame: dict) -> list:
    _require()
    return list(_envelope.validate(frame))


def known_modules() -> Tuple[str, ...]:
    return MODULES
