class PlanRepairEngine:

    def repair(
        self,
        validated_plan
    ):

        repaired_plan = []

        for action in validated_plan:

            if (
                action.get("type") == "engineering"
                and action.get("action") == "implement"
            ):

                target = action.get(
                    "target"
                )

                repaired_plan.extend([
                    {
                        "type": "filesystem",
                        "action": "read_file",
                        "target": target
                    },
                    {
                        "type": "filesystem",
                        "action": "modify_file",
                        "target": target
                    },
                    {
                        "type": "terminal",
                        "action": "run_pytest",
                        "target": None
                    },
                    {
                        "type": "verification",
                        "action": "verify",
                        "target": target
                    }
                ])

                continue

            repaired_plan.append(
                action
            )

        return repaired_plan
