from core.vision_engine import VisionEngine
from core.reasoning_engine import ReasoningEngine
from core.context_fusion_engine import (
    ContextFusionEngine
)

from core.memory_retriever import (
    MemoryRetriever
)

from core.contextual_prompt_builder import (
    ContextualPromptBuilder
)

from core.llm_planner import LLMPlanner


class CognitiveLoop:

    def __init__(self):

        self.vision = VisionEngine()

        self.reasoner = ReasoningEngine()

        self.fusion = (
            ContextFusionEngine()
        )

        self.memory = (
            MemoryRetriever()
        )

        self.prompt_builder = (
            ContextualPromptBuilder()
        )

        self.planner = LLMPlanner()

    def think(self):

        # STEP 1 — OBSERVE
        vision_data = (
            self.vision.analyze_screen()
        )

        # STEP 2 — REASON
        reasoning_data = (
            self.reasoner.analyze_context(
                vision_data
            )
        )

        # STEP 3 — CONTEXT FUSION
        context = self.fusion.fuse(

            vision_data,

            reasoning_data

        )

        # STEP 4 — MEMORY RETRIEVAL
        memories = (
            self.memory.retrieve_relevant_context(
                context
            )
        )

        # STEP 5 — BUILD CONTEXTUAL PROMPT
        prompt = (
            self.prompt_builder.build(

                context,

                memories

            )
        )

        # STEP 6 — GENERATE PLAN
        plan = (
            self.planner.create_plan(
                prompt
            )
        )

        return {

            "context": context,

            "retrieved_memories":
                memories,

            "prompt":
                prompt,

            "plan":
                plan

        }