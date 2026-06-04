from core.action_validator import ActionValidator
from core.adaptive_executor import AdaptiveExecutor

validator = ActionValidator()

executor = AdaptiveExecutor()

context = {

    "activity": "coding",

    "active_window":
        "Visual Studio Code"

}

result = executor.execute_action(

    "open terminal",

    validator,

    context

)

print("\n=== EXECUTION RESULT ===\n")

print(result)