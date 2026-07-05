from core.planner_pipeline import (
    PlannerPipeline
)

pipeline = PlannerPipeline()

raw_plan = """
Open VS Code
Create workflow_validator.py
Run git status

class Example:
    pass
"""

result = pipeline.process(raw_plan)

for key, value in result.items():
    print()
    print(key.upper())
    print(value)