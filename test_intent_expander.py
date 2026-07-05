from core.intent_expander import IntentExpander


expander = IntentExpander()

tests = [

    {
        "raw": "Implement parser",
        "action": None,
        "target": None
    },

    {
        "raw": "Implement validator",
        "action": None,
        "target": None
    },

    {
        "raw": "Implement router",
        "action": None,
        "target": None
    },

    {
        "raw": "Implement planner",
        "action": None,
        "target": None
    },

    {
        "raw": "Open VS Code",
        "type": "application",
        "action": "open_app",
        "target": "vscode"
    }

]

for test in tests:

    print()

    print("INPUT:")
    print(test)

    print()

    print("OUTPUT:")
    print(
        expander.expand(test)
    )
