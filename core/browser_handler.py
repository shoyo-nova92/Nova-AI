import webbrowser
import pyautogui
from core.application_handler import ApplicationHandler

class BrowserHandler:

    def __init__(self):
            self.apps = (
            ApplicationHandler()
        )

    def focus_browser(self):

        if self.apps.focus_app(
            "Chrome"
        )["success"]:

            return True

        if self.apps.focus_app(
            "Edge"
        )["success"]:

            return True

        return False

    def open_website(

        self,

        url

    ):

        try:

            webbrowser.open(
                url
            )

            return {

                "success": True,

                "action":
                    f"open {url}"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def search_query(

        self,

        query

    ):

        try:

            url = (

                "https://www.google.com/search?q="
                + query.replace(
                    " ",
                    "+"
                )

            )

            webbrowser.open(
                url
            )

            return {

                "success": True,

                "action":
                    f"search {query}"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def open_youtube(self):

        return self.open_website(
            "https://youtube.com"
        )
    
    def open_new_tab(self):

        try:

            if not self.focus_browser():

                return {

                    "success": False,

                    "reason":
                        "no browser window found"

                }

            pyautogui.hotkey(

                "ctrl",

                "t"

            )

            return {

                "success": True,

                "action":
                    "open new tab"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def close_tab(self):

        try:

            if not self.focus_browser():

                return {

                    "success": False,

                    "reason":
                        "no browser window found"

                }

            pyautogui.hotkey(

                "ctrl",

                "w"

            )

            return {

                "success": True,

                "action":
                    "close tab"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def next_tab(self):

        try:

            if not self.focus_browser():

                return {

                    "success": False,

                    "reason":
                        "no browser window found"

                }

            pyautogui.hotkey(

                "ctrl",

                "tab"

            )

            return {

                "success": True,

                "action":
                    "next tab"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def previous_tab(self):

        try:

            if not self.focus_browser():

                return {

                    "success": False,

                    "reason":
                        "no browser window found"

                }

            pyautogui.hotkey(

                "ctrl",

                "shift",

                "tab"

            )

            return {

                "success": True,

                "action":
                    "previous tab"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }