from core.runtime_state import (
    RuntimeState
)

print(
    "\n=== RUNTIME STATES ===\n"
)

for state in RuntimeState:

    print(
        state.value
    )