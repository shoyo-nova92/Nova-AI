from PyQt6.QtWidgets import (
    QWidget,
    QLabel,
    QLineEdit,
    QPushButton
)

from PyQt6.QtCore import Qt, QPoint
from PyQt6.QtGui import QColor, QPainter


class NovaOrb(QWidget):

    def __init__(self):
        super().__init__()

        self.state = "Idle"
        self.color = QColor(0, 120, 255)

        self.drag_pos = None

        self.setFixedSize(250, 220)

        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint |
            Qt.WindowType.WindowStaysOnTopHint
        )

        self.setAttribute(
            Qt.WidgetAttribute.WA_TranslucentBackground
        )

        # orb label
        self.label = QLabel(self.state, self)
        self.label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.label.setGeometry(50, 80, 150, 30)
        self.label.setStyleSheet(
            "color: white; font-size: 16px;"
        )

        # text input
        self.input_box = QLineEdit(self)
        self.input_box.setGeometry(40, 150, 170, 35)
        self.input_box.setPlaceholderText("Type command...")

        # send button
        self.send_button = QPushButton("Send", self)
        self.send_button.setGeometry(90, 190, 70, 25)

    def get_text_command(self):
        text = self.input_box.text().strip()
        self.input_box.clear()
        return text

    def set_state(self, text, color):
        self.state = text
        self.color = QColor(*color)
        self.label.setText(text)
        self.update()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(
            QPainter.RenderHint.Antialiasing
        )

        painter.setBrush(self.color)
        painter.setPen(Qt.PenStyle.NoPen)

        painter.drawEllipse(75, 20, 100, 100)

    # draggable
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