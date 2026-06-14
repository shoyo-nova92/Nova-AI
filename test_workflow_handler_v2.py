from core.workflow_handler import (
    WorkflowHandler
)

workflow = (
    WorkflowHandler()
)

result = (

    workflow.prepare_coding_environment()

)

print(
    "\n=== WORKFLOW V2 ===\n"
)

print(result)