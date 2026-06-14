from core.application_handler import (
    ApplicationHandler
)

handler = (
    ApplicationHandler()
)

print(
    "\n=== APPLICATION HANDLER ===\n"
)

result = handler.open_app(
    "vscode"
)

print(result)