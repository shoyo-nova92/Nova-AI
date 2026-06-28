from core.task_translator import TaskTranslator

translator = TaskTranslator()

tests = [
    "Open VS Code",
    "Open terminal",
    "git status",
    "Create workflow_validator.py",
    "Create folder docs",
]

for t in tests:
    print(t)
    print(translator.translate(t))
    print()