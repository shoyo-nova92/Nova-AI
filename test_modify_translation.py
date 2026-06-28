from core.task_translator import TaskTranslator

translator = TaskTranslator()

tests = [

    "Add execution latency tracking in executor/core.ts",

    "Update config.json",

    "Modify logger.py",

    "Edit memory/retriever.py"

]

for t in tests:

    print()
    print(t)

    print(
        translator.translate(t)
    )