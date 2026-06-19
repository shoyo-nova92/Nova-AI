import pygetwindow as gw
import psutil


class DesktopObserver:

    def get_active_window(self):

        try:

            window = gw.getActiveWindow()

            if window:

                return {

                    "success": True,

                    "window_title":
                        window.title

                }

            return {

                "success": False,

                "reason":
                    "no active window"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def get_running_apps(self):

        apps = []

        try:

            for proc in psutil.process_iter(

                ["name"]

            ):

                try:

                    name = proc.info["name"]

                    if name:

                        apps.append(name)

                except:
                    pass

            return {

                "success": True,

                "apps":
                    sorted(
                        list(
                            set(apps)
                        )
                    )

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }