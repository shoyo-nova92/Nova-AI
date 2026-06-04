from core.plugin_loader import PluginLoader


class CapabilityRouter:

    def __init__(self):
        self.loader = PluginLoader()

    def route(self, command):
        command = command.lower()

        if "rename pdf" in command or "rename my pdf" in command:
            plugin = self.loader.load_plugin("pdf_renamer")
            return plugin

        return None