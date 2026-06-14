from core.goal_runtime import (
    GoalRuntime
)

runtime = (
    GoalRuntime()
)

results = runtime.run_goal(
    "open notepad"
)

print(
    "\n=== GOAL RESULTS ===\n"
)

for result in results:

    print(result)