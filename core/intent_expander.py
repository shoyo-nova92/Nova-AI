import re


class IntentExpander:

    def __init__(self):

        self.intent_map = {

            "parser": "parser.py",

            "validator": "workflow_validator.py",

            "validation": "workflow_validator.py",

            "router": "execution_router.py",

            "planner": "llm_planner.py",

            "translator": "task_translator.py",

            "memory": "memory_retriever.py",

            "context": "context_fusion_engine.py",

            "reasoning": "reasoning_engine.py",

            "vision": "vision_engine.py"

        }

        self.intent_actions = {

            "implement":
                "implement",

            "add":
                "implement",

            "build":
                "implement",

            "optimize":
                "optimize",

            "refactor":
                "refactor",

            "test":
                "test",

            "fix":
                "fix",

            "document":
                "document",

            "cleanup":
                "cleanup",

            "clean up":
                "cleanup"

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

        intent_action = self._detect_intent_action(
            raw
        )

        if intent_action is None:
            return action

        target = self._detect_target(
            raw
        )

        if target is None:
            return action

        return {

            "type":
                "engineering",

            "action":
                intent_action,

            "target":
                target

        }

    def _detect_intent_action(
        self,
        raw
    ):

        for keyword, intent_action in self.intent_actions.items():

            if self._matches_keyword(
                raw,
                keyword
            ):

                return intent_action

        return None

    def _detect_target(
        self,
        raw
    ):

        for keyword, target in self.intent_map.items():

            if self._matches_keyword(
                raw,
                keyword
            ):

                return target

        return None

    def _matches_keyword(
        self,
        raw,
        keyword
    ):

        if " " in keyword:

            return keyword in raw

        return re.search(
            rf"\b{re.escape(keyword)}\b",
            raw
        ) is not None
