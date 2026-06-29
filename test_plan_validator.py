from core.plan_normalizer import PlanNormalizer
from core.plan_validator import PlanValidator

normalizer = PlanNormalizer()
validator = PlanValidator()

plan = [
    "Open VS Code",
    "Create workflow_validator.py",
    "Implement parser"
]

actions = normalizer.normalize(plan)

validated = validator.validate(actions)

for action in validated:
    print(action)