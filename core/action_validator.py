class ActionValidator:

    def validate(
        self,
        action,
        context
    ):

        activity = context["activity"]

        active_window = (
            context["active_window"]
        ).lower()

        action = action.lower()

        result = {

            "valid": False,

            "confidence": 0.0,

            "reason": ""

        }

        # CODING CONTEXT
        if activity == "coding":

            coding_actions = [

                "open terminal",
                "debug",
                "run",
                "test",
                "search documentation",
                "review architecture"

            ]

            if any(
                keyword in action
                for keyword in coding_actions
            ):

                result["valid"] = True

                result["confidence"] = 0.9

                result["reason"] = (
                    "Action matches coding workflow."
                )

        # DESIGN CONTEXT
        elif activity == "design_editing":

            design_actions = [

                "export",
                "open photoshop",
                "import",
                "edit",
                "render"

            ]

            if any(
                keyword in action
                for keyword in design_actions
            ):

                result["valid"] = True

                result["confidence"] = 0.9

                result["reason"] = (
                    "Action matches design workflow."
                )

        # STUDY CONTEXT
        elif activity == "studying":

            study_actions = [

                "open notes",
                "study",
                "summarize",
                "review"

            ]

            if any(
                keyword in action
                for keyword in study_actions
            ):

                result["valid"] = True

                result["confidence"] = 0.85

                result["reason"] = (
                    "Action matches study workflow."
                )

        # HIGH RISK ACTIONS
        dangerous_actions = [

            "delete",
            "format",
            "shutdown",
            "remove"

        ]

        if any(
            keyword in action
            for keyword in dangerous_actions
        ):

            result["valid"] = False

            result["confidence"] = 0.1

            result["reason"] = (
                "High-risk action requires approval."
            )

        return result