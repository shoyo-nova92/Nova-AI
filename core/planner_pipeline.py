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

from core.plan_validator import (
    PlanValidator
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

        self.validator = (
            PlanValidator()
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

        validated = (
            self.validator.validate(
                normalized
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

            "validated_plan":
                validated

        }
