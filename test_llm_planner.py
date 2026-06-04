from core.llm_planner import LLMPlanner

planner = LLMPlanner()

goal = input("Goal: ")

plan = planner.create_plan(goal)

print("\nGenerated Plan:\n")

for step in plan:
    print("-", step)