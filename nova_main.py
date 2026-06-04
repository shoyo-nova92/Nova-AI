import sys
import threading
import time

from PyQt6.QtWidgets import QApplication

from ui.orb import NovaOrb
from core.parser import IntentParser
from core.executor import Executor
from core.logger import NovaLogger
from core.voice import VoiceEngine
from core.wake_local import LocalWake


SESSION_TIMEOUT = 300  # 5 min


class NovaSession:
    def __init__(self):
        self.active = False
        self.last_activity = time.time()

    def activate(self):
        self.active = True
        self.last_activity = time.time()

    def refresh(self):
        self.last_activity = time.time()

    def should_sleep(self):
        return (
            self.active and
            (time.time() - self.last_activity > SESSION_TIMEOUT)
        )

    def sleep(self):
        self.active = False


def should_exit(command):
    triggers = [
        "exit",
        "quit",
        "shutdown nova",
        "terminate"
    ]

    command = command.lower()

    return any(trigger in command for trigger in triggers)


def process_command(
    command,
    orb,
    parser,
    executor,
    logger,
    voice,
    session
):
    if not command.strip():
        return

    command = command.lower().strip()

    # instant exit

    if should_exit(command):
        orb.set_state("Bye", (255, 50, 50))
        voice.speak("Shutting down")
        sys.exit()

    # typed wake command

    if command in ["nova", "hey nova"]:
        session.activate()
        orb.set_state("Listening", (0, 220, 120))
        voice.speak("Yes?")
        return

    # if command contains wake phrase
    # explicit wake command

    if command in ["nova", "hey nova"]:
        session.activate()
        orb.set_state("Listening", (0, 220, 120))
        voice.speak("Yes?")
        return


    # command includes wake phrase + actual task

    if "nova" in command:
        command = (
            command.replace("nova", "")
            .replace("hey", "")
            .strip()
        )

        session.activate()


    # if still sleeping → reject command

    elif not session.active:
        voice.speak("Nova is sleeping. Say Nova first.")
        orb.set_state("Idle", (0, 120, 255))
        return


    session.refresh()

    orb.set_state("Thinking", (255, 170, 0))

    result = parser.parse(command)
    response = executor.execute(result)

    logger.write(command, response)

    orb.set_state("Speaking", (180, 0, 255))
    voice.speak(response)

    orb.set_state("Listening", (0, 220, 120))


def voice_loop(
    orb,
    parser,
    executor,
    logger,
    voice,
    wake,
    session
):
    while True:
        try:
            # sleep mode
            if not session.active:
                orb.set_state("Idle", (0, 120, 255))

                wake_text = wake.listen_for_nova()

                if wake_text:
                    session.activate()

                    orb.set_state(
                        "Listening",
                        (0, 220, 120)
                    )

                    voice.speak("Yes?")

            else:
                # auto sleep
                if session.should_sleep():
                    session.sleep()

                    orb.set_state(
                        "Idle",
                        (0, 120, 255)
                    )

                    voice.speak("Going to sleep")
                    continue

                command = voice.listen()

                process_command(
                    command,
                    orb,
                    parser,
                    executor,
                    logger,
                    voice,
                    session
                )

        except Exception as e:
            print("Voice Error:", e)
            orb.set_state("Error", (255, 0, 0))
            time.sleep(2)


def main():
    app = QApplication(sys.argv)

    orb = NovaOrb()
    orb.show()

    parser = IntentParser()
    executor = Executor()
    logger = NovaLogger()
    voice = VoiceEngine()
    wake = LocalWake()

    session = NovaSession()

    # text input
    orb.send_button.clicked.connect(
        lambda: process_command(
            orb.get_text_command(),
            orb,
            parser,
            executor,
            logger,
            voice,
            session
        )
    )

    # background voice thread
    threading.Thread(
        target=voice_loop,
        args=(
            orb,
            parser,
            executor,
            logger,
            voice,
            wake,
            session
        ),
        daemon=True
    ).start()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()