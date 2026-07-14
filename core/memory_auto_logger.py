import json
import os
from datetime import datetime
from pathlib import Path


class MemoryAutoLogger:

    def __init__(self):

        base_dir = os.path.dirname(os.path.abspath(__file__))
        base_parent = os.path.dirname(base_dir)
        self.memory_file = Path(os.path.join(base_parent, "memory", "runtime_history.json"))

        self.memory_file.parent.mkdir(
            exist_ok=True
        )

        if not self.memory_file.exists():

            with open(
                self.memory_file,
                "w"
            ) as f:

                json.dump([], f)

    def log_event(

        self,

        event_type,

        goal,

        details

    ):

        with open(

            self.memory_file,

            "r"

        ) as f:

            history = json.load(f)

        event = {

            "event_id":
                len(history) + 1,

            "timestamp":
                str(datetime.now()),

            "event_type":
                event_type,

            "goal":
                goal,

            "details":
                details

        }

        history.append(event)

        with open(

            self.memory_file,

            "w"

        ) as f:

            json.dump(

                history,

                f,

                indent=4

            )

        return event