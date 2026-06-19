from core.desktop_understanding_engine import (
    DesktopUnderstandingEngine
)

engine = DesktopUnderstandingEngine()

result = engine.understand(

    {
        "active_window":
        "main.py - Visual Studio Code"
    },

    [
        {
            "intent":"save_document"
        },
        {
            "intent":"export_project"
        }
    ]

)

print(result)