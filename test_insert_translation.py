from core.task_translator import TaskTranslator

translator = TaskTranslator()

result = translator.translate("Insert import logging at line 3")

assert result["action"] == "insert_at_line", result
assert result["target"] == "parser.py", result
assert result["parameters"]["line"] == 3, result
assert result["parameters"]["content"] == "import logging", result

print("insert translation ok")
