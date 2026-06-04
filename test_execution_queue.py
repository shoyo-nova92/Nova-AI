from core.planner import Planner
from core.parser import IntentParser
from core.executor import Executor
from core.execution_queue import ExecutionQueue


planner = Planner()
parser = IntentParser()
executor = Executor()
queue = ExecutionQueue(executor, parser)

goal = input("Enter Goal: ")

plan = planner.create_plan(goal)

print("\nGenerated Plan:")
for step in plan:
    print("-", step)

queue.run_plan(plan)

