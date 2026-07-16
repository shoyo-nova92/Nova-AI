from core.git_handler import GitHandler

handler = GitHandler()
result = handler.git_add('.')

assert result["action"] == "git_add", result
assert "exit_code" in result, result
print("git add ok")
