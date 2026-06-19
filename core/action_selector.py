class ActionSelector:

    def choose_action(
        self,
        understanding
    ):

        actions = (
            understanding.get(
                "available_actions",
                []
            )
        )

        if "save_document" in actions:

            return {

                "recommended_action":
                    "save_document",

                "reason":
                    "Document can be saved"

            }

        if "export_project" in actions:

            return {

                "recommended_action":
                    "export_project",

                "reason":
                    "Project appears exportable"

            }

        return {

            "recommended_action":
                "observe",

            "reason":
                "No obvious action"

        }