from core.runtime_state import (
    RuntimeState
)

from core.application_handler import (
    ApplicationHandler
)

from core.filesystem_handler import (
    FilesystemHandler
)

from core.browser_handler import (
    BrowserHandler
)

from core.terminal_handler import (
    TerminalHandler
)

from core.execution_verifier import (
    ExecutionVerifier
)

from core.adaptive_retry_engine import (
    AdaptiveRetryEngine
)

from core.self_correction_engine import (
    SelfCorrectionEngine
)

from core.execution_memory import (
    ExecutionMemory
)

from core.execution_confidence import (
    ExecutionConfidence
)

from core.skill_system import (
    SkillSystem
)

from core.memory_auto_logger import (
    MemoryAutoLogger
)

import time


class ExecutionRouter:

    def __init__(self):

        self.state = (
            RuntimeState.PENDING
        )

        self.apps = (
            ApplicationHandler()
        )

        self.filesystem = (
            FilesystemHandler()
        )

        self.browser = (
            BrowserHandler()
        )

        self.terminal = (
            TerminalHandler()
        )

        self.verifier = (
            ExecutionVerifier()
        )

        self.retry_engine = (
            AdaptiveRetryEngine()
        )

        self.corrector = (
            SelfCorrectionEngine()
        )

        self.memory = (
            ExecutionMemory()
        )

        self.confidence = (
            ExecutionConfidence()
        )

        self.skills = (
            SkillSystem()
        )

        self.logger = (
            MemoryAutoLogger()
        )

        self.routes = {

            (
                "application",
                "open_app"
            ):
                lambda target:
                    self.apps.open_app(
                        target
                    ),

            (
                "browser",
                "search"
            ):
                lambda target:
                    self.browser.search_query(
                        target
                    ),

            (
                "filesystem",
                "create_folder"
            ):
                lambda target:
                    self.filesystem.create_folder(
                        target
                    ),

            (
                "filesystem",
                "create_file"
            ):
                lambda target:
                    self.filesystem.create_file(
                        target
                    ),

            (
                "terminal",
                "open_terminal"
            ):
                lambda target:
                    self.terminal.open_terminal(),

            (
                "terminal",
                "git_status"
            ):
                lambda target:
                    self.terminal.git_status()

        }

    def execute(

        self,

        action_type,

        target=None

    ):

        return self.route(
            {
                "type":
                    self._infer_action_type(
                        action_type
                    ),

                "action":
                    action_type,

                "action_type":
                    action_type,

                "target":
                    target
            }
        )

    def route(

        self,

        action

    ):

        action_category = action.get(
            "type"
        )

        action_name = action.get(
            "action"
        )

        target = action.get(
            "target"
        )

        action_label = (
            f"{action_name} {target}"
        )

        self.state = (
            RuntimeState.RUNNING
        )

        start_time = time.time()

        result = None

        route_key = (
            action_category,
            action_name
        )

        handler = self.routes.get(
            route_key
        )

        if handler:

            result = (
                handler(
                    target
                )
            )

        else:

            result = {

                "success": False,

                "reason":
                    f"Unknown action: {action_category}:{action_name}"

            }

        self.state = (
            RuntimeState.VERIFYING
        )

        verification = {

            "success": False,

            "reason":
                "verification skipped"

        }

        if result.get("success"):

            verification = (

                self.verifier.verify(

                    action_label

                )

            )

        else:

            verification = {

                "success": False,

                "reason":

                    result.get(

                        "reason",

                        "execution failed"

                    )

            }

        recovery = None

        if not verification.get("success"):

            self.state = (
                RuntimeState.RECOVERING
            )

            recovery = (

                self.retry_engine.retry(

                    action_label,

                    verification

                )

            )

            correction = (

                self.corrector.diagnose(

                    action_label,

                    verification[
                        "reason"
                    ]

                )

            )

            recovery[
                "self_correction"
            ] = correction

            self.state = (
                RuntimeState.FAILED
            )

        else:

            self.state = (
                RuntimeState.COMPLETE
            )

        duration = round(

            time.time() - start_time,

            2

        )

        success = (

            self.state
            ==
            RuntimeState.COMPLETE

        )

        self.memory.record(

            action=action_label,

            success=success,

            duration=duration,

            failure_reason=None

            if success

            else verification["reason"]

        )

        skill_name = action_name

        if target:
            skill_name = f"{action_name}:{target}"

        self.skills.update_skill(
            skill_name,
            verification["success"]
        )

        self.logger.log_event(

            event_type=
                "execution_success"
                if success
                else
                "execution_failure",

            goal=action_label,

            details={

                "duration":
                    duration,

                "state":
                    self.state.value

            }

        )

        confidence = self.confidence.estimate(
            skill_name
        )

        return {

            "state":
                self.state.value,

            "execution":
                result,

            "verification":
                verification,

            "recovery":
                recovery,

            "confidence":
                confidence,

            "duration":
                duration

        }

    def _infer_action_type(
        self,
        action_name
    ):

        if action_name in [
            "create_file",
            "create_folder"
        ]:

            return "filesystem"

        if action_name in [
            "open_terminal",
            "git_status"
        ]:

            return "terminal"

        if action_name == "open_app":

            return "application"

        if action_name == "search":

            return "browser"

        return None
