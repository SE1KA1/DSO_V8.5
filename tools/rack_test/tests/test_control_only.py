import tempfile
import time
import unittest
from pathlib import Path

from tools.rack_test import checks
from tools.rack_test.config import RackConfig
from tools.rack_test.control import RackControl
from tools.rack_test.fakedev import FakeDevice
from tools.rack_test.integrity import IntegrityTracker
from tools.rack_test.scenario import Scenario
from tools.rack_test.stats import RunStats


class ControlOnlyTests(unittest.TestCase):
    def setUp(self):
        self.fake = FakeDevice(host="127.0.0.1", port=1,
                               control_host="127.0.0.1", control_port=0,
                               interval=0.1, log=lambda *_: None)
        self.fake.start()
        if not self.fake.wait_ready(5.0):
            raise RuntimeError("fake control server did not start")
        host, port = self.fake.control_address
        cfg = RackConfig()
        cfg.device_host = host
        cfg.device_port = port
        cfg.control_timeout_s = 3.0
        cfg.control_retries = 2
        cfg.control_retry_delay_s = 0.05
        cfg.control_wait_s = 3.0
        self.cfg = cfg
        self.tmp = tempfile.TemporaryDirectory()
        self.run_dir = Path(self.tmp.name)
        self.stats = RunStats(cfg)
        self.integrity = IntegrityTracker()
        self.control = RackControl(cfg, log=lambda *_: None)
        self.scenario = Scenario(cfg, None, self.control, self.run_dir,
                                 self.stats, self.integrity,
                                 log=lambda *_: None)

    def tearDown(self):
        self.control.close()
        self.fake.stop()
        self.tmp.cleanup()

    def test_control_only_passes_without_telemetry(self):
        summary = self.scenario.run_control_only(do_sweep=True,
                                                 do_reboot=False)
        by_id = {c["id"]: c for c in summary["checks"]}
        for cid in ("PCI-04", "PCI-05", "PCI-06", "PCI-07",
                    "ATECC-02", "ATECC-03", "ATECC-04", "ATECC-05"):
            self.assertEqual(by_id[cid]["status"], checks.PASS, cid)
        self.assertEqual(by_id["INT-01"]["status"], checks.SKIP)
        self.assertEqual(by_id["PCI-01"]["status"], checks.SKIP)
        self.assertEqual(summary["counts"][checks.FAIL], 0)
        self.assertEqual(summary["environment"]["mode"], "control-only")
        self.assertFalse(summary["environment"]["telemetry_seen"])
        # The sweep is verified by ack + get_switch, not telemetry.
        self.assertFalse(summary["sweep"]["telemetry_checked"])
        self.assertTrue(summary["sweep"]["restored"])
        self.assertTrue((self.run_dir / "summary.json").exists())
        self.assertTrue((self.run_dir / "human_todo.json").exists())


if __name__ == "__main__":
    unittest.main()
