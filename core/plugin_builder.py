import os


class PluginBuilder:

    def build_plugin(self, plugin_name):
        plugin_code = f'''
class {plugin_name.capitalize()}Plugin:

    name = "{plugin_name}"

    def run(self):
        print("{plugin_name} plugin executed successfully.")
'''

        filepath = f"plugins/{plugin_name}.py"

        with open(filepath, "w") as f:
            f.write(plugin_code)

        print(f"Plugin '{plugin_name}' created successfully.")

        return filepath