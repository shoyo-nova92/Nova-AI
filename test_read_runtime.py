from core.nova_runtime import NovaRuntime


runtime = NovaRuntime()

result = runtime.process_goal(
    "Read core/task_translator.py"
)


print("\nRUNTIME RESULT")
print(result)


# --------------------------------------------------
# 1. Runtime must return a result
# --------------------------------------------------

assert isinstance(result, dict), (
    f"Runtime returned invalid result: {result}"
)


# --------------------------------------------------
# 2. Runtime must contain executions
# --------------------------------------------------

executions = result.get(
    "executions",
    []
)

assert executions, (
    f"Runtime produced no executions: {result}"
)


# --------------------------------------------------
# 3. Find the read_file execution
# --------------------------------------------------

read_record = None

for record in executions:

    action = record.get(
        "action",
        {}
    )

    if action.get("action") == "read_file":

        read_record = record
        break


assert read_record is not None, (
    f"Runtime never executed read_file. "
    f"Executions: {executions}"
)


# --------------------------------------------------
# 4. Validate read_file target
# --------------------------------------------------

action = read_record.get(
    "action",
    {}
)

assert action.get(
    "target"
) == "core/task_translator.py", (
    f"Wrong read target: {action}"
)


# --------------------------------------------------
# 5. Validate runtime action result
# --------------------------------------------------

action_result = read_record.get(
    "result",
    {}
)

assert action_result.get(
    "state"
) == "completed", (
    f"read_file did not complete: {action_result}"
)


# --------------------------------------------------
# 6. Validate execution
# --------------------------------------------------

execution = action_result.get(
    "execution",
    {}
)

assert execution.get(
    "success"
) is True, (
    f"read_file execution failed: {execution}"
)


# --------------------------------------------------
# 7. Validate verification
# --------------------------------------------------

verification = action_result.get(
    "verification",
    {}
)

assert verification.get(
    "success"
) is True, (
    f"read_file verification failed: {verification}"
)


print("\nread runtime ok")  