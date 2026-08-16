import os
import subprocess
import psutil
import pygetwindow as gw
import time

from core.app_search_engine import AppSearchEngine


class ApplicationHandler:

    def __init__(self, app_search_engine=None):
        self.app_search_engine = app_search_engine or AppSearchEngine(auto_index=True)

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

    def _focus_if_running(self, focus_target):
        """If an application is already running, focus its window instead of launching a second instance."""
        if not focus_target:
            return None
        try:
            windows = gw.getAllTitles()
            for title in windows:
                if not title:
                    continue
                if focus_target.lower() in title.lower():
                    try:
                        window_list = gw.getWindowsWithTitle(title)
                        if window_list:
                            window = window_list[0]
                            window.activate()
                            return {"success": True, "focused": True, "window": title}
                    except Exception:
                        continue
            return None
        except Exception:
            return None

    def _launch_hardcoded_fallback(self, app_name: str) -> dict:
        """Original well-known hardcoded executable fallback paths (used only if AppSearchEngine fails)."""
        normalized = app_name.strip().lower()

        # VS Code / Visual Studio Code
        if normalized in {"vscode", "vs code", "visual studio code"}:
            focus_result = self._focus_if_running("Visual Studio Code")
            if focus_result:
                return {
                    "success": True,
                    "action": "open_app",
                    "target": "vscode",
                    "focused": True,
                    "window": focus_result.get("window"),
                    "fallback": True,
                }
            vscode_candidates = [
                os.path.join(os.environ.get("LOCALAPPDATA", ""), "Programs", "Microsoft VS Code", "Code.exe"),
                os.path.join(os.environ.get("PROGRAMFILES", ""), "Microsoft VS Code", "Code.exe"),
                os.path.join(os.environ.get("PROGRAMFILES(X86)", ""), "Microsoft VS Code", "Code.exe"),
            ]
            for candidate in vscode_candidates:
                if candidate and os.path.exists(candidate):
                    subprocess.Popen([candidate])
                    return {"success": True, "target": candidate, "fallback": True}

        # Google Chrome
        if normalized in {"chrome", "google chrome", "googlechrome"}:
            chrome_candidates = [
                os.path.join(os.environ.get("PROGRAMFILES", ""), "Google", "Chrome", "Application", "chrome.exe"),
                os.path.join(os.environ.get("PROGRAMFILES(X86)", ""), "Google", "Chrome", "Application", "chrome.exe"),
                os.path.join(os.environ.get("LOCALAPPDATA", ""), "Google", "Chrome", "Application", "chrome.exe"),
            ]
            for candidate in chrome_candidates:
                if candidate and os.path.exists(candidate):
                    subprocess.Popen([candidate])
                    return {"success": True, "target": candidate, "fallback": True}

        # Microsoft Edge
        if normalized in {"edge", "microsoft edge", "msedge"}:
            edge_candidates = [
                os.path.join(os.environ.get("PROGRAMFILES(X86)", ""), "Microsoft", "Edge", "Application", "msedge.exe"),
                os.path.join(os.environ.get("PROGRAMFILES", ""), "Microsoft", "Edge", "Application", "msedge.exe"),
            ]
            for candidate in edge_candidates:
                if candidate and os.path.exists(candidate):
                    subprocess.Popen([candidate])
                    return {"success": True, "target": candidate, "fallback": True}

        # Notepad
        if normalized in {"notepad"}:
            try:
                os.startfile("notepad.exe")
                return {"success": True, "target": "notepad.exe", "fallback": True}
            except Exception as e:
                return {"success": False, "reason": str(e), "fallback": True}

        # Calculator
        if normalized in {"calculator", "calc"}:
            try:
                subprocess.Popen("calc.exe")
                return {"success": True, "target": "calc.exe", "fallback": True}
            except Exception as e:
                return {"success": False, "reason": str(e), "fallback": True}

        return {"success": False, "reason": f"fallback not found for app '{app_name}'"}

    def open_app(self, app_name):
        """AppSearchEngine primary. If lookup fails, fall back to original hardcoded behavior."""
        try:
            search_result = self.app_search_engine.launch_app(app_name)
            if search_result.get("success"):
                return search_result

            # AppSearchEngine did not find the app; fall back to original behavior
            fallback = self._launch_hardcoded_fallback(app_name)
            if fallback.get("success"):
                return fallback
            return {
                "success": False,
                "reason": search_result.get("reason") or fallback.get("reason") or "app not found",
                "app_search_result": search_result,
                "fallback_result": fallback,
            }

        except Exception as e:
            return {
                "success": False,
                "reason": str(e)
            }

    def close_app(self, app_name):
        """Use dynamic AppSearchEngine.close_app() for close."""
        try:
            result = self.app_search_engine.close_app(app_name)
            if result.get("success"):
                return result
            return result

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
