from core.workflow_handler import (
    WorkflowHandler
)

handler = (
    WorkflowHandler()
)

result = (
    handler.prepare_coding_environment()
)

print(
    "\n=== WORKFLOW V2 ===\n"
)

for item in result["progress"]:

    print(item)

print()

print(

    "Completed:",

    result["steps_completed"]

)

print(

    "Failed:",

    result["steps_failed"]

)

print(

    "Duration:",

    result["duration"]

)

print()

print(result)