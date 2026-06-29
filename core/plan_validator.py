class PlanValidator:

    REQUIRED_FIELDS = [

        "type",

        "action",

        "target"

    ]

    def validate(
        self,
        actions
    ):

        valid_actions = []

        for action in actions:

            if not isinstance(
                action,
                dict
            ):
                continue

            if not all(
                field in action
                for field in self.REQUIRED_FIELDS
            ):
                continue

            if action["action"] is None:
                continue

            valid_actions.append(
                action
            )

        return valid_actions