from core.execution_verifier import ExecutionVerifier

verifier = ExecutionVerifier()

result = verifier.verify(
    "open notepad"
)

print("\n=== VERIFICATION RESULT ===\n")
print(result)