"""Switch panel GUI — replicate the LCD switch-panel page on PC.

Click a DIP switch to flip the corresponding switch on the device over TCP
5005 (JSON control command ``set_sw``). A 500 ms poll of ``get_switch`` keeps
the UI in sync with device-side changes (e.g. EC1 toggles on the LCD).

Usage:
    python main.py [host [port]]
Default host 127.0.0.1 connects to the dsosim simulator; point it at the
DSO's IP for real hardware.
"""

from __future__ import annotations

import sys

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import (QApplication, QComboBox, QGridLayout, QHBoxLayout,
                               QLabel, QLineEdit, QMainWindow, QPlainTextEdit,
                               QPushButton, QStatusBar, QVBoxLayout, QWidget)

from serial_transport import SerialTransport, discover_ch340
from transport import SwitchTransport
from widgets import LedWidget, SwitchWidget

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 5005

# 模型位序（bit0=B1 … bit13=B14；bit9=B10 悬空不可控，不显示）
DIP10_INDICES = list(range(9))      # B1..B9 → 模型 idx 0-8
DIP4_INDICES = [10, 11, 12, 13]     # B11..B14 → 模型 idx 10-13
ALL_INDICES = DIP10_INDICES + DIP4_INDICES
LED_COUNT = 3
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

        self._build_ui()

        self._poll = QTimer(self)
        self._poll.setInterval(POLL_MS)
        self._poll.timeout.connect(self._poll_state)

    # ---- UI ---------------------------------------------------------------

    def _build_ui(self) -> None:
        central = QWidget()
        root = QVBoxLayout(central)

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

        # 10-bit DIP (B1..B9, 3 x 3)
        title = QLabel("10-BIT DIP")
        title.setStyleSheet("font-weight: bold;")
        root.addWidget(title)
        g10 = QGridLayout()
        for c, idx in enumerate(DIP10_INDICES):
            sw = SwitchWidget(idx, label=f"B{idx + 1}")
            sw.clicked.connect(self._on_switch_clicked)
            self._sws.append(sw)
            g10.addWidget(sw, c // 3, c % 3)
        root.addLayout(g10)

        # 4-bit DIP (B11..B14, 1 x 4)
        title = QLabel("4-BIT DIP")
        title.setStyleSheet("font-weight: bold;")
        root.addWidget(title)
        g4 = QHBoxLayout()
        for idx in DIP4_INDICES:
            sw = SwitchWidget(idx, label=f"B{idx + 1}")
            sw.clicked.connect(self._on_switch_clicked)
            self._sws.append(sw)
            g4.addWidget(sw)
        g4.addStretch(1)
        root.addLayout(g4)

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
        self.setMinimumSize(820, 800)
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
        t = self._new_transport()
        if self._link.currentText() == "TCP":
            host = self._host.text().strip() or DEFAULT_HOST
            try:
                port = int(self._port.text().strip())
            except ValueError:
                self._set_status("invalid port")
                return
            ok = t.connect(host, port)
            label = f"{host}:{port}"
        else:
            com = self._com.currentData() or self._com.currentText().strip()
            try:
                baud = int(self._baud.text().strip())
            except ValueError:
                self._set_status("invalid baud")
                return
            ok = t.connect(com, baud)
            label = f"{com}@{baud}"
        self._transport.close()
        self._transport = t
        if ok:
            self._set_status(f"connected to {label}")
            self._append_log(f"connected to {label}")
            self._btn_conn.setEnabled(False)
            self._btn_disc.setEnabled(True)
            self._poll.start()
            self._poll_state()
        else:
            self._set_status(f"connect to {label} failed")
            self._append_log(f"connect to {label} FAILED")

    def _on_disconnect(self) -> None:
        self._transport.close()
        self._poll.stop()
        self._btn_conn.setEnabled(True)
        self._btn_disc.setEnabled(False)
        self._set_status("disconnected")

    def _on_switch_clicked(self, idx: int) -> None:
        if not self._transport.is_open:
            return
        desired = 0 if ((self._sw_bits >> idx) & 1) else 1
        self._seq += 1
        self._append_log(f"> set_sw sw={idx} on={desired}")
        self._transport.send_cmd("set_sw", {"sw": idx, "on": desired}, self._seq)
        # optimistic local update; the poll reconciles with the device
        if desired:
            self._sw_bits |= 1 << idx
        else:
            self._sw_bits &= ~(1 << idx)
        self._apply()

    def _poll_state(self) -> None:
        st = self._transport.query_state(timeout=0.4, on_line=self._append_log)
        if st is None:
            self._set_status("no reply — check link")
            return
        sw, led = st
        if sw != self._sw_bits or led != self._led_bits:
            self._sw_bits = sw
            self._led_bits = led
            self._apply()
        self._set_status("online")

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
