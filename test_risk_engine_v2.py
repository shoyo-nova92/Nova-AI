from core.risk_engine_v2 import (
    RiskEngineV2
)

engine = RiskEngineV2()

actions = [

    "open notepad",

    "delete project folder",

    "format drive"

]

for action in actions:

    print(
        f"\nACTION: {action}"
    )

    print(
        engine.assess(
            action
        )
    )