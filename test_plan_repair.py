from core.plan_repair_engine import PlanRepairEngine


engine = PlanRepairEngine()

plan = [

    {
        "type": "engineering",
        "action": "implement",
        "target": "parser.py"
    }

]

print(engine.repair(plan))
