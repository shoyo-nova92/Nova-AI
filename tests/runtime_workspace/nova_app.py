import sys

from PyQt6.QtWidgets import QApplication

from core.nova_runtime import NovaRuntime
from ui.orb import NovaOrb


def handle_goal(orb, runtime=None):
    goal = orb.get_text_command()
    if not goal:
        return

    orb.set_state("Thinking", (255, 170, 0))

    if runtime is None:
        runtime = NovaRuntime()

    result = runtime.process_goal(goal)
    metadata = result.get("metadata", {}) if isinstance(result, dict) else {}

    if metadata.get("exit_requested"):
        orb.set_state("Bye", (255, 50, 50))
        QApplication.quit()
        return

    status = result.get("status", "UNKNOWN") if isinstance(result, dict) else str(result)
    success = bool(result.get("success")) if isinstance(result, dict) else False

    if success:
        orb.set_state(status, (0, 220, 120))
    else:
        orb.set_state(status, (255, 50, 50))


def main():
    app = QApplication(sys.argv)
    orb = NovaOrb()
    orb.show()

    orb.send_button.clicked.connect(
        lambda: handle_goal(orb)
    )

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
