from core.desktop_observer import (
    DesktopObserver
)

observer = DesktopObserver()

print(
    "\n=== ACTIVE WINDOW ===\n"
)

print(
    observer.get_active_window()
)

print(
    "\n=== RUNNING APPS ===\n"
)

apps = observer.get_running_apps()

print(
    apps["apps"][:20]
)