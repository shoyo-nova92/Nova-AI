from core.execution_router import (
    ExecutionRouter
)


class WorkflowHandler:

    def __init__(self):

        self.router = (
            ExecutionRouter()
        )

    def prepare_coding_environment(
        self
    ):

        results = []

        results.append(

            self.router.execute(

                action_type="open_app",

                target="vscode"

            )

        )

        results.append(

            self.router.execute(

                action_type="open_terminal"

            )

        )

        results.append(

            self.router.execute(

                action_type="git_status"

            )

        )

        return {

            "workflow":
                "prepare_coding_environment",

            "results":
                results

        }