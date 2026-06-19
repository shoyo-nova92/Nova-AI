from core.environment_awareness_engine import (
    EnvironmentAwarenessEngine
)

engine = (
    EnvironmentAwarenessEngine()
)

result = (
    engine.analyze()
)

print(
    "\n=== ENVIRONMENT AWARENESS ===\n"
)

print(result)