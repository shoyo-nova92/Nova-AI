class ContextFusionEngine:

    def fuse(
        self,
        vision_data,
        reasoning_data,
        memory_data=None
    ):

        active_window = (
            vision_data["active_window"]["title"]
        )

        visible_text = (
            vision_data["visible_text"]
        )

        activity = (
            reasoning_data["current_activity"]
        )

        combined_text = (
            str(active_window) +
            " " +
            str(visible_text)
        ).lower()

        context = {

            "activity": activity,

            "active_window": active_window,

            "project": None,

            "focus": None,

            "workflow_stage": None,

            "environment_summary": None

        }

        # NOVA DEVELOPMENT DETECTION
        if (

            "nova" in combined_text
            or "reasoning_engine" in combined_text
            or "cognitive_loop" in combined_text
            or "vision_engine" in combined_text

        ):

            context["project"] = (
                "Nova v0.6"
            )

            context["focus"] = (
                "AI operating system development"
            )

            context["workflow_stage"] = (
                "cognitive architecture development"
            )

            context["environment_summary"] = (

                "User is actively developing "
                "Nova cognitive systems inside VSCode."

            )

        # CODING CONTEXT
        elif activity == "coding":

            context["project"] = (
                "Software Development"
            )

            context["focus"] = (
                "Coding workflow"
            )

            context["workflow_stage"] = (
                "implementation"
            )

            context["environment_summary"] = (

                "User is currently coding "
                "inside development environment."

            )

        # STUDY CONTEXT
        elif activity == "studying":

            context["project"] = (
                "Study Session"
            )

            context["focus"] = (
                "Learning"
            )

            context["workflow_stage"] = (
                "knowledge acquisition"
            )

            context["environment_summary"] = (

                "User is focused on studying."

            )

        # DESIGN CONTEXT
        elif activity == "design_editing":

            context["project"] = (
                "Creative Work"
            )

            context["focus"] = (
                "Design editing"
            )

            context["workflow_stage"] = (
                "creative production"
            )

            context["environment_summary"] = (

                "User is actively editing design assets."

            )

        # DEFAULT
        else:

            context["environment_summary"] = (

                "Unable to determine "
                "high-confidence context."

            )

        return context