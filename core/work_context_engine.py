from core.project_context_engine import (
    ProjectContextEngine
)

from core.objective_tracker import (
    ObjectiveTracker
)

from core.task_stack import (
    TaskStack
)

from core.project_memory import (
    ProjectMemory
)


class WorkContextEngine:

    def __init__(self):

        self.project_context = (
            ProjectContextEngine()
        )

        self.objective_tracker = (
            ObjectiveTracker()
        )

        self.task_stack = (
            TaskStack()
        )

        self.project_memory = (
            ProjectMemory()
        )

    def get_work_context(self):

        return {

            "project_context":
                self.project_context.get_context(),

            "objective":
                self.objective_tracker.get_goal(),

            "task_stack":
                self.task_stack.get_stack(),

            "recent_decisions":
                self.project_memory.load_memory()[-5:]

        }