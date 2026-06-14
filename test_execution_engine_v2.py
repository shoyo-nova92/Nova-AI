from core.execution_engine_v2 import (
    ExecutionEngineV2
)

engine = (
    ExecutionEngineV2()
)

result = engine.execute(
    "open notepad"
)

print(
    "\n=== EXECUTION RESULT ===\n"
)

print(result)