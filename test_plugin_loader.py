from core.plugin_loader import PluginLoader

loader = PluginLoader()

plugin = loader.load_plugin("pdf_renamer")

print(plugin)