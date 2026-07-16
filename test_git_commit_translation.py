from core.task_translator import TaskTranslator

translator = TaskTranslator()

for step in ["Commit changes", 'Commit with message "feat: parser improvements"', "Create commit"]:
    result = translator.translate(step)
    assert result["action"] == "git_commit", result
    assert result["type"] == "git", result

print("git commit translation ok")
