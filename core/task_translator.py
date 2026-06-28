import re


class TaskTranslator:

    def translate(
        self,
        step
    ):

        normalized_step = (
            step
            or
            ""
        ).lower()
        
        
        file_target = (
            self._extract_create_file_target(
                step
            )
        )

        if file_target:

            return {

                "type":
                    "filesystem",

                "action":
                    "create_file",

                "action_type":
                    "create_file",

                "target":
                    file_target

            }

        folder_target = (
            self._extract_create_folder_target(
                step
            )
        )

        if folder_target:

            return {

                "type":
                    "filesystem",

                "action":
                    "create_folder",

                "action_type":
                    "create_folder",

                "target":
                    folder_target

            }

        modify_target = (
            self._extract_modify_file_target(
                step
            )
        )

        if modify_target:

            return {

                "type":
                    "filesystem",

                "action":
                    "modify_file",

                "action_type":
                    "modify_file",

                "target":
                    modify_target

            }
        vscode_open_phrases = [

            "open vscode",
            "open vs code",
            "open visual studio code",
            "launch vscode",
            "launch vs code",
            "launch visual studio code",
            "start vscode",
            "start vs code",
            "start visual studio code",
            "focus vscode",
            "focus vs code",
            "focus visual studio code"

        ]

        if any(
            phrase in normalized_step
            for phrase in vscode_open_phrases
        ):

            return {

                "type":
                    "application",

                "action":
                    "open_app",

                "action_type":
                    "open_app",

                "target":
                    "vscode"

            }

        if "git status" in normalized_step:

            return {

                "type":
                    "terminal",

                "action":
                    "git_status",

                "action_type":
                    "git_status",

                "target":
                    None

            }

        if "terminal" in normalized_step:

            return {

                "type":
                    "terminal",

                "action":
                    "open_terminal",

                "action_type":
                    "open_terminal",

                "target":
                    None

            }

        return {

            "type":
                None,

            "action":
                None,

            "action_type":
                None,

            "target":
                None

        }

    def _extract_create_file_target(
        self,
        step
    ):

        if not step:

            return None

        normalized_step = step.lower()

        if (
            "create" not in normalized_step
            and not re.search(
                r"\badd\s+file\b",
                normalized_step
            )
        ):

            return None

        file_pattern = (
            r"[\w./\\-]+"
            r"\."
            r"(?:json|yaml|yml|tsx|jsx|toml|html|"
            r"css|txt|ini|py|ts|js|md)"
        )

        backtick_match = re.search(
            rf"`({file_pattern})`",
            step,
            re.IGNORECASE
        )

        if backtick_match:

            return self._clean_target(
                backtick_match.group(1)
            )

        create_match = re.search(
            rf"\b(?:create\s+(?:file\s+)?|add\s+file\s+)"
            rf"[\"']?({file_pattern})[\"']?",
            step,
            re.IGNORECASE
        )

        if create_match:

            return self._clean_target(
                create_match.group(1)
            )

        return None

    def _extract_create_folder_target(
        self,
        step
    ):

        if not step:

            return None

        folder_match = re.search(
            r"\b(?:create|add)\b\s+"
            r"(?:folder|directory)\s+"
            r"[\"']?([\w./\\-]+)[\"']?",
            step,
            re.IGNORECASE
        )

        if folder_match:

            return self._clean_target(
                folder_match.group(1)
            )

        return None

    def _clean_target(
        self,
        target
    ):

        return (
            target
            .strip()
            .strip("`'\".,:;")
            .replace("\\", "/")
        )

    def _extract_modify_file_target(
        self,
        step
    ):

        if not step:

            return None

        file_pattern = (
            r"[\w./\\-]+"
            r"\."
            r"(?:json|yaml|yml|tsx|jsx|toml|html|"
            r"css|txt|ini|py|ts|js|md)"
        )

        direct_match = re.search(
            rf"\b(?:update|modify|edit)\b\s+"
            rf"[\"']?({file_pattern})[\"']?",
            step,
            re.IGNORECASE
        )

        if direct_match:

            return self._clean_target(
                direct_match.group(1)
            )

        add_to_match = re.search(
            rf"\badd\b\s+.+?\s+(?:to|in)\s+"
            rf"[\"']?({file_pattern})[\"']?",
            step,
            re.IGNORECASE
        )

        if add_to_match:

            return self._clean_target(
                add_to_match.group(1)
            )

        return None