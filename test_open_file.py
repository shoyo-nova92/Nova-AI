from core.filesystem_handler import (
    FilesystemHandler
)

handler = FilesystemHandler()

result = handler.open_file(

    r"C:\Users\shour\OneDrive\Desktop\Nova\Novav0.6\requirements.txt"

)

print()

print("=== OPEN FILE TEST ===")

print()

print(result)