from core.task_translator import TaskTranslator

translator = TaskTranslator()

plain_result = translator.translate("Run pytest")
assert plain_result["action"] == "run_pytest", plain_result
assert plain_result["target"] is None, plain_result

file_result = translator.translate("Run test_task_translator.py")
assert file_result["action"] == "run_pytest", file_result
assert file_result["target"] == "test_task_translator.py", file_result

print("pytest translation ok")
