"""Build the machine-readable summary and the Markdown report."""

from __future__ import annotations

import json
import platform
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

from . import __version__, metadata
from .checks import CheckResult, FAIL, NA, PASS, PENDING, SKIP, WARN
from .config import RackConfig
from .sink import read_json, write_atomic_json

OPTIONAL_SKIP = {"REBOOT-01", "SOAK-01"}


def _counts(results: List[CheckResult]) -> Dict[str, int]:
    counts = {PASS: 0, FAIL: 0, WARN: 0, SKIP: 0, NA: 0, PENDING: 0}
    for item in results:
        counts[item.status] = counts.get(item.status, 0) + 1
    return counts


def _verdict(results: List[CheckResult], counts: Dict[str, int]) -> str:
    if counts.get(FAIL, 0) > 0:
        return "FAIL"
    for item in results:
        if item.status in (SKIP, PENDING) and item.id not in OPTIONAL_SKIP:
            return "INCOMPLETE"
    return "PASS"


def _check_dicts(results: List[CheckResult]) -> List[dict]:
    return [item.to_dict() for item in results]


def build_summary(cfg: RackConfig, run_dir: Path, results: List[CheckResult],
                  ctx: dict, started: str, ended: str,
                  control_ok: bool, control_error: str,
                  telemetry_seen: bool) -> dict:
    counts = _counts(results)
    stats = ctx.get("final") or {}
    integrity = ctx.get("integrity") or {}
    return {
        "schema": "dso.rack_test.summary/1",
        "harness_version": __version__,
        "run_id": Path(run_dir).name,
        "started_at": started,
        "ended_at": ended,
        "verdict": _verdict(results, counts),
        "counts": counts,
        "environment": {
            "device": "%s:%d" % (cfg.device_host, int(cfg.device_port)),
            "bind": "%s:%d" % (cfg.bind_host, int(cfg.bind_port)),
            "decoder_dir": cfg.decoder_dir,
            "state_dir": cfg.state_dir,
            "python": sys.version.split()[0],
            "platform": platform.platform(),
            "operator": cfg.operator,
            "mode": ctx.get("mode", "full"),
            "firmware": cfg.firmware,
            "note": cfg.note,
            "telemetry_seen": bool(telemetry_seen),
            "control_ok": bool(control_ok),
            "control_error": control_error,
            "allow_reboot": bool(cfg.allow_reboot),
            "allow_license_clear": bool(cfg.allow_license_clear),
            "auth_flow": bool(cfg.auth_user and cfg.auth_password),
            "freq_hz": cfg.freq_hz,
            "baseline_s": cfg.baseline_s,
            "soak_s": ctx.get("soak_s"),
            "config": cfg.redacted_dict(),
        },
        "stats": stats,
        "integrity": integrity,
        "sweep": ctx.get("sweep"),
        "atecc": ctx.get("atecc"),
        "checks": _check_dicts(results),
        "human_todo": metadata.HUMAN_TODO,
        "discrepancies": metadata.DISCREPANCIES,
        "limitations": metadata.LIMITATIONS,
        "artifacts": {
            "telemetry": "telemetry_raw.jsonl",
            "events": "events.jsonl",
            "status": "status.json",
            "human_todo": "human_todo.json",
            "summary": "summary.json",
            "report": "report.md",
        },
    }


# ---------------------------------------------------------------------------
# Markdown
# ---------------------------------------------------------------------------

def _cell(value: Any, limit: int = 220) -> str:
    if value is None:
        return ""
    if isinstance(value, (dict, list)):
        text = json.dumps(value, ensure_ascii=False, separators=(",", ":"))
    else:
        text = str(value)
    text = text.replace("|", "\\|").replace("\n", " ")
    if len(text) > limit:
        text = text[:limit - 3] + "..."
    return text


def _table(headers: List[str], rows: List[List[Any]]) -> str:
    out = ["| " + " | ".join(headers) + " |",
           "|" + "|".join(["---"] * len(headers)) + "|"]
    for row in rows:
        out.append("| " + " | ".join(_cell(v) for v in row) + " |")
    return "\n".join(out)


def load_answers(path: Optional[str]) -> Optional[dict]:
    """Load a flat answers object.

    Accepts either a plain ``{"HUM-01": true, ...}`` file or the run's
    ``human_todo.json`` (reads its ``answers``/``answers_template`` object).
    """
    if not path:
        return None
    data = read_json(Path(path))
    if not isinstance(data, dict):
        raise ValueError("human answers must be a JSON object")
    for key in ("answers", "answers_template"):
        nested = data.get(key)
        if isinstance(nested, dict):
            return nested
    return data


def apply_human_answers(summary: dict, answers: Optional[dict]) -> dict:
    if not answers:
        return summary
    for item in summary.get("checks", []):
        if item["id"] in answers:
            value = answers[item["id"]]
            if value is None or value == "":
                continue          # unfilled template entry stays PENDING
            if value in (True, "pass", "PASS", 1, "1", "ok", "OK"):
                item["status"] = PASS
                item["detail"] = ""
            elif value in (False, "fail", "FAIL", 0, "0", "no", "NO"):
                item["status"] = FAIL
                item["detail"] = "human observed: fail"
            else:
                item["status"] = FAIL
                item["detail"] = str(value)
            item["measured"] = dict(item.get("measured") or {})
            item["measured"]["answer"] = value
    counts = {PASS: 0, FAIL: 0, WARN: 0, SKIP: 0, NA: 0, PENDING: 0}
    for item in summary.get("checks", []):
        counts[item["status"]] = counts.get(item["status"], 0) + 1
    summary["counts"] = counts
    summary["verdict"] = _verdict(
        [CheckResult(id=i["id"], module=i["module"], title=i["title"],
                     status=i["status"]) for i in summary.get("checks", [])],
        counts)
    return summary


def generate(run_dir: Path, human_answers: Optional[str] = None) -> Tuple[Path, Path]:
    run_dir = Path(run_dir)
    summary_path = run_dir / "summary.json"
    summary = read_json(summary_path)
    if not isinstance(summary, dict):
        raise FileNotFoundError("summary.json not found in %s" % run_dir)
    answers = load_answers(human_answers)
    if answers:
        summary = apply_human_answers(summary, answers)
        write_atomic_json(summary_path, summary)
    report_path = run_dir / "report.md"
    report_path.write_text(render(summary), encoding="utf-8")
    return (report_path, summary_path)


def render(summary: dict) -> str:
    env = summary.get("environment") or {}
    counts = summary.get("counts") or {}
    stats = summary.get("stats") or {}
    integrity = summary.get("integrity") or {}
    lines: List[str] = []
    add = lines.append

    add("# 过桥板模块机架远程测试报告")
    add("")
    add("- 运行 ID：`%s`" % summary.get("run_id"))
    add("- 开始 / 结束：%s → %s" % (summary.get("started_at"),
                                   summary.get("ended_at")))
    add("- 设备：`%s`；采集监听：`%s`" % (env.get("device"), env.get("bind")))
    add("- Harness 版本：%s；Python：%s" % (summary.get("harness_version"),
                                           env.get("python")))
    add("- 操作者：%s；固件标记：%s；模式：%s"
        % (env.get("operator") or "-", env.get("firmware") or "-",
           env.get("mode") or "full"))
    if env.get("note"):
        add("- 备注：%s" % env["note"])
    add("")

    add("## 1. 总结")
    add("")
    add("**总判定：%s**" % summary.get("verdict"))
    add("")
    add(_table(["PASS", "FAIL", "WARN", "SKIP", "N/A", "PENDING"],
               [[counts.get(PASS, 0), counts.get(FAIL, 0), counts.get(WARN, 0),
                 counts.get(SKIP, 0), counts.get(NA, 0), counts.get(PENDING, 0)]]))
    add("")
    add("- 遥测是否出现：%s" % ("是" if env.get("telemetry_seen") else "否"))
    add("- 控制通道：%s%s" % ("可用" if env.get("control_ok") else "不可用",
                             "" if not env.get("control_error") else
                             "（%s）" % env["control_error"]))
    add("- 看门狗：显式排除；其帧只参与 seq/ts 连续性核算。")
    add("")

    add("## 2. 逐项判定")
    add("")
    rows = []
    for item in summary.get("checks", []):
        rows.append([item.get("id"), item.get("module"), item.get("status"),
                     item.get("measured"), item.get("thresholds"),
                     item.get("detail"), "; ".join(item.get("source") or [])])
    add(_table(["ID", "模块", "状态", "实测", "阈值", "说明", "代码出处"], rows))
    add("")

    add("## 3. 遥测完整性")
    add("")
    if env.get("mode") == "control-only":
        add("- control-only 模式：未启动遥测接收器，本节不适用。")
        add("")
    conn = stats.get("connections") or {}
    add("- 实时帧率：%.2f/s" % float(stats.get("frame_rate") or 0))
    add("- 看门狗帧（忽略）：%d" % int(stats.get("watchdog_frames") or 0))
    add("- 坏帧：%d；非法行：%d；超长行：%d"
        % (int(stats.get("bad_frames") or 0), int(stats.get("invalid_lines") or 0),
           int(stats.get("oversize_lines") or 0)))
    add("- 连接：接受 %d，当前 %d，最大并发 %d，RST %d，FIN %d，拒绝 %d"
        % (int(conn.get("accepted") or 0), int(conn.get("active") or 0),
           int(conn.get("max_concurrent") or 0), int(conn.get("rst_closes") or 0),
           int(conn.get("fin_closes") or 0), int(conn.get("rejected") or 0)))
    a2_accepts = int(conn.get("a2_accepts") or 0)
    legacy_lines = int(stats.get("legacy_lines") or 0)
    if a2_accepts == 0 and legacy_lines > 0:
        add("- legacy A2：稳定（收到 %d 行 legacy 帧，无连断风暴）" % legacy_lines)
    elif a2_accepts == 0:
        add("- legacy A2：未观察到")
    else:
        add("- legacy A2：接受 %d（RST %d），空连接已 RST 关闭（旧固件连断风暴）"
            % (a2_accepts, int(conn.get("a2_rst") or 0)))
    add("- seq 缺口：%d；最大缺口：%s；重复：%d；回绕：seq %d / ts %d"
        % (int(integrity.get("seq_gaps") or 0), integrity.get("max_seq_gap"),
           int(integrity.get("duplicates") or 0),
           int(integrity.get("seq_wraps") or 0),
           int(integrity.get("ts_wraps") or 0)))
    add("- 重启：%d（epoch=%s）"
        % (int(integrity.get("reboots") or 0), integrity.get("epoch")))
    if integrity.get("gaps"):
        add("")
        add("最近 seq 缺口：")
        add(_table(["期望", "实际", "差", "墙钟 ms"],
                   [[g.get("expected"), g.get("got"), g.get("delta"),
                     g.get("wall_ms")] for g in integrity.get("gaps", [])[-10:]]))
    add("")

    modules = stats.get("modules") or {}
    if modules:
        add("### 各模块帧统计")
        add("")
        add(_table(["模块", "帧数", "事件数", "坏帧", "最大间隔 ms", "最后 ts_ms"],
                   [[m, v.get("count"), v.get("events"), v.get("bad_frames"),
                     v.get("max_gap_ms"), v.get("last_ts_ms")]
                    for m, v in modules.items()]))
        add("")

    sweep = summary.get("sweep") or {}
    if sweep and not sweep.get("skipped"):
        add("## 4. 开关扫描明细")
        add("")
        rows = []
        for step in sweep.get("steps") or []:
            rows.append([step.get("index"), step.get("ack_on"),
                         step.get("bit_get_on"), step.get("bit_tel_on"),
                         step.get("ack_off"), step.get("bit_get_off"),
                         step.get("bit_tel_off"), step.get("latency_ms"),
                         "OK" if step.get("ok") else "FAIL"])
        add(_table(["idx", "on ack", "get bit", "tel bit", "off ack",
                    "get bit", "tel bit", "ms", "结果"], rows))
        add("")
        add("- 基线 sw：%s；结束 sw：%s；恢复：%s"
            % (sweep.get("baseline_sw"), sweep.get("final_sw"),
               "是" if sweep.get("restored") else "否"))
        add("- 非法索引：%s" % _cell(sweep.get("invalid")))
        add("- B10（idx 9）：%s" % _cell(sweep.get("idx9")))
        add("")
    elif sweep.get("skipped"):
        add("## 4. 开关扫描")
        add("")
        add("跳过：%s" % sweep.get("reason", "control unavailable"))
        add("")

    add("## 5. 人工观察")
    add("")
    rows = []
    by_id = {item.get("id"): item for item in summary.get("checks", [])}
    for item in summary.get("human_todo") or []:
        check = by_id.get(item["id"])
        rows.append([item["id"], item["title"], item["instruction"],
                     check.get("status") if check else "N/A",
                     (check or {}).get("measured", {}).get("answer")])
    add(_table(["ID", "项目", "说明", "状态", "答案"], rows))
    add("")

    add("## 6. 已知代码与规格差异")
    add("")
    for item in summary.get("discrepancies") or []:
        add("### %s — %s" % (item["id"], item["title"]))
        add("")
        add(item["detail"])
        add("")
        add("出处：%s" % "; ".join(item.get("source") or []))
        add("")

    add("## 7. 限制与 N/A（所需激励）")
    add("")
    add(_table(["项目", "状态", "需要的激励/说明"],
               [[i["item"], i["status"], i["stimulus"]]
                for i in summary.get("limitations") or []]))
    add("")

    add("## 8. 产物")
    add("")
    add(_table(["文件", "说明"],
               [[name, path] for name, path in
                (summary.get("artifacts") or {}).items()]))
    add("")
    return "\n".join(lines)
