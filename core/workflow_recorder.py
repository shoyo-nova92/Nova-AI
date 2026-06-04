import json
import os


class WorkflowRecorder:

    def __init__(self):
        self.file = "recorded_workflows.json"

        if not os.path.exists(self.file):
            with open(self.file, "w") as f:
                json.dump({}, f)

    def record_workflow(self, workflow_name, steps):
        with open(self.file, "r") as f:
            data = json.load(f)

        data[workflow_name] = steps

        with open(self.file, "w") as f:
            json.dump(data, f, indent=4)

        print(f"Recorded workflow: {workflow_name}")

    def get_workflow(self, workflow_name):
        with open(self.file, "r") as f:
            data = json.load(f)

        return data.get(workflow_name, None)