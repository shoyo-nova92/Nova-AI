from core.task_translator import TaskTranslator

translator = TaskTranslator()

result = translator.translate("Run hello.py")

assert result["action"] == "run_python", result
assert result["target"] == "hello.py", result

print("run translation ok")