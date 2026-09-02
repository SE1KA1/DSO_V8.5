"""Self-drawn widgets for the switch-panel GUI (mirror ui_switch_panel.c).

Layout mirrors the LCD page: 3 LED dots across the top, a 2x5 grid of
10-bit DIP switches, then a 1x4 row of 4-bit DIP switches. Signal names are
not shown, per the design constraint.
"""

from __future__ import annotations

from PySide6.QtCore import QRect, Qt, Signal
from PySide6.QtGui import QColor, QFont, QPainter, QPen
from PySide6.QtWidgets import QWidget

# Palette mirroring the LCD theme (ui_theme.h)
CLR_TEXT_DIM = QColor(0x55, 0x5C, 0x6B)
CLR_TEXT_HI = QColor(0xE8, 0xEC, 0xF4)
CLR_CARD = QColor(0x18, 0x1C, 0x26)
CLR_CARD_BR = QColor(0x2A, 0x30, 0x3E)
CLR_DIV = QColor(0x3A, 0x41, 0x50)
CLR_RUN_FG = QColor(0x3F, 0xC6, 0x5B)     # green = ON
CLR_RUN_BG = QColor(0x18, 0x28, 0x20)
CLR_OFF_TOP = QColor(0x18, 0x28, 0x20)
CLR_POWER = QColor(0x44, 0xAA, 0xFF)      # blue
CLR_ALARM = QColor(0xFF, 0x50, 0x3A)      # red

LED_ON_COLORS = (CLR_RUN_FG, CLR_POWER, CLR_ALARM)
LED_LABELS = ("HEART", "POWER", "ALARM")


class LedWidget(QWidget):
    """A single LED dot + label + number, like the LCD's draw_led."""

    def __init__(self, index: int, parent=None) -> None:
        super().__init__(parent)
        self._index = index
        self._on = False
        self.setFixedSize(150, 96)

    def set_on(self, on: bool) -> None:
        if on != self._on:
            self._on = on
            self.update()

    def paintEvent(self, event) -> None:  # noqa: N802 (Qt name)
        del event
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        w = self.width()
        cx, cy, r = w // 2, 42, 16

        # dot: colored when ON, grey when OFF
        p.setPen(QPen(CLR_TEXT_DIM, 1))
        p.setBrush(LED_ON_COLORS[self._index] if self._on else CLR_TEXT_DIM)
        p.drawEllipse(cx - r, cy - r, 2 * r, 2 * r)

        # label text stays grey regardless of LED state (visible on light bg)
        p.setPen(CLR_TEXT_DIM)
        small = QFont()
        small.setPointSize(8)
        p.setFont(small)
        p.drawText(QRect(0, cy + r + 6, w, 16),
                   Qt.AlignmentFlag.AlignCenter, LED_LABELS[self._index])
        p.drawText(QRect(0, cy + r + 22, w, 16),
                   Qt.AlignmentFlag.AlignCenter, f"LED{self._index + 1}")
        p.end()


class SwitchWidget(QWidget):
    """A vertical 2-segment DIP switch (ON green top / OFF grey bottom).

    Clicking emits :attr:`clicked` with the switch index; the caller decides
    the new state (toggle) and sends the command.
    """

    clicked = Signal(int)

    def __init__(self, index: int, label: str | None = None, parent=None) -> None:
        super().__init__(parent)
        self._index = index
        self._label = label if label is not None else f"SW{index + 1}"
        self._on = False
        self.setFixedSize(132, 106)
        self.setCursor(Qt.CursorShape.PointingHandCursor)

    @property
    def index(self) -> int:
        return self._index

    def set_on(self, on: bool) -> None:
        if on != self._on:
            self._on = on
            self.update()

    def mousePressEvent(self, event) -> None:  # noqa: N802 (Qt name)
        if event.button() == Qt.MouseButton.LeftButton:
            self.clicked.emit(self._index)

    def paintEvent(self, event) -> None:  # noqa: N802 (Qt name)
        del event
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        w = self.width()
        x, y = 3, 2
        sw_w, sw_h = w - 6, 62
        mid = y + sw_h // 2

        # card
        p.setPen(QPen(CLR_CARD_BR, 1))
        p.setBrush(CLR_CARD)
        p.drawRoundedRect(x, y, sw_w, sw_h, 4, 4)

        # top (ON) / bottom (OFF) segments
        p.setPen(Qt.PenStyle.NoPen)
        if self._on:
            p.setBrush(CLR_RUN_FG)
            p.drawRect(x + 3, y + 3, sw_w - 6, mid - y - 3)
            p.setBrush(CLR_OFF_TOP)
            p.drawRect(x + 3, mid, sw_w - 6, sw_h - mid - 3)
        else:
            p.setBrush(CLR_OFF_TOP)
            p.drawRect(x + 3, y + 3, sw_w - 6, mid - y - 3)
            p.setBrush(CLR_RUN_BG)
            p.drawRect(x + 3, mid, sw_w - 6, sw_h - mid - 3)

        # divider
        p.setPen(QPen(CLR_DIV, 1))
        p.drawLine(x + 3, mid, x + sw_w - 3, mid)

        # ON / OFF markers
        f = QFont()
        f.setPointSize(8)
        p.setFont(f)
        p.setPen(CLR_TEXT_HI if self._on else CLR_TEXT_DIM)
        p.drawText(QRect(x, y + 2, sw_w, 14),
                   Qt.AlignmentFlag.AlignCenter, "ON" if self._on else "")
        p.drawText(QRect(x, mid + 2, sw_w, 14),
                   Qt.AlignmentFlag.AlignCenter, "" if self._on else "OFF")

        # switch label (e.g. "B1" / "B11")
        f.setPointSize(10)
        p.setFont(f)
        p.setPen(CLR_TEXT_HI)
        p.drawText(QRect(0, y + sw_h + 2, w, 20),
                   Qt.AlignmentFlag.AlignCenter, self._label)
        p.end()
