import json
import os
from datetime import datetime


class ExecutionMemory:

    FILE_PATH = (
        "memory/execution_history.json"
    )

    def __init__(self):

        base_dir = os.path.dirname(os.path.abspath(__file__))
        base_parent = os.path.dirname(base_dir)
        self.FILE_PATH = os.path.join(base_parent, "memory", "execution_history.json")

        os.makedirs(
            os.path.dirname(self.FILE_PATH),
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

        now = datetime.now()
        data.append({

            "day":
                now.strftime("%A"),

            "date":
                now.strftime("%Y-%m-%d"),

            "time":
                now.strftime("%H:%M:%S"),

            "timestamp":
                str(now),

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