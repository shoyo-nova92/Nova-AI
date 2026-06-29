from core.task_translator import TaskTranslator

translator = TaskTranslator()

tests = [
    "Open VS Code",
    "Create workflow_validator.py",
    "Run git status",
    "Implement parser"
]

for t in tests:
    print("\nSTEP:", t)
    print(translator.translate(t))