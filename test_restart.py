from core.application_handler import (
    ApplicationHandler
)

handler = (
    ApplicationHandler()
)

print(
    "\n=== RESTART TEST ===\n"
)

result = handler.restart_app(
    "Code",
    "vscode"
)

print(result)