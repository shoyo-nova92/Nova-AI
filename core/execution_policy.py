class ExecutionPolicy:

    SAFE = "SAFE"
    CONFIRMATION_REQUIRED = "CONFIRMATION_REQUIRED"
    BLOCKED = "BLOCKED"

    def classify(
        self,
        action
    ):

        action_type = action.get(
            "type"
        )

        action_name = action.get(
            "action"
        )

        safe_actions = {

            (
                "filesystem",
                "create_file"
            ),

            (
                "filesystem",
                "create_folder"
            ),

            (
                "terminal",
                "git_status"
            ),

            (
                "terminal",
                "open_terminal"
            ),

            (
                "application",
                "open_app"
            ),

            (
                "browser",
                "search"
            )

        }

        confirmation_actions = {

            (
                "filesystem",
                "delete_file"
            ),

            (
                "filesystem",
                "delete_folder"
            ),

            (
                "filesystem",
                "move_file"
            ),

            (
                "filesystem",
                "rename_file"
            ),

            (
                "application",
                "install_app"
            )

        }

        blocked_actions = {

            (
                "system",
                "format_disk"
            ),

            (
                "system",
                "shutdown"
            ),

            (
                "system",
                "edit_registry"
            )

        }

        action_key = (
            action_type,
            action_name
        )

        if action_key in safe_actions:

            return {

                "status":
                    self.SAFE,

                "allowed":
                    True,

                "reason":
                    "action is safe"

            }

        if action_key in confirmation_actions:

            return {

                "status":
                    self.CONFIRMATION_REQUIRED,

                "allowed":
                    False,

                "reason":
                    "confirmation required"

            }

        if action_key in blocked_actions:

            return {

                "status":
                    self.BLOCKED,

                "allowed":
                    False,

                "reason":
                    "action is blocked"

            }

        return {

            "status":
                self.CONFIRMATION_REQUIRED,

            "allowed":
                False,

            "reason":
                "unknown action requires review"

        }
