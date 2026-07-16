from core.task_translator import TaskTranslator

translator = TaskTranslator()

for step in ["Pull latest", "Update repository", "Sync repository", "Git pull"]:
    result = translator.translate(step)
    assert result["action"] == "git_pull", result
    assert result["type"] == "git", result

print("git pull translation ok")
