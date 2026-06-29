from core.plan_normalizer import PlanNormalizer

normalizer = PlanNormalizer()

plan = [
    "Open VS Code",
    "Create workflow_validator.py",
    "Run git status",
    "Implement parser"
]

normalized = normalizer.normalize(plan)

print("Normalized:")
print(normalized)
print("Count:", len(normalized))