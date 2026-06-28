from core.nova_runtime import NovaRuntime

runtime = NovaRuntime()

result = runtime.process_goal(
    "Create workflow_validator.py"
)

print(result)