from core.execution_policy import ExecutionPolicy

policy = ExecutionPolicy()

tests = [

    {
        "type": "filesystem",
        "action": "create_file"
    },

    {
        "type": "system",
        "action": "shutdown"
    },

    {
        "type": "filesystem",
        "action": "delete_file"
    }

]

for t in tests:

    print("\nACTION:")
    print(t)

    print(
        policy.classify(t)
    )