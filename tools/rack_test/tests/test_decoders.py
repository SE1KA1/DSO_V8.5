import json
import unittest

from tools.rack_test import decoders
from tools.rack_test.config import RackConfig


class DecoderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        decoders.install_decoder_path(RackConfig().decoder_dir)
        import envelope  # noqa: F401  (loaded by the path installer)
        cls.envelope = envelope

    def test_classify_module_frame(self):
        line = self.envelope.build(
            "sht40", "module.data",
            {"temp_dezi": 245, "rh_centi": 4523, "temp_ok": 1,
             "hi_alarm": 0, "level": 0}, seq=1, ts_ms=1000)
        kind, obj, reason = decoders.classify_line(line)
        self.assertEqual(kind, "telemetry", reason)
        self.assertEqual(obj["module"], "sht40")
        self.assertEqual(decoders.decode_module(obj), [])

    def test_watchdog_is_not_schema_checked(self):
        line = self.envelope.build(
            "watchdog", "module.data",
            {"feed_en": 1, "kick_count": 1, "pfo": 0,
             "reset_cause": 0x7F, "uptime_ms": 1000}, seq=1, ts_ms=1000)
        kind, obj, reason = decoders.classify_line(line)
        self.assertEqual(kind, "telemetry", reason)
        self.assertEqual(decoders.decode_module(obj), [])

    def test_real_atecc_frame_fields(self):
        # rev is a hex string and device_id is extra; the reference validator
        # must accept the actual firmware frame.
        data = {"present": 1, "comm_ok": 1, "device_id": "Eep21025E269FA2E",
                "chip_info": {"serial4": "a2c4f16d", "part": "608B",
                              "rev": "00000200", "lock_config": 1,
                              "lock_data": 1},
                "login_state": "LOGGED_OUT", "fail_cnt": 0}
        line = self.envelope.build("atecc608b", "module.data", data,
                                   seq=1, ts_ms=1000)
        kind, obj, _ = decoders.classify_line(line)
        self.assertEqual(kind, "telemetry")
        self.assertEqual(decoders.decode_module(obj), [])

    def test_firmware_login_state_values(self):
        # Firmware reports NO_LICENSE / AUTHORIZED, not the spec enum.
        for value in ("NO_LICENSE", "AUTHORIZED"):
            data = {"present": 1, "comm_ok": 1,
                    "chip_info": {"serial4": "a2c4f16d", "part": "608B",
                                  "rev": "00000200", "lock_config": 1,
                                  "lock_data": 1},
                    "login_state": value, "fail_cnt": 0}
            line = self.envelope.build("atecc608b", "module.data", data,
                                       seq=1, ts_ms=1000)
            kind, obj, _ = decoders.classify_line(line)
            self.assertEqual(kind, "telemetry")
            self.assertEqual(decoders.decode_module(obj), [], value)

    def test_legacy_line(self):
        line = json.dumps({"protocolVersion": "1.0",
                           "messageType": "device.status",
                           "deviceId": "board-001", "seq": 1,
                           "payload": {}})
        kind, obj, _ = decoders.classify_line(line)
        self.assertEqual(kind, "legacy")

    def test_bad_json(self):
        kind, obj, reason = decoders.classify_line("not json")
        self.assertEqual(kind, "invalid")
        self.assertIn("bad json", reason)

    def test_unknown_module(self):
        line = json.dumps({"ver": "1.0", "type": "module.data",
                           "module": "mystery", "seq": 1, "ts_ms": 1,
                           "data": {}})
        kind, obj, reason = decoders.classify_line(line)
        self.assertEqual(kind, "invalid")
        self.assertIn("unknown module", reason)

    def test_bad_schema(self):
        line = self.envelope.build(
            "sht40", "module.data",
            {"temp_dezi": 245, "rh_centi": 4523, "temp_ok": 2,
             "hi_alarm": 0, "level": 0}, seq=1, ts_ms=1000)
        kind, obj, _ = decoders.classify_line(line)
        self.assertEqual(kind, "telemetry")
        self.assertTrue(decoders.decode_module(obj))


if __name__ == "__main__":
    unittest.main()
