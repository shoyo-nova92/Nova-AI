from core.application_handler import (
    ApplicationHandler
)

from core.terminal_handler import (
    TerminalHandler
)


class WorkflowHandler:

    def __init__(self):

        self.apps = (
            ApplicationHandler()
        )

        self.terminal = (
            TerminalHandler()
        )

    def prepare_coding_environment(
        self
    ):

        results = []

        results.append(

            self.apps.open_app(
                "vscode"
            )

        )

        results.append(

            self.terminal.open_terminal()

        )

        results.append(

            self.terminal.git_status()

        )

        return {

            "workflow":
                "prepare_coding_environment",

            "results":
                results

        }