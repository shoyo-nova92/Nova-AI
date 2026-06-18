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

    def execute(

        self,

        action_type,

        target=None

    ):

        self.state = (
            RuntimeState.RUNNING
        )

        start_time = time.time()

        result = None

        if action_type == "open_app":

            result = (

                self.apps.open_app(
                    target
                )

            )

        elif action_type == "search":

            result = (

                self.browser.search_query(
                    target
                )

            )

        elif action_type == "create_folder":

            result = (

                self.filesystem.create_folder(
                    target
                )

            )

        elif action_type == "open_terminal":

            result = (

                self.terminal.open_terminal()

            )

        elif action_type == "git_status":

            result = (

                self.terminal.git_status()

            )

        else:

            result = {

                "success": False,

                "reason":
                    f"Unknown action: {action_type}"

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

                    f"{action_type} {target}"

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

                    f"{action_type} {target}",

                    verification

                )

            )

            correction = (

                self.corrector.diagnose(

                    f"{action_type} {target}",

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

            action=f"{action_type} {target}",

            success=success,

            duration=duration,

            failure_reason=None

            if success

            else verification["reason"]

        )

        skill_name = action_type

        if target:
            skill_name = f"{action_type}:{target}"

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

            goal=f"{action_type} {target}",

            details={

                "duration":
                    duration,

                "state":
                    self.state.value

            }

        )

        self.logger.log_event(

            event_type=
                "execution_success"
                if success
                else
                "execution_failure",

            goal=f"{action_type} {target}",

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