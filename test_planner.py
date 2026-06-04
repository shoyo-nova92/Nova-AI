from core.planner import Planner

planner = Planner()

command = input("Enter Goal:")

plan = planner.create_plan(command)

print("\nGenerated Plan:")
for step in plan:
    print("-", step)