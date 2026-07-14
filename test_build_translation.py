from core.task_translator import TaskTranslator

translator = TaskTranslator()

for step in ["Build project", "Compile project", "Run build", "Build frontend"]:
    result = translator.translate(step)
    assert result["action"] == "build_project", result
    assert result["type"] == "terminal", result

print("build translation ok")
