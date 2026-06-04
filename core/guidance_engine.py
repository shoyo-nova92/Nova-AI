class GuidanceEngine:

    def guide(self, app_name, user_goal):
        app_name = app_name.lower()
        user_goal = user_goal.lower()

        if "photoshop" in app_name:
            if "crop" in user_goal:
                return (
                    "Crop tool is usually "
                    "on the left toolbar."
                )

            elif "export" in user_goal:
                return (
                    "Go to File → Export → "
                    "Export As."
                )

        elif "chrome" in app_name:
            if "download" in user_goal:
                return (
                    "Use Ctrl+D or right click "
                    "the image and save it."
                )

        return (
            "I understand your context "
            "but need better guidance logic."
        )