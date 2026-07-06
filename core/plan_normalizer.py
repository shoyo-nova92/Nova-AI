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

            raw_step = None

            if isinstance(
                item,
                str
            ):

                raw_step = item

            elif isinstance(
                item,
                dict
            ):

                if item.get("type") not in [
                    PlanParser.ACTION,
                    PlanParser.TEXT
                ]:
                    continue

                raw_step = item.get(
                    "text"
                )

            if not raw_step:
                continue

            action = (
                self.translator.translate(
                    raw_step
                )
            )

            if action.get("action") is not None:

                normalized_plan.append(
                    action
                )

            else:

                normalized_plan.append({

                    "raw":
                        raw_step,

                    "type":
                        None,

                    "action":
                        None,

                    "target":
                        None

                })

        return normalized_plan
