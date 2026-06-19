from core.environment_model import (
    EnvironmentModel
)

from core.desktop_observer import (
    DesktopObserver
)


class EnvironmentUpdater:

    def __init__(self):

        self.model = (
            EnvironmentModel()
        )

        self.observer = (
            DesktopObserver()
        )

    def refresh(self):

        active = (
            self.observer.get_active_window()
        )

        if active["success"]:

            self.model.update(

                window=
                    active[
                        "window_title"
                    ]
            )

        return (
            self.model.get_state()
        )