class IntentExpander:

    def __init__(self):

        self.intent_map = {

            "parser": "parser.py",

            "validator": "workflow_validator.py",

            "router": "execution_router.py",

            "planner": "llm_planner.py",

            "translator": "task_translator.py",

            "memory": "memory_retriever.py",

            "context": "context_fusion_engine.py",

            "reasoning": "reasoning_engine.py",

            "vision": "vision_engine.py"

        }

    def expand(
        self,
        action
    ):

        if not isinstance(
            action,
            dict
        ):
            return action

        if action.get(
            "action"
        ) is not None:

            expanded = dict(
                action
            )

            expanded.pop(
                "raw",
                None
            )

            return expanded

        raw = (
            action.get(
                "raw"
            )
            or
            ""
        ).lower()

        if "implement" not in raw:
            return action

        for keyword, target in self.intent_map.items():

            if keyword in raw:

                return {

                    "type":
                        "engineering",

                    "action":
                        "implement",

                    "target":
                        target

                }

        return action
