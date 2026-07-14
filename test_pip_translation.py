from core.task_translator import TaskTranslator

translator = TaskTranslator()

for step in ["Install requests", "Install numpy", "pip install pandas", "Install package pyyaml"]:
    result = translator.translate(step)
    assert result["action"] == "pip_install", result
    assert result["type"] == "terminal", result

print("pip translation ok")