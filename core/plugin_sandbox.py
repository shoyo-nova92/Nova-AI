import importlib
import traceback


class PluginSandbox:

    def test_plugin(self, plugin_name):
        try:
            module = importlib.import_module(
                f"plugins.{plugin_name}"
            )

            plugin_class = None

            for attr in dir(module):
                obj = getattr(module, attr)

                if isinstance(obj, type):
                    plugin_class = obj
                    break

            if not plugin_class:
                return {
                    "status": "failed",
                    "reason": "No plugin class found"
                }

            plugin = plugin_class()

            if hasattr(plugin, "run"):
                print(
                    f"Testing {plugin_name}..."
                )

                plugin.run()

            return {
                "status": "success",
                "plugin": plugin_name
            }

        except Exception as e:
            return {
                "status": "failed",
                "plugin": plugin_name,
                "error": str(e),
                "trace": traceback.format_exc()
            }