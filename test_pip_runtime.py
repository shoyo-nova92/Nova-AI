from core.nova_runtime import NovaRuntime

runtime = NovaRuntime()
result = runtime.router.route({
    "type": "terminal",
    "action": "pip_install",
    "action_type": "pip_install",
    "target": "requests",
})

assert result["state"] == "complete" or result["state"] == "failed", result
assert result["execution"]["action"] == "pip_install", result

print("pip runtime ok")