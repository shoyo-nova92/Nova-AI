from core.execution_verifier import (
    ExecutionVerifier
)

verifier = (
    ExecutionVerifier()
)

print(
    "\n=== VERIFICATION ===\n"
)

result = verifier.verify(
    "open vscode"
)

print(result)