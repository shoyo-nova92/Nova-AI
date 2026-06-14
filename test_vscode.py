# test_vscode.py

from core.application_handler import (
    ApplicationHandler
)

handler = (
    ApplicationHandler()
)

result = handler.open_app(
    "vscode"
)

print(result)