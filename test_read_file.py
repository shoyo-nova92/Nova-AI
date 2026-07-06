from core.filesystem_handler import (
    FilesystemHandler
)


handler = FilesystemHandler()

result = handler.read_file(
    "core/task_translator.py"
)

print(result["success"])
print(result["lines"])
print(result["content"][:300])
