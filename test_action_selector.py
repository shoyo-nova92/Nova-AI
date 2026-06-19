from core.action_selector import (
    ActionSelector
)

selector = ActionSelector()

result = selector.choose_action(

    {
        "application":"VS Code",

        "activity":"coding",

        "available_actions":[
            "save_document"
        ]
    }

)

print(result)