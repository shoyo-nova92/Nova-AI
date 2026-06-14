from core.execution_memory import (
    ExecutionMemory
)

memory = (
    ExecutionMemory()
)

memory.record(

    action="open notepad",

    success=True,

    duration=1.2

)

memory.record(

    action="open notepad",

    success=True,

    duration=1.1

)

memory.record(

    action="open notepad",

    success=False,

    duration=2.5,

    failure_reason="process not found"

)

print(
    "\n=== EXECUTION STATS ===\n"
)

print(

    memory.get_stats(
        "open notepad"
    )

)