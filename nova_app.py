import sys

from PyQt6.QtWidgets import QApplication

from core.nova_runtime import NovaRuntime
from ui.orb import NovaOrb


def handle_goal(orb, runtime):
    goal = orb.get_text_command()
    if not goal:
        return

    runtime.process_goal(goal)


def main():
    app = QApplication(sys.argv)
    orb = NovaOrb()
    runtime = NovaRuntime()
    orb.show()

    orb.send_button.clicked.connect(
        lambda: handle_goal(orb, runtime)
    )

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
