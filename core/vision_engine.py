from core.screen_capture import ScreenCapture
from core.ocr_reader import OCRReader
from core.system_state import SystemState

class VisionEngine:

    def __init__(self):

        self.screen = ScreenCapture()
        self.ocr = OCRReader()
        self.system = SystemState()

    def analyze_screen(self):

        # Capture Screenshot
        screenshot_result = self.screen.capture_screen()

        # System Context
        active_window = self.system.get_active_window_title()

        running_apps = self.system.get_running_apps()

        if (
            screenshot_result.get("status")
            !=
            "success"
        ):

            return {

                "active_window":
                    active_window,

                "running_apps":
                    running_apps,

                "visible_text":
                    screenshot_result

            }

        image_path = screenshot_result["filename"]

        # OCR
        ocr_result = self.ocr.read_text(image_path)

        return {

            "active_window": active_window,

            "running_apps": running_apps,

            "visible_text": ocr_result

        }
