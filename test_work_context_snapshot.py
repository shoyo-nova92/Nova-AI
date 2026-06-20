from core.work_context_snapshot import (
    WorkContextSnapshot
)

snapshot = (
    WorkContextSnapshot()
)

result = snapshot.save_snapshot({

    "project":
        "Nova",

    "objective":
        "Build Goal Runtime",

    "task":
        "Create Planner"

})

print(result)