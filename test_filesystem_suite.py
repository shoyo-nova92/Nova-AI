from core.filesystem_handler import (
    FilesystemHandler
)

handler = FilesystemHandler()

print("\n=== FILESYSTEM SUITE ===\n")

import time

print(
    handler.create_folder(
        "Nova_Test"
    )
)

time.sleep(3)

print(
    handler.open_folder(
        "Nova_Test"
    )
)

time.sleep(3)

print(
    handler.rename_folder(
        "Nova_Test",
        "Nova_Test_Renamed"
    )
)