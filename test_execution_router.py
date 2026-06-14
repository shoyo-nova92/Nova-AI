from core.execution_router import (
    ExecutionRouter
)

router = (
    ExecutionRouter()
)

result = router.execute(

    action_type="open_app",

    target="vscode"

)

print(
    "\n=== ROUTER RESULT ===\n"
)

print(result)