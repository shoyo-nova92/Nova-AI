class PlannerConfidence:

    def estimate(
        self,
        quality_report
    ):

        quality = quality_report.get(
            "quality"
        )

        action_ratio = quality_report.get(
            "action_ratio",
            0.0
        )

        confidence = action_ratio

        if quality == "excellent":
            confidence += 0.20

        elif quality == "good":
            confidence += 0.10

        elif quality == "poor":
            confidence -= 0.10

        elif quality == "failed":
            confidence = 0.0

        confidence = max(
            0.0,
            min(
                1.0,
                confidence
            )
        )

        return {

            "confidence": round(
                confidence,
                2
            ),

            "quality": quality,

            "recommended_execution":

                confidence >= 0.60

        }