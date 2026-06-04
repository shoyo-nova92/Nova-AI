from core.nova_runtime import (
    NovaRuntime
)

nova = NovaRuntime()

result = nova.run()

print(
    "\n=== RUNTIME STATE ===\n"
)

print(result)