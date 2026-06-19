from core.environment_updater import (
    EnvironmentUpdater
)

updater = (
    EnvironmentUpdater()
)

print(
    "\n=== ENVIRONMENT UPDATE ===\n"
)

print(
    updater.refresh()
)