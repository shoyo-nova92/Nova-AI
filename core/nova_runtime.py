from datetime import datetime

from core.planner_pipeline import (
    PlannerPipeline
)

from core.vision_engine import VisionEngine
from core.reasoning_engine import ReasoningEngine
from core.runtime_trace import RuntimeTrace
from core.runtime_events import RuntimeEvents

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

        self.vision = VisionEngine()

        self.trace = RuntimeTrace()

        self.reasoner = (
            ReasoningEngine()
        )

        self.fusion = (
            ContextFusionEngine()
        )

        self.memory = (
            MemoryRetriever()
        )

        self.planner = (
            LLMPlanner()
        )

        self.pipeline = (
            PlannerPipeline()
        )

        self.router = (
            ExecutionRouter()
        )

        self.policy = (
            ExecutionPolicy()
        )

    def run(
        self,
        goal=None
    ):

        print(
            "\n=== NOVA RUNTIME STARTED ===\n"
        )

        runtime_goal = (

            goal

            or

            "continue_current_workflow"

        )

        self.trace.start_trace(
            runtime_goal
        )

        # -------------------------------------------------
        # STEP 1 — OBSERVE
        # -------------------------------------------------

        vision_data = (
            self.vision.analyze_screen()
        )

        self.trace.log_event(
            RuntimeEvents.OBSERVE_COMPLETE,
            vision_data
        )

        # -------------------------------------------------
        # STEP 2 — REASON
        # -------------------------------------------------

        reasoning_data = (

            self.reasoner.analyze_context(
                vision_data
            )

        )

        # -------------------------------------------------
        # STEP 3 — CONTEXT FUSION
        # -------------------------------------------------

        context = self.fusion.fuse(

            vision_data,

            reasoning_data

        )

        self.trace.log_event(

            RuntimeEvents.CONTEXT_UPDATED,

            context

        )

        # -------------------------------------------------
        # STEP 4 — MEMORY
        # -------------------------------------------------

        memories = (

            self.memory.retrieve_relevant_context(
                context
            )

        )

        # -------------------------------------------------
        # STEP 5 — PLAN
        # -------------------------------------------------

        raw_plan = self.planner.create_plan(
            runtime_goal,
            context,
            memories
        )

        planner_result = (
            self.pipeline.process(
                raw_plan
            )
        )

        validated_plan = (
            planner_result[
                "validated_plan"
            ]
        )

        self.trace.log_event(

            RuntimeEvents.PLAN_CREATED,

            planner_result

        )

        # -------------------------------------------------
        # STEP 6 — POLICY → EXECUTE
        # -------------------------------------------------

        execution_results = []

        for action in validated_plan:

            print("\n========================")
            print("ACTION:")
            print(action)

            policy = (
                self.policy.classify(
                    action
                )
            )

            print("POLICY:")
            print(policy)

            if policy["allowed"]:

                result = (
                    self.router.route(
                        action
                    )
                )

            else:

                result = {

                    "success": False,

                    "reason":
                        policy["reason"]

                }

            print("RESULT:")
            print(result)

            execution_record = {

                "action":
                    action,

                "policy":
                    policy,

                "result":
                    result

            }

            execution_results.append(
                execution_record
            )

            self.trace.log_event(

                RuntimeEvents.ACTION_EXECUTED,

                execution_record

            )

        # -------------------------------------------------
        # STEP 7 — COMPLETE
        # -------------------------------------------------

        runtime_state = {

            "status":
                "executing",

            "goal":
                runtime_goal,

            "context":
                context,

            "memories":
                memories,

            "raw_plan":
                planner_result["raw_plan"],

            "parsed_plan":
                planner_result["parsed_plan"],

            "quality":
                planner_result["quality"],

            "confidence":
                planner_result["confidence"],

            "normalized_plan":
                planner_result["normalized_plan"],

            "validated_plan":
                planner_result["validated_plan"],

            "executions":
                execution_results,

            "timestamp":
                datetime.now().isoformat()

        }

        self.trace.log_event(

            RuntimeEvents.GOAL_COMPLETED,

            runtime_state

        )

        self.trace.save_trace()

        return runtime_state

    def process_goal(
        self,
        goal
    ):

        return self.run(
            goal
        )
