from core.plan_normalizer import PlanNormalizer

plan = [
    {"type": "action", "text": "Open VS Code"},
    {"type": "code_start"},
    {"type": "code", "text": "print('hello')"},
    {"type": "code_end"},
    {"type": "action", "text": "Create workflow_validator.py"},
    {"type": "action", "text": "Run git status"},
    {"type": "text", "text": "This is documentation"}
]

normalizer = PlanNormalizer()

normalized = normalizer.normalize(plan)

print("Normalized:")
print(normalized)

print("Count:", len(normalized))