import datetime
from core.session_continuity import (
    SessionContinuity
)


class WorkContextSnapshot:

    def __init__(self):

        self.session = (
            SessionContinuity()
        )

    def save_snapshot(

        self,

        work_context

    ):

        snapshot = {

            "project":
                work_context.get(
                    "project"
                ),

            "objective":
                work_context.get(
                    "objective"
                ),

            "task":
                work_context.get(
                    "task"
                ),

            "timestamp":
                str(
                    datetime.datetime.now()
                )

        }

        self.session.save_session(
            snapshot
        )

        return snapshot