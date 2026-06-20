import json
from datetime import datetime


class RuntimeTrace:

    def __init__(self):

        self.trace = {}

    def start_trace(
        self,
        goal
    ):

        self.trace = {

            "goal": goal,

            "events": [],

            "timestamp":
                str(datetime.now())

        }

    def log_event(

        self,

        event_type,

        data

    ):

        self.trace["events"].append({

            "event": event_type,

            "data": data

        })

    def save_trace(self):

        try:

            with open(

                "memory/runtime_traces.json",

                "a",

                encoding="utf-8"

            ) as file:

                file.write(

                    json.dumps(
                        self.trace
                    )

                    + "\n"

                )

            return {

                "success": True

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }