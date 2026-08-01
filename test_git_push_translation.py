from core.task_translator import TaskTranslator

translator = TaskTranslator()

for step in ["Push changes", "Publish repository", "Git push", "Upload commits"]:
    result = translator.translate(step)
    assert result["action"] == "git_push", result
    assert result["type"] == "git", result

print("git push translation ok")
