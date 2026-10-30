import tempfile
import time
import unittest
from pathlib import Path

from tools.rack_test import decoders
from tools.rack_test.config import MODULES, RackConfig
from tools.rack_test.fakedev import FRAME_ORDER, FakeDevice
from tools.rack_test.integrity import IntegrityTracker
from tools.rack_test.receiver import RackReceiver
from tools.rack_test.stats import RunStats


def wait_for(predicate, timeout=5.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return True
        time.sleep(0.02)
    return False


class Stack:
    def __init__(self, inject=None):
        cfg = RackConfig()
        cfg.bind_host = "127.0.0.1"
        cfg.bind_port = 0
        cfg.status_interval_s = 3600
        cfg.summary_interval_s = 3600
        cfg.storm_interval_s = 3600
        decoders.install_decoder_path(cfg.decoder_dir)
        self.cfg = cfg
        self.tmp = tempfile.TemporaryDirectory()
        self.stats = RunStats(cfg)
        self.integrity = IntegrityTracker()
        self.receiver = RackReceiver(cfg, self.stats, self.integrity,
                                     Path(self.tmp.name), None, None,
                                     log=lambda *_: None)
        self.receiver.bind()
        self.receiver.start()
        self.receiver.wait_ready(3.0)
        self.fake = FakeDevice(host="127.0.0.1",
                               port=self.receiver.local_port,
                               control_port=0, interval=0.05,
                               inject=inject, log=lambda *_: None)
        self.fake.start()
        self.fake.wait_ready(5.0)

    def close(self):
        self.fake.stop()
        self.receiver.stop()
        self.tmp.cleanup()


class FakeDeviceTests(unittest.TestCase):
    def setUp(self):
        self.stack = Stack()
        self.assertTrue(self.stack.fake.wait_passes(3, 10.0))

    def tearDown(self):
        self.stack.close()

    def test_frame_order_and_shared_seq(self):
        frames = self.stack.fake.last_frames
        self.assertEqual([f["module"] for f in frames], list(FRAME_ORDER))
        seqs = [f["seq"] for f in frames]
        self.assertEqual(seqs, list(range(seqs[0], seqs[0] + len(seqs))))
        ts = {f["ts_ms"] for f in frames}
        self.assertEqual(len(ts), 1)

    def test_receiver_sees_all_modules(self):
        self.assertTrue(wait_for(
            lambda: all(self.stack.stats.snapshot()["modules"][m]["count"] >= 3
                        for m in MODULES), 5.0))
        snap = self.stack.stats.snapshot()
        self.assertGreaterEqual(snap["watchdog_frames"], 3)
        self.assertEqual(snap["bad_frames"], 0)
        self.assertEqual(self.stack.integrity.snapshot()["seq_gaps"], 0)

    def test_seq_gap_injection(self):
        fake = FakeDevice(inject=["seq_gap=3"])
        fake._faults["seq_gap"] = 0
        for _ in range(7):
            fake._next_seq()
        # The injection is applied after the first full pass (seq >= 7):
        # next seq would be 8, plus the 3-frame gap -> 11.
        fake._faults["seq_gap"] = 3
        self.assertEqual(fake._next_seq(), 11)


class RebootInjectionTests(unittest.TestCase):
    def test_reboot_is_detected(self):
        stack = Stack(inject=["reboot"])
        try:
            self.assertTrue(stack.fake.wait_passes(1, 10.0))
            self.assertTrue(wait_for(lambda: stack.fake.reboots >= 1, 5.0))
            self.assertTrue(wait_for(
                lambda: stack.integrity.snapshot()["reboots"] >= 1, 5.0))
        finally:
            stack.close()


if __name__ == "__main__":
    unittest.main()
