"""Self-drawn widgets for the switch-panel GUI (mirror ui_switch_panel.c).

Layout mirrors the LCD page: 3 LED dots across the top, a 2x5 grid of
10-bit DIP switches, then a 1x4 row of 4-bit DIP switches. Signal names are
not shown, per the design constraint.
"""

from __future__ import annotations

from PySide6.QtCore import QRect, Qt, Signal
from PySide6.QtGui import QColor, QFont, QLinearGradient, QPainter, QPen
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
CLR_TEMP = QColor(0xFF, 0xB3, 0x00)       # yellow = temperature alarm
CLR_ALARM = QColor(0xFF, 0x50, 0x3A)      # red = current alarm
CLR_ETH = QColor(0x44, 0xAA, 0xFF)        # blue = ethernet link
CLR_USB = QColor(0x26, 0xC6, 0xDA)        # cyan = usb activity

LED_ON_COLORS = (CLR_RUN_FG, CLR_TEMP, CLR_ALARM, CLR_ETH, CLR_USB)
LED_LABELS = ("HEART", "TEMP", "CURRENT", "ETH", "USB")


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
    """A compact DIP switch controlled by a horizontal swipe.

    Press and drag sideways past :attr:`SWIPE_THRESHOLD` px: drag LEFT → ON
    (card turns green), drag RIGHT → OFF (card turns dark). Short drags leave
    the switch unchanged. Emits :attr:`swiped` with (index, on); the caller
    sends the set_sw command.
    """

    swiped = Signal(int, int)          # (switch index, 1=ON / 0=OFF)

    SWIPE_THRESHOLD = 16               # horizontal px needed to trigger

    def __init__(self, index: int, label: str | None = None, parent=None,
                 accent: tuple[QColor, QColor] | None = None) -> None:
        super().__init__(parent)
        self._index = index
        self._label = label if label is not None else f"SW{index + 1}"
        # ON 渐变色：默认绿（拨码开关）；accent 供 MUTE 等非绿控件区分
        self._acc = accent if accent is not None else \
            (QColor(0x52, 0xD6, 0x6C), QColor(0x2A, 0x8F, 0x42))
        self._on = False
        self._press = None             # press position (QPointF) while dragging
        self._preview = None           # None | bool target state during drag
        self.setFixedSize(46, 23)
        self.setToolTip(label)
        self.setCursor(Qt.CursorShape.OpenHandCursor)

    @property
    def index(self) -> int:
        return self._index

    def set_on(self, on: bool) -> None:
        if on != self._on:
            self._on = on
            if self._preview is None:  # don't fight a live swipe preview
                self.update()

    # ---- swipe interaction -------------------------------------------------

    def mousePressEvent(self, event) -> None:  # noqa: N802 (Qt name)
        if event.button() == Qt.MouseButton.LeftButton:
            self._press = event.position()
            self._preview = None
            self.setCursor(Qt.CursorShape.ClosedHandCursor)
            self.grabMouse()

    def mouseMoveEvent(self, event) -> None:  # noqa: N802 (Qt name)
        if self._press is None:
            return
        dx = event.position().x() - self._press.x()
        if dx <= -self.SWIPE_THRESHOLD:
            target = True               # swipe LEFT  → ON (green)
        elif dx >= self.SWIPE_THRESHOLD:
            target = False              # swipe RIGHT → OFF (dark)
        else:
            target = None               # below threshold: keep current look
        if target is not None and target != self._preview:
            self._preview = target
            self.update()

    def mouseReleaseEvent(self, event) -> None:  # noqa: N802 (Qt name)
        if self._press is None:
            return
        dx = event.position().x() - self._press.x()
        self.releaseMouse()
        self.setCursor(Qt.CursorShape.OpenHandCursor)
        decided: bool | None = None
        if dx <= -self.SWIPE_THRESHOLD:
            decided = True
        elif dx >= self.SWIPE_THRESHOLD:
            decided = False
        self._preview = None
        self._press = None
        if decided is not None and decided != self._on:
            self._on = decided          # optimistic local visual
            self.swiped.emit(self._index, 1 if decided else 0)
        self.update()

    # ---- painting ----------------------------------------------------------

    def paintEvent(self, event) -> None:  # noqa: N802 (Qt name)
        del event
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        w = self.width()
        h = self.height()
        on = self._preview if self._preview is not None else self._on

        # 迷你开关：卡片几乎占满部件（B 编号走 tooltip，不再占一行）
        card = QRect(3, 2, w - 6, h - 4)
        half = card.width() // 2
        card_rad = card.height() // 2          # 药丸形圆角
        thumb_rad = (card.height() - 2) // 2

        # 轨道：ON 绿色带纵向渐变（顶部亮） / OFF 深色
        p.setPen(Qt.PenStyle.NoPen)
        if on:
            track = QLinearGradient(0, card.top(), 0, card.bottom())
            track.setColorAt(0.0, self._acc[0])
            track.setColorAt(1.0, self._acc[1])
            p.setBrush(track)
        else:
            p.setBrush(CLR_OFF_TOP)
        p.drawRoundedRect(card, card_rad, card_rad)

        # 白色圆角 thumb：ON 在左半（右侧露绿） / OFF 在右半（左侧露深色）
        inset = 2
        th = card.height() - 2 * inset
        tw = half - inset                       # ≈半宽
        if on:
            thumb = QRect(card.left() + inset, card.top() + inset, tw, th)
        else:
            thumb = QRect(card.left() + half + 1, card.top() + inset, tw, th)
        p.setBrush(Qt.GlobalColor.white)
        p.drawRoundedRect(thumb, thumb_rad, thumb_rad)

        # 卡片描边（统一深色，不随 ON 变绿，避免"包围"感）
        p.setPen(QPen(CLR_CARD_BR, 1))
        p.setBrush(Qt.BrushStyle.NoBrush)
        p.drawRoundedRect(card, card_rad, card_rad)
        p.end()
