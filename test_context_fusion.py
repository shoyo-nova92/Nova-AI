from core.vision_engine import VisionEngine
from core.reasoning_engine import ReasoningEngine
from core.context_fusion_engine import ContextFusionEngine

vision = VisionEngine()

reasoner = ReasoningEngine()

fusion = ContextFusionEngine()

vision_data = vision.analyze_screen()

reasoning_data = reasoner.analyze_context(
    vision_data
)

context = fusion.fuse(
    vision_data,
    reasoning_data
)

print("\n=== FUSED CONTEXT ===\n")

print(context)