from core.task_translator import TaskTranslator

translator = TaskTranslator()

result = translator.translate("Rollback parser.py")

assert result["action"] == "rollback_file", result
assert result["target"] == "parser.py", result

print("rollback translation ok")
