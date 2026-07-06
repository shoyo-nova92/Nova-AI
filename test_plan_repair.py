from core.plan_repair_engine import PlanRepairEngine


engine = PlanRepairEngine()

plan = [

    {
        "type": "engineering",
        "action": "implement",
        "target": "parser.py"
    },

    {
        "type": "engineering",
        "action": "optimize",
        "target": "llm_planner.py"
    },

    {
        "type": "engineering",
        "action": "refactor",
        "target": "execution_router.py"
    },

    {
        "type": "engineering",
        "action": "test",
        "target": "parser.py"
    }

]

print(engine.repair(plan))
