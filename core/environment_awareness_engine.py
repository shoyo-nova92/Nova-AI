from core.desktop_capture import DesktopCapture
from core.ocr_reader import OCRReader

from core.ui_semantics_engine import (
    UISemanticsEngine
)

from core.desktop_understanding_engine import (
    DesktopUnderstandingEngine
)

from core.action_selector import (
    ActionSelector
)

from core.environment_model import (
    EnvironmentModel
)


class EnvironmentAwarenessEngine:

    def __init__(self):

        self.capture = (
            DesktopCapture()
        )

        self.ocr = (
            OCRReader()
        )

        self.ui_semantics = (
            UISemanticsEngine()
        )

        self.understanding = (
            DesktopUnderstandingEngine()
        )

        self.selector = (
            ActionSelector()
        )

        self.environment = (
            EnvironmentModel()
        )

    def analyze(self):

        screenshot = (
            self.capture.capture_screen()
        )

        image_path = (
            screenshot["filepath"]
        )

        ocr_result = (
            self.ocr.read_text_with_positions(
                image_path
            )
        )

        visible_text = (
            ocr_result.get(
                "elements",
                []
            )
        )

        semantic_result = (
            self.ui_semantics.analyze(
                visible_text
            )
        )

        context_data = {

        "active_window":
            "Visual Studio Code"

        }

        understanding = (

            self.understanding.understand(

                context_data,

                semantic_result

            )

        )

        action = (
            self.selector.choose_action(
                understanding
            )
        )

        self.environment.update(

            app=
                understanding.get(
                    "application"
                ),

            screen=
                understanding.get(
                    "screen_state"
                )
        )

        return {

            "understanding":
                understanding,

            "recommended_action":
                action,

            "environment":
                self.environment.get_state()

        }