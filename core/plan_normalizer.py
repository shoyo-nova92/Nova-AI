from core.task_translator import (
    TaskTranslator
)

from core.plan_parser import (
    PlanParser
)


class PlanNormalizer:

    def __init__(self):

        self.translator = (
            TaskTranslator()
        )

    def normalize(
        self,
        parsed_plan
    ):

        normalized_plan = []

        for item in parsed_plan:

            if item["type"] != PlanParser.ACTION:
                continue

            action = (
                self.translator.translate(
                    item["text"]
                )
            )

            if action.get("action") is not None:

                normalized_plan.append(
                    action
                )

        return normalized_plan