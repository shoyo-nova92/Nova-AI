from core.vision_engine import VisionEngine
from core.reasoning_engine import ReasoningEngine
from core.context_fusion_engine import (
    ContextFusionEngine
)
from core.execution_verifier import (
    ExecutionVerifier
)

from core.adaptive_retry_engine import (
    AdaptiveRetryEngine
)

from core.memory_retriever import (
    MemoryRetriever
)

from core.contextual_prompt_builder import (
    ContextualPromptBuilder
)

from core.llm_planner import LLMPlanner


class NovaRuntime:

    def __init__(self):
        self.verifier = ExecutionVerifier()
        self.retry_engine = AdaptiveRetryEngine()
        self.vision = VisionEngine()

        self.reasoner = (
            ReasoningEngine()
        )

        self.fusion = (
            ContextFusionEngine()
        )

        self.memory = (
            MemoryRetriever()
        )

        self.prompt_builder = (
            ContextualPromptBuilder()
        )

        self.planner = (
            LLMPlanner()
        )

    def run(self):

        print(
            "\n=== NOVA RUNTIME STARTED ===\n"
        )

        # STEP 1
        vision_data = (
            self.vision.analyze_screen()
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

        # STEP 4
        memories = (
            self.memory.retrieve_relevant_context(
                context
            )
        )

        # STEP 5
        prompt = (
            self.prompt_builder.build(

                context,

                memories

            )
        )

        # STEP 6
        plan = (
            self.planner.create_plan(
                prompt
            )
        )

        # STEP 7
        verification = {
            "success": False,
            "reason": "Verification not executed."
        }

        recovery = {
            "status": "not_executed"
        }
        
        verification = (
            self.verifier.verify(
                "open notepad"
            )
        )

        # STEP 8

        recovery = (
            self.retry_engine.retry(

                "open notepad",

                verification

            )
        )

        runtime_state = {

            "status":
                "planning",

            "context":
                context,

            "memories":
                memories,

            "plan":
                plan,

            "verification":
                verification,

            "recovery":
                recovery

        }

        return runtime_state 