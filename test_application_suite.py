from core.application_handler import (
    ApplicationHandler
)

handler = ApplicationHandler()

print("\n=== APPLICATION SUITE ===\n")

import time

print(
    handler.focus_app(
        "Visual Studio Code"
    )
)

time.sleep(3)

print(
    handler.minimize_app(
        "Visual Studio Code"
    )
)

time.sleep(3)

print(
    handler.maximize_app(
        "Visual Studio Code"
    )
)