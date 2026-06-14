from core.filesystem_handler import (
    FilesystemHandler
)

handler = (
    FilesystemHandler()
)

result = handler.create_folder(
    "Nova_Test"
)

print(
    "\n=== FILESYSTEM RESULT ===\n"
)

print(result)