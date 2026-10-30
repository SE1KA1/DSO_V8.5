import json
import tempfile
import unittest
from pathlib import Path

from tools.rack_test.cli import _load_cfg, build_parser


class CliTests(unittest.TestCase):
    def test_parser_accepts_all_commands(self):
        parser = build_parser()
        for argv in (["serve"], ["watch"], ["cmd", "get_switch"],
                     ["run"], ["dryrun"], ["report", "--run-dir", "x"]):
            args = parser.parse_args(argv)
            self.assertTrue(callable(args.func), argv)

    def test_config_precedence(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "cfg.json"
            path.write_text(json.dumps({"baseline_s": 12.0,
                                        "soak_s": 34.0,
                                        "bind_port": 0,
                                        "decoder_dir":
                                        "D:/DSO_V8.5/module_telemetry/python_ref"}),
                            encoding="utf-8")
            parser = build_parser()
            args = parser.parse_args(["dryrun", "--config", str(path),
                                      "--baseline", "7"])
            cfg = _load_cfg(args)
            self.assertEqual(cfg.soak_s, 34.0)       # file value
            self.assertEqual(cfg.baseline_s, 7.0)    # CLI wins

    def test_serve_bind_override(self):
        parser = build_parser()
        args = parser.parse_args(["serve", "--bind", "127.0.0.1:6005"])
        cfg = _load_cfg(args)
        self.assertEqual(cfg.bind_host, "127.0.0.1")
        self.assertEqual(cfg.bind_port, 6005)


if __name__ == "__main__":
    unittest.main()
