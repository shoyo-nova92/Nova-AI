class TaskStack:

    def __init__(self):

        self.project = None
        self.milestone = None
        self.task = None
        self.subtask = None

    def update(

        self,

        project=None,

        milestone=None,

        task=None,

        subtask=None

    ):

        if project:
            self.project = project

        if milestone:
            self.milestone = milestone

        if task:
            self.task = task

        if subtask:
            self.subtask = subtask

    def get_stack(self):

        return {

            "project":
                self.project,

            "milestone":
                self.milestone,

            "task":
                self.task,

            "subtask":
                self.subtask

        }