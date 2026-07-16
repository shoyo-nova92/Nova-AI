from core.task_translator import TaskTranslator

translator = TaskTranslator()

for step in ["Stage changes", "Git add", "Stage parser.py", "Add all changes"]:
    result = translator.translate(step)
    assert result["action"] == "git_add", result
    assert result["type"] == "git", result

print("git translation ok")
