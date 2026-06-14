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
    "\n=== WORKFLOW RESULT ===\n"
)

print(result)