from core.execution_router import (
    ExecutionRouter
)

router = (
    ExecutionRouter()
)

result = router.execute(

    action_type="open_app",

    target="fakeapp"

)

print(
    "\n=== RECOVERY TEST ===\n"
)

print(result)