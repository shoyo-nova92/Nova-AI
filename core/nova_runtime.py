import logging

from core.runtime_context import RuntimeContext
from core.task_translator import TaskTranslator
from core.recovery_engine import RecoveryEngine
from core.vision_engine import VisionEngine
from core.reasoning_engine import ReasoningEngine
from core.runtime_trace import RuntimeTrace
from core.runtime_events import RuntimeEvents
from core.runtime_state import RuntimeState
from core.execution_router import ExecutionRouter
from core.execution_policy import ExecutionPolicy
from core.context_fusion_engine import ContextFusionEngine
from core.memory_retriever import MemoryRetriever
from core.planner_pipeline import PlannerPipeline

logger = logging.getLogger(__name__)


FAST_PATH_ACTIONS = {   
    "open_app",
    "close_app",
    "read_file",
    "modify_file",
    "run_python",
    "run_pytest",
    "create_file",
    "create_folder",
    "replace_text",
    "append_file",
    "insert_at_line",
    "rollback_file",
    "git_status",
    "git_add",
    "git_commit",
    "git_checkout",
    "git_pull",
    "git_push",
    "pip_install",
    "build_project",
    "open_terminal",
}


class NovaRuntime:

    def __init__(self, llm_client):
        self.state = RuntimeState.IDLE
        self.current_goal = None
        self.ctx = None

        self.task_translator = TaskTranslator()
        self.vision = VisionEngine()
        self.trace = RuntimeTrace()
        self.reasoner = ReasoningEngine()
        self.fusion = ContextFusionEngine()
        self.memory = MemoryRetriever()
        self.llm_client = llm_client
        self.pipeline = PlannerPipeline()
        self.router = ExecutionRouter()
        self.policy = ExecutionPolicy()
        self.recovery_engine = RecoveryEngine()
        self.learning_engine = None

    def process_goal(self, goal):
        self.current_goal = goal
        normalized_goal = (goal or "").strip().lower()
        ctx = RuntimeContext(goal=goal)
        self.ctx = ctx

        self.trace.start_trace(goal)
        self._log("OBSERVE", f"Starting runtime orchestration for goal: {goal}")

        ctx = self._observe(ctx)
        ctx = self._build_context(ctx)
        ctx = self._retrieve_memory(ctx)
        ctx = self._create_plan(ctx)
        ctx = self._execute(ctx)
        ctx = self._verify(ctx)

        if ctx.status == "VERIFICATION_FAILED" or (ctx.verification and not ctx.verification.get("success")):
            ctx = self._recover(ctx)

        ctx = self._learn(ctx)
        self._finalize(ctx.to_dict())
        return ctx.to_dict()

    def cancel_goal(self):
        self.state = RuntimeState.FAILED
        self._log("CANCEL", "Goal cancelled")
        if self.ctx:
            self.ctx.status = "CANCELLED"
            self.ctx.metadata["cancelled"] = True
            return self.ctx.to_dict()
        return RuntimeContext(status="CANCELLED", metadata={"cancelled": True}).to_dict()

    def get_runtime_status(self):
        return {
            "state": self.state.value if hasattr(self.state, "value") else str(self.state),
            "goal": self.current_goal,
        }

    def get_current_goal(self):
        return self.current_goal

    # ----------------------------------------------------
    # Module 2 Controller State Transitions
    # ----------------------------------------------------

    def _observe(self, ctx: RuntimeContext) -> RuntimeContext:
        self.state = RuntimeState.OBSERVING
        ctx.status = "OBSERVING"
        self._log("OBSERVE", "Collecting environment context")
        ctx.vision = self.vision.analyze_screen()
        self.trace.log_event(RuntimeEvents.OBSERVE_COMPLETE, ctx.vision)
        return ctx

    def _build_context(self, ctx: RuntimeContext) -> RuntimeContext:
        self.state = RuntimeState.CONTEXT_BUILDING
        ctx.status = "CONTEXT_BUILDING"
        self._log("CONTEXT", "Building context from observations")
        ctx.reasoning = self.reasoner.analyze_context(ctx.vision)
        ctx.context = self.fusion.fuse(ctx.vision, ctx.reasoning)
        self.trace.log_event(RuntimeEvents.CONTEXT_UPDATED, ctx.context)
        return ctx

    def _retrieve_memory(self, ctx: RuntimeContext) -> RuntimeContext:
        self.state = RuntimeState.MEMORY_RETRIEVAL
        ctx.status = "MEMORY_RETRIEVAL"
        self._log("MEMORY", "Retrieving relevant memories")
        ctx.memories = self.memory.retrieve_relevant_context(ctx.context)
        return ctx

    def _create_plan(self, ctx: RuntimeContext) -> RuntimeContext:
        self.state = RuntimeState.PLANNING
        ctx.status = "PLANNING"

        # Module 3 — Fast Path check via TaskTranslator
        translated = self.task_translator.translate(ctx.goal)
        action_name = translated.get("action") if isinstance(translated, dict) else None

        if action_name and action_name in FAST_PATH_ACTIONS:
            self._log("PLAN", f"Fast Path matched action: {action_name}. Skipping planner.")
            fast_plan = [translated]
            ctx.raw_plan = fast_plan
            ctx.parsed_plan = fast_plan
            ctx.normalized_plan = fast_plan
            ctx.expanded_plan = fast_plan
            ctx.validated_plan = fast_plan
            ctx.repaired_plan = fast_plan
            ctx.confidence = 1.0
            ctx.metadata["fast_path"] = True
            return ctx

        # Module 4 — Planner Path for complex goals
        self._log(
            "PLAN",
            "Creating plan via unified LLMClient and PlannerPipeline"
        )

        planning_prompt = f"""
        Create an executable plan for the following user goal.

        USER GOAL:
        {ctx.goal}

        CURRENT CONTEXT:
        {ctx.context}

        RELEVANT MEMORIES:
        {ctx.memories}

        Return only the plan. Do not explain your reasoning.
        """

        raw_plan = self.llm_client.generate(
            prompt=planning_prompt,
            system_prompt="""

            You are Nova's internal planning module.

            Your ONLY responsibility is creating an execution plan.

            You are NOT the assistant.
            You are NOT responding to the user.

            Never:
            - answer questions
            - explain concepts
            - provide tutorials
            - give instructions for humans
            - mention shortcuts
            - mention menus
            - mention Windows settings
            - provide alternatives
            - provide options

            Your output will be consumed by Nova's execution pipeline.

            Return ONLY atomic execution steps.

            Format:

            1. action
            2. action
            3. action


            GOOD:

            User:
            "Create a python file called test.py and write hello world"

            Output:

            1. Create a file named test.py.
            2. Write "hello world" into test.py.
            3. Save the file.


            BAD:

            - Open Notepad by pressing Windows key.
            - Use File menu.
            - Option A / Option B.
            - Here is how you can do it manually.


            Rules:

            Each step must describe what Nova must perform.

            Not what the user should do.

            If the task requires unavailable visual interaction:

            Return:

            1. Perform visual interaction: <description>


            For information requests:

            Return:

            1. Provide information response to user.


            Return nothing except the numbered plan.

            """)

        planner_result = self.pipeline.process(raw_plan)

        ctx.raw_plan = raw_plan
        ctx.parsed_plan = planner_result.get("parsed_plan", [])
        ctx.normalized_plan = planner_result.get("normalized_plan", [])
        ctx.expanded_plan = planner_result.get("expanded_plan", [])
        ctx.validated_plan = planner_result.get("validated_plan", [])
        ctx.repaired_plan = planner_result.get("repaired_plan", [])
        ctx.confidence = planner_result.get("confidence", 1.0)
        ctx.metadata["fast_path"] = False

        self.trace.log_event(RuntimeEvents.PLAN_CREATED, planner_result)
        return ctx

    def _execute(self, ctx: RuntimeContext) -> RuntimeContext:
        self.state = RuntimeState.EXECUTING
        ctx.status = "EXECUTING"

        plan_to_execute = ctx.repaired_plan or ctx.raw_plan
        ctx.executions = []

        for action in plan_to_execute:
            normalized_action = action
            if not isinstance(action, dict):
                translated_action = self.task_translator.translate(str(action))
                if isinstance(translated_action, dict) and translated_action.get("action"):
                    normalized_action = translated_action

            self._log("EXECUTE", f"Executing action: {normalized_action}")
            policy = self.policy.classify(normalized_action)

            if policy.get("allowed"):
                execution_result = self.router.route(normalized_action)
            else:
                execution_result = {
                    "success": False,
                    "reason": policy.get("reason", "policy blocked action"),
                    "action": normalized_action.get("action") if isinstance(normalized_action, dict) else str(normalized_action),
                }

            execution_entry = {
                "action": normalized_action,
                "policy": policy,
                "result": execution_result,
            }
            ctx.executions.append(execution_entry)
            self.trace.log_event(RuntimeEvents.ACTION_EXECUTED, execution_entry)

        return ctx

    def _verify(self, ctx: RuntimeContext) -> RuntimeContext:
        self.state = RuntimeState.VERIFYING
        ctx.status = "VERIFYING"

        if not ctx.executions:
            ctx.verification = {"success": True, "reason": "No executions to verify"}
            return ctx

        last_execution = ctx.executions[-1]
        action = last_execution.get("action", {})
        action_name = action.get("action", "") if isinstance(action, dict) else str(action)
        action_target = action.get("target", "") if isinstance(action, dict) else ""

        target_str = f"{action_name} {action_target}".strip()
        self._log("VERIFY", f"Verifying action: {target_str}")

        exec_result = last_execution.get("result", {})

        router_verification = exec_result.get("verification")

        if router_verification:
            verification = router_verification
        else:
            verification = self.router.verifier.verify(target_str)
        
        # Check router execution status if available
        if isinstance(exec_result, dict):
            if exec_result.get("state") == "failed" or (
                isinstance(exec_result.get("execution"), dict)
                and exec_result.get("execution", {}).get("success") is False
            ):
                verification["success"] = False
                if "reason" not in verification or not verification["reason"]:
                    verification["reason"] = exec_result.get("reason") or exec_result.get("execution", {}).get("reason", "Action execution failed")

        ctx.verification = verification

        if not verification.get("success"):
            ctx.status = "VERIFICATION_FAILED"
            self._log("RECOVER", f"Verification failed: {verification.get('reason')}")

        return ctx

    def _recover(self, ctx: RuntimeContext) -> RuntimeContext:
        self.state = RuntimeState.RECOVERING
        ctx.status = "RECOVERING"
        self._log("RECOVER", "Executing recovery engine self-healing routine")

        last_execution = ctx.executions[-1] if ctx.executions else {"action": {}}
        failed_action = last_execution.get("action", {})

        recovery_info = self.recovery_engine.recover(failed_action, ctx.verification, ctx)
        recovery_action = recovery_info.get("recovery_action")

        if recovery_action:
            self._log("RECOVER", f"Attempting recovery action: {recovery_action}")
            policy = self.policy.classify(recovery_action)
            recovery_result = self.router.route(recovery_action)

            rec_execution_entry = {
                "action": recovery_action,
                "policy": policy,
                "result": recovery_result,
                "is_recovery": True,
            }
            ctx.executions.append(rec_execution_entry)

            # Prefer the verification already produced by the router for the recovery action.
            # Fall back to a direct verifier call only if the router does not attach verification.
            rec_action_name = recovery_action.get("action", "") if isinstance(recovery_action, dict) else str(recovery_action)
            rec_target = recovery_action.get("target", "") if isinstance(recovery_action, dict) else ""
            target_str = f"{rec_action_name} {rec_target}".strip()

            re_verification = recovery_result.get("verification")
            if not isinstance(re_verification, dict):
                re_verification = self.router.verifier.verify(target_str)

            if recovery_result.get("state") != "failed" and re_verification.get("success") is not False:
                re_verification["success"] = True
                ctx.verification = re_verification
                ctx.status = "RECOVERED"
                recovery_info["recovered"] = True
            else:
                ctx.verification = re_verification
                ctx.status = "FAILED"
                recovery_info["recovered"] = False

        ctx.recovery = recovery_info
        return ctx

    def _learn(self, ctx: RuntimeContext) -> RuntimeContext:
        self.state = RuntimeState.LEARNING
        if ctx.status not in ("FAILED", "VERIFICATION_FAILED"):
            ctx.status = "COMPLETED"
            self.state = RuntimeState.COMPLETED

        ctx.learning = {
            "recorded": True,
            "goal": ctx.goal,
            "executions_count": len(ctx.executions),
            "status": ctx.status,
        }

        self._log("LEARN", "Storing runtime outcome")
        self.trace.log_event(RuntimeEvents.GOAL_COMPLETED, {
            "goal": ctx.goal,
            "executions": ctx.executions,
            "status": ctx.status,
        })
        self.trace.save_trace()
        return ctx

    def _finalize(self, result):
        self.trace.log_event(RuntimeEvents.GOAL_COMPLETED, result)
        self.trace.save_trace()

    def _log(self, stage, message):
        self.trace.log_event(stage, message)
        logger.info("[%s] %s", stage, message)

    def run(self, goal=None):
        return self.process_goal(goal or self.current_goal)
