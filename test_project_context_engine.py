from core.project_context_engine import (
    ProjectContextEngine
)

engine = (
    ProjectContextEngine()
)

print(
    engine.get_context()
)