from core.plan_parser import (
    PlanParser
)

from core.plan_quality_analyzer import (
    PlanQualityAnalyzer
)

from core.planner_confidence import (
    PlannerConfidence
)

from core.plan_normalizer import (
    PlanNormalizer
)

from core.intent_expander import (
    IntentExpander
)

from core.plan_validator import (
    PlanValidator
)

from core.plan_repair_engine import (
    PlanRepairEngine
)


class PlannerPipeline:

    def __init__(self):

        self.parser = (
            PlanParser()
        )

        self.analyzer = (
            PlanQualityAnalyzer()
        )

        self.confidence = (
            PlannerConfidence()
        )

        self.normalizer = (
            PlanNormalizer()
        )

        self.expander = (
            IntentExpander()
        )

        self.validator = (
            PlanValidator()
        )

        self.repair_engine = (
            PlanRepairEngine()
        )

    def process(
        self,
        raw_plan
    ):

        parser_input = raw_plan

        if isinstance(
            raw_plan,
            list
        ):

            parser_input = (
                "\n".join(raw_plan)
            )

        parsed = (
            self.parser.parse(
                parser_input
            )
        )

        quality = (
            self.analyzer.analyze(
                parsed
            )
        )

        confidence = (
            self.confidence.estimate(
                quality
            )
        )

        normalized = (
            self.normalizer.normalize(
                parsed
            )
        )

        expanded = [
            self.expander.expand(
                action
            )
            for action in normalized
        ]

        validated = (
            self.validator.validate(
                expanded
            )
        )

        repaired = (
            self.repair_engine.repair(
                validated
            )
        )

        return {

            "raw_plan":
                raw_plan,

            "parsed_plan":
                parsed,

            "quality":
                quality,

            "confidence":
                confidence,

            "normalized_plan":
                normalized,

            "expanded_plan":
                expanded,

            "validated_plan":
                validated,

            "repaired_plan":
                repaired

        }
