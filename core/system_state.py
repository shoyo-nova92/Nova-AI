import pygetwindow as gw
import psutil

class SystemState:

    def get_active_window_title(self):
        try:
            window = gw.getActiveWindow()

            if window:
                return {
                    "title": window.title
                }
            
            return {
                "title": "No active window"
            }
        except Exception as e:
            return {
                "title": f"Error: {str(e)}"
            }
    
    def get_running_apps(self):
        apps = set()

        useful_keywords = [
            "chrome",
            "code",
            "photoshop",
            "spotify",
            "notion",
            "discord",
            "excel",
            "word",
            "powerpoint",
            "edge",
            "notepad",
            "figma",
            "obs",
            "steam",
            "calculator",
            "python"
        ]

        for process in psutil.process_iter(["name"]):
            try:
                name = process.info.get("name")

                if not name:
                    continue

                lower_name = name.lower()

                if any(
                    keyword in lower_name
                    for keyword in useful_keywords
                ):
                    apps.add(name)

            except (
                psutil.NoSuchProcess,
                psutil.AccessDenied,
                psutil.ZombieProcess
            ):
                continue

        return sorted(list(apps))