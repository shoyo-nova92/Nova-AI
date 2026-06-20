from core.project_memory import (
    ProjectMemory
)

memory = ProjectMemory()

memory.add_decision(

    decision=
    "Build Work Context Layer",

    reason=
    "Required before Goal Runtime",

    outcome=
    "In Progress"

)

print(

    memory.load_memory()

)