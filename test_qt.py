import sys

from PyQt6.QtWidgets import QApplication, QLabel

app = QApplication(sys.argv)

label = QLabel("Qt DPI Test")
label.resize(300, 100)
label.show()

sys.exit(app.exec())