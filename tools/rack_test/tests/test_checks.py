import unittest

from tools.rack_test import checks
from tools.rack_test.config import MODULES, RackConfig


def slot(count=60, max_gap=1000, last=None, fields=None, events=0):
    return {"count": count, "events": events, "bad_frames": 0,
            "first_wall_ms": 0, "last_wall_ms": 60000,
            "last_ts_ms": 60000, "max_gap_ms": max_gap,
            "fields": fields or {}, "last_data": last or {}}


def snapshot(**overrides):
    modules = {m: slot() for m in MODULES}
    modules.update(overrides)
    return {
        "modules": modules,
        "events": {},
        "bad_frames": 0,
        "invalid_lines": 0,
        "oversize_lines": 0,
        "frame_rate": 7.0,
        "watchdog_frames": 60,
        "connections": {"reconnects": 0, "a2_accepts": 100,
                        "max_concurrent": 2, "rejected": 0, "a2_rst": 100},
    }


def field(low, high, last=None, count=60):
    return {"min": low, "max": high, "sum": (low + high) * count // 2,
            "count": count, "last": high if last is None else last}


class ChecksTests(unittest.TestCase):
    def setUp(self):
        self.cfg = RackConfig()

    def _find(self, results, cid):
        for item in results:
            if item.id == cid:
                return item
        raise AssertionError("missing check %s" % cid)

    # ---------------------------------------------------------------- sht40
    def test_sht40_pass(self):
        stats = snapshot(sht40=slot(last={"temp_ok": 1, "level": 0,
                                          "hi_alarm": 0, "rh_centi": 4500},
                                    fields={"temp_dezi": field(240, 250),
                                            "rh_centi": field(4500, 4550)}))
        results = checks.module_checks(self.cfg, stats, stats)
        self.assertEqual(self._find(results, "SHT-01").status, checks.PASS)

    def test_sht40_invalid_fails(self):
        stats = snapshot(sht40=slot(last={"temp_ok": 0, "level": 0},
                                    fields={"temp_dezi": field(240, 250)}))
        results = checks.module_checks(self.cfg, stats, stats)
        self.assertEqual(self._find(results, "SHT-01").status, checks.FAIL)

    def test_sht40_level_fails(self):
        stats = snapshot(sht40=slot(last={"temp_ok": 1, "level": 1},
                                    fields={"temp_dezi": field(580, 590)}))
        results = checks.module_checks(self.cfg, stats, stats)
        self.assertEqual(self._find(results, "SHT-01").status, checks.FAIL)

    def test_sht40_near_warning(self):
        stats = snapshot(sht40=slot(last={"temp_ok": 1, "level": 0},
                                    fields={"temp_dezi": field(550, 560)}))
        results = checks.module_checks(self.cfg, stats, stats)
        self.assertEqual(self._find(results, "SHT-01").status, checks.WARN)

    # ---------------------------------------------------------------- INA
    def test_ina10_pass_and_warn(self):
        stats = snapshot(ina226_u10=slot(
            last={"level": 0, "over_cur": 0, "valid": 1},
            fields={"cur_ma": field(49, 51)}))
        results = checks.module_checks(self.cfg, stats, stats)
        self.assertEqual(self._find(results, "INA10-01").status, checks.PASS)
        stats = snapshot(ina226_u10=slot(
            last={"level": 0, "over_cur": 0, "valid": 1},
            fields={"cur_ma": field(56, 58)}))
        results = checks.module_checks(self.cfg, stats, stats)
        self.assertEqual(self._find(results, "INA10-01").status, checks.WARN)

    def test_ina10_overcurrent_fails(self):
        stats = snapshot(ina226_u10=slot(
            last={"level": 1, "over_cur": 1, "valid": 1},
            fields={"cur_ma": field(60, 62)}))
        results = checks.module_checks(self.cfg, stats, stats)
        self.assertEqual(self._find(results, "INA10-01").status, checks.FAIL)

    def test_ina44_boundaries(self):
        stats = snapshot(ina226_u44=slot(
            last={"level": 0, "over_cur": 0, "valid": 1},
            fields={"cur_ma": field(118, 120)}))
        results = checks.module_checks(self.cfg, stats, stats)
        self.assertEqual(self._find(results, "INA44-01").status, checks.PASS)
        stats = snapshot(ina226_u44=slot(
            last={"level": 0, "over_cur": 0, "valid": 1},
            fields={"cur_ma": field(130, 135)}))
        results = checks.module_checks(self.cfg, stats, stats)
        self.assertEqual(self._find(results, "INA44-01").status, checks.WARN)
        stats = snapshot(ina226_u44=slot(
            last={"level": 2, "over_cur": 1, "valid": 1},
            fields={"cur_ma": field(160, 170)}))
        results = checks.module_checks(self.cfg, stats, stats)
        self.assertEqual(self._find(results, "INA44-01").status, checks.FAIL)

    # ---------------------------------------------------------------- BGM
    def test_bgm_no_signal(self):
        stats = snapshot(bgm_adc_dac=slot(
            last={"freq_valid": 0, "freq_state": 4, "freq_millihz": 0,
                  "gate_us": 0, "edge_count": 0, "ref_ticks": 0,
                  "overflow_count": 0, "last_edge_age_ms": 4000,
                  "overrange": 0, "sample_format": 0, "sample_rate": 0,
                  "overrun_count": 0},
            fields={"overrun_count": field(0, 0)}))
        results = checks.module_checks(self.cfg, stats, stats)
        self.assertEqual(self._find(results, "BGM-01").status, checks.PASS)

    def test_bgm_valid_without_source_fails(self):
        stats = snapshot(bgm_adc_dac=slot(
            last={"freq_valid": 1, "freq_state": 3, "freq_millihz": 1000000,
                  "gate_us": 1, "edge_count": 1, "ref_ticks": 1,
                  "overflow_count": 0, "last_edge_age_ms": 1,
                  "overrange": 0, "sample_format": 0, "sample_rate": 1,
                  "overrun_count": 0}))
        results = checks.module_checks(self.cfg, stats, stats)
        self.assertEqual(self._find(results, "BGM-01").status, checks.FAIL)

    # ---------------------------------------------------------------- PCAL
    def test_missing_module_fails(self):
        stats = snapshot(ina226_u10=slot(count=0, last={},
                                         fields={}),
                         sht40=slot(count=0, last={}, fields={}),
                         pcal6524=slot(count=0, last={}, fields={}))
        results = checks.module_checks(self.cfg, stats, stats)
        self.assertEqual(self._find(results, "INA10-01").status, checks.FAIL)
        self.assertEqual(self._find(results, "SHT-01").status, checks.FAIL)
        self.assertEqual(self._find(results, "PCI-01").status, checks.FAIL)

    def test_bgm_with_external_source(self):
        stats = snapshot(bgm_adc_dac=slot(
            last={"freq_valid": 1, "freq_state": 3, "freq_millihz": 1000000,
                  "gate_us": 100000, "edge_count": 100, "ref_ticks": 200,
                  "overflow_count": 0, "last_edge_age_ms": 5,
                  "overrange": 0, "sample_format": 0, "sample_rate": 1,
                  "overrun_count": 0},
            fields={"freq_millihz": field(999999, 1000000, last=1000000),
                    "overrun_count": field(0, 0)}))
        results = checks.module_checks(self.cfg, stats, stats, freq_hz=1000)
        self.assertEqual(self._find(results, "BGM-01").status, checks.PASS)
        results = checks.module_checks(self.cfg, stats, stats, freq_hz=2000)
        self.assertEqual(self._find(results, "BGM-01").status, checks.FAIL)

    def test_pcal_bit9_and_derived_bits(self):
        stats = snapshot(pcal6524=slot(
            last={"sw": 0x200, "yds": 0x06, "eth_link": 0, "usb_active": 0}))
        results = checks.module_checks(self.cfg, stats, stats)
        self.assertEqual(self._find(results, "PCI-01").status, checks.FAIL)
        self.assertEqual(self._find(results, "PCI-02").status, checks.PASS)
        self.assertEqual(self._find(results, "PCI-03").status, checks.PASS)

    def test_pcal_eth_link_consistency(self):
        stats = snapshot(pcal6524=slot(
            last={"sw": 0, "yds": 0x0E, "eth_link": 0, "usb_active": 0}))
        results = checks.module_checks(self.cfg, stats, stats)
        self.assertEqual(self._find(results, "PCI-02").status, checks.FAIL)

    # ---------------------------------------------------------------- ATECC
    def test_atecc_pass(self):
        info = {"serial4": "a2c4f16d", "part": "608B", "rev": "00000200",
                "lock_config": 1, "lock_data": 1}
        stats = snapshot(atecc608b=slot(
            last={"present": 1, "comm_ok": 1, "device_id": "Eep21025E269FA2E",
                  "chip_info": info, "login_state": "LOGGED_OUT",
                  "fail_cnt": 0}))
        results = checks.module_checks(self.cfg, stats, stats)
        self.assertEqual(self._find(results, "ATECC-01").status, checks.PASS)

    def test_atecc_digit_only_serial_and_unlocked(self):
        # The real chip reports serial4=01232916 (all digits) and 0/0 locks.
        info = {"serial4": "01232916", "part": "608B", "rev": "00006003",
                "lock_config": 0, "lock_data": 0}
        stats = snapshot(atecc608b=slot(
            last={"present": 1, "comm_ok": 1,
                  "device_id": "Eep21025E269FA2E",
                  "chip_info": info, "login_state": "NO_LICENSE",
                  "fail_cnt": 0}))
        results = checks.module_checks(self.cfg, stats, stats)
        item = self._find(results, "ATECC-01")
        self.assertEqual(item.status, checks.WARN)
        self.assertIn("unlocked", item.detail)

    def test_atecc_bad_serial_fails(self):
        info = {"serial4": "zzzzzzzz", "part": "608B", "rev": "0",
                "lock_config": 1, "lock_data": 1}
        stats = snapshot(atecc608b=slot(
            last={"present": 1, "comm_ok": 1, "chip_info": info,
                  "login_state": "NO_LICENSE", "fail_cnt": 0}))
        results = checks.module_checks(self.cfg, stats, stats)
        self.assertEqual(self._find(results, "ATECC-01").status, checks.FAIL)

    def test_switch_telemetry_missed_is_warn(self):
        sweep = {
            "baseline_sw": 0,
            "steps": [{"index": 0, "cmd_ok": True, "telemetry_ok": False,
                       "ok": False, "latency_ms": 100}],
            "invalid": [{"index": 14, "rejected": True,
                         "error": "bad_param"},
                        {"index": -1, "rejected": True,
                         "error": "bad_param"}],
            "idx9": {"ack_ok": True, "bit_changed": False},
            "restored": True,
            "final_sw": 0,
            "control_lost": False,
            "telemetry_checked": True,
        }
        results = checks.switch_checks(self.cfg, sweep)
        self.assertEqual(self._find(results, "PCI-04").status, checks.WARN)
        self.assertEqual(self._find(results, "PCI-05").status, checks.PASS)

    def test_switch_control_lost_skips_secondary_checks(self):
        sweep = {
            "baseline_sw": 0,
            "steps": [{"index": 0, "ok": False,
                       "error": "cannot connect: connection refused"}],
            "invalid": [],
            "idx9": {},
            "restored": False,
            "final_sw": None,
            "control_lost": True,
        }
        results = checks.switch_checks(self.cfg, sweep)
        self.assertEqual(self._find(results, "PCI-04").status, checks.FAIL)
        for cid in ("PCI-05", "PCI-06", "PCI-07"):
            self.assertEqual(self._find(results, cid).status, checks.SKIP, cid)

    def test_atecc_absent_fails(self):
        stats = snapshot(atecc608b=slot(
            last={"present": 0, "comm_ok": 0, "login_state": "LOGGED_OUT",
                  "fail_cnt": 0}))
        results = checks.module_checks(self.cfg, stats, stats)
        self.assertEqual(self._find(results, "ATECC-01").status, checks.FAIL)

    # ------------------------------------------------------------ integrity
    def test_integrity_clean(self):
        stats = snapshot()
        integ = {"seq_gaps": 0, "duplicates": 0, "reboots": 0, "epoch": 0,
                 "max_seq_gap": 0}
        results = checks.integrity_checks(self.cfg, stats, stats, integ)
        for cid in ("INT-01", "INT-02", "INT-03", "INT-04", "INT-05", "INT-06"):
            self.assertEqual(self._find(results, cid).status, checks.PASS, cid)

    def test_integrity_gap_without_reconnect_fails(self):
        stats = snapshot()
        integ = {"seq_gaps": 2, "duplicates": 0, "reboots": 0, "epoch": 0,
                 "max_seq_gap": 3}
        results = checks.integrity_checks(self.cfg, stats, stats, integ)
        self.assertEqual(self._find(results, "INT-03").status, checks.FAIL)

    def test_integrity_gap_with_reconnect_is_warn(self):
        stats = snapshot()
        stats["connections"]["reconnects"] = 1
        integ = {"seq_gaps": 2, "duplicates": 0, "reboots": 0, "epoch": 0,
                 "max_seq_gap": 3}
        results = checks.integrity_checks(self.cfg, stats, stats, integ)
        self.assertEqual(self._find(results, "INT-03").status, checks.WARN)

    def test_unplanned_reboot_fails(self):
        stats = snapshot()
        integ = {"seq_gaps": 0, "duplicates": 0, "reboots": 1, "epoch": 1,
                 "max_seq_gap": 0}
        results = checks.integrity_checks(self.cfg, stats, stats, integ,
                                          planned_reboots=0)
        self.assertEqual(self._find(results, "INT-04").status, checks.FAIL)
        results = checks.integrity_checks(self.cfg, stats, stats, integ,
                                          planned_reboots=1)
        self.assertEqual(self._find(results, "INT-04").status, checks.PASS)

    # ---------------------------------------------------------------- human
    def test_human_pending_and_answered(self):
        results = checks.human_checks(self.cfg, None)
        self.assertTrue(all(r.status == checks.PENDING for r in results))
        results = checks.human_checks(self.cfg, {"HUM-01": True,
                                                 "HUM-02": False})
        self.assertEqual(self._find(results, "HUM-01").status, checks.PASS)
        self.assertEqual(self._find(results, "HUM-02").status, checks.FAIL)


if __name__ == "__main__":
    unittest.main()
