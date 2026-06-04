from core.system_state import SystemState
from core.screen_capture import ScreenCapture
from core.ocr_reader import OCRReader
from core.error_handler import NovaErrorHandler


class ContextEngine:
    def __init__(self):
        self.system = None
        self.screen = None
        self.ocr = None

    def understand_current_context(self):
        try:
            if self.system is None:
                self.system = SystemState()

            if self.screen is None:
                self.screen = ScreenCapture()

            if self.ocr is None:
                self.ocr = OCRReader()

            active_window = self.system.get_active_window_title()
            running_apps = self.system.get_running_apps()

            screenshot_result = self.screen.capture_screen()

            if screenshot_result["status"] != "success":
                return screenshot_result

            image_path = screenshot_result["filename"]

            ocr_result = self.ocr.read_text(image_path)

            return {
                    "active_window": active_window,
                    "running_apps": running_apps[:10],
                    "visible_text": ocr_result.get("text", [])
                }

        except Exception as e:
            return NovaErrorHandler.handle(
                e,
                "ContextEngine"
            )