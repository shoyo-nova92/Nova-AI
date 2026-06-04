class PlanValidator:

    VALID_ACTIONS = [
        "open",
        "search",
        "click",
        "close",
        "download",
        "play",
        "create",
        "move",
        "delete"
    ]

    def validate(self, steps):
        clean_steps = []

        for step in steps:
            lower = step.lower()

            if any(
                lower.startswith(action)
                for action in self.VALID_ACTIONS
            ):
                clean_steps.append(step)

        return clean_steps