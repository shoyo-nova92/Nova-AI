import json
import os


class SessionContinuity:

    def __init__(self):

        self.session_file = (
            "memory/session_context.json"
        )

    def save_session(

        self,

        work_context

    ):

        with open(

            self.session_file,

            "w",

            encoding="utf-8"

        ) as file:

            json.dump(

                work_context,

                file,

                indent=4

            )

    def load_session(self):

        if not os.path.exists(
            self.session_file
        ):

            return {}

        with open(

            self.session_file,

            "r",

            encoding="utf-8"

        ) as file:

            return json.load(file)