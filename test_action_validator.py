from core.action_validator import ActionValidator

validator = ActionValidator()

context = {

    "activity": "coding",

    "active_window":
        "Visual Studio Code"

}

actions = [

    "open terminal",

    "debug code",

    "delete system32"

]

for action in actions:

    result = validator.validate(
        action,
        context
    )

    print("\nACTION:", action)

    print(result)