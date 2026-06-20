from core.work_context_engine import (
    WorkContextEngine
)

engine = (
    WorkContextEngine()
)

engine.objective_tracker.set_goal(
    "Build Work Context Layer"
)

engine.task_stack.update(

    project="Nova",

    milestone="v0.9.1",

    task="Build Work Context Layer",

    subtask="Create Work Context Engine"

)

print(
    engine.get_work_context()
)