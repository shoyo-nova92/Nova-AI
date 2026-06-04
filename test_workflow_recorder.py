from core.workflow_recorder import WorkflowRecorder

recorder = WorkflowRecorder()

steps = [
    "open chrome",
    "open photoshop",
    "search thumbnail references"
]

recorder.record_workflow(
    "thumbnail workflow",
    steps
)

saved = recorder.get_workflow(
    "thumbnail workflow"
)

print(saved)