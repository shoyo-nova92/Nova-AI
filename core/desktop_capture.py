from PIL import ImageGrab
from pathlib import Path


class DesktopCapture:

    def capture_screen(self):

        try:

            Path(
                "memory/screenshots"
            ).mkdir(

                parents=True,
                exist_ok=True

            )

            filepath = (

                "memory/screenshots/"
                "latest_screen.png"

            )

            screenshot = (
                ImageGrab.grab()
            )

            screenshot.save(
                filepath
            )

            return {

                "success": True,

                "filepath":
                    filepath

            }

        except Exception as e:

            return {

                "success": False,

                "reason":
                    str(e)

            }