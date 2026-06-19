class DesktopUnderstandingEngine:

    def understand(
        self,
        context_data,
        semantic_elements
    ):

        understanding = {

            "application": None,
            "activity": None,
            "screen_state": None,
            "available_actions": []

        }

        active_window = (
            context_data.get(
                "active_window",
                ""
            )
        ).lower()

        if "visual studio code" in active_window:

            understanding["application"] = (
                "VS Code"
            )

            understanding["activity"] = (
                "coding"
            )

            understanding["screen_state"] = (
                "editing_code"
            )

        for element in semantic_elements:

            intent = element["intent"]

            if intent not in understanding[
                "available_actions"
            ]:

                understanding[
                    "available_actions"
                ].append(intent)

        return understanding