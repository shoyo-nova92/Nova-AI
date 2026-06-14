from core.terminal_handler import (
    TerminalHandler
)

handler = (
    TerminalHandler()
)

result = handler.git_status()

print(
    "\n=== TERMINAL RESULT ===\n"
)

print(result)