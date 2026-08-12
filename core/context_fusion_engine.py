class ContextFusionEngine:

    def fuse(
        self,
        vision_data,
        reasoning_data,
        memory_data=None
    ):

        if not vision_data:
            vision_data = {}

        window_block = vision_data.get("active_window", {})
        if isinstance(window_block, dict):
            active_window = window_block.get("title") or window_block.get("name") or "unknown"
        else:
            active_window = str(window_block or "unknown")

        visible_text = vision_data.get("visible_text", {})
        if isinstance(visible_text, dict) and "text" in visible_text:
            visible_text = visible_text.get("text", [])
            visible_text = " ".join(str(item) for item in visible_text)
        elif isinstance(visible_text, dict):
            visible_text = " ".join(str(value) for value in visible_text.values())
        else:
            visible_text = str(visible_text or "")

        activity = reasoning_data.get("current_activity", "unknown") if reasoning_data else "unknown"

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