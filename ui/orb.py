import math

from PyQt6.QtWidgets import (
    QWidget,
    QLabel,
    QLineEdit,
    QPushButton
)

from PyQt6.QtCore import (
    Qt,
    QTimer
)

from PyQt6.QtGui import (
    QColor,
    QPainter
)


class NovaOrb(QWidget):

    def __init__(self):
        super().__init__()

        self.state = "Idle"
        self.color = QColor(0, 120, 255)

        self.orb_size = 100
        self.phase = 0.0
        self.drag_pos = None

        self.setFixedSize(280, 260)

        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint |
            Qt.WindowType.WindowStaysOnTopHint
        )

        self.setAttribute(
            Qt.WidgetAttribute.WA_TranslucentBackground
        )

        # -------------------------
        # Status Label
        # -------------------------

        self.label = QLabel(self.state, self)
        self.label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.label.setGeometry(40, 135, 200, 28)
        self.label.setStyleSheet("""
            color: white;
            font-size:16px;
            font-weight:500;
            background: transparent;
        """)

        # -------------------------
        # Input
        # -------------------------

        self.input_box = QLineEdit(self)
        self.input_box.setGeometry(40, 175, 200, 35)
        self.input_box.setPlaceholderText("Type command...")

        # -------------------------
        # Send Button
        # -------------------------

        self.send_button = QPushButton("Send", self)
        self.send_button.setGeometry(105, 220, 70, 28)

        # Enter = Send
        self.input_box.returnPressed.connect(
            self.send_button.click
        )

    # ==================================================

    def get_text_command(self):

        text = self.input_box.text().strip()

        self.input_box.clear()
        self.input_box.setFocus()

        return text

    # ==================================================

    def set_state(self, text, color):

        self.state = text
        self.color = QColor(*color)

        self.label.setText(text)

        if text == "Listening":
            self.start_pulse()
        else:
            self.stop_pulse()

        self.update()

    # ==================================================

    def paintEvent(self, event):

        painter = QPainter(self)
        painter.setRenderHint(
            QPainter.RenderHint.Antialiasing
        )

        painter.setPen(Qt.PenStyle.NoPen)

        # ---------- Soft Glow ----------

        glow_size = self.orb_size + 18

        glow_color = QColor(
            self.color.red(),
            self.color.green(),
            self.color.blue(),
            55
        )

        painter.setBrush(glow_color)

        glow_x = (self.width() - glow_size) // 2
        glow_y = 20 + (110 - glow_size) // 2

        painter.drawEllipse(
            glow_x,
            glow_y,
            glow_size,
            glow_size
        )

        # ---------- Main Orb ----------

        painter.setBrush(self.color)

        size = self.orb_size

        x = (self.width() - size) // 2
        y = 20 + (110 - size) // 2

        painter.drawEllipse(
            x,
            y,
            size,
            size
        )

    # ==================================================

    def start_pulse(self):

        if hasattr(self, "pulse_timer"):
            return

        self.phase = 0

        self.pulse_timer = QTimer(self)
        self.pulse_timer.timeout.connect(
            self.update_pulse
        )
        self.pulse_timer.start(16)   # ~60 FPS

    def update_pulse(self):

        self.phase += 0.08

        self.orb_size = int(
            100 + math.sin(self.phase) * 5
        )

        self.update()

    def stop_pulse(self):

        if hasattr(self, "pulse_timer"):
            self.pulse_timer.stop()
            self.pulse_timer.deleteLater()
            del self.pulse_timer

        self.orb_size = 100

        self.update()

    # ==================================================

    def mousePressEvent(self, event):

        if event.button() == Qt.MouseButton.LeftButton:
            self.drag_pos = event.globalPosition().toPoint()

    def mouseMoveEvent(self, event):

        if self.drag_pos:

            delta = (
                event.globalPosition().toPoint()
                - self.drag_pos
            )

            self.move(
                self.x() + delta.x(),
                self.y() + delta.y()
            )

            self.drag_pos = event.globalPosition().toPoint()

    def mouseReleaseEvent(self, event):

        self.drag_pos = None