from core.desktop_capture import (
    DesktopCapture
)

capture = (
    DesktopCapture()
)

print(
    "\n=== SCREENSHOT TEST ===\n"
)

print(
    capture.capture_screen()
)