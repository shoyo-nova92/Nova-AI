from core.action_abstraction import (
    ActionAbstraction
)

planner = (
    ActionAbstraction()
)

actions = planner.expand(
    "prepare coding environment"
)

print(
    "\n=== ACTION GRAPH ===\n"
)

for step in actions:

    print(step)