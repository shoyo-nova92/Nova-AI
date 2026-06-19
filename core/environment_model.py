from datetime import datetime


class EnvironmentModel:

    def __init__(self):

        self.current_app = None

        self.current_window = None

        self.current_project = None

        self.current_goal = None

        self.current_screen = None

        self.last_updated = None

    def update(

        self,

        app=None,

        window=None,

        project=None,

        goal=None,

        screen=None

    ):

        if app:
            self.current_app = app

        if window:
            self.current_window = window

        if project:
            self.current_project = project

        if goal:
            self.current_goal = goal

        if screen:
            self.current_screen = screen

        self.last_updated = str(
            datetime.now()
        )

    def get_state(self):

        return {

            "current_app":
                self.current_app,

            "current_window":
                self.current_window,

            "current_project":
                self.current_project,

            "current_goal":
                self.current_goal,

            "current_screen":
                self.current_screen,

            "last_updated":
                self.last_updated
        }