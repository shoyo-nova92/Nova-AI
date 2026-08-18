import sys

from PyQt6.QtWidgets import QApplication

from ui.orb import NovaOrb
from core.parser import IntentParser
from core.executor import Executor
from core.logger import NovaLogger


def should_exit(command: str):
    command = command.lower()

    triggers = [
        "bye",
        "exit",
        "quit",
        "shutdown nova",
        "terminate"
    ]

    return any(t in command for t in triggers)


def process_command(command, orb, parser, executor, logger):

    command = command.strip()

    if not command:
        return  

    if should_exit(command):``
        orb.set_state("Bye", (255, 50, 50))
        QApplication.quit()
        return

    orb.set_state("Thinking", (255, 170, 0))

    try:
        result = parser.parse(command)

        response = executor.execute(result)

        logger.write(command, response)

    except Exception as e:
        response = f"Error: {e}"

    orb.set_state("Speaking", (180, 0, 255))

    print()
    print("=" * 60)
    print("USER :", command)
    print("NOVA :", response)
    print("=" * 60)

    orb.set_state("Listening", (0, 220, 120))


def main():

    print("Starting Nova...")

    app = QApplication(sys.argv)

    print("Loading UI...")
    orb = NovaOrb()
    orb.show()

    print("Loading Parser...")
    parser = IntentParser()

    print("Loading Executor...")
    executor = Executor()

    print("Loading Logger...")
    logger = NovaLogger()

    print("Nova Ready.")

    orb.send_button.clicked.connect(
        lambda: process_command(
            orb.get_text_command(),
            orb,
            parser,
            executor,
            logger
        )
    )

    sys.exit(app.exec())


if __name__ == "__main__":
    main()