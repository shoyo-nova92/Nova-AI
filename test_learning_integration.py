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
    "\n=== LEARNING TEST ===\n"
)

print(result)