import time

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

        start_time = time.time()

        results = []

        progress = []

        steps_completed = 0

        steps_failed = 0

        workflow_steps = [

            (
                "Open VS Code",

                "open_app",

                "vscode"

            ),

            (
                "Open Terminal",

                "open_terminal",

                None

            ),

            (
                "Git Status",

                "git_status",

                None

            )

        ]

        for index, step in enumerate(

            workflow_steps,

            start=1

        ):

            step_name = step[0]

            action_type = step[1]

            target = step[2]

            result = self.router.execute(

                action_type=action_type,

                target=target

            )

            results.append(
                result
            )

            if result["state"] == "complete":

                steps_completed += 1

                progress.append(

                    f"[{index}/{len(workflow_steps)}] "
                    f"{step_name} ✓"

                )

            else:

                steps_failed += 1

                progress.append(

                    f"[{index}/{len(workflow_steps)}] "
                    f"{step_name} ✗"

                )

        duration = round(

            time.time() - start_time,

            2

        )

        return {

            "workflow":
                "prepare_coding_environment",

            "steps_completed":
                steps_completed,

            "steps_failed":
                steps_failed,

            "duration":
                duration,

            "progress":
                progress,

            "results":
                results

        }