from core.nova_runtime import NovaRuntime

runtime = NovaRuntime()

result = runtime.process_goal(
    "prepare coding environment"
)

print(result)