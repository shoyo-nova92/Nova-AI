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
                "filesystem",
                "read_file"
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
                "terminal",
                "run_python"
            ),

            (
                "terminal",
                "run_pytest"
            ),

            (
                "terminal",
                "pip_install"
            ),

            (
                "terminal",
                "build_project"
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
                "modify_file"
            ),

            (
                "filesystem",
                "replace_text"
            ),

            (
                "filesystem",
                "append_file"
            ),

            (
                "filesystem",
                "insert_at_line"
            ),

            (
                "filesystem",
                "rollback_file"
            ),

            (
                "terminal",
                "run_python"
            ),

            (
                "terminal",
                "run_pytest"
            ),

            (
                "terminal",
                "pip_install"
            ),

            (
                "terminal",
                "build_project"
            ),

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
