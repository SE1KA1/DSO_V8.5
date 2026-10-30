import socket
import tempfile
import time
import unittest
from pathlib import Path

from tools.rack_test import decoders
from tools.rack_test.config import RackConfig
from tools.rack_test.integrity import IntegrityTracker
from tools.rack_test.receiver import RackReceiver
from tools.rack_test.stats import RunStats


def wait_for(predicate, timeout=3.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return True
        time.sleep(0.02)
    return False


class ReceiverTests(unittest.TestCase):
    def setUp(self):
        cfg = RackConfig()
        cfg.bind_host = "127.0.0.1"
        cfg.bind_port = 0
        cfg.status_interval_s = 3600
        cfg.summary_interval_s = 3600
        cfg.storm_interval_s = 3600
        cfg.raw_max_bytes = 1024 * 1024
        decoders.install_decoder_path(cfg.decoder_dir)
        import envelope
        self.envelope = envelope
        self.tmp = tempfile.TemporaryDirectory()
        self.cfg = cfg
        self.stats = RunStats(cfg)
        self.integrity = IntegrityTracker()
        self.receiver = RackReceiver(cfg, self.stats, self.integrity,
                                     Path(self.tmp.name), None, None,
                                     log=lambda *_: None)
        self.receiver.bind()
        self.receiver.start()
        self.assertTrue(self.receiver.wait_ready(3.0))

    def tearDown(self):
        self.receiver.stop()
        self.tmp.cleanup()

    # ------------------------------------------------------------------ utils
    def _connect(self):
        return socket.create_connection(("127.0.0.1", self.receiver.local_port),
                                        timeout=2.0)

    def _sht_line(self, seq=1, ts=1000):
        return self.envelope.build(
            "sht40", "module.data",
            {"temp_dezi": 245, "rh_centi": 4523, "temp_ok": 1,
             "hi_alarm": 0, "level": 0}, seq=seq, ts_ms=ts)

    def _watchdog_line(self, seq=6, ts=6000):
        return self.envelope.build(
            "watchdog", "module.data",
            {"feed_en": 1, "kick_count": 1, "pfo": 0,
             "reset_cause": 0, "uptime_ms": 6000}, seq=seq, ts_ms=ts)

    # ------------------------------------------------------------------ tests
    def test_receives_a_frame(self):
        sock = self._connect()
        sock.sendall((self._sht_line() + "\r\n").encode())
        sock.close()
        self.assertTrue(wait_for(
            lambda: self.stats.snapshot()["modules"]["sht40"]["count"] == 1))
        snap = self.stats.snapshot()
        self.assertEqual(snap["modules"]["sht40"]["last_data"]["temp_dezi"], 245)
        self.assertGreaterEqual(snap["connections"]["fin_closes"], 1)

    def test_split_packet(self):
        line = self._sht_line(seq=2, ts=2000)
        cut = len(line) // 2
        sock = self._connect()
        sock.sendall((line[:cut]).encode())
        time.sleep(0.05)
        sock.sendall((line[cut:] + "\r\n").encode())
        sock.close()
        self.assertTrue(wait_for(
            lambda: self.stats.snapshot()["modules"]["sht40"]["count"] == 1))

    def test_empty_connection_is_rst_closed(self):
        sock = self._connect()
        sock.close()
        self.assertTrue(wait_for(
            lambda: self.stats.snapshot()["connections"]["a2_accepts"] >= 1))
        snap = self.stats.snapshot()
        self.assertGreaterEqual(snap["connections"]["rst_closes"], 1)
        self.assertGreaterEqual(snap["connections"]["a2_rst"], 1)

    def test_watchdog_is_ignored_but_seq_continues(self):
        sock = self._connect()
        sock.sendall((self._watchdog_line(seq=1, ts=1000) + "\r\n").encode())
        sock.sendall((self._sht_line(seq=2, ts=2000) + "\r\n").encode())
        sock.close()
        self.assertTrue(wait_for(
            lambda: self.stats.snapshot()["modules"]["sht40"]["count"] == 1))
        snap = self.stats.snapshot()
        self.assertEqual(snap["watchdog_frames"], 1)
        self.assertEqual(snap["modules"]["sht40"]["count"], 1)
        # watchdog consumed seq 1, sht40 seq 2 -> no gap
        self.assertEqual(self.integrity.snapshot()["seq_gaps"], 0)

    def test_oversize_line_counted(self):
        sock = self._connect()
        sock.sendall(b"X" * 5000)
        time.sleep(0.1)
        sock.sendall(b"\n")
        sock.close()
        self.assertTrue(wait_for(
            lambda: self.stats.snapshot()["oversize_lines"] >= 1))

    def test_bad_module_schema_counted(self):
        line = self.envelope.build(
            "sht40", "module.data",
            {"temp_dezi": 245, "rh_centi": 4523, "temp_ok": 2,
             "hi_alarm": 0, "level": 0}, seq=1, ts_ms=1000)
        sock = self._connect()
        sock.sendall((line + "\r\n").encode())
        sock.close()
        self.assertTrue(wait_for(
            lambda: self.stats.snapshot()["bad_frames"] >= 1))
        snap = self.stats.snapshot()
        self.assertEqual(snap["modules"]["sht40"]["count"], 0)
        self.assertEqual(snap["modules"]["sht40"]["bad_frames"], 1)

    def test_legacy_line_classified(self):
        line = ('{"protocolVersion":"1.0","messageType":"device.status",'
                '"deviceId":"board-001","seq":1,"payload":{}}\r\n')
        sock = self._connect()
        sock.sendall(line.encode())
        time.sleep(0.1)
        sock.close()
        snap = self.stats.snapshot()
        self.assertEqual(snap["legacy_lines"], 1)
        self.assertEqual(snap["connections"]["legacy"], 1)


if __name__ == "__main__":
    unittest.main()
