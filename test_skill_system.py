from core.skill_system import (
    SkillSystem
)

skills = (
    SkillSystem()
)

skills.update_skill(
    "vscode",
    True
)

skills.update_skill(
    "vscode",
    True
)

skills.update_skill(
    "vscode",
    False
)

print(
    "\n=== SKILL STATS ===\n"
)

print(

    skills.get_skill_stats(
        "vscode"
    )

)