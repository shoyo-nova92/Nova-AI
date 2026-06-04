import json
import os
from datetime import datetime

class SemanticMemoryEngine:

    def __init__(self):

        self.memory_file = (
            "semantic_memory.json"
        )

        # Create file if missing
        if not os.path.exists(
            self.memory_file
        ):

            with open(
                self.memory_file,
                "w"
            ) as f:

                json.dump([], f)

    def save_memory(

        self,

        project,
        task,
        status,
        tags

    ):

        memory = {

            "timestamp":
                str(datetime.now()),

            "project":
                project,

            "task":
                task,

            "status":
                status,

            "tags":
                tags

        }

        data = self.load_memories()

        data.append(memory)

        with open(
            self.memory_file,
            "w"
        ) as f:

            json.dump(
                data,
                f,
                indent=4
            )

        return memory

    def load_memories(self):

        with open(
            self.memory_file,
            "r"
        ) as f:

            return json.load(f)

    def search_memory(

        self,
        keyword

    ):

        keyword = keyword.lower()

        memories = (
            self.load_memories()
        )

        results = []

        for memory in memories:

            searchable = str(
                memory
            ).lower()

            if keyword in searchable:

                results.append(memory)

        return results