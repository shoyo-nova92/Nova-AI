import json
import os
from datetime import datetime


class ExecutionMemory:

    FILE_PATH = (
        "memory/execution_history.json"
    )

    def __init__(self):

        os.makedirs(
            "memory",
            exist_ok=True
        )

        if not os.path.exists(
            self.FILE_PATH
        ):

            with open(
                self.FILE_PATH,
                "w"
            ) as f:

                json.dump(
                    [],
                    f,
                    indent=4
                )

    def record(

        self,

        action,

        success,

        duration,

        failure_reason=None

    ):

        with open(
            self.FILE_PATH,
            "r"
        ) as f:

            data = json.load(f)

        data.append({

            "timestamp":
                str(datetime.now()),

            "action":
                action,

            "success":
                success,

            "duration":
                duration,

            "failure_reason":
                failure_reason

        })

        with open(
            self.FILE_PATH,
            "w"
        ) as f:

            json.dump(
                data,
                f,
                indent=4
            )

    def get_stats(

        self,

        action

    ):

        with open(
            self.FILE_PATH,
            "r"
        ) as f:

            data = json.load(f)

        records = [

            x for x in data

            if x["action"] == action

        ]

        if not records:

            return {

                "action":
                    action,

                "executions":
                    0,

                "success_rate":
                    0

            }

        successes = sum(

            1

            for x in records

            if x["success"]

        )

        return {

            "action":
                action,

            "executions":
                len(records),

            "success_rate":
                round(
                    successes
                    / len(records)
                    * 100,
                    2
                )

        }