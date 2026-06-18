from core.workflow_handler import (
    WorkflowHandler
)

import time

workflow = WorkflowHandler()

print("\n=== WORKFLOW SUITE ===\n")

result = (

    workflow.prepare_coding_environment()

)

print(result)