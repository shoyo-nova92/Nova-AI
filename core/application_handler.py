import subprocess
import psutil
import pygetwindow as gw
import os
import time


class ApplicationHandler:

    def is_running(self, process_keyword):

        try:

            for proc in psutil.process_iter(["name"]):

                try:

                    if process_keyword.lower() in proc.info["name"].lower():

                        return True

                except:

                    continue

            return False

        except:

            return False

    def open_app(self, app_name):

        try:

            app_name = app_name.lower()

            if app_name == "notepad":

                subprocess.Popen("notepad.exe")

            elif app_name == "calculator":

                subprocess.Popen("calc.exe")

            elif app_name == "vscode":

                if self.is_running("Code"):

                    focus_result = self.focus_app("Visual Studio Code")

                    if focus_result["success"]:

                        return {

                            "success": True,

                            "action": "focus vscode"

                        }

                vscode_paths = [

                    r"C:\Users\shour\AppData\Local\Programs\Microsoft VS Code\Code.exe",

                    r"C:\Program Files\Microsoft VS Code\Code.exe"

                ]

                for path in vscode_paths:

                    if os.path.exists(path):

                        subprocess.Popen(path)

                        return {

                            "success": True,

                            "action": "open vscode"

                        }

                return {

                    "success": False,

                    "reason": "VSCode executable not found"

                }

            else:

                return {

                    "success": False,

                    "reason":
                        f"Unknown app: {app_name}"

                }

            return {

                "success": True,

                "action":
                    f"open {app_name}"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def close_app(self, process_name):

        try:

            for proc in psutil.process_iter():

                if process_name.lower() in proc.name().lower():

                    proc.kill()

                    return {

                        "success": True,

                        "action":
                            f"close {process_name}"

                    }

            return {

                "success": False,

                "reason":
                    "process not found"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def focus_app(self, title_keyword):

        try:

            windows = gw.getAllTitles()

            for title in windows:

                if title_keyword.lower() in title.lower():

                    window = gw.getWindowsWithTitle(
                        title
                    )[0]

                    window.activate()

                    return {

                        "success": True,

                        "window": title

                    }

            return {

                "success": False,

                "reason":
                    "window not found"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def minimize_app(self, title_keyword):

        try:

            windows = gw.getWindowsWithTitle(
                title_keyword
            )

            if windows:

                windows[0].minimize()

                return {

                    "success": True,

                    "action":
                        f"minimize {title_keyword}"

                }

            return {

                "success": False,

                "reason":
                    "window not found"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }


    def maximize_app(self, title_keyword):

        try:

            windows = gw.getWindowsWithTitle(
                title_keyword
            )

            if windows:

                windows[0].maximize()

                return {

                    "success": True,

                    "action":
                        f"maximize {title_keyword}"

                }

            return {

                "success": False,

                "reason":
                    "window not found"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }


    def restart_app(
        self,
        process_name,
        app_name
    ):

        close_result = self.close_app(
            process_name
        )

        if not close_result["success"]:

            return close_result

        timeout = 5

        while timeout > 0:

            if not self.is_running(
                process_name
            ):
                break

            time.sleep(1)

            timeout -= 1

        return self.open_app(
            app_name
        )