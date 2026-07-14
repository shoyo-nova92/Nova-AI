from core.task_translator import TaskTranslator

translator = TaskTranslator()

result = translator.translate("Append hello to notes.txt")

assert result["action"] == "append_file", result
assert result["target"] == "notes.txt", result
assert result["parameters"]["content"] == "hello", result

print("append translation ok")
