import importlib


class PluginLoader:

    def load_plugin(self, plugin_name):
        try:
            module = importlib.import_module(
                f"plugins.{plugin_name}"
            )

            for attr in dir(module):
                obj = getattr(module, attr)

                if isinstance(obj, type):
                    return obj()

            return None

        except Exception as e:
            print(f"Plugin load error: {e}")
            return None