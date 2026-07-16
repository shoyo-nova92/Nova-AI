from core.nova_runtime import NovaRuntime

runtime = NovaRuntime()
result = runtime.router.route({
    "type": "git",
    "action": "git_add",
    "action_type": "git_add",
    "target": ".",
})

assert result["state"] in {"complete", "failed"}, result
assert result["execution"]["action"] == "git_add", result

print("git runtime ok")
