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

from core.git_handler import (
    GitHandler
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
        self.state = RuntimeState.IDLE

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

        self.git = (
            GitHandler()
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
                "filesystem",
                "read_file"
            ):
                lambda target:
                    self.filesystem.read_file(
                        target
                    ),

            (
                "filesystem",
                "modify_file"
            ):
                lambda target:
                    self.filesystem.modify_file(
                        target,
                        ""
                    ),

            (
                "filesystem",
                "replace_text"
            ):
                lambda target:
                    self.filesystem.replace_text(
                        target,
                        "",
                        ""
                    ),

            (
                "filesystem",
                "append_file"
            ):
                lambda target:
                    self.filesystem.append_file(
                        target,
                        ""
                    ),

            (
                "filesystem",
                "insert_at_line"
            ):
                lambda target:
                    self.filesystem.insert_at_line(
                        target,
                        0,
                        ""
                    ),

            (
                "filesystem",
                "rollback_file"
            ):
                lambda target:
                    self.filesystem.rollback_file(
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
                "run_python"
            ):
                lambda target:
                    self.terminal.run_python(
                        target
                    ),

            (
                "terminal",
                "run_pytest"
            ):
                lambda target:
                    self.terminal.run_pytest(
                        target
                    ),

            (
                "terminal",
                "pip_install"
            ):
                lambda target:
                    self.terminal.pip_install(
                        target
                    ),

            (
                "terminal",
                "build_project"
            ):
                lambda target:
                    self.terminal.build_project(
                        target
                    ),

            (
                "terminal",
                "git_status"
            ):
                lambda target:
                    self.terminal.git_status(),

            (
                "git",
                "git_add"
            ):
                lambda target:
                    self.git.git_add(target),

            (
                "git",
                "git_commit"
            ):
                lambda target:
                    self.git.git_commit(target),

            (
                "git",
                "git_checkout"
            ):
                lambda target:
                    self.git.git_checkout(target),

            (
                "git",
                "git_pull"
            ):
                lambda target:
                    self.git.git_pull(),

            (
                "git",
                "git_push"
            ):
                lambda target:
                    self.git.git_push()

        }

    def execute(

        self,

        action_type,

        target=None,
        new_content=None

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
                    target,

                "new_content":
                    new_content
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

        self.state = RuntimeState.EXECUTING

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

            if action_name == "modify_file":

                result = self.filesystem.modify_file(
                    target,
                    action.get("new_content", "")
                )

            elif action_name == "replace_text":

                parameters = action.get("parameters", {})

                result = self.filesystem.replace_text(
                    target,
                    parameters.get("old"),
                    parameters.get("new")
                )

            elif action_name == "append_file":

                parameters = action.get("parameters", {})

                result = self.filesystem.append_file(
                    target,
                    parameters.get("content", "")
                )

            elif action_name == "insert_at_line":

                parameters = action.get("parameters", {})

                result = self.filesystem.insert_at_line(
                    target,
                    parameters.get("line"),
                    parameters.get("content", "")
                )

            elif action_name == "rollback_file":

                result = self.filesystem.rollback_file(
                    target
                )

            else:

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

        if (

            action_name == "read_file"

            and

            result.get("success")

        ):

            result = {

                "success": True,

                "action": result["action"],

                "path": result["path"],

                "lines": result["lines"]

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

            rollback_result = None

            if action_name in {

                "modify_file",

                "replace_text",

                "append_file",

                "insert_at_line"

            } and target:

                rollback_result = (
                    self.filesystem.rollback_file(
                        target
                    )
                )

            recovery = (

                self.retry_engine.retry(

                    action_label,

                    verification

                )

            )

            if rollback_result is not None:

                recovery["rollback"] = rollback_result

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

            self.state = RuntimeState.COMPLETED

        duration = round(

            time.time() - start_time,

            2

        )

        success = (

            self.state
            ==
            RuntimeState.COMPLETED

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
            "create_folder",
            "read_file",
            "modify_file",
            "replace_text",
            "append_file",
            "insert_at_line",
            "rollback_file"
        ]:

            return "filesystem"

        if action_name in [
            "open_terminal",
            "git_status",
            "run_python",
            "run_pytest",
            "pip_install",
            "build_project"
        ]:

            return "terminal"

        if action_name in [
            "git_add",
            "git_commit",
            "git_checkout",
            "git_pull",
            "git_push"
        ]:

            return "git"

        if action_name == "open_app":

            return "application"

        if action_name == "search":

            return "browser"

        return None
