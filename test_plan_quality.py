from core.plan_quality_analyzer import (
    PlanQualityAnalyzer
)

parsed = [

    {
        "type": "action",
        "text": "Open VS Code"
    },

    {
        "type": "action",
        "text": "Run git status"
    },

    {
        "type": "code"
    },

    {
        "type": "code"
    },

    {
        "type": "text"
    }

]

analyzer = (
    PlanQualityAnalyzer()
)

result = analyzer.analyze(
    parsed
)

print(result)