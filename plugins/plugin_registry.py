import json
import os


class PluginRegistry:

    def __init__(self):
        self.file = "plugins_registry.json"

        if not os.path.exists(self.file):
            with open(self.file, "w") as f:
                json.dump({}, f)

    def register_plugin(self, plugin_name):
        with open(self.file, "r") as f:
            data = json.load(f)

        data[plugin_name] = {
            "status": "installed"
        }

        with open(self.file, "w") as f:
            json.dump(data, f, indent=4)

        print(f"{plugin_name} registered.")