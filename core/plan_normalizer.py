from core.task_translator import (
    TaskTranslator
)


class PlanNormalizer:

    def __init__(self):

        self.translator = (
            TaskTranslator()
        )

    def normalize(
        self,
        plan
    ):

        normalized_plan = []

        for step in plan:

            action = (
                self.translator.translate(
                    step
                )
            )

            if action.get("action") is not None:
                normalized_plan.append(action)

        return normalized_plan