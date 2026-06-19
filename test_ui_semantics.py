from core.ui_semantics_engine import (
    UISemanticsEngine
)

engine = UISemanticsEngine()

result = engine.analyze([

    {
        "text": "Save"
    },

    {
        "text": "Export"
    }

])

print(result)