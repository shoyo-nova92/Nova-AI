from core.task_translator import TaskTranslator

translator = TaskTranslator()

tests = [

    "Add execution latency tracking in executor/core.ts",

    "Add schema validation to context_fusion/input.ts",

    "Update workflow_validator.py",

    "Modify executor/core.ts",

    "Edit memory/retriever.py"

]

for t in tests:

    print("\n", t)
    print(translator.translate(t))
    print(file_match)