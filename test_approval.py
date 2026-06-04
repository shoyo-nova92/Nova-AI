from core.plugin_approval import PluginApproval

approval = PluginApproval()

plugin_name = input(
    "Plugin Name: "
)

result = approval.request_approval(
    plugin_name
)

print(result)