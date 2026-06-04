import sys
from PyQt6.QtWidgets import QApplication
from ui.orb import NovaOrb

app = QApplication(sys.argv)

orb = NovaOrb()
orb.show()

sys.exit(app.exec())