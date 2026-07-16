from datetime import datetime

from core.planner_pipeline import (
    PlannerPipeline
)

from core.vision_engine import VisionEngine
from core.reasoning_engine import ReasoningEngine
from core.runtime_trace import RuntimeTrace
from core.runtime_events import RuntimeEvents
from core.runtime_state import RuntimeState

from core.execution_router import (
    ExecutionRouter
)

from core.execution_policy import (
    ExecutionPolicy
)

from core.context_fusion_engine import (
    ContextFusionEngine
)

from core.memory_retriever import (
    MemoryRetriever
)

from core.llm_planner import (
    LLMPlanner
)


class NovaRuntime:

    def __init__(self):
        self.state = RuntimeState.IDLE
        self.current_goal = None
        self.vision = VisionEngine()
        self.trace = RuntimeTrace()
        self.reasoner = ReasoningEngine()
        self.fusion = ContextFusionEngine()
        self.memory = MemoryRetriever()
        self.planner = LLMPlanner()
        self.pipeline = PlannerPipeline()
        self.router = ExecutionRouter()
        self.policy = ExecutionPolicy()
        self.recovery_engine = None
        self.learning_engine = None

    def process_goal(self, goal):
        self.current_goal = goal
        self.state = RuntimeState.OBSERVING
        self.trace.start_trace(goal)
        self._log("OBSERVE", "Starting runtime orchestration")

        result = self._run_pipeline(goal)
        self._finalize(result)
        return result

    def cancel_goal(self):
        self.state = RuntimeState.FAILED
        self._log("CANCEL", "Goal cancelled")
        return self._build_result(False, "Goal cancelled", metadata={"cancelled": True})

    def get_runtime_status(self):
        return {
            "state": self.state.value,
            "goal": self.current_goal,
        }

    def get_current_goal(self):
        return self.current_goal

    def _run_pipeline(self, goal):
        self._log("OBSERVE", "Collecting environment context")
        vision_data = self.vision.analyze_screen()
        self.trace.log_event(RuntimeEvents.OBSERVE_COMPLETE, vision_data)

        self.state = RuntimeState.CONTEXT_BUILDING
        self._log("CONTEXT", "Building context from observations")
        reasoning_data = self.reasoner.analyze_context(vision_data)
        context = self.fusion.fuse(vision_data, reasoning_data)
        self.trace.log_event(RuntimeEvents.CONTEXT_UPDATED, context)

        self.state = RuntimeState.MEMORY_RETRIEVAL
        self._log("MEMORY", "Retrieving relevant memories")
        memories = self.memory.retrieve_relevant_context(context)

        self.state = RuntimeState.PLANNING
        self._log("PLAN", "Creating a plan")
        raw_plan = self.planner.create_plan(goal, context, memories)
        planner_result = self.pipeline.process(raw_plan)
        repaired_plan = planner_result.get("repaired_plan", [])
        self.trace.log_event(RuntimeEvents.PLAN_CREATED, planner_result)

        self.state = RuntimeState.VALIDATING_PLAN
        self._log("VALIDATE", "Validating execution plan")

        execution_results = []
        for action in repaired_plan:
            self.state = RuntimeState.EXECUTING
            self._log("EXECUTE", f"Executing action: {action}")
            policy = self.policy.classify(action)
            if policy.get("allowed"):
                execution_result = self.router.route(action)
            else:
                execution_result = {
                    "success": False,
                    "reason": policy.get("reason", "policy blocked action"),
                    "action": action.get("action"),
                }
            execution_results.append({
                "action": action,
                "policy": policy,
                "result": execution_result,
            })
            self.trace.log_event(RuntimeEvents.ACTION_EXECUTED, execution_results[-1])

            self.state = RuntimeState.VERIFYING
            self._log("VERIFY", f"Verifying action: {action}")
            verification = self.router.verifier.verify(f"{action.get('action')} {action.get('target')}")
            if not verification.get("success"):
                self.state = RuntimeState.RECOVERING
                self._log("RECOVER", verification.get("reason", "verification failed"))
                return self._build_result(False, "Runtime execution failed", data={
                    "goal": goal,
                    "context": context,
                    "memories": memories,
                    "plan": planner_result,
                    "executions": execution_results,
                    "verification": verification,
                }, confidence=planner_result.get("confidence", 0.0), metadata={"recovered": False})

        self.state = RuntimeState.LEARNING
        self._log("LEARN", "Storing runtime outcome")
        self.trace.log_event(RuntimeEvents.GOAL_COMPLETED, {
            "goal": goal,
            "executions": execution_results,
        })
        self.trace.save_trace()
        self.state = RuntimeState.COMPLETED
        return self._build_result(True, "Goal completed", data={
            "goal": goal,
            "context": context,
            "memories": memories,
            "plan": planner_result,
            "executions": execution_results,
        }, confidence=planner_result.get("confidence", 0.0), metadata={"completed": True})

    def _build_result(self, success, message, data=None, confidence=0.0, metadata=None):
        return {
            "success": success,
            "data": data or {},
            "message": message,
            "confidence": confidence,
            "metadata": metadata or {},
        }

    def _finalize(self, result):
        self.trace.log_event(RuntimeEvents.GOAL_COMPLETED, result)
        self.trace.save_trace()

    def _log(self, stage, message):
        self.trace.log_event(stage, message)
        print(f"[{stage}] {message}")

    def run(self, goal=None):
        return self.process_goal(goal or self.current_goal)
