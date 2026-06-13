from core.memory_auto_logger import (
    MemoryAutoLogger
)

logger = MemoryAutoLogger()

result = logger.log_event(

    event_type="plan_success",

    goal="Improve OCR performance",

    details={

        "plan_steps": 5,

        "verification": True

    }

)

print(
    "\n=== LOGGED EVENT ===\n"
)

print(result)