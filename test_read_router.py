from core.execution_router import (
    ExecutionRouter
)

router = ExecutionRouter()

result = router.route(

    {

        "type": "filesystem",

        "action": "read_file",

        "action_type": "read_file",

        "target": "core/task_translator.py"

    }

)

print(result)