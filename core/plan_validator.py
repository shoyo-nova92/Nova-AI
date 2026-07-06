class PlanValidator:

    REQUIRED_FIELDS = [

        "type",

        "action",

        "target"

    ]

    ALLOWED_ACTIONS = {

        "application": {
            "open_app"
        },

        "browser": {
            "search"
        },

        "engineering": {
            "cleanup",
            "document",
            "fix",
            "implement",
            "optimize",
            "refactor",
            "test"
        },

        "filesystem": {
            "create_file",
            "create_folder",
            "delete_file",
            "delete_folder",
            "modify_file",
            "move_file",
            "read_file",
            "rename_file"
        },

        "terminal": {
            "git_status",
            "open_terminal",
            "run_benchmark",
            "run_pytest"
        },

        "verification": {
            "verify"
        }

    }

    TARGET_OPTIONAL = {

        (
            "terminal",
            "git_status"
        ),

        (
            "terminal",
            "open_terminal"
        ),

        (
            "terminal",
            "run_benchmark"
        ),

        (
            "terminal",
            "run_pytest"
        )

    }

    def validate(
        self,
        actions
    ):

        valid_actions = []

        for action in actions:

            if not isinstance(
                action,
                dict
            ):
                continue

            if not all(
                field in action
                for field in self.REQUIRED_FIELDS
            ):
                continue

            if action["action"] is None:
                continue

            action_type = action.get(
                "type"
            )

            action_name = action.get(
                "action"
            )

            if not self._is_allowed_action(
                action_type,
                action_name
            ):
                continue

            if not self._has_valid_target(
                action_type,
                action_name,
                action.get("target")
            ):
                continue

            valid_actions.append(
                action
            )

        return valid_actions

    def _is_allowed_action(
        self,
        action_type,
        action_name
    ):

        allowed_actions = self.ALLOWED_ACTIONS.get(
            action_type
        )

        if allowed_actions is None:
            return False

        return action_name in allowed_actions

    def _has_valid_target(
        self,
        action_type,
        action_name,
        target
    ):

        if (
            action_type,
            action_name
        ) in self.TARGET_OPTIONAL:
            return True

        if target is None:
            return False

        if isinstance(
            target,
            str
        ):

            return bool(
                target.strip()
            )

        return True
