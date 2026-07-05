class PlanQualityAnalyzer:

    def analyze(
        self,
        parsed_plan
    ):

        total_items = len(parsed_plan)

        if total_items == 0:

            return {

                "total_items": 0,

                "actions": 0,

                "code_blocks": 0,

                "text_blocks": 0,

                "action_ratio": 0.0,

                "quality": "failed",

                "reason": "empty plan"

            }

        actions = 0
        code_blocks = 0
        text_blocks = 0

        for item in parsed_plan:

            item_type = item.get("type")

            if item_type == "action":
                actions += 1

            elif item_type == "code":
                code_blocks += 1

            elif item_type == "text":
                text_blocks += 1

        action_ratio = actions / total_items

        if action_ratio >= 0.70:

            quality = "excellent"

            reason = (
                "planner produced mostly executable actions"
            )

        elif action_ratio >= 0.40:

            quality = "good"

            reason = (
                "planner contains a balanced mix of actions and context"
            )

        elif action_ratio >= 0.10:

            quality = "poor"

            reason = (
                "planner produced mostly explanatory content"
            )

        else:

            quality = "failed"

            reason = (
                "planner produced almost no executable actions"
            )

        return {

            "total_items": total_items,

            "actions": actions,

            "code_blocks": code_blocks,

            "text_blocks": text_blocks,

            "action_ratio": round(
                action_ratio,
                2
            ),

            "quality": quality,

            "reason": reason

        }