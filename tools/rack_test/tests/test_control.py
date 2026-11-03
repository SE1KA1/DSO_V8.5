import unittest

from tools.auth_server.device_client import DeviceError
from tools.rack_test.config import RackConfig
from tools.rack_test.control import RackControl, _redact_flow_output
from tools.rack_test.fakedev import FakeDevice


class ControlTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # The fake's telemetry side is pointed at a closed port; only its
        # control server is used here.
        cls.fake = FakeDevice(host="127.0.0.1", port=1,
                              control_host="127.0.0.1", control_port=0,
                              interval=0.1, log=lambda *_: None)
        cls.fake.start()
        if not cls.fake.wait_ready(5.0):
            raise RuntimeError("fake control server did not start")
        host, port = cls.fake.control_address
        cfg = RackConfig()
        cfg.device_host = host
        cfg.device_port = port
        cfg.control_timeout_s = 3.0
        cfg.control_retries = 2
        cfg.control_retry_delay_s = 0.05
        cls.control = RackControl(cfg, log=lambda *_: None)

    @classmethod
    def tearDownClass(cls):
        cls.control.close()
        cls.fake.stop()

    def test_get_switch(self):
        data = self.control.get_switch()
        self.assertEqual(data["temp"], 245)
        self.assertEqual(data["eth_link"], 1)
        self.assertEqual(data["usb_active"], 0)

    def test_probe_is_short_and_works(self):
        data = self.control.probe(timeout_s=2.0)
        self.assertEqual(data["temp"], 245)
        self.assertEqual(data["sw"], 0)

    def test_set_sw_and_readback(self):
        before = int(self.control.get_switch()["sw"])
        ack = self.control.set_sw(3, 1)
        self.assertEqual(ack["result"], "ok")
        after = int(self.control.get_switch()["sw"])
        self.assertEqual((after >> 3) & 1, 1)
        self.control.set_sw(3, 0)
        again = int(self.control.get_switch()["sw"])
        self.assertEqual((again >> 3) & 1, 0)
        self.assertEqual((again ^ before) & ~0x200, 0)

    def test_index_nine_is_noop(self):
        before = int(self.control.get_switch()["sw"])
        ack = self.control.set_sw(9, 1)
        self.assertEqual(ack["result"], "ok")
        after = int(self.control.get_switch()["sw"])
        self.assertEqual(before, after)

    def test_invalid_index(self):
        for index in (14, -1):
            ack = self.control.set_sw(index, 1)
            self.assertEqual(ack["result"], "error")
            self.assertEqual(ack["error"], "bad_param")

    def test_atecc_identity_and_signature(self):
        devid = self.control.atecc_get_devid()
        self.assertEqual(devid["result"], "ok")
        self.assertEqual(devid["data"]["device_id"], "Eep21025E269FA2E")
        pub = self.control.atecc_get_pubkey()
        self.assertEqual(len(pub["data"]["pubkey_hex"]), 128)
        auth = self.control.atecc_auth_start("00" * 32)
        self.assertEqual(auth["result"], "ok")
        self.assertEqual(len(auth["data"]["signature_hex"]), 128)
        self.assertEqual(auth["data"]["self_verify"], 1)

    def test_license_query_and_clear(self):
        lic = self.control.atecc_get_license()
        self.assertEqual(lic["result"], "ok")
        self.assertIn("login_state", lic["data"])
        clear = self.control.atecc_license_clear()
        self.assertEqual(clear["result"], "ok")

    def test_unknown_command_is_detected(self):
        with self.assertRaises(DeviceError):
            self.control.send("mystery.command")

    def test_flow_output_license_prefix_is_redacted(self):
        text = ("device_id   : Eep21025E269FA2E\n"
                "license     : abcdefghij0123456789... (123 bytes)\n"
                "verify      : OK\n")
        out = _redact_flow_output(text)
        self.assertIn("license     : <redacted>", out)
        self.assertNotIn("abcdefghij0123456789", out)


if __name__ == "__main__":
    unittest.main()
