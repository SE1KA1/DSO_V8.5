import tempfile
import unittest
from pathlib import Path

from tools.rack_test import checks, report
from tools.rack_test.config import RackConfig
from tools.rack_test.sink import read_json, write_atomic_json


def base_stats():
    return {
        "modules": {}, "frame_rate": 7.0, "watchdog_frames": 60,
        "bad_frames": 0, "invalid_lines": 0, "oversize_lines": 0,
        "connections": {"accepted": 2, "active": 1, "max_concurrent": 2,
                        "rejected": 0, "rst_closes": 100, "fin_closes": 1,
                        "a2_accepts": 100, "a2_rst": 100, "reconnects": 0},
    }


class ReportTests(unittest.TestCase):
    def _summary(self, rounds):
        cfg = RackConfig()
        results = [
            checks.CheckResult("SHT-01", "sht40", "temperature", checks.PASS,
                               measured={"temp": 245}, thresholds={"max": 580},
                               source=["User/dso/dso_sht40.h:30"]),
            checks.CheckResult("HUM-01", "board", "YDS2 on", checks.PENDING,
                               source=["README"]),
        ]
        if rounds:
            results.append(checks.CheckResult(
                "INT-03", "all", "seq continuity", checks.FAIL,
                measured={"gaps": 1}, detail="one gap"))
        ctx = {"final": base_stats(), "integrity": {"seq_gaps": 0},
               "soak_s": 0, "sweep": None, "atecc": {}}
        return report.build_summary(cfg, Path("run-1"), results, ctx,
                                    "start", "end", True, "", True)

    def test_summary_shape_and_verdict(self):
        summary = self._summary(False)
        self.assertEqual(summary["schema"], "dso.rack_test.summary/1")
        self.assertEqual(summary["verdict"], "INCOMPLETE")
        self.assertEqual(summary["counts"][checks.PASS], 1)
        self.assertEqual(summary["counts"][checks.PENDING], 1)
        self.assertTrue(summary["discrepancies"])
        self.assertTrue(summary["limitations"])

    def test_fail_verdict(self):
        summary = self._summary(True)
        self.assertEqual(summary["verdict"], "FAIL")

    def test_generate_from_human_todo_template(self):
        summary = self._summary(False)
        with tempfile.TemporaryDirectory() as tmp:
            run_dir = Path(tmp)
            write_atomic_json(run_dir / "summary.json", summary)
            write_atomic_json(run_dir / "human_todo.json", {
                "answers_template": {"HUM-01": True},
            })
            _, summary_path = report.generate(
                run_dir, str(run_dir / "human_todo.json"))
            merged = read_json(summary_path)
            by_id = {c["id"]: c for c in merged["checks"]}
            self.assertEqual(by_id["HUM-01"]["status"], checks.PASS)

            # An unfilled (null/empty) template entry stays PENDING.
            write_atomic_json(run_dir / "summary.json", summary)
            write_atomic_json(run_dir / "human_todo.json", {
                "answers_template": {"HUM-01": None},
            })
            _, summary_path = report.generate(
                run_dir, str(run_dir / "human_todo.json"))
            merged = read_json(summary_path)
            by_id = {c["id"]: c for c in merged["checks"]}
            self.assertEqual(by_id["HUM-01"]["status"], checks.PENDING)

    def test_generate_and_merge_answers(self):
        summary = self._summary(False)
        with tempfile.TemporaryDirectory() as tmp:
            run_dir = Path(tmp)
            write_atomic_json(run_dir / "summary.json", summary)
            answers = {"HUM-01": True}
            write_atomic_json(run_dir / "answers.json", answers)
            report_path, summary_path = report.generate(
                run_dir, str(run_dir / "answers.json"))
            text = report_path.read_text(encoding="utf-8")
            self.assertIn("过桥板模块机架远程测试报告", text)
            self.assertIn("已知代码与规格差异", text)
            self.assertIn("限制与 N/A", text)
            self.assertIn("SHT-01", text)
            merged = read_json(summary_path)
            self.assertEqual(merged["verdict"], "PASS")
            self.assertEqual(merged["counts"][checks.PASS], 2)


if __name__ == "__main__":
    unittest.main()
