from core.plugin_sandbox import PluginSandbox

sandbox = PluginSandbox()

plugin_name = input(
    "Plugin to test: "
)

result = sandbox.test_plugin(
    plugin_name
)

print(result)