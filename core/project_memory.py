import json
import os


class ProjectMemory:

    def __init__(self):

        self.memory_file = (
            "memory/project_memory.json"
        )

    def add_decision(

        self,

        decision,

        reason,

        outcome=None

    ):

        memories = self.load_memory()

        memories.append({

            "decision": decision,

            "reason": reason,

            "outcome": outcome

        })

        with open(

            self.memory_file,

            "w",

            encoding="utf-8"

        ) as file:

            json.dump(

                memories,

                file,

                indent=4

            )

    def load_memory(self):

        if not os.path.exists(
            self.memory_file
        ):

            return []

        with open(

            self.memory_file,

            "r",

            encoding="utf-8"

        ) as file:

            return json.load(file)