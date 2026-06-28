from core.task_translator import (
    TaskTranslator
)


translator = TaskTranslator()

assert translator.translate(
    "Create workflow_validator.py"
) == {
    "action_type": "create_file",
    "target": "workflow_validator.py"
}

assert translator.translate(
    "Create `executor_state_machine.ts` with states"
) == {
    "action_type": "create_file",
    "target": "executor_state_machine.ts"
}

assert translator.translate(
    "Create `test/workflow_simulator.ts` with mock context scenarios"
) == {
    "action_type": "create_file",
    "target": "test/workflow_simulator.ts"
}

assert translator.translate(
    "Create folder docs"
) == {
    "action_type": "create_folder",
    "target": "docs"
}

assert translator.translate(
    "Configure VSCode debugging"
) == {
    "action_type": None,
    "target": None
}

assert translator.translate(
    "Open VS Code"
) == {
    "action_type": "open_app",
    "target": "vscode"
}

print(
    "task translator ok"
)
