from core.nova_runtime import (
    NovaRuntime
)

runtime = NovaRuntime()

result = runtime.process_goal(

    "Read core/task_translator.py"

)

print("\nRAW PLAN")
print(result["raw_plan"])

print("\nNORMALIZED")
print(result["normalized_plan"])

print("\nVALIDATED")
print(result["validated_plan"])

print("\nREPAIRED")
print(result["repaired_plan"])

print("\nEXECUTIONS")
print(result["executions"])