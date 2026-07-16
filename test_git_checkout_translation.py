from core.task_translator import TaskTranslator

translator = TaskTranslator()

for step in ["Switch to main", "Checkout dev", "Create branch feature-ai", "Change branch"]:
    result = translator.translate(step)
    assert result["action"] == "git_checkout", result
    assert result["type"] == "git", result

print("git checkout translation ok")
