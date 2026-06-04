import pyautogui
from datetime import datetime
from core.error_handler import NovaErrorHandler

class ScreenCapture:

    def capture_screen(self):
        try:

            screenshot = pyautogui.screenshot()

            filename = (
                f"screenshot_"
                f"{datetime.now().strftime('%Y%m%d_%H%M%S')}.png"
            )

            screenshot.save(filename)

            return {
                "status": "success",
                "filename": filename
            }

        except Exception as e:
            return NovaErrorHandler.handle(
                e,
                "ScreenCapture"
            )