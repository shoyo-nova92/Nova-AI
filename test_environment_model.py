from core.environment_model import (
    EnvironmentModel
)

env = EnvironmentModel()

env.update(

    app="VS Code",

    project="Nova",

    goal="Build Environment Layer",

    screen="Editing environment_model.py"
)

print(
    "\n=== ENVIRONMENT MODEL ===\n"
)

print(
    env.get_state()
)