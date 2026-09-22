"""Switch panel GUI — replicate the LCD switch-panel page on PC.

DIP switches sit in a compact right-hand column; press and swipe LEFT to turn
a switch ON, RIGHT to turn it OFF (``set_sw`` over TCP 5005). A 500 ms poll of
``get_switch`` keeps the UI in sync with device-side changes and refreshes the
SHT40 temperature readout.

Usage:
    python main.py [host [port]]
Default host 127.0.0.1 connects to the dsosim simulator; point it at the
DSO's IP for real hardware.
"""

from __future__ import annotations

import json
import sys
import time

from PySide6.QtCore import QTimer
from PySide6.QtGui import QColor
from PySide6.QtWidgets import (QApplication, QComboBox, QGridLayout, QHBoxLayout,
                               QLabel, QLineEdit, QMainWindow, QMessageBox,
                               QPlainTextEdit, QPushButton, QStatusBar,
                               QVBoxLayout, QWidget)

from serial_transport import SerialTransport, discover_ch340
from transport import SwitchTransport
from widgets import LedWidget, SwitchWidget

DEFAULT_HOST = "192.168.100.50"
DEFAULT_PORT = 5005

# 模型位序（bit0=B1 … bit13=B14；bit9=B10 悬空不可控，不显示）
DIP10_INDICES = list(range(9))      # B1..B9 → 模型 idx 0-8
DIP4_INDICES = [10, 11, 12, 13]     # B11..B14 → 模型 idx 10-13
ALL_INDICES = DIP10_INDICES + DIP4_INDICES
LED_COUNT = 5   # HEART/TEMP/CURRENT + ETH/USB 状态
POLL_MS = 500


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("DSO Switch Panel (PC)")
        self.\
            _transport = SwitchTransport()
        self._seq = 0
        self._sw_bits = 0
        self._led_bits = 0
        self._leds: list[LedWidget] = []
        self._sws: list[SwitchWidget] = []

        # set_sw round-trip latency measurement (per click → SERIAL LOG)
        self._lat_seq: int | None = None     # seq awaiting its set_sw ack
        self._lat_t0 = 0.0
        self._lat_idx = -1
        self._lat_on = 0
        self._lat_link = "?"
        self._lat_win: list[float] = []      # last up-to-10 latencies (ms)
        self._lat_win_link: str | None = None

        self._build_ui()

        self._poll = QTimer(self)
        self._poll.setInterval(POLL_MS)
        self._poll.timeout.connect(self._poll_state)

        self._ack_timer = QTimer(self)
        self._ack_timer.setInterval(4)
        self._ack_timer.timeout.connect(self._drain_acks)

        # 远程复位（Reset 键）后的自动重连：2s 一次尝试，窗口期见 _start_auto_reconnect
        self._reconnect_timer = QTimer(self)
        self._reconnect_timer.setInterval(2000)
        self._reconnect_timer.timeout.connect(self._try_reconnect)
        self._reconnect_until = 0.0

    # ---- UI ---------------------------------------------------------------

    def _build_ui(self) -> None:
        central = QWidget()
        outer = QHBoxLayout(central)
        outer.setContentsMargins(6, 6, 6, 6)
        root = QVBoxLayout()                   # left column: conn / LED / temp / log
        outer.addLayout(root, 1)
        right = QVBoxLayout()                  # right-most switch sidebar
        right.setContentsMargins(4, 0, 10, 0)  # 与窗口右缘留余地
        outer.addLayout(right)

        # connection bar
        bar = QHBoxLayout()
        bar.addWidget(QLabel("Link:"))
        self._link = QComboBox()
        self._link.addItems(["TCP", "Serial"])
        self._link.currentTextChanged.connect(self._on_link_changed)
        bar.addWidget(self._link)

        # TCP fields
        self._lbl_host = QLabel("Host:")
        bar.addWidget(self._lbl_host)
        self._host = QLineEdit(DEFAULT_HOST)
        self._host.setFixedWidth(140)
        bar.addWidget(self._host)
        self._lbl_port = QLabel("Port:")
        bar.addWidget(self._lbl_port)
        self._port = QLineEdit(str(DEFAULT_PORT))
        self._port.setFixedWidth(60)
        bar.addWidget(self._port)

        # Serial fields
        self._lbl_com = QLabel("COM:")
        bar.addWidget(self._lbl_com)
        self._com = QComboBox()
        self._com.setEditable(True)
        for dev in discover_ch340():
            self._com.addItem(f"{dev['port']}  ({dev['description']})", dev["port"])
        if self._com.count() == 0:
            self._com.addItem("COM3", "COM3")  # 占位，可手输
        self._com.setFixedWidth(150)
        bar.addWidget(self._com)
        self._lbl_baud = QLabel("Baud:")
        bar.addWidget(self._lbl_baud)
        self._baud = QLineEdit("1152000")
        self._baud.setFixedWidth(70)
        bar.addWidget(self._baud)

        self._btn_conn = QPushButton("Connect")
        self._btn_conn.clicked.connect(self._on_connect)
        bar.addWidget(self._btn_conn)
        self._btn_disc = QPushButton("Disconnect")
        self._btn_disc.clicked.connect(self._on_disconnect)
        self._btn_disc.setEnabled(False)
        bar.addWidget(self._btn_disc)

        # 远程复位键：等效按下核心板复位键（软件复位），点击后自动重连
        self._btn_reset = QPushButton("Reset")
        self._btn_reset.setToolTip("远程复位核心板（软件复位，等效按复位键）")
        self._btn_reset.clicked.connect(self._on_reset_clicked)
        self._btn_reset.setEnabled(False)
        bar.addWidget(self._btn_reset)
        bar.addStretch(1)
        root.addLayout(bar)
        self._on_link_changed(self._link.currentText())

        # LED row
        title = QLabel("STATUS LED")
        title.setStyleSheet("font-weight: bold;")
        root.addWidget(title)
        led_row = QHBoxLayout()
        for i in range(LED_COUNT):
            led = LedWidget(i)
            self._leds.append(led)
            led_row.addWidget(led)
        led_row.addStretch(1)
        root.addLayout(led_row)

        # Temperature readout (SHT40 on the bridge board; refreshed each poll)
        title = QLabel("TEMPERATURE")
        title.setStyleSheet("font-weight: bold;")
        root.addWidget(title)
        temp_row = QHBoxLayout()
        self._temp_val = QLabel("--.- °C")
        tf = self._temp_val.font()
        tf.setPointSize(18)
        tf.setBold(True)
        self._temp_val.setFont(tf)
        temp_row.addWidget(self._temp_val)
        self._temp_hint = QLabel("SHT40")
        self._temp_hint.setStyleSheet("color: #888;")
        temp_row.addWidget(self._temp_hint)
        temp_row.addStretch(1)
        root.addLayout(temp_row)

        # 右侧开关栏：全部开关(B1..B9, B11..B14)自上而下单列，编号灰字在开关右侧；
        # 按住左右滑动控制——左滑开(变绿)/右滑关(变深色)
        hint = QLabel("SWITCHES   ⟵ON   OFF⟶")
        hint.setStyleSheet("color: #888; font-weight: bold;")
        right.addWidget(hint)
        for idx in ALL_INDICES:
            row = QHBoxLayout()
            sw = SwitchWidget(idx, label=f"B{idx + 1}")
            sw.swiped.connect(self._on_swipe)
            self._sws.append(sw)
            row.addWidget(sw)
            row.addSpacing(6)
            num = QLabel(f"B{idx + 1}")
            num.setStyleSheet("color: #888;")
            row.addWidget(num)
            row.addStretch(1)
            right.addLayout(row)

        # MUTE：蜂鸣器静音滑块（非绿渐变色区分；左滑=静音，LED 闪烁不受影响）
        mrow = QHBoxLayout()
        self._mute = SwitchWidget(99, label="MUTE",
                                  accent=(QColor(0xFF, 0xB3, 0x4D),
                                          QColor(0xC2, 0x45, 0x1C)))
        self._mute.swiped.connect(self._on_mute_swipe)
        mrow.addWidget(self._mute)
        mrow.addSpacing(6)
        mlabel = QLabel("MUTE")
        mlabel.setStyleSheet("color: #888;")
        mrow.addWidget(mlabel)
        mrow.addStretch(1)
        right.addLayout(mrow)
        right.addStretch(1)

        # 串口原始输出日志面板（显示收到的所有行，含 [SER]/[BOOT] 等调试信息）
        log_title = QLabel("SERIAL LOG")
        log_title.setStyleSheet("font-weight: bold;")
        root.addWidget(log_title)
        self._log = QPlainTextEdit()
        self._log.setReadOnly(True)
        self._log.setMaximumBlockCount(2000)
        self._log.setMinimumHeight(150)
        self._log.setMaximumHeight(220)
        root.addWidget(self._log)

        self.setCentralWidget(central)
        self.setMinimumSize(700, 700)
        self.setStatusBar(QStatusBar())
        self._set_status("disconnected")

    # ---- actions -----------------------------------------------------------

    def _on_link_changed(self, link: str) -> None:
        is_tcp = (link == "TCP")
        for w in (self._lbl_host, self._host, self._lbl_port, self._port):
            w.setVisible(is_tcp)
        for w in (self._lbl_com, self._com, self._lbl_baud, self._baud):
            w.setVisible(not is_tcp)

    def _new_transport(self):
        """Create the transport matching the current Link selection."""
        if self._link.currentText() == "TCP":
            return SwitchTransport()
        return SerialTransport()

    def _on_connect(self) -> None:
        self._connect_current(silent=False)

    def _connect_current(self, silent: bool = False) -> bool:
        """按当前 Link 选择建立连接并启动轮询；silent=True 供自动重连复用。

        @return 连接是否成功。
        """
        t = self._new_transport()
        if self._link.currentText() == "TCP":
            host = self._host.text().strip() or DEFAULT_HOST
            try:
                port = int(self._port.text().strip())
            except ValueError:
                self._set_status("invalid port")
                return False
            ok = t.connect(host, port)
            label = f"{host}:{port}"
        else:
            com = self._com.currentData() or self._com.currentText().strip()
            try:
                baud = int(self._baud.text().strip())
            except ValueError:
                self._set_status("invalid baud")
                return False
            ok = t.connect(com, baud)
            label = f"{com}@{baud}"
        self._transport.close()
        self._transport = t
        if ok:
            if not silent:
                self._append_log(f"connected to {label}")
            self._set_status(f"connected to {label}")
            self._btn_conn.setEnabled(False)
            self._btn_disc.setEnabled(True)
            self._btn_reset.setEnabled(True)
            self._poll.start()
            self._poll_state()
        else:
            if not silent:
                self._append_log(f"connect to {label} FAILED")
            self._set_status(f"connect to {label} failed")
        return ok

    def _on_disconnect(self) -> None:
        self._reconnect_timer.stop()
        self._transport.close()
        self._poll.stop()
        self._ack_timer.stop()
        self._lat_seq = None
        self._lat_win.clear()
        self._btn_conn.setEnabled(True)
        self._btn_disc.setEnabled(False)
        self._btn_reset.setEnabled(False)
        self._set_status("disconnected")

    # ---- 远程复位与自动重连 ------------------------------------------------

    def _on_reset_clicked(self) -> None:
        """Reset 键：二次确认后下发 reboot，并进入自动重连窗口。"""
        if not self._transport.is_open:
            return
        ans = QMessageBox.question(
            self, "远程复位",
            "将远程复位核心板（软件复位，等效按下复位键）。\n"
            "设备会短暂离线，并在数秒内自动重连。是否继续？",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No)
        if ans != QMessageBox.StandardButton.Yes:
            return
        self._seq += 1
        self._append_log(f"> reboot (soft) seq={self._seq}")
        self._transport.send_cmd("reboot", {"mode": "soft"}, self._seq)
        self._start_auto_reconnect(30.0)

    def _start_auto_reconnect(self, seconds: float) -> None:
        """开启自动重连窗口（设备复位期间轮询重连）。"""
        self._reconnect_until = time.monotonic() + seconds
        self._set_status("设备复位中，自动重连…")
        self._append_log(f"[RESET] 等待设备重启（最多 {int(seconds)}s，自动重连）")
        if not self._reconnect_timer.isActive():
            self._reconnect_timer.start()

    def _try_reconnect(self) -> None:
        if time.monotonic() > self._reconnect_until:
            self._reconnect_timer.stop()
            self._set_status("重连超时 — 请手动 Connect")
            self._append_log("[RESET] 自动重连超时，请手动连接")
            return
        if self._transport.is_open and self._transport.query_state(timeout=0.3) is not None:
            # 设备已回来且旧连接仍可用
            self._reconnect_timer.stop()
            self._set_status("online")
            self._append_log("[RESET] 设备已恢复（连接未断）")
            return
        self._transport.close()
        self._poll.stop()
        if self._connect_current(silent=True):
            self._reconnect_timer.stop()
            self._set_status("online")
            self._append_log("[RESET] 设备已恢复，自动重连成功")
        else:
            self._set_status("设备复位中，自动重连…")

    def _on_mute_swipe(self, idx: int, on: int) -> None:
        """MUTE 滑块：左滑=on=静音。发 set_buzzer，状态由轮询收敛。"""
        if not self._transport.is_open:
            return
        mute = 1 if on else 0
        self._seq += 1
        self._append_log(f"> set_buzzer on={mute}")
        self._transport.send_cmd("set_buzzer", {"on": mute}, self._seq)

    def _on_swipe(self, idx: int, on: int) -> None:
        """SwitchWidget swipe result: LEFT=ON(1) / RIGHT=OFF(0), absolute set."""
        if not self._transport.is_open:
            return
        desired = 1 if on else 0
        self._seq += 1
        self._append_log(f"> set_sw sw={idx} on={desired}")
        self._transport.send_cmd("set_sw", {"sw": idx, "on": desired}, self._seq)
        # 记录发送时刻：同一 seq 的 set_sw ack 回来即结算往返延迟
        self._lat_seq = self._seq
        self._lat_t0 = time.monotonic()
        self._lat_idx = idx
        self._lat_on = desired
        self._lat_link = self._link.currentText()
        if not self._ack_timer.isActive():
            self._ack_timer.start()
        # optimistic local update; the poll reconciles with the device
        if desired:
            self._sw_bits |= 1 << idx
        else:
            self._sw_bits &= ~(1 << idx)
        self._apply()

    def _poll_state(self) -> None:
        st = self._transport.query_state(timeout=0.4, on_line=self._append_log,
                                         on_ack=self._on_ack)
        if st is None:
            self._set_status("no reply — check link")
            return
        sw, led, temp_dezi, temp_ok, eth_link, usb_active, buzzer_mute = st
        # 折叠 ETH(bit3)/USB(bit4) 状态位，与设备 led bit0-2 一起驱动 5 个 LED
        combined = led | ((1 if eth_link else 0) << 3) | ((1 if usb_active else 0) << 4)
        if sw != self._sw_bits or combined != self._led_bits:
            self._sw_bits = sw
            self._led_bits = combined
            self._apply()
        # MUTE 与设备静音状态同步（设备为准，覆盖本地点按）
        if self._mute._on != bool(buzzer_mute):
            self._mute.set_on(bool(buzzer_mute))
        self._set_temp_display(temp_dezi, temp_ok, alarm=bool((led >> 1) & 1))
        self._set_status("online")

    # ---- set_sw 往返延迟：每次打 SERIAL LOG，每 10 次 avg/min/max -------------

    def _on_ack(self, obj: dict) -> None:
        """背罜：set_sw ack 被 poll 的 query_state 消费时也结算（防漏测）。"""
        if (self._lat_seq is not None
                and obj.get("cmd") == "set_sw"
                and obj.get("seq") == self._lat_seq):
            self._finalize_latency()

    def _drain_acks(self) -> None:
        """4ms 定时非阻塞排空 rx，尽快抓到 set_sw ack（不冻结 UI）。"""
        if self._lat_seq is None:
            self._ack_timer.stop()
            return
        while self._lat_seq is not None:
            line = self._transport.next_line(timeout=0)
            if line is None:
                break
            self._append_log(line)               # 顺带保留原始行
            try:
                obj = json.loads(line)
            except json.JSONDecodeError:
                continue
            if (obj.get("type") == "ack" and obj.get("cmd") == "set_sw"
                    and obj.get("seq") == self._lat_seq):
                self._finalize_latency()
                return
        if time.monotonic() > self._lat_t0 + 2.0:
            self._append_log(f"[LAT] set_sw B{self._lat_idx + 1} "
                             f"on={self._lat_on} → timeout ({self._lat_link})")
            self._lat_seq = None
            self._ack_timer.stop()

    def _finalize_latency(self) -> None:
        if self._lat_seq is None:
            return
        dt_ms = (time.monotonic() - self._lat_t0) * 1000.0
        self._append_log(f"[LAT] set_sw B{self._lat_idx + 1} on={self._lat_on} "
                         f"→ {dt_ms:.1f} ms ({self._lat_link})")
        # 通道切换时清窗，避免混统计
        if getattr(self, "_lat_win_link", None) != self._lat_link:
            self._lat_win.clear()
            self._lat_win_link = self._lat_link
        self._lat_win.append(dt_ms)
        if len(self._lat_win) >= 10:
            avg = sum(self._lat_win) / len(self._lat_win)
            lo, hi = min(self._lat_win), max(self._lat_win)
            self._append_log(f"[LAT] last10 avg={avg:.1f} min={lo:.1f} "
                             f"max={hi:.1f} ms ({self._lat_link})")
            self._lat_win.clear()
        self._lat_seq = None
        self._ack_timer.stop()

    def _set_temp_display(self, temp_dezi: int | None, temp_ok: int,
                          alarm: bool) -> None:
        """温度标签：temp_ok=0 或字段缺失 → "--.- °C"；报警(led bit2)→ 红色。"""
        if temp_ok and temp_dezi is not None:
            text = f"{temp_dezi / 10.0:.1f} °C"
        else:
            text = "--.- °C"
        self._temp_val.setText(text)
        if alarm:
            self._temp_val.setStyleSheet("color: #FF3B30;")
        elif temp_ok and temp_dezi is not None:
            self._temp_val.setStyleSheet("color: #1B1F27;")
        else:
            self._temp_val.setStyleSheet("color: #9AA0A6;")

    def _apply(self) -> None:
        for sw in self._sws:
            sw.set_on(bool((self._sw_bits >> sw.index) & 1))
        for i, led in enumerate(self._leds):
            led.set_on(bool((self._led_bits >> i) & 1))

    def _set_status(self, text: str) -> None:
        self.statusBar().showMessage(text)

    def _append_log(self, text: str) -> None:
        from datetime import datetime
        ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        self._log.appendPlainText(f"[{ts}] {text}")

    def closeEvent(self, event) -> None:  # noqa: N802 (Qt name)
        self._reconnect_timer.stop()
        self._transport.close()
        super().closeEvent(event)


def main(argv: list[str] | None = None) -> int:
    global DEFAULT_HOST, DEFAULT_PORT
    args = argv if argv is not None else sys.argv[1:]
    if len(args) >= 1:
        DEFAULT_HOST = args[0]
    if len(args) >= 2:
        DEFAULT_PORT = int(args[1])
    app = QApplication(sys.argv)
    win = MainWindow()
    win.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
