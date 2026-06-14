from core.self_correction_engine import (
    SelfCorrectionEngine
)

engine = (
    SelfCorrectionEngine()
)

result = engine.diagnose(

    action="open notepad",

    failure_reason=
    "process not found"

)

print(
    "\n=== SELF CORRECTION ===\n"
)

print(result)