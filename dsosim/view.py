"""
Offline waveform viewer — renders simulator frames to a self-contained
HTML/SVG file that opens in any browser (zero Python GUI dependencies).

Usage:
    python -m dsosim.view                # snapshot of default device
    python -m dsosim.view -o wave.html   # custom output file
    python -m dsosim.view --live 1.0     # regenerate every 1.0s (refresh browser)

The output mimics the DSO Scope page: grid, CH1/CH2 waveforms, and the
measurement readout row.
"""

from __future__ import annotations

import argparse
import html
import os
import time

from .device import DSOSimulator
from .waveform import DISPLAY_WIDTH, DISPLAY_HEIGHT, Y_MID

SVG_W = 800
SVG_H = 480
GRID_X = 10   # vertical grid lines
GRID_Y = 8    # horizontal grid lines (voltage div)


def _y_scale_pts(ys: list[int]) -> str:
    """Convert display-Y int16 points to SVG polyline points."""
    pts = []
    for x, y in enumerate(ys):
        pts.append(f"{x},{SVG_H - 1 - y}")
    return " ".join(pts)


def _svg_wave(ys: list[int], color: str, opacity: float = 1.0) -> str:
    if not ys:
        return ""
    return (f'<polyline points="{_y_scale_pts(ys)}" fill="none" '
            f'stroke="{color}" stroke-width="2" opacity="{opacity}"/>')


def _fmt_hz(hz: float) -> str:
    if hz >= 1e6:
        return f"{hz/1e6:.3f} MHz"
    if hz >= 1e3:
        return f"{hz/1e3:.3f} kHz"
    return f"{hz:.0f} Hz"


def _fmt_v(v: float) -> str:
    return f"{v:.3f} V"


def render_frame(dev: DSOSimulator) -> str:
    """Render the current simulator state to a full HTML page."""
    f = dev.read_frame()

    grid = []
    for i in range(GRID_X + 1):
        x = i * (SVG_W / GRID_X)
        grid.append(f'<line x1="{x}" y1="0" x2="{x}" y2="{SVG_H}" '
                    f'stroke="#222" stroke-width="1"/>')
    for i in range(GRID_Y + 1):
        y = i * (SVG_H / GRID_Y)
        grid.append(f'<line x1="0" y1="{y}" x2="{SVG_W}" y2="{y}" '
                    f'stroke="#222" stroke-width="1"/>')

    ch1_wave = _svg_wave(f.ch1.get("y", []), "#ffff00")   # yellow
    ch2_wave = _svg_wave(f.ch2.get("y", []), "#00ffff")   # cyan

    # measurement readout row (mirrors Scope 测量区)
    readout = []
    c1 = f.ch1
    c2 = f.ch2
    readout.append(f'<div class="m">CH1 '
                   f'F={_fmt_hz(c1["freq_hz"])} '
                   f'Vpp={_fmt_v(c1["vpp_v"])} '
                   f'Vrms={_fmt_v(c1["vrms_v"])} '
                   f'Duty={c1["duty_pct"]:.1f}%</div>')
    readout.append(f'<div class="m">CH2 '
                   f'F={_fmt_hz(c2["freq_hz"])} '
                   f'Vpp={_fmt_v(c2["vpp_v"])} '
                   f'Vrms={_fmt_v(c2["vrms_v"])} '
                   f'Duty={c2["duty_pct"]:.1f}%</div>')
    readout.append(f'<div class="m">TB={f.tb["sec_div_ns"]}ns/div '
                   f'SR={f.tb["sample_rate_hz"]/1e6:.2f}MS/s '
                   f'state={f.state} '
                   f'running={"RUN" if f.status["running"] else "STOP"}</div>')

    # top bar (顶栏)
    topbar = (f'<div class="bar">CH1 <span style="color:#ffff00">■</span> | '
              f'CH2 <span style="color:#00ffff">■</span> | '
              f'TRIG {f.trig["mode"]} {f.trig["source"]} {f.trig["slope"]} '
              f'Lv={f.trig["level_v"]:.2f}V | '
              f'overrun={f.status["overrun"]} | '
              f'errors={len(f.status["errors"])}</div>')

    page = f"""<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>dsosim waveform</title>
<style>
  body {{ background:#000; color:#ddd; font-family:monospace; }}
  .bar {{ padding:4px 8px; border-bottom:1px solid #444; }}
  .readout {{ padding:6px 8px; border-top:1px solid #444; }}
  .m {{ display:inline-block; margin-right:20px; }}
  svg {{ display:block; margin:0 auto; background:#0a0a0a; }}
</style></head><body>
  {topbar}
  <svg width="{SVG_W}" height="{SVG_H}" viewBox="0 0 {SVG_W} {SVG_H}">
    <rect width="100%" height="100%" fill="#0a0a0a"/>
    {"".join(grid)}
    {ch1_wave}
    {ch2_wave}
  </svg>
  <div class="readout">{"".join(readout)}</div>
</body></html>
"""
    return page


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="dsosim.view", description="render waveforms to HTML")
    p.add_argument("-o", "--output", default="waveform.html")
    p.add_argument("--live", type=float, default=0.0,
                   help="regenerate every N seconds (0 = single snapshot)")
    p.add_argument("--seed", type=int, default=None)
    args = p.parse_args(argv)

    dev = DSOSimulator(seed=args.seed)
    dev.set_waveform(1, "sine", freq_hz=2000, amplitude_v=1.0)
    dev.set_waveform(2, "square", freq_hz=500, amplitude_v=0.5)

    if args.live > 0:
        print(f"live render every {args.live}s → {args.output} (refresh browser)")
        try:
            while True:
                with open(args.output, "w", encoding="utf-8") as fp:
                    fp.write(render_frame(dev))
                time.sleep(args.live)
        except KeyboardInterrupt:
            return 0

    with open(args.output, "w", encoding="utf-8") as fp:
        fp.write(render_frame(dev))
    print(f"wrote {args.output} — open in browser")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
