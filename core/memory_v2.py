import json
import os


class MemoryV2:

    def __init__(self):
        self.memory_file = "memory.json"

        if not os.path.exists(self.memory_file):
            with open(self.memory_file, "w") as f:
                json.dump({}, f)

    def save_workflow(self, workflow_name, steps):
        with open(self.memory_file, "r") as f:
            data = json.load(f)

        data[workflow_name] = steps

        with open(self.memory_file, "w") as f:
            json.dump(data, f, indent=4)

        print(f"Workflow '{workflow_name}' saved.")

    def load_workflow(self, workflow_name):
        with open(self.memory_file, "r") as f:
            data = json.load(f)

        return data.get(workflow_name, None)