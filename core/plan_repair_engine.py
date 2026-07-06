class PlanRepairEngine:

    USE_ACTION_TARGET = "__target__"

    REPAIR_RULES = {

        "cleanup": [
            ("filesystem", "read_file", USE_ACTION_TARGET),
            ("filesystem", "modify_file", USE_ACTION_TARGET),
            ("terminal", "run_pytest", None),
            ("verification", "verify", USE_ACTION_TARGET)
        ],

        "document": [
            ("filesystem", "read_file", USE_ACTION_TARGET),
            ("filesystem", "modify_file", USE_ACTION_TARGET),
            ("verification", "verify", USE_ACTION_TARGET)
        ],

        "fix": [
            ("filesystem", "read_file", USE_ACTION_TARGET),
            ("filesystem", "modify_file", USE_ACTION_TARGET),
            ("terminal", "run_pytest", None),
            ("verification", "verify", USE_ACTION_TARGET)
        ],

        "implement": [
            ("filesystem", "read_file", USE_ACTION_TARGET),
            ("filesystem", "modify_file", USE_ACTION_TARGET),
            ("terminal", "run_pytest", None),
            ("verification", "verify", USE_ACTION_TARGET)
        ],

        "optimize": [
            ("filesystem", "read_file", USE_ACTION_TARGET),
            ("filesystem", "modify_file", USE_ACTION_TARGET),
            ("terminal", "run_benchmark", None),
            ("terminal", "run_pytest", None),
            ("verification", "verify", USE_ACTION_TARGET)
        ],

        "refactor": [
            ("filesystem", "read_file", USE_ACTION_TARGET),
            ("filesystem", "modify_file", USE_ACTION_TARGET),
            ("terminal", "run_pytest", None),
            ("verification", "verify", USE_ACTION_TARGET)
        ],

        "test": [
            ("terminal", "run_pytest", None),
            ("verification", "verify", USE_ACTION_TARGET)
        ]

    }

    def repair(
        self,
        validated_plan
    ):

        repaired_plan = []

        for action in validated_plan:

            if (
                action.get("type") == "engineering"
                and action.get("action") in self.REPAIR_RULES
            ):

                repaired_plan.extend(
                    self._build_workflow(
                        action
                    )
                )

                continue

            repaired_plan.append(
                action
            )

        return repaired_plan

    def _build_workflow(
        self,
        action
    ):

        target = action.get(
            "target"
        )

        workflow = []

        for (
            action_type,
            action_name,
            action_target
        ) in self.REPAIR_RULES[action["action"]]:

            if action_target == self.USE_ACTION_TARGET:

                action_target = target

            workflow.append({

                "type":
                    action_type,

                "action":
                    action_name,

                "target":
                    action_target

            })

        return workflow
