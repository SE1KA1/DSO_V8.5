"""CLI contract tests — every command exercised against dsosim.

The CLI and the HTTP API share `flow.py`, but the CLI's arguments and exit
codes were never covered by tests. These run `service.main([...])` in-process
with `STATE_DIR` pointed at a temp dir.

Run from the repo root:
    python -m unittest tools.auth_server.cli_test -v
"""

from __future__ import annotations

import contextlib
import io
import os
import socket
import sys
import tempfile
import unittest
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__)))))

from dsosim.atecc import AteccSimulator
from dsosim.device import DSOSimulator
from dsosim.server import SimulatorServer
from tools.auth_server import service

DEV_ID = "Eep21025E269FA2E"
USER = "stu001"
PWD = "cli-test-pass"


class CliContract(unittest.TestCase):
    def setUp(self) -> None:
        import tools.auth_server.store as store_mod
        old = store_mod.PBKDF2_ITERATIONS
        store_mod.PBKDF2_ITERATIONS = 1_000
        self.addCleanup(setattr, store_mod, "PBKDF2_ITERATIONS", old)

        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.state = Path(self.tmp.name)
        self.old_state = service.STATE_DIR
        service.STATE_DIR = self.state
        self.addCleanup(setattr, service, "STATE_DIR", self.old_state)

        self.sim = AteccSimulator(DEV_ID, provisioned=True)
        dev = DSOSimulator(seed=1)
        dev.atecc = self.sim
        self.srv = SimulatorServer(dev, host="127.0.0.1", port=0)
        self.srv.start()
        self.addCleanup(self.srv.stop)
        self.port = self.srv.server_address[1]

        # The simulator must trust the CA created by `init`, so init first and
        # then point the simulator at that CA's public key.
        rc, out = self.cli("init")
        self.assertEqual(rc, 0, out)
        from tools.auth_server.keystore import CAKey
        ca = CAKey.load_or_create(self.state / "ca_key.pem")
        self.sim.ca_pub_hex = ca.public_hex()

        self.assertEqual(self.cli("add-user", USER, "--password", PWD)[0], 0)
        self.assertEqual(self.cli("register", DEV_ID, self.sim.pubkey_hex(),
                                  "--host", "127.0.0.1",
                                  "--port", str(self.port))[0], 0)
        self.assertEqual(self.cli("bind", USER, DEV_ID, "--days", "30")[0], 0)

    def cli(self, *args):
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            rc = service.main(list(args))
        return rc, buf.getvalue()

    # ---- happy path ------------------------------------------------------
    def test_flow_then_license(self) -> None:
        rc, out = self.cli("flow", "--host", "127.0.0.1", "--port", str(self.port),
                           "--user", USER, "--password", PWD)
        self.assertEqual(rc, 0, out)
        self.assertIn("device proof: OK", out)
        self.assertIn("verify      : OK", out)

        rc, out = self.cli("license", "--host", "127.0.0.1", "--port", str(self.port))
        self.assertEqual(rc, 0, out)
        self.assertIn("login_state : AUTHORIZED", out)

    def test_renew_changes_license(self) -> None:
        self.assertEqual(self.cli("flow", "--host", "127.0.0.1", "--port", str(self.port),
                                  "--user", USER, "--password", PWD)[0], 0)
        rc, out = self.cli("renew", "--host", "127.0.0.1", "--port", str(self.port),
                           "--user", USER, "--password", PWD)
        self.assertEqual(rc, 0, out)
        self.assertIn("renewed", out)

    def test_revoke_then_renew_refused(self) -> None:
        self.assertEqual(self.cli("revoke", "--device", DEV_ID)[0], 0)
        rc, out = self.cli("renew", "--host", "127.0.0.1", "--port", str(self.port),
                           "--user", USER, "--password", PWD)
        self.assertNotEqual(rc, 0, out)
        self.assertIn("no active", out)

    def test_negtest_all_pass(self) -> None:
        rc, out = self.cli("negtest", "--host", "127.0.0.1", "--port", str(self.port),
                           "--user", USER, "--password", PWD)
        self.assertEqual(rc, 0, out)
        self.assertIn("7/7 PASS", out)

    # ---- argument / policy paths ----------------------------------------
    def test_bind_refuses_duplicate(self) -> None:
        rc, out = self.cli("bind", USER, DEV_ID, "--days", "10")
        self.assertEqual(rc, 3, out)
        self.assertIn("bind refused", out)

        rc, out = self.cli("bind", USER, DEV_ID, "--days", "10", "--force")
        self.assertEqual(rc, 0, out)

    def test_bind_unknown_user_or_device(self) -> None:
        rc, out = self.cli("bind", "ghost", DEV_ID, "--days", "1")
        self.assertEqual(rc, 2, out)
        self.assertIn("unknown user", out)
        rc, out = self.cli("bind", USER, "Eep9999999999999", "--days", "1")
        self.assertEqual(rc, 2, out)
        self.assertIn("unknown device", out)

    def test_probe_unreachable_is_reported(self) -> None:
        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            closed_port = probe.getsockname()[1]
        rc, out = self.cli("probe", "--host", "127.0.0.1", "--port", str(closed_port))
        self.assertNotEqual(rc, 0)
        self.assertIn("device error", out)

    def test_verify_license_rejects_garbage(self) -> None:
        rc, out = self.cli("verify-license", "not-a-token")
        self.assertNotEqual(rc, 0, out)


if __name__ == "__main__":
    unittest.main(verbosity=2)
