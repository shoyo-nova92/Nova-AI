from core.plan_parser import PlanParser

parser = PlanParser()

raw = "\n".join([
    "1. Create workflow_validator.py",
    "",
    "```python",
    "class WorkflowValidator:",
    "    pass",
    "```",
    "",
    "Run git status",
    "",
    "This validator checks workflow integrity."
])

parsed = parser.parse(raw)

for item in parsed:
    print(item)