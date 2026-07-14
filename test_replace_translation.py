from core.task_translator import TaskTranslator

translator = TaskTranslator()

result = translator.translate("Replace print with logger.info")

assert result["action"] == "replace_text", result
assert result["target"] == "parser.py", result
assert result["parameters"]["old"] == "print", result
assert result["parameters"]["new"] == "logger.info", result

print("replace translation ok")
