from core.objective_tracker import (
    ObjectiveTracker
)

tracker = (
    ObjectiveTracker()
)

tracker.set_goal(
    "Build Work Context Layer"
)

print(
    tracker.get_goal()
)