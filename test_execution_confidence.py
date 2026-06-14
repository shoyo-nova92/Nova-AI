from core.execution_confidence import (
    ExecutionConfidence
)

engine = (
    ExecutionConfidence()
)

result = engine.estimate(
    "open notepad"
)

print(
    "\n=== EXECUTION CONFIDENCE ===\n"
)

print(result)