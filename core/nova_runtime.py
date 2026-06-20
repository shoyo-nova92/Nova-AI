from core.vision_engine import VisionEngine
from core.reasoning_engine import ReasoningEngine
from core.runtime_trace import RuntimeTrace
from core.runtime_events import RuntimeEvents

from core.execution_router import (
    ExecutionRouter
)
from core.action_interpreter import (
    ActionInterpreter
)
from core.context_fusion_engine import (
    ContextFusionEngine
)

from core.memory_retriever import (
    MemoryRetriever
)

from core.llm_planner import LLMPlanner


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

        self.router = (
            ExecutionRouter()
        )

        self.interpreter = (
            ActionInterpreter()
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

        # STEP 1
        vision_data = (
            self.vision.analyze_screen()
        )

        self.trace.log_event(
            RuntimeEvents.OBSERVE_COMPLETE,
            vision_data
        )

        # STEP 2
        reasoning_data = (
            self.reasoner.analyze_context(
                vision_data
            )
        )

        # STEP 3
        context = self.fusion.fuse(

            vision_data,

            reasoning_data

        )

        self.trace.log_event(
            RuntimeEvents.CONTEXT_UPDATED,
            context
        )

        # STEP 4
        memories = (
            self.memory.retrieve_relevant_context(
                context
            )
        )

        # STEP 5
        plan = (
            self.planner.create_plan(
                runtime_goal,
                context,
                memories
            )
        )

        self.trace.log_event(
            RuntimeEvents.PLAN_CREATED,
            {
                "plan": plan
            }
        )

        execution_results = []

        for step in plan:

            action = (
                self.interpreter.interpret(
                    step
                )
            )

            if action["action_type"]:
                result = self.router.execute(
                    action_type=action[
                        "action_type"
                    ],
                    target=action[
                        "target"
                    ]
                )

            else:

                result = {

                    "success": False,

                    "reason":
                        f"unknown plan step: {step}"

                }

            execution_results.append(
                result
            )

            self.trace.log_event(
                RuntimeEvents.ACTION_EXECUTED,
                {

                    "step":
                        step,

                    "action":
                        action,

                    "result":
                        result

                }
            )

        runtime_state = {

            "status":
                "executing",

            "context":
                context,

            "memories":
                memories,

            "plan":
                plan,

            "executions":
                execution_results
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
