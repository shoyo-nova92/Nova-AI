from core.planner_confidence import (
    PlannerConfidence
)

confidence = (
    PlannerConfidence()
)

report = {

    "quality":
        "good",

    "action_ratio":
        0.40

}

print(

    confidence.estimate(
        report
    )

)