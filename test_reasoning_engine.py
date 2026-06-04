from core.vision_engine import VisionEngine
from core.reasoning_engine import ReasoningEngine

vision = VisionEngine()

reasoner = ReasoningEngine()

vision_data = vision.analyze_screen()

print("\n=== VISION DATA ===")
print(vision_data)

print("\n=== ACTIVE WINDOW ===")
print(vision_data["active_window"]["title"])

print("\n=== OCR TEXT ===")
print(vision_data["visible_text"])

result = reasoner.analyze_context(
    vision_data
)

print("\n=== REASONING RESULT ===")
print(result)